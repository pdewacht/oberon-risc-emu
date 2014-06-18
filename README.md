Oberon RISC Emulator
====================

This is an emulator for the Oberon RISC machine. For more information, see:
http://www.inf.ethz.ch/personal/wirth/ and http://projectoberon.com/.

Requirements: a C99 compiler (e.g. [GCC](http://gcc.gnu.org/),
[clang](http://clang.llvm.org/)) and [SDL2](http://libsdl.org/).

A suitable disk image can be downloaded from http://projectoberon.com/ (in
S3RISCinstall.zip).


Command line options
--------------------

Usage: `risc [options] disk-image.img`

* `--fullscreen` Start the emulator in fullscreen mode.
* `--size <width>x<height>` Use a non-standard window size.
  This requires modified Display and Input modules, see
  [the Oberon directory](Oberon/).
* `--serial-fd <fd>` Send serial I/O to file descriptor fd (input) and
  fd+1 (output). (You probably won't need this.)


Keyboard and mouse
------------------

The Oberon system assumes you have a US keyboard and a three button mouse.
You can use the left alt key to emulate a middle click.

The following keys are available:
* `Alt-F4` Quit the emulator.
* `F11` or `Shift-Command-F` Toggle fullscreen mode.
* `F12` Soft-reset the Oberon machine.


Transferring files
------------------

First start the PCLink1 task by middle-clicking on the PCLink1.Run command.
Transfer files using the pcreceive.sh and pcsend.sh scripts.


Clipboard integration
---------------------

Transfer and compile the Clipboard.Mod driver. This makes these commands
available:

* `Clipboard.Paste`
* `Clipboard.CopySelection`
* `Clipboard.CopyViewer`


Known issues
------------

* The wireless network interface is not emulated.
* Proper documentation is needed.


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
