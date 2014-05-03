#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include <SDL.h>
#include "risc.h"
#include "disk.h"
#include "pclink.h"
#include "raw-serial.h"
#include "sdl-ps2.h"
#include "sdl-clipboard.h"

#define CPU_HZ 25000000
#define FPS 60

static uint32_t BLACK = 0x657b83, WHITE = 0xfdf6e3;
//static uint32_t BLACK = 0x000000, WHITE = 0xFFFFFF;
//static uint32_t BLACK = 0x0000FF, WHITE = 0xFFFF00;
//static uint32_t BLACK = 0x000000, WHITE = 0x00FF00;


static void update_texture(struct RISC *risc, SDL_Texture *texture);
static double scale_display(SDL_Window *window, const SDL_Rect *risc_rect, SDL_Rect *display_rect);
static int clamp(int x, int min, int max);


static void usage() {
  fprintf(stderr, "Usage: risc [--fullscreen] [--size <width>x<height>] disk-file-name\n");
  exit(1);
}

static void sdl_error_if(bool error, const char *msg) {
  if (error) {
    fprintf(stderr, "%s: %s\n", msg, SDL_GetError());
    exit(1);
  }
}

static int best_display(const SDL_Rect *rect) {
  int result = SDL_WINDOWPOS_UNDEFINED;
  int display_cnt = SDL_GetNumVideoDisplays();
  for (int i = 0; i < display_cnt; i++) {
    SDL_Rect bounds;
    SDL_GetDisplayBounds(i, &bounds);
    if (bounds.h == rect->h && bounds.w >= rect->w) {
      result = SDL_WINDOWPOS_UNDEFINED_DISPLAY(i);
      if (bounds.w == rect->w)
        break;  // exact match
    }
  }
  return result;
}

static struct option long_options[] = {
  { "fullscreen", no_argument,       NULL, 'f' },
  { "size",       required_argument, NULL, 's' },
  { "serial-fd",  required_argument, NULL, 'F' },
  { NULL }
};

int main (int argc, char *argv[]) {
  struct RISC *risc = risc_new();
  risc_set_serial(risc, &pclink);
  risc_set_clipboard(risc, &sdl_clipboard);

  bool fullscreen = false;
  SDL_Rect risc_rect = {
    .w = RISC_FRAMEBUFFER_WIDTH,
    .h = RISC_FRAMEBUFFER_HEIGHT
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "fS:F:", long_options, NULL)) != -1) {
    switch (opt) {
      case 'f': {
        fullscreen = true;
        break;
      }
      case 's': {
        int w, h;
        if (sscanf(optarg, "%dx%d", &w, &h) != 2) {
          usage();
        }
        risc_rect.w = clamp(w, 32, RISC_FRAMEBUFFER_WIDTH) & ~31;
        risc_rect.h = clamp(h, 32, RISC_FRAMEBUFFER_HEIGHT);
        risc_rect.y = RISC_FRAMEBUFFER_HEIGHT - risc_rect.h;
        risc_screen_size_hack(risc, risc_rect.w, risc_rect.h);
        break;
      }
      case 'F': {
        risc_set_serial(risc, raw_serial_new(atoi(optarg), atoi(optarg) + 1));
        break;
      }
      default: {
        usage();
      }
    }
  }
  if (optind != argc - 1) {
    usage();
  }
  risc_set_spi(risc, 1, disk_new(argv[optind]));

  sdl_error_if(SDL_Init(SDL_INIT_VIDEO) != 0, "Unable to initialize SDL");
  atexit(SDL_Quit);
  SDL_EnableScreenSaver();
  SDL_ShowCursor(false);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");

  int window_flags = SDL_WINDOW_HIDDEN;
  int window_pos = SDL_WINDOWPOS_UNDEFINED;
  if (fullscreen) {
    window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    window_pos = best_display(&risc_rect);
  }
  SDL_Window *window = SDL_CreateWindow("Project Oberon",
                                        window_pos, window_pos,
                                        risc_rect.w, risc_rect.h,
                                        window_flags);
  sdl_error_if(window == NULL, "Could not create window");

  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
  sdl_error_if(renderer == NULL, "Could not create renderer");

  SDL_Texture *texture = SDL_CreateTexture(renderer,
                                           SDL_PIXELFORMAT_ARGB8888,
                                           SDL_TEXTUREACCESS_STREAMING,
                                           RISC_FRAMEBUFFER_WIDTH,
                                           RISC_FRAMEBUFFER_HEIGHT);
  sdl_error_if(texture == NULL, "Could not create texture");

  SDL_Rect display_rect;
  double display_scale = scale_display(window, &risc_rect, &display_rect);
  update_texture(risc, texture);
  SDL_ShowWindow(window);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, &risc_rect, &display_rect);
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
            display_scale = scale_display(window, &risc_rect, &display_rect);
          }
          break;
        }

        case SDL_MOUSEMOTION: {
          int scaled_x = (int)round((event.motion.x - display_rect.x) / display_scale);
          int scaled_y = (int)round((event.motion.y - display_rect.y) / display_scale);
          int x = clamp(scaled_x, 0, risc_rect.w - 1);
          int y = clamp(scaled_y, 0, risc_rect.h - 1);
          bool mouse_is_offscreen = x != scaled_x || y != scaled_y;
          if (mouse_is_offscreen != mouse_was_offscreen) {
            SDL_ShowCursor(mouse_is_offscreen);
            mouse_was_offscreen = mouse_is_offscreen;
          }
          risc_mouse_moved(risc, x, risc_rect.h - y - 1);
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

    update_texture(risc, texture);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, &risc_rect, &display_rect);
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

