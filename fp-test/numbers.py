#!/usr/bin/env python3

import random
import sys

count = int(sys.argv[1]) if len(sys.argv) > 1 else 25000

numbers = []
for e in range(256):
    numbers.append((e << 23) | 0)
    numbers.append((e << 23) | 1)
    numbers.append((e << 23) | 0x7fffff)
    numbers.append((e << 23) | 0x7ffffe)
numbers.extend([x | 0x80000000 for x in numbers])
numbers.extend(random.getrandbits(32) for x in range(count - len(numbers)))

print("const int numbers_cnt = %d;" % count)
print("const uint32_t numbers[%d] = {" % count)
print(",\n".join('0x%08X' % x for x in numbers))
print("};")
