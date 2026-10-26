#include "cpu_fixture.h"

#define UNIMPLEMENTED_CSR 0x7FF

static void test_csrrw_writes_old_csr_value_to_rd(void) {
  write_csr(MSCRATCH, 0x1234);
  set_reg(2, 0xABCD);

  execute_raw(CSRRW(1, MSCRATCH, 2));

  TEST_ASSERT_EQUAL_HEX64(0x1234, get_reg(1));
}

static void test_csrrw_writes_rs1_value_to_csr(void) {
  set_reg(2, 0xABCD);

  execute_raw(CSRRW(1, MSCRATCH, 2));

  TEST_ASSERT_EQUAL_HEX64(0xABCD, read_csr(MSCRATCH));
}

static void test_csrrw_advances_pc(void) {
  cpu_set_pc(cpu, RAM_BASE);

  execute_raw(CSRRW(1, MSCRATCH, 2));

  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 4, cpu_get_pc(cpu));
}

static void test_csrrw_with_rd_equal_rs1_swaps_register_and_csr(void) {
  write_csr(MSCRATCH, 0x1234);
  set_reg(2, 0xABCD);

  execute_raw(CSRRW(2, MSCRATCH, 2));

  TEST_ASSERT_EQUAL_HEX64(0x1234, get_reg(2));
  TEST_ASSERT_EQUAL_HEX64(0xABCD, read_csr(MSCRATCH));
}

static void test_csrrs_sets_rs1_bits_in_csr(void) {
  write_csr(MSCRATCH, 0xF0);
  set_reg(2, 0x0F);

  execute_raw(CSRRS(1, MSCRATCH, 2));

  TEST_ASSERT_EQUAL_HEX64(0xF0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(MSCRATCH));
}

static void test_access_to_unimplemented_csr_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRRW(1, UNIMPLEMENTED_CSR, 2));
}

static void test_csrr_reads_csr_with_address_above_0x7ff(void) {
  write_csr(MCAUSE, 0xFF);
  set_reg(1, 0xDEAD);

  execute_raw(CSRR(1, MHARTID));

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
}

static void test_csrrw_to_read_only_csr_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRRW(1, MHARTID, 2));
}

static void test_csrrs_with_rs1_other_than_x0_writes_even_zero(void) {
  set_reg(5, 0);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRRS(1, MHARTID, 5));
}

static void test_csrrc_clears_rs1_bits_in_csr(void) {
  write_csr(MSCRATCH, 0xFF);
  set_reg(2, 0x0F);

  execute_raw(CSRRC(1, MSCRATCH, 2));

  TEST_ASSERT_EQUAL_HEX64(0xFF, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(0xF0, read_csr(MSCRATCH));
}

static void test_csrrc_with_rs1_x0_does_not_write(void) {
  write_csr(MCAUSE, 0xFF);

  execute_raw(CSRRC(1, MHARTID, 0));

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(MCAUSE));
}

static void test_csrrwi_writes_zero_extended_immediate(void) {
  write_csr(MSCRATCH, 0x1234);

  execute_raw(CSRRWI(1, MSCRATCH, 31));

  TEST_ASSERT_EQUAL_HEX64(0x1234, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(0x1F, read_csr(MSCRATCH));
}

static void test_csrrsi_sets_immediate_bits_in_csr(void) {
  write_csr(MSCRATCH, 0x01);

  execute_raw(CSRRSI(1, MSCRATCH, 16));

  TEST_ASSERT_EQUAL_HEX64(0x01, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(0x11, read_csr(MSCRATCH));
}

static void test_csrrci_clears_immediate_bits_in_csr(void) {
  write_csr(MSCRATCH, 0xFF);

  execute_raw(CSRRCI(1, MSCRATCH, 1));

  TEST_ASSERT_EQUAL_HEX64(0xFF, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(0xFE, read_csr(MSCRATCH));
}

static void test_csrrsi_and_csrrci_with_zero_immediate_do_not_write(void) {
  write_csr(MCAUSE, 0xFF);

  execute_raw(CSRRSI(1, MHARTID, 0));
  execute_raw(CSRRCI(1, MHARTID, 0));

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(MCAUSE));
}

static void test_csrrwi_always_writes_even_zero_immediate(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRRWI(1, MHARTID, 0));
}

#define SYSTEM_FUNCT3_4_X1_MSCRATCH_X2 RV_I(MSCRATCH, 2, 4, 1, ASM_SYSTEM)

static void test_system_with_reserved_funct3_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SYSTEM_FUNCT3_4_X1_MSCRATCH_X2);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_csrrw_writes_old_csr_value_to_rd);
  RUN_TEST(test_csrrw_writes_rs1_value_to_csr);
  RUN_TEST(test_csrrw_advances_pc);
  RUN_TEST(test_csrrw_with_rd_equal_rs1_swaps_register_and_csr);
  RUN_TEST(test_csrrs_sets_rs1_bits_in_csr);
  RUN_TEST(test_access_to_unimplemented_csr_raises_illegal_instruction);
  RUN_TEST(test_csrr_reads_csr_with_address_above_0x7ff);
  RUN_TEST(test_csrrw_to_read_only_csr_raises_illegal_instruction);
  RUN_TEST(test_csrrs_with_rs1_other_than_x0_writes_even_zero);
  RUN_TEST(test_csrrc_clears_rs1_bits_in_csr);
  RUN_TEST(test_csrrc_with_rs1_x0_does_not_write);
  RUN_TEST(test_csrrwi_writes_zero_extended_immediate);
  RUN_TEST(test_csrrsi_sets_immediate_bits_in_csr);
  RUN_TEST(test_csrrci_clears_immediate_bits_in_csr);
  RUN_TEST(test_csrrsi_and_csrrci_with_zero_immediate_do_not_write);
  RUN_TEST(test_csrrwi_always_writes_even_zero_immediate);
  RUN_TEST(test_system_with_reserved_funct3_raises_illegal_instruction);
  return UNITY_END();
}
