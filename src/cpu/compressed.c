#include "riscv/cpu/decoder.h"
#include "riscv/util/bits.h"
#include <stdbool.h>

#define C_OP_HI 1
#define C_OP_LO 0
#define C_FUNCT3_HI 15
#define C_FUNCT3_LO 13
#define C_FUNCT4_HI 15
#define C_FUNCT4_LO 12
#define C_FUNCT2_HI 11
#define C_FUNCT2_LO 10
#define C_ARITH_HI 6
#define C_ARITH_LO 5
#define C_WORD_BIT 12
#define C_RD_HI 11
#define C_RD_LO 7
#define C_RS2_HI 6
#define C_RS2_LO 2
#define C_PRIME_HIGH_HI 9
#define C_PRIME_HIGH_LO 7
#define C_PRIME_LOW_HI 4
#define C_PRIME_LOW_LO 2
#define C_PRIME_BASE 8

#define CI_IMM_5_BIT 12
#define CI_IMM_4_0_HI 6
#define CI_IMM_4_0_LO 2

#define CIW_IMM_5_4_HI 12
#define CIW_IMM_5_4_LO 11
#define CIW_IMM_9_6_HI 10
#define CIW_IMM_9_6_LO 7
#define CIW_IMM_2_BIT 6
#define CIW_IMM_3_BIT 5

#define CL_IMM_5_3_HI 12
#define CL_IMM_5_3_LO 10
#define CLW_IMM_2_BIT 6
#define CLW_IMM_6_BIT 5
#define CLD_IMM_7_6_HI 6
#define CLD_IMM_7_6_LO 5

#define ADDI16SP_IMM_9_BIT 12
#define ADDI16SP_IMM_4_BIT 6
#define ADDI16SP_IMM_6_BIT 5
#define ADDI16SP_IMM_8_7_HI 4
#define ADDI16SP_IMM_8_7_LO 3
#define ADDI16SP_IMM_5_BIT 2

#define CJ_IMM_11_BIT 12
#define CJ_IMM_4_BIT 11
#define CJ_IMM_9_8_HI 10
#define CJ_IMM_9_8_LO 9
#define CJ_IMM_10_BIT 8
#define CJ_IMM_6_BIT 7
#define CJ_IMM_7_BIT 6
#define CJ_IMM_3_1_HI 5
#define CJ_IMM_3_1_LO 3
#define CJ_IMM_5_BIT 2

#define CB_IMM_8_BIT 12
#define CB_IMM_4_3_HI 11
#define CB_IMM_4_3_LO 10
#define CB_IMM_7_6_HI 6
#define CB_IMM_7_6_LO 5
#define CB_IMM_2_1_HI 4
#define CB_IMM_2_1_LO 3
#define CB_IMM_5_BIT 2

#define LWSP_IMM_5_BIT 12
#define LWSP_IMM_4_2_HI 6
#define LWSP_IMM_4_2_LO 4
#define LWSP_IMM_7_6_HI 3
#define LWSP_IMM_7_6_LO 2

#define LDSP_IMM_5_BIT 12
#define LDSP_IMM_4_3_HI 6
#define LDSP_IMM_4_3_LO 5
#define LDSP_IMM_8_6_HI 4
#define LDSP_IMM_8_6_LO 2

#define SWSP_IMM_5_2_HI 12
#define SWSP_IMM_5_2_LO 9
#define SWSP_IMM_7_6_HI 8
#define SWSP_IMM_7_6_LO 7

#define SDSP_IMM_5_3_HI 12
#define SDSP_IMM_5_3_LO 10
#define SDSP_IMM_8_6_HI 9
#define SDSP_IMM_8_6_LO 7

#define CI_IMM_WIDTH 6
#define CB_IMM_WIDTH 9
#define CJ_IMM_WIDTH 12
#define ADDI16SP_IMM_WIDTH 10
#define U_IMM_WIDTH 20

#define C_ILLEGAL 0
#define EBREAK_IMM 1
#define REG_ZERO 0
#define REG_RA 1
#define REG_SP 2

