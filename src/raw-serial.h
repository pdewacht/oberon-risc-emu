#ifndef RAW_SERIAL_H
#define RAW_SERIAL_H

#include "risc-io.h"

struct RISC_Serial *raw_serial_new(const char *filename_in, const char *filename_out);

#endif  // SERIAL_H
