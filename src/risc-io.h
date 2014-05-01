#ifndef RISC_IO_H
#define RISC_IO_H

#include <stdint.h>

struct RISC_Serial {
  uint32_t (*read_status)(const struct RISC_Serial *);
  uint32_t (*read_data)(const struct RISC_Serial *);
  void (*write_data)(const struct RISC_Serial *, uint32_t);
};

struct RISC_SPI {
  uint32_t (*read_data)(const struct RISC_SPI *);
  void (*write_data)(const struct RISC_SPI *, uint32_t);
};

struct RISC_Clipboard {
  void (*write_control)(const struct RISC_Clipboard *, uint32_t);
  uint32_t (*read_control)(const struct RISC_Clipboard *);
  void (*write_data)(const struct RISC_Clipboard *, uint32_t);
  uint32_t (*read_data)(const struct RISC_Clipboard *);
};

#endif  // RISC_IO_H
