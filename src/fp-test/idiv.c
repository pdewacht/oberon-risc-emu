#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "../risc-fp.h"
#include "numbers.inc"
#include "Divider.inc"

static bool clk __attribute__((unused));

static struct idiv v_idiv(uint32_t in_x, uint32_t in_y, bool signed_div) {
  x = in_x;
  y = in_y;
  u = signed_div;
  run = 0;
  cycle();
  run = 1;
  do {
    cycle();
  } while (stall());
  return (struct idiv){ quot(), rem() };
}


int main() {
  int count = 0;
  int errors = 0;
  for (int s = 0; s < 2; s++) {
    for (int i = 0; i < numbers_cnt; i++) {
      for (int j = 0; j < numbers_cnt; j++) {
        struct idiv v = v_idiv(numbers[i], numbers[j], s);
        struct idiv e = idiv(numbers[i], numbers[j], s);
        bool error = v.quot != e.quot || v.rem != e.rem;
        if (error && errors < 20) {
          printf("idiv (signed=%d): 0x%x %d => v (%d,%d) | emu (%d,%d)\n",
                 s, numbers[i], numbers[j],
                 v.quot, v.rem, e.quot, e.rem);
        }
        errors += error;
        count += 1;
      }
      if ((i % 500) == 0) {
        int p = count * 100LL / numbers_cnt / numbers_cnt / 2;
        printf("idiv: %d%% (%d errors)\n", p, errors);
      }
    }
  }
  printf("idiv: errors: %d tests: %d\n", errors, count);
  return errors != 0;
}