enum CQuadrant { C_QUADRANT_0 = 0, C_QUADRANT_1 = 1, C_QUADRANT_2 = 2 };
enum CFunct3Quadrant0 {
  C_ADDI4SPN = 0,
  C_LW = 2,
  C_LD = 3,
  C_SW = 6,
  C_SD = 7
};
enum CFunct3Quadrant1 {
  C_ADDI = 0,
  C_ADDIW_JAL = 1,
  C_LI = 2,
  C_LUI_ADDI16SP = 3,
  C_MISC_ALU = 4,
  C_J = 5,
  C_BEQZ = 6,
  C_BNEZ = 7
};
enum CFunct3Quadrant2 {
  C_SLLI = 0,
  C_LWSP = 2,
  C_LDSP = 3,
  C_JR_MV_ADD = 4,
  C_SWSP = 6,
  C_SDSP = 7
};
enum CFunct2 { C_SRLI = 0, C_SRAI = 1, C_ANDI = 2, C_ARITH = 3 };
enum CArith { C_SUB = 0, C_XOR = 1, C_OR = 2, C_AND = 3 };
enum CArithWord { C_SUBW = 0, C_ADDW = 1 };
enum CFunct4 { C_JR_MV = 0x8, C_JALR_ADD = 0x9 };

static uint32_t encode_i(int32_t imm, unsigned rs1, unsigned funct3,
                         unsigned rd, unsigned opcode) {
  return (uint32_t)(imm & low_mask(I_IMM_WIDTH)) << I_IMM_LO | rs1 << RS1_LO |
         funct3 << FUNCT3_LO | rd << RD_LO | opcode;
}

static uint32_t encode_r(unsigned funct7, unsigned rs2, unsigned rs1,
                         unsigned funct3, unsigned rd, unsigned opcode) {
  return funct7 << FUNCT7_LO | rs2 << RS2_LO | rs1 << RS1_LO |
         funct3 << FUNCT3_LO | rd << RD_LO | opcode;
}

static uint32_t encode_s(int32_t imm, unsigned rs2, unsigned rs1,
                         unsigned funct3, unsigned opcode) {
  return (uint32_t)bits(imm, 11, 5) << S_IMM_11_5_LO | rs2 << RS2_LO |
         rs1 << RS1_LO | funct3 << FUNCT3_LO |
         (uint32_t)bits(imm, 4, 0) << S_IMM_4_0_LO | opcode;
}

static uint32_t encode_b(int32_t imm, unsigned rs2, unsigned rs1,
                         unsigned funct3) {
  return (uint32_t)bits(imm, 12, 12) << B_IMM_12_BIT |
         (uint32_t)bits(imm, 10, 5) << B_IMM_10_5_LO | rs2 << RS2_LO |
         rs1 << RS1_LO | funct3 << FUNCT3_LO |
         (uint32_t)bits(imm, 4, 1) << B_IMM_4_1_LO |
         (uint32_t)bits(imm, 11, 11) << B_IMM_11_BIT | OPCODE_BRANCH;
}

static uint32_t encode_j(int32_t imm, unsigned rd) {
  return (uint32_t)bits(imm, 20, 20) << J_IMM_20_BIT |
         (uint32_t)bits(imm, 10, 1) << J_IMM_10_1_LO |
         (uint32_t)bits(imm, 11, 11) << J_IMM_11_BIT |
         (uint32_t)bits(imm, 19, 12) << J_IMM_19_12_LO | rd << RD_LO |
         OPCODE_JAL;
}

static uint32_t encode_u(int32_t imm20, unsigned rd, unsigned opcode) {
  return (uint32_t)(imm20 & low_mask(U_IMM_WIDTH)) << U_IMM_LO | rd << RD_LO |
         opcode;
}

static uint32_t place(uint16_t c, unsigned hi, unsigned lo, unsigned to) {
  return (uint32_t)bits(c, hi, lo) << to;
}

static unsigned prime(uint16_t c, unsigned hi, unsigned lo) {
  return C_PRIME_BASE + (unsigned)bits(c, hi, lo);
}

static unsigned ci_uimm(uint16_t c) {
  return place(c, CI_IMM_5_BIT, CI_IMM_5_BIT, 5) |
         place(c, CI_IMM_4_0_HI, CI_IMM_4_0_LO, 0);
}

static int32_t ci_imm(uint16_t c) {
  return sign_extend(ci_uimm(c), CI_IMM_WIDTH);
}

