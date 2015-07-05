// Decodes 'AsciiCoder.DecodeFiles' archives

use std::io::*;
use std::fs;
use std::env;
use std::process::exit;

trait ByteIterator: Iterator<Item=u8> {}
impl<T: Iterator<Item=u8>> ByteIterator for T {}

fn skip_header<T: ByteIterator>(bytes: &mut T) -> bool {
    let command = b"AsciiCoder.DecodeFiles";
    let mut idx = 0;
    while let Some(ch) = bytes.next() {
        if ch != command[idx] {
            idx = 0
        }
        if ch == command[idx] {
            idx += 1;
            if idx == command.len() {
                return true
            }
        }
    }
    false
}

fn decode<T: ByteIterator>(bytes: &mut T) -> Option<Vec<u8>> {
    const BASE: u8 = 48;
    let mut vec = Vec::new();
    let mut bits = 0;
    let mut buf: u32 = 0;
    for ch in bytes.filter(|&a| a > 32) {
        if ch >= BASE && ch < BASE + 64 {
            buf |= ((ch - BASE) as u32) << bits;
            bits += 6;
            if bits >= 8 {
                vec.push((buf & 0xFF) as u8);
                buf >>= 8;
                bits -= 8;
            }
        } else {
            return match ch {
                b'#' if bits == 0 => Some(vec),
                b'%' if bits == 2 => Some(vec),
                b'$' if bits == 4 => Some(vec),
                _ => None
            }
        }
    }
    None
}

fn read_number<T: ByteIterator>(bytes: &mut T) -> Option<i32> {
    let mut n: i32 = 0;
    let mut bits = 0;
    while let Some(b) = bytes.next() {
        if b >= 0x80 {
            n |= ((b - 0x80) as i32) << bits;
            bits += 7;
            if bits >= 32 {
                return None
            }
        } else {
            n |= (((b as i32) ^ 0x40) - 0x40) << bits;
            return Some(n)
        }
    }
    None
}

fn decompress<T: ByteIterator>(bytes: &mut T) -> Option<Vec<u8>> {
    let size = match read_number(bytes) {
        Some(n) if n >= 0 => n,
        _ => return None
    };

    const N: usize = 16384;
    let mut table = [0u8; N];
    let mut vec = Vec::new();
    let mut hash: usize = 0;
    let mut buf: u32 = 0;
    let mut bits = 0;

    for _ in 0..size {
        if bits == 0 {
            buf = match bytes.next() {
                Some(b) => b as u32,
                None => return None
            };
            bits = 8
        }

        let misprediction = (buf & 1) != 0;
        buf >>= 1;
        bits -= 1;

        let data: u8;
        if !misprediction {
            data = table[hash]
        } else {
            let b = match bytes.next() {
                Some(b) => b,
                None => return None
            };
            buf |= (b as u32) << bits;
            data = (buf & 0xFF) as u8;
            buf >>= 8;
            table[hash] = data;
        }
        vec.push(data);
        hash = (16 * hash + data as usize) % N;
    }
    Some(vec)
}

fn read_name<T: ByteIterator>(bytes: &mut T) -> Option<String> {
    let vec = bytes.skip_while(|&b| b <= 32)
        .take_while(|&b| b > 32)
        .collect();
    let name = String::from_utf8(vec).unwrap();
    if name.is_empty() { None } else { Some(name) }
}

macro_rules! printerr {
    ($prog_name: expr, $err: expr, $fmt: expr) => (
        (writeln!(&mut stderr(),
                  concat!("{}: ", $fmt, ": {}"),
                  $prog_name, err)
         .unwrap()));
    ($prog_name: expr, $err: expr, $fmt: expr, $($arg: tt)*) => (
        (writeln!(&mut stderr(),
                  concat!("{}: ", $fmt, ": {}"),
                  $prog_name, $($arg)*, $err)
         .unwrap()))
}

macro_rules! printerrx {
    ($prog_name: expr, $fmt: expr) => (
        (writeln!(&mut stderr(),
                  concat!("{}: ", $fmt),
                  $prog_name)
         .unwrap()));
    ($prog_name: expr, $fmt: expr, $($arg: tt)*) => (
        (writeln!(&mut stderr(),
                  concat!("{}: ", $fmt),
                  $prog_name, $($arg)*)
         .unwrap()))
}

fn main() {
    let mut verbose = false;
    let mut directory = None;
    let mut input_file = None;

    let mut args = env::args();
    let progname = args.next().unwrap().rsplit('/').next().unwrap().to_owned();
    while let Some(opt) = args.next() {
        match opt.as_ref() {
            "-v" | "--verbose" => {
                verbose = true
            },
            "-C" | "--directory" if directory.is_none() => {
                directory = args.next()
            },
            s if !s.starts_with("-") && input_file.is_none() => {
                input_file = Some(opt.clone())
            },
            _ => {
                writeln!(&mut stderr(), "Usage: {} [-v] [-C DIR] [FILE]", progname).unwrap();
                exit(1)
            }
        }
    }

    let input: Box<Read> = match input_file {
        None => Box::new(stdin()),
        Some(filename) => match fs::File::open(&filename) {
            Ok(f) => Box::new(BufReader::new(f)),
            Err(e) => {
                printerr!(progname, e, "can't open '{}'", filename);
                exit(1)
            }
        }
    };
    let mut input: Box<Iterator<Item=u8>> = Box::new(input.bytes().map(|x| x.unwrap()));

    if let Some(directory) = directory {
        if let Err(e) = env::set_current_dir(&directory) {
            if e.kind() != ErrorKind::NotFound {
                printerr!(progname, e, "can't change to directory '{}'", directory);
                exit(1);
            }
            if let Err(e) = fs::create_dir_all(&directory) {
                printerr!(progname, e, "can't create directory '{}'", directory);
                exit(1);
            }
            if let Err(e) = env::set_current_dir(&directory) {
                printerr!(progname, e, "can't change to directory '{}'", directory);
                exit(1);
            }
        }
    }

    let mut compressed = false;
    let mut names = Vec::new();
    if !skip_header(&mut input) {
        printerrx!(progname, "no AsciiCoder.DecodeFiles archive found");
        exit(1);
    }
    while let Some(name) = read_name(&mut input) {
        match name.as_ref() {
            "~" => break,
            "%" => compressed = true,
            _ => names.push(name)
        }
    }

    for name in &names {
        if verbose {
            println!("{}", name);
        }

        let mut data = match decode(&mut input) {
            Some(vec) => vec,
            None => {
                printerrx!(progname, "can't decode '{}' (input file truncated?)", name);
                exit(1)
            }
        };
        if compressed {
            data = match decompress(&mut data.into_iter()) {
                Some(vec) => vec,
                None => {
                    printerrx!(progname, "can't decompress '{}'", name);
                    exit(1)
                }
            }
        };

        let mut file = match fs::File::create(&name) {
            Ok(f) => f,
            Err(e) => {
                printerr!(progname, e, "can't create file '{}'", name);
                continue;
            }
        };

        if let Err(e) = file.write_all(&data) {
            printerr!(progname, e, "can't write file '{}'", name);
            fs::remove_file(&name).unwrap();
            continue;
        }
    }
}
