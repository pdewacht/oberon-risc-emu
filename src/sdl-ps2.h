#ifndef SDL_PS2_H
#define SDL_PS2_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_PS2_CODE_LEN 8

#ifndef STATIC_BUFSIZ
#define STATIC_BUFSIZ static
#endif

int ps2_encode(int sdl_scancode, bool make, uint8_t out[STATIC_BUFSIZ MAX_PS2_CODE_LEN]);

#endif  // SDL_PS2_H
