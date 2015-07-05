// Dumps the ASCII content of Oberon texts. Doesn't properly
// parse the files so doesn't work very well.

use std::io::*;
use std::process::exit;

fn ob2unix() -> Result<()> {
    let mut input = stdin();
    let mut output = stdout();

    let mut buf = [0u8; 1024];
    let res = try!(copy(&mut input.by_ref().take(6),
                        &mut &mut buf[..])) as usize;
    let is_oberon = res == 6 && ((buf[0] == 240 && buf[1] == 1)
                                 || buf[0] == 1 && buf[1] == 240);

    if is_oberon {
        // skip header
        let size = (buf[2] as u64) | (buf[3] as u64) << 8
            | (buf[4] as u64) << 16 | (buf[5] as u64) << 24;
        try!(copy(&mut input.by_ref().take(size - 6), &mut sink()));

        // translate '\r' to '\n'
        loop {
            let res = try!(copy(&mut input.by_ref().take(buf.len() as u64),
                                &mut &mut buf[..])) as usize;
            if res == 0 {
                break
            }
            for ch in &mut buf[..res] {
                if *ch == b'\r' {
                    *ch = b'\n'
                }
            }
            try!(copy(&mut &buf[..res], &mut output));
        };

    } else {
        // Not an Oberon text file: copy input to output
        try!(copy(&mut &buf[..res], &mut output));
        try!(copy(&mut input, &mut output));
    }

    Ok(())
}

fn main() {
    if let Err(e) = ob2unix() {
        writeln!(&mut stderr(), "ob2unix: {}", e).unwrap();
        exit(1);
    }
}
