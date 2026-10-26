#include "cpu_fixture.h"

static void test_addi_writes_immediate_to_rd(void) {
  execute_raw(ADDI(1, 0, 5));

  TEST_ASSERT_EQUAL_UINT64(5, get_reg(1));
}

static void test_addi_adds_immediate_to_rs1_value(void) {
  set_reg(2, 100);

  execute_raw(ADDI(1, 2, 7));

  TEST_ASSERT_EQUAL_UINT64(107, get_reg(1));
}

static void test_addi_carries_past_bit_31(void) {
  set_reg(2, 0x7FFFFFFF);

  execute_raw(ADDI(1, 2, 1));

  TEST_ASSERT_EQUAL_HEX64(0x80000000, get_reg(1));
}

static void test_addi_sign_extends_immediate_to_xlen(void) {
  execute_raw(ADDI(1, 0, -1));

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-1, get_reg(1));
}

static void test_addi_with_bit_30_set_in_immediate_still_adds(void) {
  set_reg(2, 100);

  execute_raw(ADDI(1, 2, 1024));

  TEST_ASSERT_EQUAL_HEX64(1124, get_reg(1));
}

static void test_andi_ands_immediate_with_rs1_value(void) {
  set_reg(2, 0xF0);

  execute_raw(ANDI(1, 2, 0x3C));

  TEST_ASSERT_EQUAL_HEX64(0x30, get_reg(1));
}

static void test_ori_ors_immediate_with_rs1_value(void) {
  set_reg(2, 0xF0);

  execute_raw(ORI(1, 2, 0x0F));

  TEST_ASSERT_EQUAL_HEX64(0xFF, get_reg(1));
}

static void test_xori_xors_immediate_with_rs1_value(void) {
  set_reg(2, 0xF0);

  execute_raw(XORI(1, 2, 0xFF));

  TEST_ASSERT_EQUAL_HEX64(0x0F, get_reg(1));
}