static unsigned addi4spn_imm(uint16_t c) {
  return place(c, CIW_IMM_5_4_HI, CIW_IMM_5_4_LO, 4) |
         place(c, CIW_IMM_9_6_HI, CIW_IMM_9_6_LO, 6) |
         place(c, CIW_IMM_2_BIT, CIW_IMM_2_BIT, 2) |
         place(c, CIW_IMM_3_BIT, CIW_IMM_3_BIT, 3);
}

static unsigned cl_word_imm(uint16_t c) {
  return place(c, CL_IMM_5_3_HI, CL_IMM_5_3_LO, 3) |
         place(c, CLW_IMM_2_BIT, CLW_IMM_2_BIT, 2) |
         place(c, CLW_IMM_6_BIT, CLW_IMM_6_BIT, 6);
}

static unsigned cl_double_imm(uint16_t c) {
  return place(c, CL_IMM_5_3_HI, CL_IMM_5_3_LO, 3) |
         place(c, CLD_IMM_7_6_HI, CLD_IMM_7_6_LO, 6);
}

static int32_t addi16sp_imm(uint16_t c) {
  return sign_extend(place(c, ADDI16SP_IMM_9_BIT, ADDI16SP_IMM_9_BIT, 9) |
                         place(c, ADDI16SP_IMM_4_BIT, ADDI16SP_IMM_4_BIT, 4) |
                         place(c, ADDI16SP_IMM_6_BIT, ADDI16SP_IMM_6_BIT, 6) |
                         place(c, ADDI16SP_IMM_8_7_HI, ADDI16SP_IMM_8_7_LO, 7) |
                         place(c, ADDI16SP_IMM_5_BIT, ADDI16SP_IMM_5_BIT, 5),
                     ADDI16SP_IMM_WIDTH);
}

static int32_t cj_imm(uint16_t c) {
  return sign_extend(place(c, CJ_IMM_11_BIT, CJ_IMM_11_BIT, 11) |
                         place(c, CJ_IMM_4_BIT, CJ_IMM_4_BIT, 4) |
                         place(c, CJ_IMM_9_8_HI, CJ_IMM_9_8_LO, 8) |
                         place(c, CJ_IMM_10_BIT, CJ_IMM_10_BIT, 10) |
                         place(c, CJ_IMM_6_BIT, CJ_IMM_6_BIT, 6) |
                         place(c, CJ_IMM_7_BIT, CJ_IMM_7_BIT, 7) |
                         place(c, CJ_IMM_3_1_HI, CJ_IMM_3_1_LO, 1) |
                         place(c, CJ_IMM_5_BIT, CJ_IMM_5_BIT, 5),
                     CJ_IMM_WIDTH);
}

static int32_t cb_imm(uint16_t c) {
  return sign_extend(place(c, CB_IMM_8_BIT, CB_IMM_8_BIT, 8) |
                         place(c, CB_IMM_4_3_HI, CB_IMM_4_3_LO, 3) |
                         place(c, CB_IMM_7_6_HI, CB_IMM_7_6_LO, 6) |
                         place(c, CB_IMM_2_1_HI, CB_IMM_2_1_LO, 1) |
                         place(c, CB_IMM_5_BIT, CB_IMM_5_BIT, 5),
                     CB_IMM_WIDTH);
}

static unsigned lwsp_imm(uint16_t c) {
  return place(c, LWSP_IMM_5_BIT, LWSP_IMM_5_BIT, 5) |
         place(c, LWSP_IMM_4_2_HI, LWSP_IMM_4_2_LO, 2) |
         place(c, LWSP_IMM_7_6_HI, LWSP_IMM_7_6_LO, 6);
}

static unsigned ldsp_imm(uint16_t c) {
  return place(c, LDSP_IMM_5_BIT, LDSP_IMM_5_BIT, 5) |
         place(c, LDSP_IMM_4_3_HI, LDSP_IMM_4_3_LO, 3) |
         place(c, LDSP_IMM_8_6_HI, LDSP_IMM_8_6_LO, 6);
}

static unsigned swsp_imm(uint16_t c) {
  return place(c, SWSP_IMM_5_2_HI, SWSP_IMM_5_2_LO, 2) |
         place(c, SWSP_IMM_7_6_HI, SWSP_IMM_7_6_LO, 6);
}

