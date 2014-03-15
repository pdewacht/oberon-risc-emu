// Translate SDL scancodes to PS/2 codeset 2 scancodes.

#include <SDL2/SDL.h>
#include "sdl-ps2.h"

struct k_info {
  unsigned char code;
  unsigned char type;
};

enum k_type {
  K_UNKNOWN = 0,
  K_NORMAL,
  K_EXTENDED,
  K_NUMLOCK_HACK,
  K_SHIFT_HACK,
};

static struct k_info keymap[SDL_NUM_SCANCODES];

int ps2_encode(int sdl_scancode, bool make, uint8_t out[static MAX_PS2_CODE_LEN]) {
  int i = 0;
  struct k_info info = keymap[sdl_scancode];
  switch (info.type) {
    case K_UNKNOWN: {
      break;
    }

    case K_NORMAL: {
      if (!make) {
        out[i++] = 0xF0;
      }
      out[i++] = info.code;
      break;
    }

    case K_EXTENDED: {
      out[i++] = 0xE0;
      if (!make) {
        out[i++] = 0xF0;
      }
      out[i++] = info.code;
      break;
    }

    case K_NUMLOCK_HACK: {
      // This assumes Num Lock is always active
      if (make) {
        // fake shift press
        out[i++] = 0xE0;
        out[i++] = 0x12;
      }
      out[i++] = 0xE0;
      out[i++] = info.code;
      if (!make) {
        // fake shift release
        out[i++] = 0xE0;
        out[i++] = 0xF0;
        out[i++] = 0x12;
      }
      break;
    }

    case K_SHIFT_HACK: {
      SDL_Keymod mod = SDL_GetModState();
      if (make) {
        // fake shift release
        if (mod & KMOD_LSHIFT) {
          out[i++] = 0xE0;
          out[i++] = 0xF0;
          out[i++] = 0x12;
        }
        if (mod & KMOD_RSHIFT) {
          out[i++] = 0xE0;
          out[i++] = 0xF0;
          out[i++] = 0x59;
        }
        out[i++] = 0xE0;
        out[i++] = info.code;
      } else {
        out[i++] = 0xE0;
        out[i++] = 0xF0;
        out[i++] = info.code;
        // fake shift press
        if (mod & KMOD_RSHIFT) {
          out[i++] = 0xE0;
          out[i++] = 0x59;
        }
        if (mod & KMOD_LSHIFT) {
          out[i++] = 0xE0;
          out[i++] = 0x12;
        }
      }
      break;
    }
  }
  return i;
}

static struct k_info keymap[SDL_NUM_SCANCODES] = {
  [SDL_SCANCODE_A] = { 0x1C, K_NORMAL },
  [SDL_SCANCODE_B] = { 0x32, K_NORMAL },
  [SDL_SCANCODE_C] = { 0x21, K_NORMAL },
  [SDL_SCANCODE_D] = { 0x23, K_NORMAL },
  [SDL_SCANCODE_E] = { 0x24, K_NORMAL },
  [SDL_SCANCODE_F] = { 0x2B, K_NORMAL },
  [SDL_SCANCODE_G] = { 0x34, K_NORMAL },
  [SDL_SCANCODE_H] = { 0x33, K_NORMAL },
  [SDL_SCANCODE_I] = { 0x43, K_NORMAL },
  [SDL_SCANCODE_J] = { 0x3B, K_NORMAL },
  [SDL_SCANCODE_K] = { 0x42, K_NORMAL },
  [SDL_SCANCODE_L] = { 0x4B, K_NORMAL },
  [SDL_SCANCODE_M] = { 0x3A, K_NORMAL },
  [SDL_SCANCODE_N] = { 0x31, K_NORMAL },
  [SDL_SCANCODE_O] = { 0x44, K_NORMAL },
  [SDL_SCANCODE_P] = { 0x4D, K_NORMAL },
  [SDL_SCANCODE_Q] = { 0x15, K_NORMAL },
  [SDL_SCANCODE_R] = { 0x2D, K_NORMAL },
  [SDL_SCANCODE_S] = { 0x1B, K_NORMAL },
  [SDL_SCANCODE_T] = { 0x2C, K_NORMAL },
  [SDL_SCANCODE_U] = { 0x3C, K_NORMAL },
  [SDL_SCANCODE_V] = { 0x2A, K_NORMAL },
  [SDL_SCANCODE_W] = { 0x1D, K_NORMAL },
  [SDL_SCANCODE_X] = { 0x22, K_NORMAL },
  [SDL_SCANCODE_Y] = { 0x35, K_NORMAL },
  [SDL_SCANCODE_Z] = { 0x1A, K_NORMAL },

  [SDL_SCANCODE_1] = { 0x16, K_NORMAL },
  [SDL_SCANCODE_2] = { 0x1E, K_NORMAL },
  [SDL_SCANCODE_3] = { 0x26, K_NORMAL },
  [SDL_SCANCODE_4] = { 0x25, K_NORMAL },
  [SDL_SCANCODE_5] = { 0x2E, K_NORMAL },
  [SDL_SCANCODE_6] = { 0x36, K_NORMAL },
  [SDL_SCANCODE_7] = { 0x3D, K_NORMAL },
  [SDL_SCANCODE_8] = { 0x3E, K_NORMAL },
  [SDL_SCANCODE_9] = { 0x46, K_NORMAL },
  [SDL_SCANCODE_0] = { 0x45, K_NORMAL },

  [SDL_SCANCODE_RETURN]    = { 0x5A, K_NORMAL },
  [SDL_SCANCODE_ESCAPE]    = { 0x76, K_NORMAL },
  [SDL_SCANCODE_BACKSPACE] = { 0x66, K_NORMAL },
  [SDL_SCANCODE_TAB]       = { 0x0D, K_NORMAL },
  [SDL_SCANCODE_SPACE]     = { 0x29, K_NORMAL },

