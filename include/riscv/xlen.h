#ifndef RISCV_XLEN_H
#define RISCV_XLEN_H

#include <stdint.h>

#ifndef XLEN
#define XLEN 64
#endif

#if XLEN == 64
typedef uint64_t uxlen_t;
typedef int64_t sxlen_t;
#define SHAMT_MASK 0x3f
#define SHAMT_LEN 6
#define XLEN_BYTES 8
#elif XLEN == 32
typedef uint32_t uxlen_t;
typedef int32_t sxlen_t;
#define SHAMT_MASK 0x1f
#define SHAMT_LEN 5
#define XLEN_BYTES 4
#else
#error "XLEN must be 32 or 64"
#endif

#define SHAMT_MASK_32 0x1f
#define SHAMT_LEN_32 5

#endif
