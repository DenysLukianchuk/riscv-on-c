#ifndef RISCV_BITS_H
#define RISCV_BITS_H

#include <assert.h>
#include <stdint.h>

#define BITS_PER_BYTE 8

static inline uint64_t low_mask(unsigned width) {
  return width == 64 ? ~(uint64_t)0 : ((uint64_t)1 << width) - 1;
}

static inline uint64_t bits(uint64_t value, unsigned hi, unsigned lo) {
  assert(hi < 64 && lo <= hi);
  return (value >> lo) & low_mask(hi - lo + 1);
}

static inline uint64_t clear_bits(uint64_t value, unsigned hi, unsigned lo) {
  assert(hi < 64 && lo <= hi);
  return value & ~(low_mask(hi - lo + 1) << lo);
}

static inline int32_t sign_extend(uint32_t x, unsigned width) {
  uint32_t m = 1u << (width - 1);
  return (int32_t)((x ^ m) - m);
}

#endif // RISCV_BITS_H
