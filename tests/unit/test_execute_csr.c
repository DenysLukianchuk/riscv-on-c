#include "cpu_fixture.h"

#define CSRRW_X1_MSCRATCH_X2 0x340110F3

static void test_csrrw_writes_old_csr_value_to_rd(void) {
  write_csr(CSR_REG_MSCRATCH, 0x1234);
  set_reg(2, 0xABCD);

  execute_raw(CSRRW_X1_MSCRATCH_X2);

  TEST_ASSERT_EQUAL_HEX64(0x1234, get_reg(1));
}

static void test_csrrw_writes_rs1_value_to_csr(void) {
  set_reg(2, 0xABCD);

  execute_raw(CSRRW_X1_MSCRATCH_X2);

  TEST_ASSERT_EQUAL_HEX64(0xABCD, read_csr(CSR_REG_MSCRATCH));
}

static void test_csrrw_advances_pc(void) {
  cpu_set_pc(cpu, RAM_BASE);

  execute_raw(CSRRW_X1_MSCRATCH_X2);

  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 4, cpu_get_pc(cpu));
}

#define CSRRW_X2_MSCRATCH_X2 0x34011173

static void test_csrrw_with_rd_equal_rs1_swaps_register_and_csr(void) {
  write_csr(CSR_REG_MSCRATCH, 0x1234);
  set_reg(2, 0xABCD);

  execute_raw(CSRRW_X2_MSCRATCH_X2);

  TEST_ASSERT_EQUAL_HEX64(0x1234, get_reg(2));
  TEST_ASSERT_EQUAL_HEX64(0xABCD, read_csr(CSR_REG_MSCRATCH));
}

#define CSRRS_X1_MSCRATCH_X2 0x340120F3

static void test_csrrs_sets_rs1_bits_in_csr(void) {
  write_csr(CSR_REG_MSCRATCH, 0xF0);
  set_reg(2, 0x0F);

  execute_raw(CSRRS_X1_MSCRATCH_X2);

  TEST_ASSERT_EQUAL_HEX64(0xF0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(CSR_REG_MSCRATCH));
}

#define CSRRW_X1_UNIMPLEMENTED_X2 0x7FF110F3

static void test_access_to_unimplemented_csr_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRRW_X1_UNIMPLEMENTED_X2);
}

#define CSRR_X1_MHARTID 0xF14020F3

static void test_csrr_reads_csr_with_address_above_0x7ff(void) {
  write_csr(CSR_REG_MCAUSE, 0xFF);
  set_reg(1, 0xDEAD);

  execute_raw(CSRR_X1_MHARTID);

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(CSR_REG_MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
}

#define CSRRW_X1_MHARTID_X2 0xF14110F3

static void test_csrrw_to_read_only_csr_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRRW_X1_MHARTID_X2);
}

#define CSRRS_X1_MHARTID_X5 0xF142A0F3

static void test_csrrs_with_rs1_other_than_x0_writes_even_zero(void) {
  set_reg(5, 0);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRRS_X1_MHARTID_X5);
}

#define CSRRC_X1_MSCRATCH_X2 0x340130F3

static void test_csrrc_clears_rs1_bits_in_csr(void) {
  write_csr(CSR_REG_MSCRATCH, 0xFF);
  set_reg(2, 0x0F);

  execute_raw(CSRRC_X1_MSCRATCH_X2);

  TEST_ASSERT_EQUAL_HEX64(0xFF, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(0xF0, read_csr(CSR_REG_MSCRATCH));
}

#define CSRRC_X1_MHARTID_X0 0xF14030F3

static void test_csrrc_with_rs1_x0_does_not_write(void) {
  write_csr(CSR_REG_MCAUSE, 0xFF);

  execute_raw(CSRRC_X1_MHARTID_X0);

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(CSR_REG_MCAUSE));
}

#define CSRRWI_X1_MSCRATCH_31 0x340FD0F3

static void test_csrrwi_writes_zero_extended_immediate(void) {
  write_csr(CSR_REG_MSCRATCH, 0x1234);

  execute_raw(CSRRWI_X1_MSCRATCH_31);

  TEST_ASSERT_EQUAL_HEX64(0x1234, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(0x1F, read_csr(CSR_REG_MSCRATCH));
}

#define CSRRSI_X1_MSCRATCH_16 0x340860F3

static void test_csrrsi_sets_immediate_bits_in_csr(void) {
  write_csr(CSR_REG_MSCRATCH, 0x01);

  execute_raw(CSRRSI_X1_MSCRATCH_16);

  TEST_ASSERT_EQUAL_HEX64(0x01, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(0x11, read_csr(CSR_REG_MSCRATCH));
}

#define CSRRCI_X1_MSCRATCH_1 0x3400F0F3

static void test_csrrci_clears_immediate_bits_in_csr(void) {
  write_csr(CSR_REG_MSCRATCH, 0xFF);

  execute_raw(CSRRCI_X1_MSCRATCH_1);

  TEST_ASSERT_EQUAL_HEX64(0xFF, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(0xFE, read_csr(CSR_REG_MSCRATCH));
}

#define CSRRSI_X1_MHARTID_0 0xF14060F3
#define CSRRCI_X1_MHARTID_0 0xF14070F3

static void test_csrrsi_and_csrrci_with_zero_immediate_do_not_write(void) {
  write_csr(CSR_REG_MCAUSE, 0xFF);

  execute_raw(CSRRSI_X1_MHARTID_0);
  execute_raw(CSRRCI_X1_MHARTID_0);

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(CSR_REG_MCAUSE));
}

#define CSRRWI_X1_MHARTID_0 0xF14050F3

static void test_csrrwi_always_writes_even_zero_immediate(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRRWI_X1_MHARTID_0);
}

#define SYSTEM_FUNCT3_4_X1_MSCRATCH_X2 0x340140F3

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
