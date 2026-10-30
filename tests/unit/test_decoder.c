#include "riscv/cpu/decoder.h"
#include "riscv_asm.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

// addi x1, x0, 5

static void test_decode_extracts_opcode(void) {
  struct Instruction inst = instruction_decode(ADDI(1, 0, 5));

  TEST_ASSERT_EQUAL_HEX8(0x13, inst.opcode);
}

static void test_decode_extracts_rd(void) {
  struct Instruction inst = instruction_decode(ADDI(31, 0, 5));

  TEST_ASSERT_EQUAL_UINT8(31, inst.rd);
}

static void test_decode_extracts_funct3(void) {
  struct Instruction inst = instruction_decode(ANDI(1, 0, 5));

  TEST_ASSERT_EQUAL_UINT8(7, inst.funct3);
}

static void test_decode_extracts_rs1(void) {
  struct Instruction inst = instruction_decode(ADDI(1, 31, 5));

  TEST_ASSERT_EQUAL_UINT8(31, inst.rs1);
}

static void test_decode_extracts_rs2(void) {
  struct Instruction inst = instruction_decode(ADD(1, 0, 31));

  TEST_ASSERT_EQUAL_UINT8(31, inst.rs2);
}

#define R_TYPE_FUNCT7_ALL_ONES RV_R(0x7F, 0, 0, 0, 0, ASM_OP)

static void test_decode_extracts_funct7(void) {
  struct Instruction inst = instruction_decode(R_TYPE_FUNCT7_ALL_ONES);

  TEST_ASSERT_EQUAL_HEX8(0x7F, inst.funct7);
}

static void test_decode_extracts_positive_i_type_immediate(void) {
  struct Instruction inst = instruction_decode(ADDI(1, 0, 5));

  TEST_ASSERT_EQUAL_INT32(5, inst.imm);
}

static void test_decode_sign_extends_negative_i_type_immediate(void) {
  struct Instruction inst = instruction_decode(ADDI(1, 0, -1));

  TEST_ASSERT_EQUAL_INT32(-1, inst.imm);
}

static void test_decode_assembles_s_type_immediate(void) {
  struct Instruction inst = instruction_decode(SW(2, 100, 1));

  TEST_ASSERT_EQUAL_INT32(100, inst.imm);
}

static void test_decode_extracts_rd_of_r_type_instruction(void) {
  struct Instruction inst = instruction_decode(ADD(1, 0, 31));

  TEST_ASSERT_EQUAL_UINT8(1, inst.rd);
}

static void test_decode_assembles_b_type_immediate(void) {
  struct Instruction inst = instruction_decode(BEQ(1, 2, 4094));

  TEST_ASSERT_EQUAL_INT32(4094, inst.imm);
}

static void test_decode_extracts_u_type_immediate(void) {
  struct Instruction inst = instruction_decode(LUI(1, 0x12345));

  TEST_ASSERT_EQUAL_HEX32(0x12345000, inst.imm);
}

static void test_decode_assembles_j_type_immediate(void) {
  struct Instruction inst = instruction_decode(JAL(1, 1048574));

  TEST_ASSERT_EQUAL_INT32(1048574, inst.imm);
}

static void test_decode_extracts_auipc_immediate(void) {
  struct Instruction inst = instruction_decode(AUIPC(1, 0x12345));

  TEST_ASSERT_EQUAL_HEX32(0x12345000, inst.imm);
}

static void test_decode_extracts_rd_of_jal(void) {
  struct Instruction inst = instruction_decode(JAL(1, 1048574));

  TEST_ASSERT_EQUAL_UINT8(1, inst.rd);
}

// Bits 1:0 tell the length: 11 marks a 32-bit instruction, anything else a
// 16-bit one from the C extension, which decodes as the instruction it expands
// to.

static void test_decode_gives_32_bit_instruction_length_4(void) {
  struct Instruction inst = instruction_decode(ADDI(1, 0, 5));

  TEST_ASSERT_EQUAL_UINT8(4, inst.length);
}

static void test_decode_expands_compressed_instruction(void) {
  struct Instruction inst = instruction_decode(0x9782); // c.jalr a5

  TEST_ASSERT_EQUAL_HEX8(0x67, inst.opcode);
  TEST_ASSERT_EQUAL_UINT8(1, inst.rd);
  TEST_ASSERT_EQUAL_UINT8(15, inst.rs1);
  TEST_ASSERT_EQUAL_INT32(0, inst.imm);
  TEST_ASSERT_EQUAL_UINT8(2, inst.length);
}

// raw keeps the 16 bits as fetched: an illegal instruction reports them in
// mtval, not the expansion.
static void test_decode_keeps_compressed_bits_in_raw(void) {
  struct Instruction inst = instruction_decode(0x9782);

  TEST_ASSERT_EQUAL_HEX32(0x9782, inst.raw);
}

// The fetch may read a whole word; the upper half then belongs to the next
// instruction and must not leak into this one.
static void test_decode_ignores_upper_half_after_compressed_instruction(void) {
  struct Instruction inst = instruction_decode(0x12349782);

  TEST_ASSERT_EQUAL_HEX32(0x9782, inst.raw);
  TEST_ASSERT_EQUAL_UINT8(15, inst.rs1);
  TEST_ASSERT_EQUAL_UINT8(2, inst.length);
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
  RUN_TEST(test_decode_gives_32_bit_instruction_length_4);
  RUN_TEST(test_decode_expands_compressed_instruction);
  RUN_TEST(test_decode_keeps_compressed_bits_in_raw);
  RUN_TEST(test_decode_ignores_upper_half_after_compressed_instruction);
  return UNITY_END();
}
