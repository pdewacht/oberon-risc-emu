#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <SDL.h>
#include "risc.h"
#include "sdl-ps2.h"

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

#define CPU_HZ 25000000
#define FPS 60

static uint32_t BLACK = 0x657b83, WHITE = 0xfdf6e3;
//static uint32_t BLACK = 0x000000, WHITE = 0xFFFFFF;
//static uint32_t BLACK = 0x0000FF, WHITE = 0xFFFF00;
//static uint32_t BLACK = 0x000000, WHITE = 0x00FF00;

int64_t microseconds(void);
void render_to_texture(uint32_t *framebuffer, SDL_Texture *texture);

int main (int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: risc disk-file-name\n");
    return 1;
  }
  struct RISC *risc = risc_new(argv[1]);

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
    return 1;
  }
  atexit(SDL_Quit);

  SDL_Window *window = SDL_CreateWindow("Project Oberon",
                                        SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED,
                                        SCREEN_WIDTH, SCREEN_HEIGHT,
                                        SDL_WINDOW_SHOWN);
  if (window == NULL) {
    fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
  if (renderer == NULL) {
    fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Texture *texture = SDL_CreateTexture(renderer,
                                           SDL_PIXELFORMAT_ARGB8888,
                                           SDL_TEXTUREACCESS_STREAMING,
                                           SCREEN_WIDTH, SCREEN_HEIGHT);
  if (texture == NULL) {
    fprintf(stderr, "Could not create texture: %s\n", SDL_GetError());
    return 1;
  }

  SDL_ShowCursor(false);

  bool done = false;
  int64_t emulation_start = microseconds();
  SDL_Event event;
  while (!done) {
    int64_t frame_start = microseconds();

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
          done = true;
          break;
        case SDL_MOUSEMOTION:
          risc_mouse_moved(risc, event.motion.x, SCREEN_HEIGHT - event.motion.y - 1);
          break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
          risc_mouse_button(risc, event.button.button, event.button.state);
          break;
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
          uint8_t scancode[MAX_PS2_CODE_LEN];
          int len = ps2_encode(event.key.keysym.scancode,
                               event.key.state,
                               scancode);
          risc_keyboard_input(risc, scancode, len);
          break;
        }
      }
    }

    risc_set_time(risc, (frame_start - emulation_start) / 1000);
    risc_run(risc, CPU_HZ / FPS);

    render_to_texture(risc_get_framebuffer_ptr(risc), texture);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    int64_t frame_end = microseconds();
    int delay = (frame_start + 1000000/FPS - frame_end) / 1000;
    if (delay > 0) {
      SDL_Delay(delay);
    }
  }

  return 0;
}

int64_t microseconds(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

void render_to_texture(uint32_t *framebuffer, SDL_Texture *texture) {
  static uint32_t cache[SCREEN_WIDTH * SCREEN_HEIGHT / 32];
  static uint32_t buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

  static bool first = true;
  if (first) {
    first = false;
    memset(cache, 0, sizeof(cache));
    for (size_t i = 0; i < sizeof(buffer)/sizeof(buffer[0]); ++i) {
      buffer[i] = BLACK;
    }
    SDL_UpdateTexture(texture, NULL, buffer, SCREEN_WIDTH * 4);
  }

  // TODO: move dirty rectangle tracking into emulator core?
  int dirty_y1 = SCREEN_HEIGHT;
  int dirty_y2 = 0;
  int dirty_x1 = SCREEN_WIDTH / 32;
  int dirty_x2 = 0;

  int idx = 0;
  for (int line = SCREEN_HEIGHT - 1; line >= 0; line--) {
    for (int col = 0; col < SCREEN_WIDTH / 32; col++) {
      uint32_t pixels = framebuffer[idx];
      if (pixels != cache[idx]) {
        cache[idx] = pixels;
        if (line < dirty_y1) dirty_y1 = line;
        if (line > dirty_y2) dirty_y2 = line;
        if (col < dirty_x1) dirty_x1 = col;
        if (col > dirty_x2) dirty_x2 = col;

        uint32_t *buf_ptr = &buffer[line * SCREEN_WIDTH + col * 32];
        for (int b = 0; b < 32; b++) {
          *buf_ptr++ = (pixels & 1) ? WHITE : BLACK;
          pixels >>= 1;
        }
      }
      ++idx;
    }
  }

  if (dirty_y1 <= dirty_y2) {
    SDL_Rect rect = {
      .x = dirty_x1 * 32,
      .y = dirty_y1,
      .w = (dirty_x2 - dirty_x1 + 1) * 32,
      .h = dirty_y2 - dirty_y1 + 1,
    };
    void *ptr = &buffer[dirty_y1 * SCREEN_WIDTH + dirty_x1 * 32];
    SDL_UpdateTexture(texture, &rect, ptr, SCREEN_WIDTH * 4);
  }
}
