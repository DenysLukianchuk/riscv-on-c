#include "riscv/cpu/registers.h"
#include "unity.h"

static Registers *regs;

void setUp(void) { regs = registers_create(); }

void tearDown(void) { registers_destroy(regs); }

static void test_written_value_can_be_read_back(void) {
  registers_write(regs, 5, 0xDEADBEEF);

  TEST_ASSERT_EQUAL_HEX64(0xDEADBEEF, registers_read(regs, 5));
}

static void test_x0_is_hardwired_to_zero(void) {
  registers_write(regs, 0, 0xDEADBEEF);

  TEST_ASSERT_EQUAL_HEX64(0, registers_read(regs, 0));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_written_value_can_be_read_back);
  RUN_TEST(test_x0_is_hardwired_to_zero);
  return UNITY_END();
}