static unsigned sdsp_imm(uint16_t c) {
  return place(c, SDSP_IMM_5_3_HI, SDSP_IMM_5_3_LO, 3) |
         place(c, SDSP_IMM_8_6_HI, SDSP_IMM_8_6_LO, 6);
}

static uint32_t expand_quadrant_0(uint16_t c) {
  unsigned low = prime(c, C_PRIME_LOW_HI, C_PRIME_LOW_LO);
  unsigned high = prime(c, C_PRIME_HIGH_HI, C_PRIME_HIGH_LO);

  switch (bits(c, C_FUNCT3_HI, C_FUNCT3_LO)) {
  case C_ADDI4SPN:
    if (addi4spn_imm(c) == 0)
      return C_ILLEGAL;
    return encode_i(addi4spn_imm(c), REG_SP, FUNCT3_ADD_SUB, low,
                    OPCODE_OP_IMM);
  case C_LW:
    return encode_i(cl_word_imm(c), high, FUNCT3_LW, low, OPCODE_LOAD);
  case C_SW:
    return encode_s(cl_word_imm(c), low, high, FUNCT3_SW, OPCODE_STORE);
  case C_LD:
    if (XLEN == 32)
      return C_ILLEGAL;
    return encode_i(cl_double_imm(c), high, FUNCT3_LD, low, OPCODE_LOAD);
  case C_SD:
    if (XLEN == 32)
      return C_ILLEGAL;
    return encode_s(cl_double_imm(c), low, high, FUNCT3_SD, OPCODE_STORE);
  }
  return C_ILLEGAL;
}

static uint32_t expand_misc_alu(uint16_t c) {
  unsigned rd = prime(c, C_PRIME_HIGH_HI, C_PRIME_HIGH_LO);
  unsigned rs2 = prime(c, C_PRIME_LOW_HI, C_PRIME_LOW_LO);
  unsigned shamt = ci_uimm(c);

  switch (bits(c, C_FUNCT2_HI, C_FUNCT2_LO)) {
  case C_SRLI:
    if (shamt >= XLEN)
      return C_ILLEGAL;
    return encode_i(shamt, rd, FUNCT3_SRL_SRA, rd, OPCODE_OP_IMM);
  case C_SRAI:
    if (shamt >= XLEN)
      return C_ILLEGAL;
    return encode_i(shamt | 1u << SHIFT_IMM_ARITHMETIC_BIT, rd, FUNCT3_SRL_SRA,
                    rd, OPCODE_OP_IMM);
  case C_ANDI:
    return encode_i(ci_imm(c), rd, FUNCT3_AND, rd, OPCODE_OP_IMM);
  }

  unsigned arith = bits(c, C_ARITH_HI, C_ARITH_LO);
  if (bits(c, C_WORD_BIT, C_WORD_BIT)) {
    if (XLEN == 32)
      return C_ILLEGAL;
    switch (arith) {
    case C_SUBW:
      return encode_r(FUNCT7_SUB, rs2, rd, FUNCT3_ADD_SUB, rd, OPCODE_OP_32);
    case C_ADDW:
      return encode_r(FUNCT7_ADD, rs2, rd, FUNCT3_ADD_SUB, rd, OPCODE_OP_32);
    }
    return C_ILLEGAL;
  }
  switch (arith) {
  case C_SUB:
    return encode_r(FUNCT7_SUB, rs2, rd, FUNCT3_ADD_SUB, rd, OPCODE_OP);
  case C_XOR:
    return encode_r(FUNCT7_ADD, rs2, rd, FUNCT3_XOR, rd, OPCODE_OP);
  case C_OR:
    return encode_r(FUNCT7_ADD, rs2, rd, FUNCT3_OR, rd, OPCODE_OP);
  case C_AND:
    return encode_r(FUNCT7_ADD, rs2, rd, FUNCT3_AND, rd, OPCODE_OP);
  }
  return C_ILLEGAL;
}

