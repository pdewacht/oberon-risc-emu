CFLAGS = -g -Os -Wall -Wextra -Wconversion -Wno-sign-conversion -Wno-unused-parameter
SDL2_CONFIG = sdl2-config

RISC_CFLAGS = $(CFLAGS) -std=c99 `$(SDL2_CONFIG) --cflags --libs` -lm

RISC_SOURCE = \
	src/sdl-main.c \
	src/sdl-ps2.c src/sdl-ps2.h \
	src/risc.c src/risc.h src/risc-boot.inc \
	src/risc-fp.c src/risc-fp.h \
	src/disk.c src/disk.h \
	src/pclink.c src/pclink.h \
	src/raw-serial.c src/raw-serial.h \
	src/sdl-clipboard.c src/sdl-clipboard.h

risc: $(RISC_SOURCE)
	$(CC) -o $@ $(filter %.c, $^) $(RISC_CFLAGS)

# Assumes SDL2 framework download, following README instructions for install.
osx: $(RISC_SOURCE)
	gcc -framework SDL2 -F /Library/Frameworks -o risc $(filter %.c, $^) \
		-I  /Library/Frameworks/SDL2.framework/Headers/

clean:
	rm -f risc