static void test_slti_compares_rs1_value_as_signed(void) {
  set_reg(2, (uxlen_t)-5);

  execute_raw(SLTI(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
}

static void test_slti_sets_rd_when_rs1_value_is_less(void) {
  set_reg(2, 1);

  execute_raw(SLTI(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
}

static void
test_sltiu_compares_against_sign_extended_immediate_as_unsigned(void) {
  set_reg(2, 5);

  execute_raw(SLTIU(1, 2, -1));

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
}

static void test_slli_shifts_rs1_value_left(void) {
  set_reg(2, 1);

  execute_raw(SLLI(1, 2, 4));

  TEST_ASSERT_EQUAL_HEX64(0x10, get_reg(1));
}

static void test_srli_shifts_zeros_into_sign_bit(void) {
  set_reg(2, (uxlen_t)-1);

  execute_raw(SRLI(1, 2, 4));

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-1 >> 4, get_reg(1));
}

static void test_srai_shifts_copies_of_sign_bit(void) {
  set_reg(2, (uxlen_t)-256);

  execute_raw(SRAI(1, 2, 4));

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-16, get_reg(1));
}

static void test_add_adds_rs1_and_rs2_values(void) {
  set_reg(2, 100);
  set_reg(3, 7);

  execute_raw(ADD(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64(107, get_reg(1));
}

static void test_sub_subtracts_rs2_value_from_rs1_value(void) {
  set_reg(2, 100);
  set_reg(3, 7);

  execute_raw(SUB(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64(93, get_reg(1));
}

static void test_sll_shifts_rs1_value_left_by_rs2_value(void) {
  set_reg(2, 1);
  set_reg(3, 4);

  execute_raw(SLL(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64(0x10, get_reg(1));
}

static void test_sll_uses_only_low_bits_of_rs2_as_shift_amount(void) {
  set_reg(2, 1);
  set_reg(3, XLEN + 4);

  execute_raw(SLL(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64(0x10, get_reg(1));
}

static void test_slt_sets_rd_when_rs1_value_is_less(void) {
  set_reg(2, 1);
  set_reg(3, 3);

  execute_raw(SLT(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
}

static void test_slt_compares_as_signed(void) {
  set_reg(2, (uxlen_t)-5);
  set_reg(3, 3);

  execute_raw(SLT(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
}

static void test_sltu_compares_as_unsigned(void) {
  set_reg(2, 5);
  set_reg(3, (uxlen_t)-1);

  execute_raw(SLTU(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
}

static void test_xor_xors_rs1_and_rs2_values(void) {
  set_reg(2, 0xF0);
  set_reg(3, 0xFF);

  execute_raw(XOR(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64(0x0F, get_reg(1));
}

static void test_srl_shifts_zeros_into_sign_bit(void) {
  set_reg(2, (uxlen_t)-1);
  set_reg(3, 4);

  execute_raw(SRL(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-1 >> 4, get_reg(1));
}

static void test_sra_shifts_copies_of_sign_bit(void) {
  set_reg(2, (uxlen_t)-256);
  set_reg(3, 4);

  execute_raw(SRA(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-16, get_reg(1));
}

static void test_sra_uses_only_low_bits_of_rs2_as_shift_amount(void) {
  set_reg(2, (uxlen_t)-256);
  set_reg(3, XLEN + 4);

  execute_raw(SRA(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-16, get_reg(1));
}

static void test_or_ors_rs1_and_rs2_values(void) {
  set_reg(2, 0xF0);
  set_reg(3, 0x0F);

  execute_raw(OR(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64(0xFF, get_reg(1));
}

static void test_and_ands_rs1_and_rs2_values(void) {
  set_reg(2, 0xF0);
  set_reg(3, 0x3C);

  execute_raw(AND(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64(0x30, get_reg(1));
}

static void test_lui_writes_upper_immediate_to_rd(void) {
  execute_raw(LUI(1, 0x12345));

  TEST_ASSERT_EQUAL_HEX64(0x12345000, get_reg(1));
}

static void test_lui_sign_extends_bit_31_to_xlen(void) {
  execute_raw(LUI(1, 0x80000));

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-1 << 31, get_reg(1));
}

static void test_auipc_adds_upper_immediate_to_pc(void) {
  cpu_set_pc(cpu, 0x80000000);

  execute_raw(AUIPC(1, 0x12345));

  TEST_ASSERT_EQUAL_HEX64(0x92345000, get_reg(1));
}

#define ADD_WITH_FUNCT7_0X40_X1_X2_X3 RV_R(0x40, 3, 2, 0, 1, ASM_OP)

static void test_op_with_unknown_funct7_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(ADD_WITH_FUNCT7_0X40_X1_X2_X3);
}

#define SLL_WITH_FUNCT7_0X20_X1_X2_X3 RV_R(0x20, 3, 2, 1, 1, ASM_OP)

static void test_op_with_funct7_0x20_outside_sub_and_sra_raises_illegal(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SLL_WITH_FUNCT7_0X20_X1_X2_X3);
}

#define SLLI_WITH_BIT_30_X1_X2_4 RV_I(0x404, 2, 1, 1, ASM_OP_IMM)

static void test_slli_with_bit_30_set_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SLLI_WITH_BIT_30_X1_X2_4);
}

static void test_slli_accepts_largest_shift_amount(void) {
  set_reg(2, 1);

  execute_raw(SLLI(1, 2, XLEN - 1));

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)1 << (XLEN - 1), get_reg(1));
}

#if XLEN == 32

static void test_slli_with_shamt_5_set_is_illegal_on_rv32(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SLLI(1, 2, 32));
}
#endif

#if XLEN == 64

static void test_addiw_sign_extends_32_bit_result(void) {
  set_reg(2, 0x7FFFFFFF);

  execute_raw(ADDIW(1, 2, 1));

  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFF80000000, get_reg(1));
}

static void test_srliw_shifts_only_low_32_bits(void) {
  set_reg(2, 0xFFFFFFFF80000000);

  execute_raw(SRLIW(1, 2, 4));

  TEST_ASSERT_EQUAL_HEX64(0x08000000, get_reg(1));
}

static void test_sraiw_shifts_copies_of_bit_31(void) {
  set_reg(2, 0x0000000080000000);

  execute_raw(SRAIW(1, 2, 4));

  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFF8000000, get_reg(1));
}

static void test_sraiw_shifts_zeros_into_positive_value(void) {
  set_reg(2, 0x0000000010000000);

  execute_raw(SRAIW(1, 2, 4));

  TEST_ASSERT_EQUAL_HEX64(0x01000000, get_reg(1));
}

#define OP_IMM_32_FUNCT3_2_X1_X2_0 RV_I(0, 2, 2, 1, ASM_OP_IMM_32)
#define OP_IMM_32_FUNCT3_3_X1_X2_0 RV_I(0, 2, 3, 1, ASM_OP_IMM_32)
#define OP_IMM_32_FUNCT3_4_X1_X2_0 RV_I(0, 2, 4, 1, ASM_OP_IMM_32)
#define OP_IMM_32_FUNCT3_6_X1_X2_0 RV_I(0, 2, 6, 1, ASM_OP_IMM_32)
#define OP_IMM_32_FUNCT3_7_X1_X2_0 RV_I(0, 2, 7, 1, ASM_OP_IMM_32)

static void test_op_imm_32_without_w_variant_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_IMM_32_FUNCT3_2_X1_X2_0);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_IMM_32_FUNCT3_3_X1_X2_0);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_IMM_32_FUNCT3_4_X1_X2_0);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_IMM_32_FUNCT3_6_X1_X2_0);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_IMM_32_FUNCT3_7_X1_X2_0);
}

static void test_w_shift_by_32_or_more_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SLLIW(1, 2, 32));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SRLIW(1, 2, 32));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SRAIW(1, 2, 32));
}

static void test_addw_sign_extends_32_bit_sum(void) {
  set_reg(2, 0x7FFFFFFF);
  set_reg(3, 1);

  execute_raw(ADDW(1, 2, 3));

  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFF80000000, get_reg(1));
}

static uxlen_t execute_op_32(uint32_t raw, uxlen_t x2, uxlen_t x3) {
  set_reg(2, x2);
  set_reg(3, x3);
  execute_raw(raw);
  return get_reg(1);
}

static void test_addw_ignores_upper_32_bits_of_operands(void) {
  TEST_ASSERT_EQUAL_HEX64(
      8, execute_op_32(ADDW(1, 2, 3), 0x100000005, 0x200000003));
}

static void test_subw_sign_extends_32_bit_difference(void) {
  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFFFFFFFFF,
                          execute_op_32(SUBW(1, 2, 3), 0, 1));
}

static void test_subw_wraps_around_in_32_bits(void) {
  TEST_ASSERT_EQUAL_HEX64(0x7FFFFFFF,
                          execute_op_32(SUBW(1, 2, 3), 0xFFFFFFFF80000000, 1));
}

static void test_sllw_shifts_low_32_bits_left(void) {
  TEST_ASSERT_EQUAL_HEX64(0x10, execute_op_32(SLLW(1, 2, 3), 1, 4));
}

static void test_sllw_sign_extends_bit_31_of_result(void) {
  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFF80000000,
                          execute_op_32(SLLW(1, 2, 3), 1, 31));
}

static void test_sllw_uses_only_low_5_bits_of_rs2(void) {
  TEST_ASSERT_EQUAL_HEX64(0x10, execute_op_32(SLLW(1, 2, 3), 1, 32 + 4));
}

static void test_srlw_shifts_only_low_32_bits(void) {
  TEST_ASSERT_EQUAL_HEX64(0x08000000,
                          execute_op_32(SRLW(1, 2, 3), 0xFFFFFFFF80000000, 4));
}

static void test_sraw_shifts_copies_of_bit_31(void) {
  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFF8000000,
                          execute_op_32(SRAW(1, 2, 3), 0x80000000, 4));
}

static void test_sraw_shifts_zeros_into_positive_value(void) {
  TEST_ASSERT_EQUAL_HEX64(0x01000000,
                          execute_op_32(SRAW(1, 2, 3), 0x10000000, 4));
}

static void test_sraw_uses_only_low_5_bits_of_rs2(void) {
  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFF8000000,
                          execute_op_32(SRAW(1, 2, 3), 0x80000000, 32 + 4));
}

