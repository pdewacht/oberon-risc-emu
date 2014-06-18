#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "risc.h"
#include "risc-fp.h"


// Our memory layout is slightly different from the FPGA implementation.
// The FPGA uses a 20-bit address bus and thus ignores the top 12 bits.
// We use all 32 bits. This allows us to have more than 1 megabyte of
// RAM.
//
// We add an extra half megabyte. This is used to allow for framebuffers
// larger than the normal 1024x768. A modified Dispay.Mod and Input.Mod
// is needed to use this feature, this is included in the Oberon
// directory.
//
// As we don't change DisplayStart, this extra memory is not useful for a
// larger heap.

#define MemSize      0x00180000
#define MemWords     (MemSize / 4)
#define ROMStart     0xFFFFF800
#define ROMWords     512
#define DisplayStart 0x000E7F00
#define IOStart      0xFFFFFFC0


struct RISC {
  uint32_t PC;
  uint32_t R[16];
  uint32_t H;
  bool     Z, N, C, V;

  uint32_t progress;
  uint32_t current_tick;
  uint32_t mouse;
  uint8_t  key_buf[16];
  uint32_t key_cnt;
  uint32_t leds;

  const struct RISC_Serial *serial;
  uint32_t spi_selected;
  const struct RISC_SPI *spi[4];
  const struct RISC_Clipboard *clipboard;

  int fb_width;   // words
  int fb_height;  // lines
  struct Damage damage;

  uint32_t RAM[MemWords];
  uint32_t ROM[ROMWords];
};

enum {
  MOV, LSL, ASR, ROR,
  AND, ANN, IOR, XOR,
  ADD, SUB, MUL, DIV,
  FAD, FSB, FML, FDV,
};

static void risc_single_step(struct RISC *risc);
static void risc_set_register(struct RISC *risc, int reg, uint32_t value);
static uint32_t risc_load_word(struct RISC *risc, uint32_t address);
static uint8_t risc_load_byte(struct RISC *risc, uint32_t address);
static void risc_store_word(struct RISC *risc, uint32_t address, uint32_t value);
static void risc_store_byte(struct RISC *risc, uint32_t address, uint8_t value);
static uint32_t risc_load_io(struct RISC *risc, uint32_t address);
static void risc_store_io(struct RISC *risc, uint32_t address, uint32_t value);

static const uint32_t bootloader[ROMWords] = {
#include "risc-boot.inc"
};


struct RISC *risc_new() {
  struct RISC *risc = calloc(1, sizeof(*risc));
  memcpy(risc->ROM, bootloader, sizeof(risc->ROM));
  risc_screen_size_hack(risc, RISC_FRAMEBUFFER_WIDTH, RISC_FRAMEBUFFER_HEIGHT);
  risc_reset(risc);
  return risc;
}

void risc_set_serial(struct RISC *risc, const struct RISC_Serial *serial) {
  risc->serial = serial;
}

void risc_set_spi(struct RISC *risc, int index, const struct RISC_SPI *spi) {
  if (index == 1 || index == 2) {
    risc->spi[index] = spi;
  }
}

void risc_set_clipboard(struct RISC *risc, const struct RISC_Clipboard *clipboard) {
  risc->clipboard = clipboard;
}

void risc_screen_size_hack(struct RISC *risc, int width, int height) {
  risc->fb_width = width / 32;
  risc->fb_height = height;
  risc->damage = (struct Damage){
    .x1 = 0,
    .y1 = 0,
    .x2 = risc->fb_width - 1,
    .y2 = risc->fb_height - 1
  };

  risc->RAM[DisplayStart/4] = 0x53697A65;  // magic value 'SIZE'
  risc->RAM[DisplayStart/4+1] = width;
  risc->RAM[DisplayStart/4+2] = height;
}

void risc_reset(struct RISC *risc) {
  risc->PC = ROMStart/4;
}

void risc_run(struct RISC *risc, int cycles) {
  risc->progress = 20;
  // The progress value is used to detect that the RISC cpu is busy
  // waiting on the millisecond counter or on the keyboard ready
  // bit. In that case it's better to just pause emulation until the
  // next frame.
  for (int i = 0; i < cycles && risc->progress; i++) {
    risc_single_step(risc);
  }
}

