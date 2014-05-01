#ifndef DISK_H
#define DISK_H

#include "risc-io.h"

struct RISC_SPI *disk_new(const char *filename);

#endif  // DISK_H