static double scale_display(SDL_Window *window, const SDL_Rect *risc_rect, SDL_Rect *display_rect) {
  int win_w, win_h;
  SDL_GetWindowSize(window, &win_w, &win_h);
  double oberon_aspect = (double)risc_rect->w / risc_rect->h;
  double window_aspect = (double)win_w / win_h;

  double scale;
  if (oberon_aspect > window_aspect) {
    scale = (double)win_w / risc_rect->w;
  } else {
    scale = (double)win_h / risc_rect->h;
  }

  int w = (int)ceil(risc_rect->w * scale);
  int h = (int)ceil(risc_rect->h * scale);
  *display_rect = (SDL_Rect){
    .w = w, .h = h,
    .x = (win_w - w) / 2,
    .y = (win_h - h) / 2
  };
  return scale;
}

static void update_texture(struct RISC *risc, SDL_Texture *texture) {
  struct Damage damage = risc_get_framebuffer_damage(risc);
  if (damage.y1 <= damage.y2) {
    uint32_t out[RISC_FRAMEBUFFER_WIDTH * RISC_FRAMEBUFFER_HEIGHT];
    uint32_t *in = risc_get_framebuffer_ptr(risc);
    uint32_t out_idx = 0;

    for (int line = damage.y2; line >= damage.y1; line--) {
      int line_start = line * (RISC_FRAMEBUFFER_WIDTH / 32);
      for (int col = damage.x1; col <= damage.x2; col++) {
        uint32_t pixels = in[line_start + col];
        for (int b = 0; b < 32; b++) {
          out[out_idx] = (pixels & 1) ? WHITE : BLACK;
          pixels >>= 1;
          out_idx++;
        }
      }
    }

    SDL_Rect rect = {
      .x = damage.x1 * 32,
      .y = RISC_FRAMEBUFFER_HEIGHT - damage.y2 - 1,
      .w = (damage.x2 - damage.x1 + 1) * 32,
      .h = (damage.y2 - damage.y1 + 1)
    };
    SDL_UpdateTexture(texture, &rect, out, rect.w * 4);
  }
}
