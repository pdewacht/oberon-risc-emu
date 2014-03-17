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
  * Mostly works. Floating point instructions are incomplete and
    not portable.

* Keyboard and mouse
  * OK. Note that Oberon assumes you have a US keyboard layout and
    a three button mouse.

* Display
  * OK. You can adjust the colors by editing `sdl-main.c`.

* SD-Card
  * Very inaccurate, but good enough for Oberon. If you're going to
    hack the SD card routines, you'll need to use real hardware.

* RS-232
  * Not implemented.

* Network
  * Not implemented.

* LEDs
  * Printed on stdout.
