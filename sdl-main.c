#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <SDL.h>
#include "risc.h"
#include "sdl-ps2.h"

#define CPU_HZ 25000000
#define FPS 60

static uint32_t BLACK = 0x657b83, WHITE = 0xfdf6e3;
//static uint32_t BLACK = 0x000000, WHITE = 0xFFFFFF;
//static uint32_t BLACK = 0x0000FF, WHITE = 0xFFFF00;
//static uint32_t BLACK = 0x000000, WHITE = 0x00FF00;


static void init_texture(SDL_Texture *texture);
static void update_texture(uint32_t *framebuffer, SDL_Texture *texture, int width, int height);

static int clamp(int x, int min, int max);
static double scale_display(SDL_Window *window, SDL_Rect *rect, int width, int height);

static void usage() {
  fprintf(stderr, "Usage: risc [--fullscreen] [--size <width> <height>] disk-file-name\n");
  exit(1);
}

int main (int argc, char *argv[]) {
  bool fullscreen = false;
  int width=0, height=0;
  while (argc > 1 && argv[1][0] == '-') {
    if (strcmp(argv[1], "--fullscreen") == 0) {
      fullscreen = true;
    } else if (strcmp(argv[1], "--size") == 0 && argc >= 5) {
      width = atoi(argv[2]);
      height = atoi(argv[3]);
      if (width < 1 || height < 1 || width > RISC_SCREEN_WIDTH || height > RISC_SCREEN_HEIGHT || width % 32 != 0) {
        usage();
      }
      argc-=2; argv+=2;
    } else {
      usage();
    }
    argc--; argv++;
  }
  if (argc != 2) {
    usage();
  }

  struct RISC *risc = risc_new(argv[1]);

  if (width) {
    risc_set_screen_size(risc, width, height);
  } else {
    width = RISC_SCREEN_WIDTH;
    height = RISC_SCREEN_HEIGHT;
  }

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
      if (bounds.w >= RISC_SCREEN_WIDTH && bounds.h == RISC_SCREEN_HEIGHT) {
        window_pos = SDL_WINDOWPOS_UNDEFINED_DISPLAY(i);
        if (bounds.w == RISC_SCREEN_WIDTH)
          break;
      }
    }
  }

  SDL_Window *window = SDL_CreateWindow("Project Oberon",
                                        window_pos, window_pos,
                                        width, height,
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
                                           width, height);
  if (texture == NULL) {
    fprintf(stderr, "Could not create texture: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Rect display_rect;
  double display_scale = scale_display(window, &display_rect, width, height);
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
            display_scale = scale_display(window, &display_rect, width, height);
          }
          break;
        }

        case SDL_MOUSEMOTION: {
          int scaled_x = (int)round((event.motion.x - display_rect.x) / display_scale);
          int scaled_y = (int)round((event.motion.y - display_rect.y) / display_scale);
          int x = clamp(scaled_x, 0, width - 1);
          int y = clamp(scaled_y, 0, height - 1);
          bool mouse_is_offscreen = x != scaled_x || y != scaled_y;
          if (mouse_is_offscreen != mouse_was_offscreen) {
            SDL_ShowCursor(mouse_is_offscreen);
            mouse_was_offscreen = mouse_is_offscreen;
          }
          risc_mouse_moved(risc, x, height - y - 1);
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
          } else if (k.sym == SDLK_F4 && (k.mod & KMOD_ALT)) {
            // Alt-F4 = quit (for when we're running without windowing system)
            if (down) {
              SDL_PushEvent(&(SDL_Event){ .type=SDL_QUIT });
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

    update_texture(risc_get_framebuffer_ptr(risc), texture, width, height);
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

static double scale_display(SDL_Window *window, SDL_Rect *rect, int width, int height) {
  int win_w, win_h;
  SDL_GetWindowSize(window, &win_w, &win_h);
  double oberon_aspect = (double)width / height;
  double window_aspect = (double)win_w / win_h;

  double scale;
  if (oberon_aspect > window_aspect) {
    scale = (double)win_w / width;
  } else {
    scale = (double)win_h / height;
  }

  int w = (int)ceil(width * scale);
  int h = (int)ceil(height * scale);
  *rect = (SDL_Rect){
    .w = w, .h = h,
    .x = (win_w - w) / 2,
    .y = (win_h - h) / 2
  };
  return scale;
}

static uint32_t cache[RISC_SCREEN_WIDTH * RISC_SCREEN_HEIGHT / 32];
static uint32_t buffer[RISC_SCREEN_WIDTH * RISC_SCREEN_HEIGHT];

static void init_texture(SDL_Texture *texture) {
  memset(cache, 0, sizeof(cache));
  for (size_t i = 0; i < sizeof(buffer)/sizeof(buffer[0]); ++i) {
    buffer[i] = BLACK;
  }
  SDL_UpdateTexture(texture, NULL, buffer, RISC_SCREEN_WIDTH * 4);
}

static void update_texture(uint32_t *framebuffer, SDL_Texture *texture, int width, int height) {
 // TODO: move dirty rectangle tracking into emulator core?
  int dirty_y1 = RISC_SCREEN_HEIGHT;
  int dirty_y2 = 0;
  int dirty_x1 = RISC_SCREEN_WIDTH / 32;
  int dirty_x2 = 0;

  int idx = 0;
  for (int line = height - 1; line >= 0; line--) {
    for (int col = 0; col < RISC_SCREEN_WIDTH / 32; col++) {
      uint32_t pixels = framebuffer[idx];
      if (col * 32 < width && pixels != cache[idx]) {
        cache[idx] = pixels;
        if (line < dirty_y1) dirty_y1 = line;
        if (line > dirty_y2) dirty_y2 = line;
        if (col < dirty_x1) dirty_x1 = col;
        if (col > dirty_x2) dirty_x2 = col;
        uint32_t *buf_ptr = &buffer[line * width + col * 32];
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
    void *ptr = &buffer[dirty_y1 * width + dirty_x1 * 32];
    SDL_UpdateTexture(texture, &rect, ptr, width * 4);
  }
}
