
CFLAGS = -g -Os -Wall -Wextra 

RISC_CFLAGS = $(CFLAGS) -std=c99 `sdl2-config --cflags --libs`

RISC_SOURCE = \
	sdl-main.c \
	sdl-ps2.c sdl-ps2.h \
	risc.c risc.h risc-boot.inc \
	risc-sd.c risc-sd.h

risc: $(RISC_SOURCE) 
	$(CC) -o $@ $(filter %.c, $^) $(RISC_CFLAGS)

clean:
	rm -f risc