  [SDL_SCANCODE_MINUS]        = { 0x4E, K_NORMAL },
  [SDL_SCANCODE_EQUALS]       = { 0x55, K_NORMAL },
  [SDL_SCANCODE_LEFTBRACKET]  = { 0x54, K_NORMAL },
  [SDL_SCANCODE_RIGHTBRACKET] = { 0x5B, K_NORMAL },
  [SDL_SCANCODE_BACKSLASH]    = { 0x5D, K_NORMAL },
  [SDL_SCANCODE_NONUSHASH]    = { 0x5D, K_NORMAL },  // same key as BACKSLASH

  [SDL_SCANCODE_SEMICOLON]  = { 0x4C, K_NORMAL },
  [SDL_SCANCODE_APOSTROPHE] = { 0x52, K_NORMAL },
  [SDL_SCANCODE_GRAVE]      = { 0x0E, K_NORMAL },
  [SDL_SCANCODE_COMMA]      = { 0x41, K_NORMAL },
  [SDL_SCANCODE_PERIOD]     = { 0x49, K_NORMAL },
  [SDL_SCANCODE_SLASH]      = { 0x4A, K_NORMAL },

  [SDL_SCANCODE_F1]  = { 0x05, K_NORMAL },
  [SDL_SCANCODE_F2]  = { 0x06, K_NORMAL },
  [SDL_SCANCODE_F3]  = { 0x04, K_NORMAL },
  [SDL_SCANCODE_F4]  = { 0x0C, K_NORMAL },
  [SDL_SCANCODE_F5]  = { 0x03, K_NORMAL },
  [SDL_SCANCODE_F6]  = { 0x0B, K_NORMAL },
  [SDL_SCANCODE_F7]  = { 0x83, K_NORMAL },
  [SDL_SCANCODE_F8]  = { 0x0A, K_NORMAL },
  [SDL_SCANCODE_F9]  = { 0x01, K_NORMAL },
  [SDL_SCANCODE_F10] = { 0x09, K_NORMAL },
  [SDL_SCANCODE_F11] = { 0x78, K_NORMAL },
  [SDL_SCANCODE_F12] = { 0x07, K_NORMAL },

  // Most of the keys below are not used by Oberon

  [SDL_SCANCODE_INSERT]   = { 0x70, K_NUMLOCK_HACK },
  [SDL_SCANCODE_HOME]     = { 0x6C, K_NUMLOCK_HACK },
  [SDL_SCANCODE_PAGEUP]   = { 0x7D, K_NUMLOCK_HACK },
  [SDL_SCANCODE_DELETE]   = { 0x71, K_NUMLOCK_HACK },
  [SDL_SCANCODE_END]      = { 0x69, K_NUMLOCK_HACK },
  [SDL_SCANCODE_PAGEDOWN] = { 0x7A, K_NUMLOCK_HACK },
  [SDL_SCANCODE_RIGHT]    = { 0x74, K_NUMLOCK_HACK },
  [SDL_SCANCODE_LEFT]     = { 0x6B, K_NUMLOCK_HACK },
  [SDL_SCANCODE_DOWN]     = { 0x72, K_NUMLOCK_HACK },
  [SDL_SCANCODE_UP]       = { 0x75, K_NUMLOCK_HACK },

  [SDL_SCANCODE_KP_DIVIDE]   = { 0x4A, K_SHIFT_HACK },
  [SDL_SCANCODE_KP_MULTIPLY] = { 0x7C, K_NORMAL },
  [SDL_SCANCODE_KP_MINUS]    = { 0x7B, K_NORMAL },
  [SDL_SCANCODE_KP_PLUS]     = { 0x79, K_NORMAL },
  [SDL_SCANCODE_KP_ENTER]    = { 0x5A, K_EXTENDED },
  [SDL_SCANCODE_KP_1]        = { 0x69, K_NORMAL },
  [SDL_SCANCODE_KP_2]        = { 0x72, K_NORMAL },
  [SDL_SCANCODE_KP_3]        = { 0x7A, K_NORMAL },
  [SDL_SCANCODE_KP_4]        = { 0x6B, K_NORMAL },
  [SDL_SCANCODE_KP_5]        = { 0x73, K_NORMAL },
  [SDL_SCANCODE_KP_6]        = { 0x74, K_NORMAL },
  [SDL_SCANCODE_KP_7]        = { 0x6C, K_NORMAL },
  [SDL_SCANCODE_KP_8]        = { 0x75, K_NORMAL },
  [SDL_SCANCODE_KP_9]        = { 0x7D, K_NORMAL },
  [SDL_SCANCODE_KP_0]        = { 0x70, K_NORMAL },
  [SDL_SCANCODE_KP_PERIOD]   = { 0x71, K_NORMAL },

  [SDL_SCANCODE_NONUSBACKSLASH] = { 0x61, K_NORMAL },
  [SDL_SCANCODE_APPLICATION]    = { 0x2F, K_EXTENDED },

  [SDL_SCANCODE_LCTRL]  = { 0x14, K_NORMAL },
  [SDL_SCANCODE_LSHIFT] = { 0x12, K_NORMAL },
  [SDL_SCANCODE_LALT]   = { 0x11, K_NORMAL },
  [SDL_SCANCODE_LGUI]   = { 0x1F, K_EXTENDED },
  [SDL_SCANCODE_RCTRL]  = { 0x14, K_EXTENDED },
  [SDL_SCANCODE_RSHIFT] = { 0x59, K_NORMAL },
  [SDL_SCANCODE_RALT]   = { 0x11, K_EXTENDED },
  [SDL_SCANCODE_RGUI]   = { 0x27, K_EXTENDED },
};
