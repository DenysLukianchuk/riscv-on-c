#include "cpu_fixture.h"

#define ADDI_X1_X0_5 0x00500093

static void test_addi_writes_immediate_to_rd(void) {
  execute_raw(ADDI_X1_X0_5);

  TEST_ASSERT_EQUAL_UINT64(5, get_reg(1));
}

#define ADDI_X1_X2_7 0x00710093

static void test_addi_adds_immediate_to_rs1_value(void) {
  set_reg(2, 100);

  execute_raw(ADDI_X1_X2_7);

  TEST_ASSERT_EQUAL_UINT64(107, get_reg(1));
}

#define ADDI_X1_X2_1 0x00110093

static void test_addi_carries_past_bit_31(void) {
  set_reg(2, 0x7FFFFFFF);

  execute_raw(ADDI_X1_X2_1);

  TEST_ASSERT_EQUAL_HEX64(0x80000000, get_reg(1));
}

#define ADDI_X1_X0_MINUS_1 0xFFF00093

static void test_addi_sign_extends_immediate_to_xlen(void) {
  execute_raw(ADDI_X1_X0_MINUS_1);

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-1, get_reg(1));
}

#define ADDI_X1_X2_1024 0x40010093

static void test_addi_with_bit_30_set_in_immediate_still_adds(void) {
  set_reg(2, 100);

  execute_raw(ADDI_X1_X2_1024);

  TEST_ASSERT_EQUAL_HEX64(1124, get_reg(1));
}

#define ANDI_X1_X2_0X3C 0x03C17093

static void test_andi_ands_immediate_with_rs1_value(void) {
  set_reg(2, 0xF0);

  execute_raw(ANDI_X1_X2_0X3C);

  TEST_ASSERT_EQUAL_HEX64(0x30, get_reg(1));
}

#define ORI_X1_X2_0X0F 0x00F16093

static void test_ori_ors_immediate_with_rs1_value(void) {
  set_reg(2, 0xF0);

  execute_raw(ORI_X1_X2_0X0F);

  TEST_ASSERT_EQUAL_HEX64(0xFF, get_reg(1));
}

#define XORI_X1_X2_0XFF 0x0FF14093

static void test_xori_xors_immediate_with_rs1_value(void) {
  set_reg(2, 0xF0);

  execute_raw(XORI_X1_X2_0XFF);

  TEST_ASSERT_EQUAL_HEX64(0x0F, get_reg(1));
}

#define SLTI_X1_X2_3 0x00312093

static void test_slti_compares_rs1_value_as_signed(void) {
  set_reg(2, (uxlen_t)-5);

  execute_raw(SLTI_X1_X2_3);

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
}

static void test_slti_sets_rd_when_rs1_value_is_less(void) {
  set_reg(2, 1);

  execute_raw(SLTI_X1_X2_3);

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
}

#define SLTIU_X1_X2_MINUS_1 0xFFF13093

static void
test_sltiu_compares_against_sign_extended_immediate_as_unsigned(void) {
  set_reg(2, 5);

  execute_raw(SLTIU_X1_X2_MINUS_1);

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
}

#define SLLI_X1_X2_4 0x00411093

static void test_slli_shifts_rs1_value_left(void) {
  set_reg(2, 1);

  execute_raw(SLLI_X1_X2_4);

  TEST_ASSERT_EQUAL_HEX64(0x10, get_reg(1));
}

#define SRLI_X1_X2_4 0x00415093

static void test_srli_shifts_zeros_into_sign_bit(void) {
  set_reg(2, (uxlen_t)-1);

  execute_raw(SRLI_X1_X2_4);

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-1 >> 4, get_reg(1));
}

#define SRAI_X1_X2_4 0x40415093

static void test_srai_shifts_copies_of_sign_bit(void) {
  set_reg(2, (uxlen_t)-256);

  execute_raw(SRAI_X1_X2_4);

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-16, get_reg(1));
}

#define ADD_X1_X2_X3 0x003100B3

static void test_add_adds_rs1_and_rs2_values(void) {
  set_reg(2, 100);
  set_reg(3, 7);

  execute_raw(ADD_X1_X2_X3);

  TEST_ASSERT_EQUAL_HEX64(107, get_reg(1));
}

