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

static void test_jal_to_misaligned_target_jumps_to_trap_handler(void) {
  write_csr(MTVEC, TRAP_HANDLER);
  cpu_set_pc(cpu, 0x80000000);

  execute_raw(JAL(1, 6));

  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
}

static void test_jal_to_misaligned_target_raises_address_misaligned(void) {
  write_csr(MCAUSE, 0xFF);
  cpu_set_pc(cpu, 0x80000000);

  execute_raw(JAL(1, 6));

  TEST_ASSERT_EQUAL_HEX64(CAUSE_MISALIGNED_FETCH, read_csr(MCAUSE));
}

static void execute_misaligned_jal(void) {
  cpu_set_pc(cpu, 0x80000000);
  set_reg(1, 0xDEAD);

  execute_raw(JAL(1, 6));
}

static void test_misaligned_jal_saves_target_address_to_mtval(void) {
  execute_misaligned_jal();

  TEST_ASSERT_EQUAL_HEX64(0x80000006, read_csr(MTVAL));
}

static void test_misaligned_jal_saves_its_own_address_to_mepc(void) {
  execute_misaligned_jal();

  TEST_ASSERT_EQUAL_HEX64(0x80000000, read_csr(MEPC));
}

static void test_misaligned_jal_leaves_rd_unchanged(void) {
  execute_misaligned_jal();

  TEST_ASSERT_EQUAL_HEX64(0xDEAD, get_reg(1));
}

static void test_jalr_to_misaligned_target_raises_address_misaligned(void) {
  write_csr(MCAUSE, 0xFF);
  cpu_set_pc(cpu, 0x80000000);
  set_reg(2, 0x80001000);

  execute_raw(JALR(1, 2, 2));

  TEST_ASSERT_EQUAL_HEX64(CAUSE_MISALIGNED_FETCH, read_csr(MCAUSE));
}

static void test_misaligned_jalr_saves_target_address_to_mtval(void) {
  cpu_set_pc(cpu, 0x80000000);
  set_reg(2, 0x80001000);

  execute_raw(JALR(1, 2, 2));

  TEST_ASSERT_EQUAL_HEX64(0x80001002, read_csr(MTVAL));
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
  RUN_TEST(test_jal_to_misaligned_target_jumps_to_trap_handler);
  RUN_TEST(test_jal_to_misaligned_target_raises_address_misaligned);
  RUN_TEST(test_misaligned_jal_saves_target_address_to_mtval);
  RUN_TEST(test_misaligned_jal_saves_its_own_address_to_mepc);
  RUN_TEST(test_misaligned_jal_leaves_rd_unchanged);
  RUN_TEST(test_jalr_to_misaligned_target_raises_address_misaligned);
  RUN_TEST(test_misaligned_jalr_saves_target_address_to_mtval);
  RUN_TEST(test_jalr_with_non_zero_funct3_raises_illegal_instruction);
  return UNITY_END();
}
