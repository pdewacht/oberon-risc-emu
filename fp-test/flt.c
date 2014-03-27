#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "../risc-fp.h"
#include "numbers.inc"
#include "FPAdder.inc"

static bool clk __attribute__((unused));
static bool u = 1;

static uint32_t v_add(uint32_t in_x, uint32_t in_y) {
  x = in_x;
  y = in_y;
  run = 0;
  cycle();
  run = 1;
  do {
    cycle();
    /* printf("run=%d stallR=%d Sum=%08x s=%08x x3=%08x y3=%08x z=%08x \n", */
    /*        run, stallR, Sum.val, s().val, x3().val, y3().val, z()); */
  } while (stallR);
  return z();
}


int main() {
  int count = 0;
  int errors = 0;
  for (int i = 0; i < numbers_cnt; i++) {
    uint32_t v = v_add(numbers[i], 0x4B00U<<16);
    uint32_t fp = fp_add(numbers[i], 0x4B00U<<16, 1, 0);
    if (v != fp && errors < 10) {
      printf("flt: %08x => v %08x | fp %08x\n", numbers[i], v, fp);
    }
    errors += fp != v;
    count += 1;
  }
  printf("flt: %d of %d errors\n", errors, count);
  return errors != 0;
}
