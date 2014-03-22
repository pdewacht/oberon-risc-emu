#include "risc-fp.h"

uint32_t fp_add(uint32_t x, uint32_t y, bool u, bool v) {
  uint32_t xs = x & 0x80000000;
  uint32_t xe;
  int32_t x0;
  if (!u) {
    xe = (x >> 23) & 0xFF;
    uint32_t xm = ((x & 0x7FFFFF) << 1) | 0x1000000;
    x0 = (int32_t)(xs ? -xm : xm);
  } else {
    xe = 150;
    x0 = (int32_t)(x & 0x00FFFFFF) << 8 >> 7;
  }

  uint32_t ys = y & 0x80000000;
  uint32_t ye = (y >> 23) & 0xFF;
  uint32_t ym = ((y & 0x7FFFFF) << 1);
  if (!u && !v) ym |= 0x1000000;
  int32_t y0 = (int32_t)(ys ? -ym : ym);

  uint32_t e0;
  int32_t x3, y3;
  if (ye > xe) {
    uint32_t shift = ye - xe;
    e0 = ye;
    x3 = shift > 31 ? x0 >> 31 : x0 >> shift;
    y3 = y0;
  } else {
    uint32_t shift = xe - ye;
    e0 = xe;
    x3 = x0;
    y3 = shift > 31 ? y0 >> 31 : y0 >> shift;
  }

  int32_t sum = x3 + y3;
  int32_t s = ((sum < 0) ? -sum : sum) + 1;

  uint32_t e1 = e0 + 1;
  uint32_t t3 = s >> 1;
  if ((s & 0x3fffffc) != 0) {
    while ((t3 & (1<<24)) == 0) {
      t3 <<= 1;
      e1--;
    }
  } else {
    t3 <<= 24;
    e1 -= 24;
  }

  if (v) {
    return (uint32_t)(sum >> 1);
  } else if ((x & 0x7FFFFFFF) == 0) {
    return !u ? y : 0;
  } else if ((y & 0x7FFFFFFF) == 0) {
    return x;
  } else if (t3 == 0 || (int32_t)e1 < 0) {
    return 0;
  } else {
    return ((uint32_t)sum & 0x80000000) | (e1 << 23) | ((t3 >> 1) & 0x7FFFFF);
  }
}

uint32_t fp_mul(uint32_t x, uint32_t y) {
  uint32_t sign = (x ^ y) & 0x80000000;
  uint32_t xe = (x >> 23) & 0xFF;
  uint32_t ye = (y >> 23) & 0xFF;

  uint32_t xm = (x & 0x7FFFFF) | 0x800000;
  uint32_t ym = (y & 0x7FFFFF) | 0x800000;
  uint64_t m = (uint64_t)xm * ym;

  uint32_t e1 = (xe + ye) - 127;
  uint32_t z0;
  if ((m & (1ULL << 47)) != 0) {
    e1++;
    z0 = (m >> 24) & 0x7FFFFF;
  } else {
    z0 = (m >> 23) & 0x7FFFFF;
  }

  if (xe == 0 || ye == 0) {
    return 0;
  } else if ((e1 & 0x100) == 0) {
    return sign | ((e1 & 0xFF) << 23) | z0;
  } else if ((e1 & 0x80) == 0) {
    return sign | (0xFF << 23) | z0;
  } else {
    return 0;
  }
}

uint32_t fp_div(uint32_t x, uint32_t y) {
  uint32_t sign = (x ^ y) & 0x80000000;
  uint32_t xe = (x >> 23) & 0xFF;
  uint32_t ye = (y >> 23) & 0xFF;

  uint32_t xm = (x & 0x7FFFFF) | 0x800000;
  uint32_t ym = (y & 0x7FFFFF) | 0x800000;
  uint32_t q1 = (uint32_t)(xm * (1ULL << 23) / ym);

  uint32_t e1 = (xe - ye) + 126;
  uint32_t q2;
  if ((q1 & 0x800000) != 0) {
    e1++;
    q2 = q1 & 0x7FFFFF;
  } else {
    q2 = (q1 << 1) & 0x7FFFFF;
  }

  if (xe == 0) {
    return 0;
  } else if (ye == 0) {
    return sign | (0xFF << 23);
  } else if ((e1 & 0x100) == 0) {
    return sign | ((e1 & 0xFF) << 23) | q2;
  } else if ((e1 & 0x80) == 0) {
    return sign | (0xFF << 23) | q2;
  } else {
    return 0;
  }
}