static uint32_t expand_quadrant_1(uint16_t c) {
  unsigned rd = bits(c, C_RD_HI, C_RD_LO);
  unsigned rs1 = prime(c, C_PRIME_HIGH_HI, C_PRIME_HIGH_LO);

  switch (bits(c, C_FUNCT3_HI, C_FUNCT3_LO)) {
  case C_ADDI:
    return encode_i(ci_imm(c), rd, FUNCT3_ADD_SUB, rd, OPCODE_OP_IMM);
  case C_ADDIW_JAL:
    if (XLEN == 32)
      return encode_j(cj_imm(c), REG_RA);
    if (rd == REG_ZERO)
      return C_ILLEGAL;
    return encode_i(ci_imm(c), rd, FUNCT3_ADD_SUB, rd, OPCODE_OP_IMM_32);
  case C_LI:
    return encode_i(ci_imm(c), REG_ZERO, FUNCT3_ADD_SUB, rd, OPCODE_OP_IMM);
  case C_LUI_ADDI16SP:
    if (rd == REG_SP) {
      if (addi16sp_imm(c) == 0)
        return C_ILLEGAL;
      return encode_i(addi16sp_imm(c), REG_SP, FUNCT3_ADD_SUB, REG_SP,
                      OPCODE_OP_IMM);
    }
    if (ci_imm(c) == 0)
      return C_ILLEGAL;
    return encode_u(ci_imm(c), rd, OPCODE_LUI);
  case C_MISC_ALU:
    return expand_misc_alu(c);
  case C_J:
    return encode_j(cj_imm(c), REG_ZERO);
  case C_BEQZ:
    return encode_b(cb_imm(c), REG_ZERO, rs1, FUNCT3_BEQ);
  case C_BNEZ:
    return encode_b(cb_imm(c), REG_ZERO, rs1, FUNCT3_BNE);
  }
  return C_ILLEGAL;
}

static uint32_t expand_jr_mv_add(uint16_t c) {
  unsigned rd = bits(c, C_RD_HI, C_RD_LO);
  unsigned rs2 = bits(c, C_RS2_HI, C_RS2_LO);
  bool link = bits(c, C_FUNCT4_HI, C_FUNCT4_LO) == C_JALR_ADD;

  if (rs2 != REG_ZERO)
    return encode_r(FUNCT7_ADD, rs2, link ? rd : REG_ZERO, FUNCT3_ADD_SUB, rd,
                    OPCODE_OP);
  if (rd == REG_ZERO)
    return link ? encode_i(EBREAK_IMM, REG_ZERO, FUNCT3_PRIV, REG_ZERO,
                           OPCODE_SYSTEM)
                : C_ILLEGAL;
  return encode_i(0, rd, FUNCT3_JALR, link ? REG_RA : REG_ZERO, OPCODE_JALR);
}

static uint32_t expand_quadrant_2(uint16_t c) {
  unsigned rd = bits(c, C_RD_HI, C_RD_LO);
  unsigned rs2 = bits(c, C_RS2_HI, C_RS2_LO);

  switch (bits(c, C_FUNCT3_HI, C_FUNCT3_LO)) {
  case C_SLLI:
    if (ci_uimm(c) >= XLEN)
      return C_ILLEGAL;
    return encode_i(ci_uimm(c), rd, FUNCT3_SLL, rd, OPCODE_OP_IMM);
  case C_LWSP:
    if (rd == REG_ZERO)
      return C_ILLEGAL;
    return encode_i(lwsp_imm(c), REG_SP, FUNCT3_LW, rd, OPCODE_LOAD);
  case C_LDSP:
    if (XLEN == 32 || rd == REG_ZERO)
      return C_ILLEGAL;
    return encode_i(ldsp_imm(c), REG_SP, FUNCT3_LD, rd, OPCODE_LOAD);
  case C_JR_MV_ADD:
    return expand_jr_mv_add(c);
  case C_SWSP:
    return encode_s(swsp_imm(c), rs2, REG_SP, FUNCT3_SW, OPCODE_STORE);
  case C_SDSP:
    if (XLEN == 32)
      return C_ILLEGAL;
    return encode_s(sdsp_imm(c), rs2, REG_SP, FUNCT3_SD, OPCODE_STORE);
  }
  return C_ILLEGAL;
}

uint32_t instruction_expand(uint16_t compressed) {
  switch (bits(compressed, C_OP_HI, C_OP_LO)) {
  case C_QUADRANT_0:
    return expand_quadrant_0(compressed);
  case C_QUADRANT_1:
    return expand_quadrant_1(compressed);
  case C_QUADRANT_2:
    return expand_quadrant_2(compressed);
  }
  return C_ILLEGAL;
}
