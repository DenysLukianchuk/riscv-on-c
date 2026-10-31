#include "riscv/cpu/fp_registers.h"
#include "unity.h"

// The f registers are 64 bits wide (FLEN = 64 with the D extension) on both
// RV32 and RV64. A single-precision value lives in the low 32 bits.

static FpRegisters *fregs;

void setUp(void) { fregs = fp_registers_create(); }

void tearDown(void) { fp_registers_destroy(fregs); }

static void test_written_value_can_be_read_back(void) {
  fp_registers_write(fregs, 5, 0x400921FB54442D18);

  TEST_ASSERT_EQUAL_HEX64(0x400921FB54442D18, fp_registers_read(fregs, 5));
}

// Unlike x0, f0 is an ordinary register.
static void test_f0_holds_a_value(void) {
  fp_registers_write(fregs, 0, 0x3FF0000000000000);

  TEST_ASSERT_EQUAL_HEX64(0x3FF0000000000000, fp_registers_read(fregs, 0));
}

static void test_registers_are_independent(void) {
  fp_registers_write(fregs, 1, 1);
  fp_registers_write(fregs, 31, 31);

  TEST_ASSERT_EQUAL_HEX64(1, fp_registers_read(fregs, 1));
  TEST_ASSERT_EQUAL_HEX64(31, fp_registers_read(fregs, 31));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_written_value_can_be_read_back);
  RUN_TEST(test_f0_holds_a_value);
  RUN_TEST(test_registers_are_independent);
  return UNITY_END();
}
