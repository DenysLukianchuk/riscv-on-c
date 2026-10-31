#ifndef RISCV_UTIL_FLOAT_H
#define RISCV_UTIL_FLOAT_H

#include "riscv/util/bits.h"
#include <stdbool.h>
#include <stdint.h>

#define NAN_BOX_MASK 0xFFFFFFFF00000000ULL
#define NAN_SINGLE 0x7FC00000u
#define NAN_DOUBLE 0x7FF8000000000000ULL

#define SINGLE_SIGN_BIT 0x80000000u
#define DOUBLE_SIGN_BIT 0x8000000000000000ULL

#define SINGLE_EXP_BITS 8
#define SINGLE_MANT_BITS 23
#define DOUBLE_EXP_BITS 11
#define DOUBLE_MANT_BITS 52

static inline uint64_t nan_box(uint32_t single) {
  return single | NAN_BOX_MASK;
}

static inline uint32_t unbox_single(uint64_t boxed) {
  if ((boxed & NAN_BOX_MASK) != NAN_BOX_MASK)
    return NAN_SINGLE;
  return (uint32_t)boxed;
}

enum SignSource { SIGN_OF_B = 0, SIGN_OF_NOT_B = 1, SIGN_OF_A_XOR_B = 2 };

static inline uint64_t sign_inject(enum SignSource source, uint64_t a,
                                   uint64_t b, uint64_t sign_bit) {
  uint64_t sign = 0;
  switch (source) {
  case SIGN_OF_B:
    sign = b;
    break;
  case SIGN_OF_NOT_B:
    sign = ~b;
    break;
  case SIGN_OF_A_XOR_B:
    sign = a ^ b;
    break;
  }
  return (a & ~sign_bit) | (sign & sign_bit);
}

enum FpClass {
  FCLASS_NEG_INF = 1 << 0,
  FCLASS_NEG_NORMAL = 1 << 1,
  FCLASS_NEG_SUBNORMAL = 1 << 2,
  FCLASS_NEG_ZERO = 1 << 3,
  FCLASS_POS_ZERO = 1 << 4,
  FCLASS_POS_SUBNORMAL = 1 << 5,
  FCLASS_POS_NORMAL = 1 << 6,
  FCLASS_POS_INF = 1 << 7,
  FCLASS_SIGNALING_NAN = 1 << 8,
  FCLASS_QUIET_NAN = 1 << 9,
};

static inline uint32_t fclass(uint64_t value, unsigned exp_bits,
                              unsigned mant_bits) {
  uint64_t mantissa = bits(value, mant_bits - 1, 0);
  uint64_t exponent = bits(value, exp_bits + mant_bits - 1, mant_bits);
  bool negative = bits(value, exp_bits + mant_bits, exp_bits + mant_bits);

  if (exponent == low_mask(exp_bits)) {
    if (mantissa == 0)
      return negative ? FCLASS_NEG_INF : FCLASS_POS_INF;
    bool quiet = bits(mantissa, mant_bits - 1, mant_bits - 1);
    return quiet ? FCLASS_QUIET_NAN : FCLASS_SIGNALING_NAN;
  }
  if (exponent == 0) {
    if (mantissa == 0)
      return negative ? FCLASS_NEG_ZERO : FCLASS_POS_ZERO;
    return negative ? FCLASS_NEG_SUBNORMAL : FCLASS_POS_SUBNORMAL;
  }
  return negative ? FCLASS_NEG_NORMAL : FCLASS_POS_NORMAL;
}

#endif // RISCV_UTIL_FLOAT_H
