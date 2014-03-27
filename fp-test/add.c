#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "../risc-fp.h"
#include "numbers.inc"
#include "FPAdder.inc"

static bool clk __attribute__((unused));

static uint32_t v_add(uint32_t in_x, uint32_t in_y) {
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
      uint32_t v = v_add(numbers[i], numbers[j]);
      uint32_t fp = fp_add(numbers[i], numbers[j], 0, 0);
      if (v != fp && errors < 10) {
        printf("add: %08x %08x => v %08x | fp %08x\n", numbers[i], numbers[j], v, fp);
      }
      errors += fp != v;
      count += 1;
    }
    if ((i % 500) == 0) {
      int p = count * 100 / numbers_cnt / numbers_cnt;
      printf("add: %d%% (%d errors)\n", p, errors);
    }
  }
  fprintf(stderr, "\n");
  printf("add: errors: %d of %d\n", errors, count);
  return errors != 0;
}
