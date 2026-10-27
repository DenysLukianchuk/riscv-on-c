#ifndef RISCV_BITS_H
#define RISCV_BITS_H

#include "riscv/xlen.h"
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

static inline uxlen_t truncate_to_size(uxlen_t value, uint8_t size) {
  assert(size == 1 || size == 2 || size == 4 || size == 8);
  if (size >= sizeof(uxlen_t))
    return value;
  return value & (((uxlen_t)1 << (size * BITS_PER_BYTE)) - 1);
}

#endif // RISCV_BITS_H