static void risc_single_step(struct RISC *risc) {
  uint32_t ir;
  if (risc->PC < MemWords) {
    ir = risc->RAM[risc->PC];
  } else if (risc->PC >= ROMStart/4 && risc->PC < ROMStart/4 + ROMWords) {
    ir = risc->ROM[risc->PC - ROMStart/4];
  } else {
    fprintf(stderr, "Branched into the void (PC=0x%08X), resetting...\n", risc->PC);
    risc_reset(risc);
    return;
  }
  risc->PC++;

  const uint32_t pbit = 0x80000000;
  const uint32_t qbit = 0x40000000;
  const uint32_t ubit = 0x20000000;
  const uint32_t vbit = 0x10000000;

  if ((ir & pbit) == 0) {
    // Register instructions
    uint32_t a  = (ir & 0x0F000000) >> 24;
    uint32_t b  = (ir & 0x00F00000) >> 20;
    uint32_t op = (ir & 0x000F0000) >> 16;
    uint32_t im =  ir & 0x0000FFFF;
    uint32_t c  =  ir & 0x0000000F;

    uint32_t a_val, b_val, c_val;
    b_val = risc->R[b];
    if ((ir & qbit) == 0) {
      c_val = risc->R[c];
    } else if ((ir & vbit) == 0) {
      c_val = im;
    } else {
      c_val = 0xFFFF0000 | im;
    }

    switch (op) {
      case MOV: {
        if ((ir & ubit) == 0) {
          a_val = c_val;
        } else if ((ir & qbit) != 0) {
          a_val = c_val << 16;
        } else if ((ir & vbit) != 0) {
          a_val = 0xD0 |   // ???
            (risc->N * 0x80000000U) |
            (risc->Z * 0x40000000U) |
            (risc->C * 0x20000000U) |
            (risc->V * 0x10000000U);
        } else {
          a_val = risc->H;
        }
        break;
      }
      case LSL: {
        a_val = b_val << (c_val & 31);
        break;
      }
      case ASR: {
        a_val = ((int32_t)b_val) >> (c_val & 31);
        break;
      }
      case ROR: {
        a_val = (b_val >> (c_val & 31)) | (b_val << (-c_val & 31));
        break;
      }
      case AND: {
        a_val = b_val & c_val;
        break;
      }
      case ANN: {
        a_val = b_val & ~c_val;
        break;
      }
      case IOR: {
        a_val = b_val | c_val;
        break;
      }
      case XOR: {
        a_val = b_val ^ c_val;
        break;
      }
      case ADD: {
        a_val = b_val + c_val;
        if ((ir & ubit) != 0 && risc->C) {
          a_val++;
        }
        risc->C = a_val < b_val;
        risc->V = ((a_val ^ c_val) & (a_val ^ b_val)) >> 31;
        break;
      }
      case SUB: {
        a_val = b_val - c_val;
        if ((ir & ubit) != 0 && risc->C) {
          a_val--;
        }
        risc->C = a_val > b_val;
        risc->V = ((b_val ^ c_val) & (a_val ^ b_val)) >> 31;
        break;
      }
      case MUL: {
        uint64_t tmp;
        if ((ir & ubit) == 0) {
          tmp = (int64_t)(int32_t)b_val * (int64_t)(int32_t)c_val;
        } else {
          tmp = (uint64_t)b_val * (uint64_t)c_val;
        }
        a_val = (uint32_t)tmp;
        risc->H = (uint32_t)(tmp >> 32);
        break;
      }
      case DIV: {
        if ((int32_t)c_val <= 0) {
          fprintf(stderr, "ERROR: PC 0x%08X: divisor %d is not positive\n", risc->PC*4 - 4, c_val);
          a_val = 0xDEADBEEF;
          risc->H = 0xDEADBEEF;
        } else {
          a_val = (int32_t)b_val / (int32_t)c_val;
          risc->H = (int32_t)b_val % (int32_t)c_val;
          if ((int32_t)risc->H < 0) {
            a_val--;
            risc->H += c_val;
          }
        }
        break;
      }
      case FAD: {
        a_val = fp_add(b_val, c_val, ir & ubit, ir & vbit);
        break;
      }
      case FSB: {
        a_val = fp_add(b_val, c_val ^ 0x80000000, ir & ubit, ir & vbit);
        break;
      }
      case FML: {
        a_val = fp_mul(b_val, c_val);
        break;
      }
      case FDV: {
        a_val = fp_div(b_val, c_val);
        break;
      }
      default: {
        abort();  // unreachable
      }
    }
    risc_set_register(risc, a, a_val);
  }
  else if ((ir & qbit) == 0) {
    // Memory instructions
    uint32_t a = (ir & 0x0F000000) >> 24;
    uint32_t b = (ir & 0x00F00000) >> 20;
    int32_t off = ir & 0x000FFFFF;
    off = off << 12 >> 12;  // sign-extend

    uint32_t address = risc->R[b] + off;
    if ((ir & ubit) == 0) {
      uint32_t a_val;
      if ((ir & vbit) == 0) {
        a_val = risc_load_word(risc, address);
      } else {
        a_val = risc_load_byte(risc, address);
      }
      risc_set_register(risc, a, a_val);
    } else {
      if ((ir & vbit) == 0) {
        risc_store_word(risc, address, risc->R[a]);
      } else {
        risc_store_byte(risc, address, (uint8_t)risc->R[a]);
      }
    }
  }
  else {
    // Branch instructions
    bool t;
    switch ((ir >> 24) & 15) {
      case 0: t = risc->N; break;
      case 1: t = risc->Z; break;
      case 2: t = risc->C; break;
      case 3: t = risc->V; break;
      case 4: t = risc->C || risc->Z; break;
      case 5: t = risc->N != risc->V; break;
      case 6: t = (risc->N != risc->V) || risc->Z; break;
      case 7: t = true; break;
      case 8: t = !risc->N; break;
      case 9: t = !risc->Z; break;
      case 10: t = !risc->C; break;
      case 11: t = !risc->V; break;
      case 12: t = !(risc->C || risc->Z); break;
      case 13: t = !(risc->N != risc->V); break;
      case 14: t = !((risc->N != risc->V) || risc->Z); break;
      case 15: t = false; break;
      default: abort();  // unreachable
    }
    if (t) {
      if ((ir & vbit) != 0) {
        risc_set_register(risc, 15, risc->PC * 4);
      }
      if ((ir & ubit) == 0) {
        uint32_t c = ir & 0x0000000F;
        risc->PC = risc->R[c] / 4;
      } else {
        int32_t off = ir & 0x00FFFFFF;
        off = off << 8 >> 8;  // sign-extend
        risc->PC = risc->PC + off;
      }
    }
  }
}

