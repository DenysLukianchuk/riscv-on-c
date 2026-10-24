#include "riscv/decoder.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

// addi x1, x0, 5
#define ADDI_X1_X0_5 0x00500093

static void test_decode_extracts_opcode(void) {
  struct Instruction inst = instruction_decode(ADDI_X1_X0_5);

  TEST_ASSERT_EQUAL_HEX8(0x13, inst.opcode);
}

#define ADDI_X31_X0_5 0x00500F93

static void test_decode_extracts_rd(void) {
  struct Instruction inst = instruction_decode(ADDI_X31_X0_5);

  TEST_ASSERT_EQUAL_UINT8(31, inst.rd);
}

#define ANDI_X1_X0_5 0x00507093

static void test_decode_extracts_funct3(void) {
  struct Instruction inst = instruction_decode(ANDI_X1_X0_5);

  TEST_ASSERT_EQUAL_UINT8(7, inst.funct3);
}

#define ADDI_X1_X31_5 0x005F8093

static void test_decode_extracts_rs1(void) {
  struct Instruction inst = instruction_decode(ADDI_X1_X31_5);

  TEST_ASSERT_EQUAL_UINT8(31, inst.rs1);
}

#define ADD_X1_X0_X31 0x01F000B3

static void test_decode_extracts_rs2(void) {
  struct Instruction inst = instruction_decode(ADD_X1_X0_X31);

  TEST_ASSERT_EQUAL_UINT8(31, inst.rs2);
}

#define R_TYPE_FUNCT7_ALL_ONES 0xFE000033

static void test_decode_extracts_funct7(void) {
  struct Instruction inst = instruction_decode(R_TYPE_FUNCT7_ALL_ONES);

  TEST_ASSERT_EQUAL_HEX8(0x7F, inst.funct7);
}

static void test_decode_extracts_positive_i_type_immediate(void) {
  struct Instruction inst = instruction_decode(ADDI_X1_X0_5);

  TEST_ASSERT_EQUAL_INT32(5, inst.imm);
}

#define ADDI_X1_X0_MINUS_1 0xFFF00093

static void test_decode_sign_extends_negative_i_type_immediate(void) {
  struct Instruction inst = instruction_decode(ADDI_X1_X0_MINUS_1);

  TEST_ASSERT_EQUAL_INT32(-1, inst.imm);
}

#define SW_X2_100_X1 0x0620A223

static void test_decode_assembles_s_type_immediate(void) {
  struct Instruction inst = instruction_decode(SW_X2_100_X1);

  TEST_ASSERT_EQUAL_INT32(100, inst.imm);
}

static void test_decode_extracts_rd_of_r_type_instruction(void) {
  struct Instruction inst = instruction_decode(ADD_X1_X0_X31);

  TEST_ASSERT_EQUAL_UINT8(1, inst.rd);
}

#define BEQ_X1_X2_4094 0x7E208FE3

static void test_decode_assembles_b_type_immediate(void) {
  struct Instruction inst = instruction_decode(BEQ_X1_X2_4094);

  TEST_ASSERT_EQUAL_INT32(4094, inst.imm);
}

#define LUI_X1_0X12345 0x123450B7

static void test_decode_extracts_u_type_immediate(void) {
  struct Instruction inst = instruction_decode(LUI_X1_0X12345);

  TEST_ASSERT_EQUAL_HEX32(0x12345000, inst.imm);
}

#define JAL_X1_1048574 0x7FFFF0EF

static void test_decode_assembles_j_type_immediate(void) {
  struct Instruction inst = instruction_decode(JAL_X1_1048574);

  TEST_ASSERT_EQUAL_INT32(1048574, inst.imm);
}

#define AUIPC_X1_0X12345 0x12345097

static void test_decode_extracts_auipc_immediate(void) {
  struct Instruction inst = instruction_decode(AUIPC_X1_0X12345);

  TEST_ASSERT_EQUAL_HEX32(0x12345000, inst.imm);
}

static void test_decode_extracts_rd_of_jal(void) {
  struct Instruction inst = instruction_decode(JAL_X1_1048574);

  TEST_ASSERT_EQUAL_UINT8(1, inst.rd);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_decode_extracts_opcode);
  RUN_TEST(test_decode_extracts_rd);
  RUN_TEST(test_decode_extracts_funct3);
  RUN_TEST(test_decode_extracts_rs1);
  RUN_TEST(test_decode_extracts_rs2);
  RUN_TEST(test_decode_extracts_funct7);
  RUN_TEST(test_decode_extracts_positive_i_type_immediate);
  RUN_TEST(test_decode_sign_extends_negative_i_type_immediate);
  RUN_TEST(test_decode_assembles_s_type_immediate);
  RUN_TEST(test_decode_extracts_rd_of_r_type_instruction);
  RUN_TEST(test_decode_assembles_b_type_immediate);
  RUN_TEST(test_decode_extracts_u_type_immediate);
  RUN_TEST(test_decode_assembles_j_type_immediate);
  RUN_TEST(test_decode_extracts_auipc_immediate);
  RUN_TEST(test_decode_extracts_rd_of_jal);
  return UNITY_END();
}
