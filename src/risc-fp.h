#ifndef RISC_FP_H
#define RISC_FP_H

#include <stdint.h>
#include <stdbool.h>

uint32_t fp_add(uint32_t x, uint32_t y, bool u, bool v);
uint32_t fp_mul(uint32_t x, uint32_t y);
uint32_t fp_div(uint32_t x, uint32_t y);

struct idiv { uint32_t quot, rem; };
struct idiv idiv(uint32_t x, uint32_t y, bool signed_div);

#endif  // RISC_FP_H