static void risc_set_register(struct RISC *risc, int reg, uint32_t value) {
  risc->R[reg] = value;
  risc->Z = value == 0;
  risc->N = (int32_t)value < 0;
}

static uint32_t risc_load_word(struct RISC *risc, uint32_t address) {
  if (address < MemSize) {
    return risc->RAM[address/4];
  } else {
    return risc_load_io(risc, address);
  }
}

static uint8_t risc_load_byte(struct RISC *risc, uint32_t address) {
  uint32_t w = risc_load_word(risc, address);
  return (uint8_t)(w >> (address % 4 * 8));
}

static void risc_update_damage(struct RISC *risc, int w) {
  int row = w / risc->fb_width;
  int col = w % risc->fb_width;
  if (row < risc->fb_height) {
    if (col < risc->damage.x1) {
      risc->damage.x1 = col;
    }
    if (col > risc->damage.x2) {
      risc->damage.x2 = col;
    }
    if (row < risc->damage.y1) {
      risc->damage.y1 = row;
    }
    if (row > risc->damage.y2) {
      risc->damage.y2 = row;
    }
  }
}

static void risc_store_word(struct RISC *risc, uint32_t address, uint32_t value) {
  if (address < DisplayStart) {
    risc->RAM[address/4] = value;
  } else if (address < MemSize) {
    risc->RAM[address/4] = value;
    risc_update_damage(risc, address/4 - DisplayStart/4);
  } else {
    risc_store_io(risc, address, value);
  }
}

static void risc_store_byte(struct RISC *risc, uint32_t address, uint8_t value) {
  if (address < MemSize) {
    uint32_t w = risc_load_word(risc, address);
    uint32_t shift = (address & 3) * 8;
    w &= ~(0xFFu << shift);
    w |= (uint32_t)value << shift;
    risc_store_word(risc, address, w);
  } else {
    risc_store_io(risc, address, (uint32_t)value);
  }
}