#define SUB_X1_X2_X3 0x403100B3

static void test_sub_subtracts_rs2_value_from_rs1_value(void) {
  set_reg(2, 100);
  set_reg(3, 7);

  execute_raw(SUB_X1_X2_X3);

  TEST_ASSERT_EQUAL_HEX64(93, get_reg(1));
}

#define SLL_X1_X2_X3 0x003110B3
#define SLT_X1_X2_X3 0x003120B3
#define SLTU_X1_X2_X3 0x003130B3
#define XOR_X1_X2_X3 0x003140B3
#define SRL_X1_X2_X3 0x003150B3
#define SRA_X1_X2_X3 0x403150B3
#define OR_X1_X2_X3 0x003160B3
#define AND_X1_X2_X3 0x003170B3

static void test_sll_shifts_rs1_value_left_by_rs2_value(void) {
  set_reg(2, 1);
  set_reg(3, 4);

  execute_raw(SLL_X1_X2_X3);

  TEST_ASSERT_EQUAL_HEX64(0x10, get_reg(1));
}

static void test_sll_uses_only_low_bits_of_rs2_as_shift_amount(void) {
  set_reg(2, 1);
  set_reg(3, XLEN + 4);

  execute_raw(SLL_X1_X2_X3);

  TEST_ASSERT_EQUAL_HEX64(0x10, get_reg(1));
}

static void test_slt_sets_rd_when_rs1_value_is_less(void) {
  set_reg(2, 1);
  set_reg(3, 3);

  execute_raw(SLT_X1_X2_X3);

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
}

static void test_slt_compares_as_signed(void) {
  set_reg(2, (uxlen_t)-5);
  set_reg(3, 3);

  execute_raw(SLT_X1_X2_X3);

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
}

static void test_sltu_compares_as_unsigned(void) {
  set_reg(2, 5);
  set_reg(3, (uxlen_t)-1);

  execute_raw(SLTU_X1_X2_X3);

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
}

static void test_xor_xors_rs1_and_rs2_values(void) {
  set_reg(2, 0xF0);
  set_reg(3, 0xFF);

  execute_raw(XOR_X1_X2_X3);

  TEST_ASSERT_EQUAL_HEX64(0x0F, get_reg(1));
}

static void test_srl_shifts_zeros_into_sign_bit(void) {
  set_reg(2, (uxlen_t)-1);
  set_reg(3, 4);

  execute_raw(SRL_X1_X2_X3);

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-1 >> 4, get_reg(1));
}

static void test_sra_shifts_copies_of_sign_bit(void) {
  set_reg(2, (uxlen_t)-256);
  set_reg(3, 4);

  execute_raw(SRA_X1_X2_X3);

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-16, get_reg(1));
}

static void test_sra_uses_only_low_bits_of_rs2_as_shift_amount(void) {
  set_reg(2, (uxlen_t)-256);
  set_reg(3, XLEN + 4);

  execute_raw(SRA_X1_X2_X3);

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-16, get_reg(1));
}

static void test_or_ors_rs1_and_rs2_values(void) {
  set_reg(2, 0xF0);
  set_reg(3, 0x0F);

  execute_raw(OR_X1_X2_X3);

  TEST_ASSERT_EQUAL_HEX64(0xFF, get_reg(1));
}

static void test_and_ands_rs1_and_rs2_values(void) {
  set_reg(2, 0xF0);
  set_reg(3, 0x3C);

  execute_raw(AND_X1_X2_X3);

  TEST_ASSERT_EQUAL_HEX64(0x30, get_reg(1));
}

#define LUI_X1_0X12345 0x123450B7

static void test_lui_writes_upper_immediate_to_rd(void) {
  execute_raw(LUI_X1_0X12345);

  TEST_ASSERT_EQUAL_HEX64(0x12345000, get_reg(1));
}

#define LUI_X1_0X80000 0x800000B7

static void test_lui_sign_extends_bit_31_to_xlen(void) {
  execute_raw(LUI_X1_0X80000);

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-1 << 31, get_reg(1));
}

