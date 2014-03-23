Oberon RISC Emulator
====================

This is an emulator for the Oberon RISC machine. For more information, see:
http://www.inf.ethz.ch/personal/wirth/ and http://projectoberon.com/.

Requirements: a C99 compiler (e.g. [GCC](http://gcc.gnu.org/),
[clang](http://clang.llvm.org/)) and [SDL2](http://libsdl.org/).

A suitable disk image can be downloaded from http://projectoberon.com/ (in
S3RISCinstall.zip).

Current emulation status
------------------------

* CPU
  * Mostly works. Floating point instructions still need testing.

* Keyboard and mouse
  * OK. Note that Oberon assumes you have a US keyboard layout and
    a three button mouse.
  * The left alt key can now be used to emulate a middle click.

* Display
  * OK. You can adjust the colors by editing `sdl-main.c`.
  * Use F11 to toggle full screne display.

* SD-Card
  * Very inaccurate, but good enough for Oberon. If you're going to
    hack the SD card routines, you'll need to use real hardware.

* RS-232
  * Implements PCLink protocol to send/receive single files at a time
    e.g. to receive Test.Mod into Oberon, run PCLink1.Start,
    then in host risc current directory, `echo Test.Mod > PCLink.REC`
  * Thanks to Paul Reed

* Network
  * Not implemented.

* LEDs
  * Printed on stdout.

* Reset button
  * Press F12 to abort if you get stuck in an infinite loop.


Copyright
---------

Copyright © 2014 Peter De Wachter

Permission to use, copy, modify, and/or distribute this software for
any purpose with or without fee is hereby granted, provided that the
above copyright notice and this permission notice appear in all
copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
