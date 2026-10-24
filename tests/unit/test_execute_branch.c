#include "cpu_fixture.h"

#define BEQ_X1_X2_16 0x00208863

static void test_beq_jumps_when_values_are_equal(void) {
  cpu_set_pc(cpu, 0x80000000);
  set_reg(1, 5);
  set_reg(2, 5);

  execute_raw(BEQ_X1_X2_16);

  TEST_ASSERT_EQUAL_HEX64(0x80000010, cpu_get_pc(cpu));
}

static void test_beq_falls_through_when_values_differ(void) {
  cpu_set_pc(cpu, 0x80000000);
  set_reg(1, 5);
  set_reg(2, 6);

  execute_raw(BEQ_X1_X2_16);

  TEST_ASSERT_EQUAL_HEX64(0x80000004, cpu_get_pc(cpu));
}

#define BRANCH_PC 0x80000000
#define TAKEN (BRANCH_PC + 16)
#define NOT_TAKEN (BRANCH_PC + 4)

#define BNE_X1_X2_16 0x00209863
#define BLT_X1_X2_16 0x0020C863
#define BGE_X1_X2_16 0x0020D863
#define BLTU_X1_X2_16 0x0020E863
#define BGEU_X1_X2_16 0x0020F863

static void execute_branch_with(uint32_t raw, uxlen_t x1, uxlen_t x2) {
  cpu_set_pc(cpu, BRANCH_PC);
  set_reg(1, x1);
  set_reg(2, x2);
  execute_raw(raw);
}

static void test_bne_jumps_when_values_differ(void) {
  execute_branch_with(BNE_X1_X2_16, 5, 6);

  TEST_ASSERT_EQUAL_HEX64(TAKEN, cpu_get_pc(cpu));
}

static void test_bne_falls_through_when_values_are_equal(void) {
  execute_branch_with(BNE_X1_X2_16, 5, 5);

  TEST_ASSERT_EQUAL_HEX64(NOT_TAKEN, cpu_get_pc(cpu));
}

static void test_blt_jumps_when_rs1_value_is_less_as_signed(void) {
  execute_branch_with(BLT_X1_X2_16, (uxlen_t)-1, 1);

  TEST_ASSERT_EQUAL_HEX64(TAKEN, cpu_get_pc(cpu));
}

static void test_blt_falls_through_when_rs1_value_is_greater_as_signed(void) {
  execute_branch_with(BLT_X1_X2_16, 1, (uxlen_t)-1);

  TEST_ASSERT_EQUAL_HEX64(NOT_TAKEN, cpu_get_pc(cpu));
}

static void test_bge_jumps_when_values_are_equal(void) {
  execute_branch_with(BGE_X1_X2_16, 5, 5);

  TEST_ASSERT_EQUAL_HEX64(TAKEN, cpu_get_pc(cpu));
}

static void test_bge_jumps_when_rs1_value_is_greater_as_signed(void) {
  execute_branch_with(BGE_X1_X2_16, 1, (uxlen_t)-1);

  TEST_ASSERT_EQUAL_HEX64(TAKEN, cpu_get_pc(cpu));
}

static void test_bge_falls_through_when_rs1_value_is_less_as_signed(void) {
  execute_branch_with(BGE_X1_X2_16, (uxlen_t)-1, 1);

  TEST_ASSERT_EQUAL_HEX64(NOT_TAKEN, cpu_get_pc(cpu));
}

static void test_bltu_jumps_when_rs1_value_is_less_as_unsigned(void) {
  execute_branch_with(BLTU_X1_X2_16, 1, (uxlen_t)-1);

  TEST_ASSERT_EQUAL_HEX64(TAKEN, cpu_get_pc(cpu));
}

static void
test_bltu_falls_through_when_rs1_value_is_greater_as_unsigned(void) {
  execute_branch_with(BLTU_X1_X2_16, (uxlen_t)-1, 1);

  TEST_ASSERT_EQUAL_HEX64(NOT_TAKEN, cpu_get_pc(cpu));
}