static uint32_t risc_load_io(struct RISC *risc, uint32_t address) {
  switch (address - IOStart) {
    case 0: {
      // Millisecond counter
      risc->progress--;
      return risc->current_tick;
    }
    case 4: {
      // Switches
      return 0;
    }
    case 8: {
      // RS232 data
      if (risc->serial) {
        return risc->serial->read_data(risc->serial);
      }
      return 0;
    }
    case 12: {
      // RS232 status
      if (risc->serial) {
        return risc->serial->read_status(risc->serial);
      }
      return 0;
    }
    case 16: {
      // SPI data
      const struct RISC_SPI *spi = risc->spi[risc->spi_selected];
      if (spi != NULL) {
        return spi->read_data(spi);
      }
      return 255;
    }
    case 20: {
      // SPI status
      // Bit 0: rx ready
      // Other bits unused
      return 1;
    }
    case 24: {
      // Mouse input / keyboard status
      uint32_t mouse = risc->mouse;
      if (risc->key_cnt > 0) {
        mouse |= 0x10000000;
      } else {
        risc->progress--;
      }
      return mouse;
    }
    case 28: {
      // Keyboard input
      if (risc->key_cnt > 0) {
        uint8_t scancode = risc->key_buf[0];
        risc->key_cnt--;
        memmove(&risc->key_buf[0], &risc->key_buf[1], risc->key_cnt);
        return scancode;
      }
      return 0;
    }
    case 40: {
      // Clipboard control
      if (risc->clipboard) {
        return risc->clipboard->read_control(risc->clipboard);
      }
      return 0;
    }
    case 44: {
      // Clipboard data
      if (risc->clipboard) {
        return risc->clipboard->read_data(risc->clipboard);
      }
      return 0;
    }
    default: {
      return 0;
    }
  }
}

static void risc_store_io(struct RISC *risc, uint32_t address, uint32_t value) {
  switch (address - IOStart) {
    case 4: {
      // LED control
      risc->leds = value;
      printf("LEDs: ");
      for (int i = 7; i >= 0; i--) {
        if (risc->leds & (1 << i)) {
          printf("%d", i);
        } else {
          printf("-");
        }
      }
      printf("\n");
      break;
    }
    case 8: {
      // RS232 data
      if (risc->serial) {
        risc->serial->write_data(risc->serial, value);
      }
      break;
    }
    case 16: {
      // SPI write
      const struct RISC_SPI *spi = risc->spi[risc->spi_selected];
      if (spi != NULL) {
        spi->write_data(spi, value);
      }
      break;
    }
    case 20: {
      // SPI control
      // Bit 0-1: slave select
      // Bit 2:   fast mode
      // Bit 3:   netwerk enable
      // Other bits unused
      risc->spi_selected = value & 3;
      break;
    }
    case 40: {
      // Clipboard control
      if (risc->clipboard) {
        risc->clipboard->write_control(risc->clipboard, value);
      }
      break;
    }
    case 44: {
      // Clipboard data
      if (risc->clipboard) {
        risc->clipboard->write_data(risc->clipboard, value);
      }
      break;
    }
  }
}


void risc_set_time(struct RISC *risc, uint32_t tick) {
  risc->current_tick = tick;
}

void risc_mouse_moved(struct RISC *risc, int mouse_x, int mouse_y) {
  if (mouse_x >= 0 && mouse_x < 4096) {
    risc->mouse = (risc->mouse & ~0x00000FFF) | mouse_x;
  }
  if (mouse_y >= 0 && mouse_y < 4096) {
    risc->mouse = (risc->mouse & ~0x00FFF000) | (mouse_y << 12);
  }
}

void risc_mouse_button(struct RISC *risc, int button, bool down) {
  if (button >= 1 && button < 4) {
    uint32_t bit = 1 << (27 - button);
    if (down) {
      risc->mouse |= bit;
    } else {
      risc->mouse &= ~bit;
    }
  }
}

void risc_keyboard_input(struct RISC *risc, uint8_t *scancodes, uint32_t len) {
  if (sizeof(risc->key_buf) - risc->key_cnt >= len) {
    memmove(&risc->key_buf[risc->key_cnt], scancodes, len);
    risc->key_cnt += len;
  }
}

uint32_t *risc_get_framebuffer_ptr(struct RISC *risc) {
  return &risc->RAM[DisplayStart/4];
}

struct Damage risc_get_framebuffer_damage(struct RISC *risc) {
  struct Damage dmg = risc->damage;
  risc->damage = (struct Damage){
    .x1 = risc->fb_width,
    .x2 = 0,
    .y1 = risc->fb_height,
    .y2 = 0
  };
  return dmg;
}