#define AUIPC_X1_0X12345 0x12345097

static void test_auipc_adds_upper_immediate_to_pc(void) {
  cpu_set_pc(cpu, 0x80000000);

  execute_raw(AUIPC_X1_0X12345);

  TEST_ASSERT_EQUAL_HEX64(0x92345000, get_reg(1));
}

#define ADD_WITH_FUNCT7_0X40_X1_X2_X3 0x803100B3

static void test_op_with_unknown_funct7_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(ADD_WITH_FUNCT7_0X40_X1_X2_X3);
}

#define SLL_WITH_FUNCT7_0X20_X1_X2_X3 0x403110B3

static void test_op_with_funct7_0x20_outside_sub_and_sra_raises_illegal(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SLL_WITH_FUNCT7_0X20_X1_X2_X3);
}

#define SLLI_WITH_BIT_30_X1_X2_4 0x40411093

static void test_slli_with_bit_30_set_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SLLI_WITH_BIT_30_X1_X2_4);
}

#define SLLI_X1_X2_XLEN_MINUS_1 (0x00011093 | ((XLEN - 1) << 20))

static void test_slli_accepts_largest_shift_amount(void) {
  set_reg(2, 1);

  execute_raw(SLLI_X1_X2_XLEN_MINUS_1);

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)1 << (XLEN - 1), get_reg(1));
}

#if XLEN == 32
#define SLLI_X1_X2_32 0x02011093

static void test_slli_with_shamt_5_set_is_illegal_on_rv32(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SLLI_X1_X2_32);
}
#endif

#if XLEN == 64
#define ADDIW_X1_X2_1 0x0011009B

static void test_addiw_sign_extends_32_bit_result(void) {
  set_reg(2, 0x7FFFFFFF);

  execute_raw(ADDIW_X1_X2_1);

  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFF80000000, get_reg(1));
}

#define SRLIW_X1_X2_4 0x0041509B

static void test_srliw_shifts_only_low_32_bits(void) {
  set_reg(2, 0xFFFFFFFF80000000);

  execute_raw(SRLIW_X1_X2_4);

  TEST_ASSERT_EQUAL_HEX64(0x08000000, get_reg(1));
}

#define SRAIW_X1_X2_4 0x4041509B

static void test_sraiw_shifts_copies_of_bit_31(void) {
  set_reg(2, 0x0000000080000000);

  execute_raw(SRAIW_X1_X2_4);

  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFF8000000, get_reg(1));
}

static void test_sraiw_shifts_zeros_into_positive_value(void) {
  set_reg(2, 0x0000000010000000);

  execute_raw(SRAIW_X1_X2_4);

  TEST_ASSERT_EQUAL_HEX64(0x01000000, get_reg(1));
}

#define OP_IMM_32_FUNCT3_2_X1_X2_0 0x0001209B
#define OP_IMM_32_FUNCT3_3_X1_X2_0 0x0001309B
#define OP_IMM_32_FUNCT3_4_X1_X2_0 0x0001409B
#define OP_IMM_32_FUNCT3_6_X1_X2_0 0x0001609B
#define OP_IMM_32_FUNCT3_7_X1_X2_0 0x0001709B

static void test_op_imm_32_without_w_variant_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_IMM_32_FUNCT3_2_X1_X2_0);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_IMM_32_FUNCT3_3_X1_X2_0);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_IMM_32_FUNCT3_4_X1_X2_0);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_IMM_32_FUNCT3_6_X1_X2_0);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_IMM_32_FUNCT3_7_X1_X2_0);
}

#define SLLIW_X1_X2_32 0x0201109B
#define SRLIW_X1_X2_32 0x0201509B
#define SRAIW_X1_X2_32 0x4201509B

static void test_w_shift_by_32_or_more_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SLLIW_X1_X2_32);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SRLIW_X1_X2_32);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SRAIW_X1_X2_32);
}

#define ADDW_X1_X2_X3 0x003100BB

static void test_addw_sign_extends_32_bit_sum(void) {
  set_reg(2, 0x7FFFFFFF);
  set_reg(3, 1);

  execute_raw(ADDW_X1_X2_X3);

  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFF80000000, get_reg(1));
}

