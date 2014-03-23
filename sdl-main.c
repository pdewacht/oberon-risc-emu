#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
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


static void init_texture(SDL_Texture *texture);
static void update_texture(uint32_t *framebuffer, SDL_Texture *texture);

static int clamp(int x, int min, int max);
static SDL_Rect scale_display(SDL_Window *window, double *scale);

static void usage() {
  fprintf(stderr, "Usage: risc [--fullscreen] disk-file-name\n");
  exit(1);
}

int main (int argc, char *argv[]) {
  bool fullscreen = false;
  while (argc > 1 && argv[1][0] == '-') {
    if (strcmp(argv[1], "--fullscreen") == 0) {
      fullscreen = true;
    } else {
      usage();
    }
    argc--; argv++;
  }
  if (argc != 2) {
    usage();
  }

  struct RISC *risc = risc_new(argv[1]);

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
    return 1;
  }
  atexit(SDL_Quit);

  SDL_EnableScreenSaver();
  SDL_ShowCursor(false);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");

  int window_pos = SDL_WINDOWPOS_UNDEFINED;
  int window_flags = SDL_WINDOW_HIDDEN;
  if (fullscreen) {
    window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    // Search for a 1024x768 display in multi-monitor configurations
    int display_cnt = SDL_GetNumVideoDisplays();
    for (int i = 0; i < display_cnt; i++) {
      SDL_Rect bounds;
      SDL_GetDisplayBounds(i, &bounds);
      if (bounds.w >= SCREEN_WIDTH && bounds.h == SCREEN_HEIGHT) {
        window_pos = SDL_WINDOWPOS_UNDEFINED_DISPLAY(i);
        if (bounds.w == SCREEN_WIDTH)
          break;
      }
    }
  }

  SDL_Window *window = SDL_CreateWindow("Project Oberon",
                                        window_pos, window_pos,
                                        SCREEN_WIDTH, SCREEN_HEIGHT,
                                        window_flags);
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

  double display_scale;
  SDL_Rect display_rect = scale_display(window, &display_scale);
  init_texture(texture);
  SDL_ShowWindow(window);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, &display_rect);
  SDL_RenderPresent(renderer);

  bool done = false;
  bool mouse_was_offscreen = false;
  while (!done) {
    uint32_t frame_start = SDL_GetTicks();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT: {
          done = true;
        }

        case SDL_WINDOWEVENT: {
          if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
            display_rect = scale_display(window, &display_scale);
          }
          break;
        }

        case SDL_MOUSEMOTION: {
          int scaled_x = (int)round((event.motion.x - display_rect.x) * display_scale);
          int scaled_y = (int)round((event.motion.y - display_rect.y) * display_scale);
          int x = clamp(scaled_x, 0, SCREEN_WIDTH - 1);
          int y = clamp(scaled_y, 0, SCREEN_HEIGHT - 1);
          bool mouse_is_offscreen = x != scaled_x || y != scaled_y;
          if (mouse_is_offscreen != mouse_was_offscreen) {
            SDL_ShowCursor(mouse_is_offscreen);
            mouse_was_offscreen = mouse_is_offscreen;
          }
          risc_mouse_moved(risc, x, SCREEN_HEIGHT - y - 1);
          break;
        }

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
          bool down = event.button.state == SDL_PRESSED;
          risc_mouse_button(risc, event.button.button, down);
          break;
        }

        case SDL_KEYDOWN:
        case SDL_KEYUP: {
          bool down = event.key.state == SDL_PRESSED;
          SDL_Keysym k = event.key.keysym;
          if (k.sym == SDLK_F12) {
            // F12 = Oberon reset
            if (down) {
              risc_reset(risc);
            }
          } else if (k.sym == SDLK_F11 ||
                     (k.sym == SDLK_f && (k.mod & KMOD_GUI) && (k.mod & KMOD_SHIFT))) {
            // F11 / Cmd-Shift-F = Toggle fullscreen
            if (down) {
              fullscreen ^= true;
              if (fullscreen) {
                SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
              } else {
                SDL_SetWindowFullscreen(window, 0);
              }
            }
          } else if (k.sym == SDLK_LALT) {
            // Emulate middle button
            risc_mouse_button(risc, 2, down);
          } else {
            // Pass other keys to Oberon
            uint8_t scancode[MAX_PS2_CODE_LEN];
            int len = ps2_encode(k.scancode, down, scancode);
            risc_keyboard_input(risc, scancode, len);
          }
          break;
        }
      }
    }

    risc_set_time(risc, frame_start);
    risc_run(risc, CPU_HZ / FPS);

    update_texture(risc_get_framebuffer_ptr(risc), texture);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, &display_rect);
    SDL_RenderPresent(renderer);

    uint32_t frame_end = SDL_GetTicks();
    int delay = frame_start + 1000/FPS - frame_end;
    if (delay > 0) {
      SDL_Delay(delay);
    }
  }
  return 0;
}


static int clamp(int x, int min, int max) {
  if (x < min) return min;
  if (x > max) return max;
  return x;
}

static SDL_Rect scale_display(SDL_Window *window, double *display_scale) {
  int win_w, win_h;
  SDL_GetWindowSize(window, &win_w, &win_h);
  double oberon_aspect = (double)SCREEN_WIDTH / SCREEN_HEIGHT;
  double window_aspect = (double)win_w / win_h;
  if (fabs(oberon_aspect - window_aspect) < 0.0001) {
    // Aspect ratios are equal
    *display_scale = (double)SCREEN_WIDTH / win_w;
    return (SDL_Rect){ .x = 0, .y = 0, .w = win_w, .h = win_h };
  }
  else if (oberon_aspect > window_aspect) {
    // Oberon display is wider than our window -- letterbox it
    *display_scale = (double)SCREEN_WIDTH / win_w;
    int h = (int)ceil(SCREEN_HEIGHT / *display_scale);
    return (SDL_Rect){ .x = 0, .y = (win_h - h) / 2, .w = win_w, .h = h };
  } else {
    // Oberon display is taller than our window
    *display_scale = (double)SCREEN_HEIGHT / win_h;
    int w = (int)ceil(SCREEN_WIDTH / *display_scale);
    return (SDL_Rect){ .x = (win_w - w) / 2, .y = 0, .w = w, .h = win_h };
  }
}

static uint32_t cache[SCREEN_WIDTH * SCREEN_HEIGHT / 32];
static uint32_t buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

static void init_texture(SDL_Texture *texture) {
  memset(cache, 0, sizeof(cache));
  for (size_t i = 0; i < sizeof(buffer)/sizeof(buffer[0]); ++i) {
    buffer[i] = BLACK;
  }
  SDL_UpdateTexture(texture, NULL, buffer, SCREEN_WIDTH * 4);
}

static void update_texture(uint32_t *framebuffer, SDL_Texture *texture) {
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
