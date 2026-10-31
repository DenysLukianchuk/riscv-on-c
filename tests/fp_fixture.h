#ifndef TESTS_FP_FIXTURE_H
#define TESTS_FP_FIXTURE_H

#include "cpu_fixture.h"
#include "riscv/cpu/fp_registers.h"

// IEEE 754 bit patterns.
#define ONE_F 0x3F800000u
#define MINUS_ONE_F 0xBF800000u
#define TWO_F 0x40000000u
#define THREE_F 0x40400000u
#define INF_F 0x7F800000u
#define MINUS_INF_F 0xFF800000u
#define MAX_F 0x7F7FFFFFu
#define SIGNALING_NAN_F 0x7F800001u
#define CANONICAL_NAN_F 0x7FC00000u
#define SIGN_F 0x80000000u

#define ONE_D 0x3FF0000000000000
#define MINUS_ONE_D 0xBFF0000000000000
#define TWO_D 0x4000000000000000
#define THREE_D 0x4008000000000000
#define PI_D 0x400921FB54442D18
#define INF_D 0x7FF0000000000000
#define SIGNALING_NAN_D 0x7FF0000000000001
#define CANONICAL_NAN_D 0x7FF8000000000000
#define SIGN_D 0x8000000000000000

// A single-precision value in a 64-bit register is "NaN-boxed": the upper 32
// bits are all ones, so the register read as a double is a NaN.
#define NAN_BOX(single) (0xFFFFFFFF00000000 | (single))

static inline uint64_t get_freg(uint8_t index) {
  return fp_registers_read(cpu_get_fp_registers(cpu), index);
}

static inline void set_freg(uint8_t index, uint64_t value) {
  fp_registers_write(cpu_get_fp_registers(cpu), index, value);
}

static inline void set_fs(uxlen_t fs) {
  write_csr(MSTATUS, (read_csr(MSTATUS) & ~(uxlen_t)MSTATUS_FS) | fs);
}

static inline uxlen_t fs(void) { return read_csr(MSTATUS) & MSTATUS_FS; }

static inline void enable_fp(void) { set_fs(MSTATUS_FS_INITIAL); }

// Runs an instruction with f1 and f2 as operands and returns f3.
static inline uint64_t run_fp(uint32_t raw, uint64_t f1, uint64_t f2) {
  enable_fp();
  set_freg(1, f1);
  set_freg(2, f2);

  execute_raw(raw);

  return get_freg(3);
}

static inline uxlen_t fflags(void) { return read_csr(FFLAGS); }

static inline void set_frm(uxlen_t rm) {
  enable_fp();
  write_csr(FRM, rm);
}

#endif // TESTS_FP_FIXTURE_H