static void test_bgeu_jumps_when_values_are_equal(void) {
  execute_branch_with(BGEU_X1_X2_16, 5, 5);

  TEST_ASSERT_EQUAL_HEX64(TAKEN, cpu_get_pc(cpu));
}

static void test_bgeu_jumps_when_rs1_value_is_greater_as_unsigned(void) {
  execute_branch_with(BGEU_X1_X2_16, (uxlen_t)-1, 1);

  TEST_ASSERT_EQUAL_HEX64(TAKEN, cpu_get_pc(cpu));
}

static void test_bgeu_falls_through_when_rs1_value_is_less_as_unsigned(void) {
  execute_branch_with(BGEU_X1_X2_16, 1, (uxlen_t)-1);

  TEST_ASSERT_EQUAL_HEX64(NOT_TAKEN, cpu_get_pc(cpu));
}

#define BEQ_X1_X2_MINUS_16 0xFE2088E3

static void test_beq_jumps_backwards_with_negative_offset(void) {
  cpu_set_pc(cpu, 0x80000100);
  set_reg(1, 5);
  set_reg(2, 5);

  execute_raw(BEQ_X1_X2_MINUS_16);

  TEST_ASSERT_EQUAL_HEX64(0x800000F0, cpu_get_pc(cpu));
}

#define BRANCH_FUNCT3_2_X1_X2_16 0x0020A863
#define BRANCH_FUNCT3_3_X1_X2_16 0x0020B863

static void test_branch_with_reserved_funct3_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(BRANCH_FUNCT3_2_X1_X2_16);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(BRANCH_FUNCT3_3_X1_X2_16);
}

#define BEQ_X1_X2_6 0x00208363

static void
test_taken_branch_to_misaligned_target_raises_address_misaligned(void) {
  write_csr(CSR_REG_MCAUSE, 0xFF);

  execute_branch_with(BEQ_X1_X2_6, 5, 5);

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(CSR_REG_MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(BRANCH_PC + 6, read_csr(CSR_REG_MTVAL));
}

static void test_not_taken_branch_with_misaligned_offset_falls_through(void) {
  execute_branch_with(BEQ_X1_X2_6, 5, 6);

  TEST_ASSERT_EQUAL_HEX64(NOT_TAKEN, cpu_get_pc(cpu));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_beq_jumps_when_values_are_equal);
  RUN_TEST(test_beq_falls_through_when_values_differ);
  RUN_TEST(test_bne_jumps_when_values_differ);
  RUN_TEST(test_bne_falls_through_when_values_are_equal);
  RUN_TEST(test_blt_jumps_when_rs1_value_is_less_as_signed);
  RUN_TEST(test_blt_falls_through_when_rs1_value_is_greater_as_signed);
  RUN_TEST(test_bge_jumps_when_values_are_equal);
  RUN_TEST(test_bge_jumps_when_rs1_value_is_greater_as_signed);
  RUN_TEST(test_bge_falls_through_when_rs1_value_is_less_as_signed);
  RUN_TEST(test_bltu_jumps_when_rs1_value_is_less_as_unsigned);
  RUN_TEST(test_bltu_falls_through_when_rs1_value_is_greater_as_unsigned);
  RUN_TEST(test_bgeu_jumps_when_values_are_equal);
  RUN_TEST(test_bgeu_jumps_when_rs1_value_is_greater_as_unsigned);
  RUN_TEST(test_bgeu_falls_through_when_rs1_value_is_less_as_unsigned);
  RUN_TEST(test_beq_jumps_backwards_with_negative_offset);
  RUN_TEST(test_branch_with_reserved_funct3_raises_illegal_instruction);
  RUN_TEST(test_taken_branch_to_misaligned_target_raises_address_misaligned);
  RUN_TEST(test_not_taken_branch_with_misaligned_offset_falls_through);
  return UNITY_END();
}