#define SUBW_X1_X2_X3 0x403100BB
#define SLLW_X1_X2_X3 0x003110BB
#define SRLW_X1_X2_X3 0x003150BB
#define SRAW_X1_X2_X3 0x403150BB

static uxlen_t execute_op_32(uint32_t raw, uxlen_t x2, uxlen_t x3) {
  set_reg(2, x2);
  set_reg(3, x3);
  execute_raw(raw);
  return get_reg(1);
}

static void test_addw_ignores_upper_32_bits_of_operands(void) {
  TEST_ASSERT_EQUAL_HEX64(
      8, execute_op_32(ADDW_X1_X2_X3, 0x100000005, 0x200000003));
}

static void test_subw_sign_extends_32_bit_difference(void) {
  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFFFFFFFFF,
                          execute_op_32(SUBW_X1_X2_X3, 0, 1));
}

static void test_subw_wraps_around_in_32_bits(void) {
  TEST_ASSERT_EQUAL_HEX64(0x7FFFFFFF,
                          execute_op_32(SUBW_X1_X2_X3, 0xFFFFFFFF80000000, 1));
}

static void test_sllw_shifts_low_32_bits_left(void) {
  TEST_ASSERT_EQUAL_HEX64(0x10, execute_op_32(SLLW_X1_X2_X3, 1, 4));
}

static void test_sllw_sign_extends_bit_31_of_result(void) {
  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFF80000000,
                          execute_op_32(SLLW_X1_X2_X3, 1, 31));
}

static void test_sllw_uses_only_low_5_bits_of_rs2(void) {
  TEST_ASSERT_EQUAL_HEX64(0x10, execute_op_32(SLLW_X1_X2_X3, 1, 32 + 4));
}

static void test_srlw_shifts_only_low_32_bits(void) {
  TEST_ASSERT_EQUAL_HEX64(0x08000000,
                          execute_op_32(SRLW_X1_X2_X3, 0xFFFFFFFF80000000, 4));
}

static void test_sraw_shifts_copies_of_bit_31(void) {
  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFF8000000,
                          execute_op_32(SRAW_X1_X2_X3, 0x80000000, 4));
}

static void test_sraw_shifts_zeros_into_positive_value(void) {
  TEST_ASSERT_EQUAL_HEX64(0x01000000,
                          execute_op_32(SRAW_X1_X2_X3, 0x10000000, 4));
}

static void test_sraw_uses_only_low_5_bits_of_rs2(void) {
  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFF8000000,
                          execute_op_32(SRAW_X1_X2_X3, 0x80000000, 32 + 4));
}

#define OP_32_FUNCT3_2_X1_X2_X3 0x003120BB
#define OP_32_FUNCT3_3_X1_X2_X3 0x003130BB
#define OP_32_FUNCT3_4_X1_X2_X3 0x003140BB
#define OP_32_FUNCT3_6_X1_X2_X3 0x003160BB
#define OP_32_FUNCT3_7_X1_X2_X3 0x003170BB

static void test_op_32_without_w_variant_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_32_FUNCT3_2_X1_X2_X3);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_32_FUNCT3_3_X1_X2_X3);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_32_FUNCT3_4_X1_X2_X3);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_32_FUNCT3_6_X1_X2_X3);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(OP_32_FUNCT3_7_X1_X2_X3);
}

#define ADDW_WITH_FUNCT7_0X40_X1_X2_X3 0x803100BB
#define SLLW_WITH_FUNCT7_0X20_X1_X2_X3 0x403110BB

static void test_op_32_with_invalid_funct7_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(ADDW_WITH_FUNCT7_0X40_X1_X2_X3);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SLLW_WITH_FUNCT7_0X20_X1_X2_X3);
}
#endif

#if XLEN == 32
#define ADDIW_X1_X2_1 0x0011009B
#define ADDW_X1_X2_X3 0x003100BB

static void test_w_instructions_are_illegal_on_rv32(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(ADDIW_X1_X2_1);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(ADDW_X1_X2_X3);
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