#define OP_32_FUNCT3_2_X1_X2_X3 RV_R(0, 3, 2, 2, 1, ASM_OP_32)
#define OP_32_FUNCT3_3_X1_X2_X3 RV_R(0, 3, 2, 3, 1, ASM_OP_32)
#define OP_32_FUNCT3_4_X1_X2_X3 RV_R(0, 3, 2, 4, 1, ASM_OP_32)
#define OP_32_FUNCT3_6_X1_X2_X3 RV_R(0, 3, 2, 6, 1, ASM_OP_32)
#define OP_32_FUNCT3_7_X1_X2_X3 RV_R(0, 3, 2, 7, 1, ASM_OP_32)

static void test_op_32_without_w_variant_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_32_FUNCT3_2_X1_X2_X3);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_32_FUNCT3_3_X1_X2_X3);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_32_FUNCT3_4_X1_X2_X3);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_32_FUNCT3_6_X1_X2_X3);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_32_FUNCT3_7_X1_X2_X3);
}

#define ADDW_WITH_FUNCT7_0X40_X1_X2_X3 RV_R(0x40, 3, 2, 0, 1, ASM_OP_32)
#define SLLW_WITH_FUNCT7_0X20_X1_X2_X3 RV_R(0x20, 3, 2, 1, 1, ASM_OP_32)

static void test_op_32_with_invalid_funct7_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(ADDW_WITH_FUNCT7_0X40_X1_X2_X3);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SLLW_WITH_FUNCT7_0X20_X1_X2_X3);
}
#endif

