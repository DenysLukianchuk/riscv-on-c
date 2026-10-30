#include "cpu_fixture.h"

static void test_jal_jumps_relative_to_pc(void) {
  cpu_set_pc(cpu, 0x80000000);

  execute_raw(JAL(0, 16));

  TEST_ASSERT_EQUAL_HEX64(0x80000010, cpu_get_pc(cpu));
}

static void test_jal_writes_return_address_to_rd(void) {
  cpu_set_pc(cpu, 0x80000000);

  execute_raw(JAL(1, 16));

  TEST_ASSERT_EQUAL_HEX64(0x80000004, get_reg(1));
}

static void test_jalr_jumps_to_rs1_value_plus_immediate(void) {
  cpu_set_pc(cpu, 0x80000000);
  set_reg(2, 0x80001000);

  execute_raw(JALR(1, 8, 2));

  TEST_ASSERT_EQUAL_HEX64(0x80001008, cpu_get_pc(cpu));
}

static void test_jalr_with_rd_equal_rs1_jumps_to_old_rs1_value(void) {
  cpu_set_pc(cpu, 0x80000000);
  set_reg(2, 0x80001000);

  execute_raw(JALR(2, 8, 2));

  TEST_ASSERT_EQUAL_HEX64(0x80001008, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL_HEX64(0x80000004, get_reg(2));
}

static void test_jalr_clears_bit_0_of_target(void) {
  cpu_set_pc(cpu, 0x80000000);
  set_reg(2, 0x80001000);

  execute_raw(JALR(0, 1, 2));

  TEST_ASSERT_EQUAL_HEX64(0x80001000, cpu_get_pc(cpu));
}

// With the C extension instructions sit on 2-byte boundaries, so a jump
// target only has to be even. Jumps cannot produce an odd one: jal offsets
// are even, and jalr clears bit 0.

static void test_jal_may_target_a_2_byte_boundary(void) {
  cpu_set_pc(cpu, 0x80000000);

  execute_raw(JAL(1, 6));

  TEST_ASSERT_EQUAL_HEX64(0x80000006, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL_HEX64(0x80000004, get_reg(1));
}

static void test_jalr_may_target_a_2_byte_boundary(void) {
  cpu_set_pc(cpu, 0x80000000);
  set_reg(2, 0x80001000);

  execute_raw(JALR(1, 2, 2));

  TEST_ASSERT_EQUAL_HEX64(0x80001002, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL_HEX64(0x80000004, get_reg(1));
}

#define JALR_FUNCT3_1_X1_0_X2 RV_I(0, 2, 1, 1, ASM_JALR)

static void test_jalr_with_non_zero_funct3_raises_illegal_instruction(void) {
  set_reg(2, 0x80001000);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(JALR_FUNCT3_1_X1_0_X2);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_jal_jumps_relative_to_pc);
  RUN_TEST(test_jal_writes_return_address_to_rd);
  RUN_TEST(test_jalr_jumps_to_rs1_value_plus_immediate);
  RUN_TEST(test_jalr_with_rd_equal_rs1_jumps_to_old_rs1_value);
  RUN_TEST(test_jalr_clears_bit_0_of_target);
  RUN_TEST(test_jal_may_target_a_2_byte_boundary);
  RUN_TEST(test_jalr_may_target_a_2_byte_boundary);
  RUN_TEST(test_jalr_with_non_zero_funct3_raises_illegal_instruction);
  return UNITY_END();
}
