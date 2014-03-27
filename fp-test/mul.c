#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "../risc-fp.h"
#include "numbers.inc"
#include "FPMultiplier.inc"

static bool clk __attribute__((unused));

static uint32_t v_mul(uint32_t in_x, uint32_t in_y) {
  x = in_x;
  y = in_y;
  run = 0;
  cycle();
  run = 1;
  do {
    cycle();
  } while (stall());
  return z();
}


int main() {
  int count = 0;
  int errors = 0;
  for (int i = 0; i < numbers_cnt; i++) {
    for (int j = 0; j < numbers_cnt; j++) {
      uint32_t v = v_mul(numbers[i], numbers[j]);
      uint32_t fp = fp_mul(numbers[i], numbers[j]);
      if (v != fp && errors < 10) {
        printf("mul: %08x %08x => v %08x | fp %08x\n", numbers[i], numbers[j], v, fp);
      }
      errors += fp != v;
      count += 1;
    }
    if ((i % 500) == 0) {
      int p = count * 100 / numbers_cnt / numbers_cnt;
      printf("mul: %d%% (%d errors)\n", p, errors);
    }
  }
  printf("mul: errors: %d tests: %d\n", errors, count);
  return errors != 0;
}
