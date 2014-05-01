#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <SDL.h>
#include "sdl-clipboard.h"

enum State { IDLE, GET, PUT };

static enum State state = IDLE;
static char *data = NULL;
static size_t data_ptr = 0;
static size_t data_len = 0;

static void reset() {
  state = IDLE;
  free(data);
  data = NULL;
  data_len = 0;
  data_ptr = 0;
}

static uint32_t clipboard_control_read(const struct RISC_Clipboard *clip) {
  uint32_t r = 0;
  reset();
  data = SDL_GetClipboardText();
  if (data) {
    data_len = strlen(data);
    if (data_len > UINT32_MAX) {
      reset();
    }
  }
  if (data_len > 0) {
    state = GET;
    r = (uint32_t)data_len;
    // Decrease length if data contains CR/LF line endings
    const char *p = data;
    while ((p = strchr(p, '\r')) != NULL) {
      if (*++p == '\n') {
        r--;
      }
    }
  }
  return r;
}

static void clipboard_control_write(const struct RISC_Clipboard *clip, uint32_t len) {
  reset();
  if (len < UINT32_MAX) {
    char *buf = malloc(len + 1);
    if (buf != 0) {
      data = buf;
      data_len = len;
      state = PUT;
    }
  }
}

static uint32_t clipboard_data_read(const struct RISC_Clipboard *clip) {
  uint32_t result = 0;
  if (state == GET) {
    assert(data && data_ptr < data_len);
    result = (uint8_t)data[data_ptr];
    data_ptr++;
    if (result == '\r' && data[data_ptr] == '\n') {
      data_ptr++;
    } else if (result == '\n') {
      result = '\r';
    }
    if (data_ptr == data_len) {
      reset();
    }
  }
  return result;
}

static void clipboard_data_write(const struct RISC_Clipboard *clip, uint32_t c) {
  if (state == PUT) {
    assert(data && data_ptr < data_len);
    if ((char)c == '\r') {
      c = '\n';
    }
    data[data_ptr] = (char)c;
    ++data_ptr;
    if (data_ptr == data_len) {
      data[data_ptr] = 0;
      SDL_SetClipboardText(data);
      reset();
    }
  }
}

const struct RISC_Clipboard sdl_clipboard = {
  .write_control = clipboard_control_write,
  .read_control = clipboard_control_read,
  .write_data = clipboard_data_write,
  .read_data = clipboard_data_read
};
