#ifndef RISCV_CPU_FPU_H
#define RISCV_CPU_FPU_H

#include <stdbool.h>
#include <stdint.h>

// IEEE 754 arithmetic on raw single (uint32_t) and double (uint64_t) bit
// patterns. Every operation reports the exceptions it raised in *flags.

enum FpRounding { FP_RNE = 0, FP_RTZ = 1, FP_RDN = 2, FP_RUP = 3, FP_RMM = 4 };

enum FpFlag {
  FP_FLAG_NX = 1 << 0,
  FP_FLAG_UF = 1 << 1,
  FP_FLAG_OF = 1 << 2,
  FP_FLAG_DZ = 1 << 3,
  FP_FLAG_NV = 1 << 4,
};

enum FpOp { FP_ADD, FP_SUB, FP_MUL, FP_DIV };

enum FpCompare { FP_LE = 0, FP_LT = 1, FP_EQ = 2 };

enum FpInt { FP_INT_W = 0, FP_INT_WU = 1, FP_INT_L = 2, FP_INT_LU = 3 };

uint32_t fpu_arith_s(enum FpOp op, uint32_t a, uint32_t b, enum FpRounding rm,
                     uint8_t *flags);
uint64_t fpu_arith_d(enum FpOp op, uint64_t a, uint64_t b, enum FpRounding rm,
                     uint8_t *flags);
uint32_t fpu_sqrt_s(uint32_t a, enum FpRounding rm, uint8_t *flags);
uint64_t fpu_sqrt_d(uint64_t a, enum FpRounding rm, uint8_t *flags);
uint32_t fpu_fma_s(uint32_t a, uint32_t b, uint32_t c, enum FpRounding rm,
                   uint8_t *flags);
uint64_t fpu_fma_d(uint64_t a, uint64_t b, uint64_t c, enum FpRounding rm,
                   uint8_t *flags);

uint32_t fpu_min_max_s(uint32_t a, uint32_t b, bool max, uint8_t *flags);
uint64_t fpu_min_max_d(uint64_t a, uint64_t b, bool max, uint8_t *flags);
bool fpu_compare_s(enum FpCompare cmp, uint32_t a, uint32_t b, uint8_t *flags);
bool fpu_compare_d(enum FpCompare cmp, uint64_t a, uint64_t b, uint8_t *flags);

// 32-bit results come back sign-extended to 64 bits, unsigned ones too.
uint64_t fpu_s_to_int(uint32_t a, enum FpInt type, enum FpRounding rm,
                      uint8_t *flags);
uint64_t fpu_d_to_int(uint64_t a, enum FpInt type, enum FpRounding rm,
                      uint8_t *flags);
uint32_t fpu_int_to_s(uint64_t value, enum FpInt type, enum FpRounding rm,
                      uint8_t *flags);
uint64_t fpu_int_to_d(uint64_t value, enum FpInt type, enum FpRounding rm,
                      uint8_t *flags);
uint64_t fpu_s_to_d(uint32_t a, uint8_t *flags);
uint32_t fpu_d_to_s(uint64_t a, enum FpRounding rm, uint8_t *flags);

#endif // RISCV_CPU_FPU_H
