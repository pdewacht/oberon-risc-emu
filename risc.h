#ifndef RISC_H
#define RISC_H

#include <stdbool.h>
#include <stdint.h>

struct RISC;

struct RISC *risc_new(const char *disk_file);
void risc_free(struct RISC *risc);

void risc_run(struct RISC *risc, int cycles);
void risc_set_time(struct RISC *risc, uint32_t tick);
void risc_mouse_moved(struct RISC *risc, int mouse_x, int mouse_y);
void risc_mouse_button(struct RISC *risc, int button, bool down);
void risc_keyboard_input(struct RISC *risc, uint8_t *scancodes, uint32_t len);

uint32_t *risc_get_framebuffer_ptr(struct RISC *risc);

#endif  // RISC_H