#if XLEN == 32

static void test_w_instructions_are_illegal_on_rv32(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(ADDIW(1, 2, 1));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(ADDW(1, 2, 3));
}
#endif

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_addi_writes_immediate_to_rd);
  RUN_TEST(test_addi_adds_immediate_to_rs1_value);
  RUN_TEST(test_addi_carries_past_bit_31);
  RUN_TEST(test_addi_sign_extends_immediate_to_xlen);
  RUN_TEST(test_addi_with_bit_30_set_in_immediate_still_adds);
  RUN_TEST(test_andi_ands_immediate_with_rs1_value);
  RUN_TEST(test_ori_ors_immediate_with_rs1_value);
  RUN_TEST(test_xori_xors_immediate_with_rs1_value);
  RUN_TEST(test_slti_compares_rs1_value_as_signed);
  RUN_TEST(test_slti_sets_rd_when_rs1_value_is_less);
  RUN_TEST(test_sltiu_compares_against_sign_extended_immediate_as_unsigned);
  RUN_TEST(test_slli_shifts_rs1_value_left);
  RUN_TEST(test_srli_shifts_zeros_into_sign_bit);
  RUN_TEST(test_srai_shifts_copies_of_sign_bit);
  RUN_TEST(test_add_adds_rs1_and_rs2_values);
  RUN_TEST(test_sub_subtracts_rs2_value_from_rs1_value);
  RUN_TEST(test_sll_shifts_rs1_value_left_by_rs2_value);
  RUN_TEST(test_sll_uses_only_low_bits_of_rs2_as_shift_amount);
  RUN_TEST(test_slt_sets_rd_when_rs1_value_is_less);
  RUN_TEST(test_slt_compares_as_signed);
  RUN_TEST(test_sltu_compares_as_unsigned);
  RUN_TEST(test_xor_xors_rs1_and_rs2_values);
  RUN_TEST(test_srl_shifts_zeros_into_sign_bit);
  RUN_TEST(test_sra_shifts_copies_of_sign_bit);
  RUN_TEST(test_sra_uses_only_low_bits_of_rs2_as_shift_amount);
  RUN_TEST(test_or_ors_rs1_and_rs2_values);
  RUN_TEST(test_and_ands_rs1_and_rs2_values);
  RUN_TEST(test_lui_writes_upper_immediate_to_rd);
  RUN_TEST(test_lui_sign_extends_bit_31_to_xlen);
  RUN_TEST(test_auipc_adds_upper_immediate_to_pc);
  RUN_TEST(test_op_with_unknown_funct7_raises_illegal_instruction);
  RUN_TEST(test_op_with_funct7_0x20_outside_sub_and_sra_raises_illegal);
  RUN_TEST(test_slli_with_bit_30_set_raises_illegal_instruction);
  RUN_TEST(test_slli_accepts_largest_shift_amount);
#if XLEN == 64
  RUN_TEST(test_addiw_sign_extends_32_bit_result);
  RUN_TEST(test_srliw_shifts_only_low_32_bits);
  RUN_TEST(test_sraiw_shifts_copies_of_bit_31);
  RUN_TEST(test_sraiw_shifts_zeros_into_positive_value);
  RUN_TEST(test_op_imm_32_without_w_variant_raises_illegal_instruction);
  RUN_TEST(test_w_shift_by_32_or_more_raises_illegal_instruction);
  RUN_TEST(test_addw_sign_extends_32_bit_sum);
  RUN_TEST(test_addw_ignores_upper_32_bits_of_operands);
  RUN_TEST(test_subw_sign_extends_32_bit_difference);
  RUN_TEST(test_subw_wraps_around_in_32_bits);
  RUN_TEST(test_sllw_shifts_low_32_bits_left);
  RUN_TEST(test_sllw_sign_extends_bit_31_of_result);
  RUN_TEST(test_sllw_uses_only_low_5_bits_of_rs2);
  RUN_TEST(test_srlw_shifts_only_low_32_bits);
  RUN_TEST(test_sraw_shifts_copies_of_bit_31);
  RUN_TEST(test_sraw_shifts_zeros_into_positive_value);
  RUN_TEST(test_sraw_uses_only_low_5_bits_of_rs2);
  RUN_TEST(test_op_32_without_w_variant_raises_illegal_instruction);
  RUN_TEST(test_op_32_with_invalid_funct7_raises_illegal_instruction);
#endif
#if XLEN == 32
  RUN_TEST(test_slli_with_shamt_5_set_is_illegal_on_rv32);
  RUN_TEST(test_w_instructions_are_illegal_on_rv32);
#endif
  return UNITY_END();
}
