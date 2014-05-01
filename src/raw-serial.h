#ifndef RAW_SERIAL_H
#define RAW_SERIAL_H

#include "risc-io.h"

struct RISC_Serial *raw_serial_new(int fd_in, int fd_out);

#endif  // SERIAL_H
