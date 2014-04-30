#ifndef RISC_H
#define RISC_H

#include <stdbool.h>
#include <stdint.h>
#include "risc-io.h"

#define RISC_FRAMEBUFFER_WIDTH 1024
#define RISC_FRAMEBUFFER_HEIGHT 768

struct RISC;

struct Damage {
  int x1, x2, y1, y2;
};

struct RISC *risc_new();
void risc_set_serial(struct RISC *risc, const struct RISC_Serial *serial);
void risc_set_spi(struct RISC *risc, int index, const struct RISC_SPI *spi);

void risc_reset(struct RISC *risc);
void risc_run(struct RISC *risc, int cycles);
void risc_set_time(struct RISC *risc, uint32_t tick);
void risc_mouse_moved(struct RISC *risc, int mouse_x, int mouse_y);
void risc_mouse_button(struct RISC *risc, int button, bool down);
void risc_keyboard_input(struct RISC *risc, uint8_t *scancodes, uint32_t len);

uint32_t *risc_get_framebuffer_ptr(struct RISC *risc);
struct Damage risc_get_framebuffer_damage(struct RISC *risc);

#endif  // RISC_H
