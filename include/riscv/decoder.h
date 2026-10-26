#ifndef RISCV_DECODER_H
#define RISCV_DECODER_H

#include "riscv/xlen.h"
#include <stdint.h>

// Bit positions of the instruction fields, as laid out in the RISC-V
// unprivileged spec ("Base Instruction Formats"): FIELD_HI..FIELD_LO.
#define OPCODE_HI 6
#define OPCODE_LO 0
#define RD_HI 11
#define RD_LO 7
#define FUNCT3_HI 14
#define FUNCT3_LO 12
#define RS1_HI 19
#define RS1_LO 15
#define RS2_HI 24
#define RS2_LO 20
#define FUNCT7_HI 31
#define FUNCT7_LO 25

// Immediates are split into pieces. A piece named <FMT>_IMM_<a>_<b> holds
// imm[a:b] and sits at instruction bits <..._HI>..<..._LO>; <FMT>_IMM_WIDTH is
// the width the immediate is sign-extended from.
#define I_IMM_HI 31
#define I_IMM_LO 20
#define I_IMM_WIDTH 12

#define S_IMM_11_5_HI 31
#define S_IMM_11_5_LO 25
#define S_IMM_4_0_HI 11
#define S_IMM_4_0_LO 7
#define S_IMM_WIDTH 12

#define B_IMM_12_BIT 31
#define B_IMM_10_5_HI 30
#define B_IMM_10_5_LO 25
#define B_IMM_4_1_HI 11
#define B_IMM_4_1_LO 8
#define B_IMM_11_BIT 7
#define B_IMM_WIDTH 13

#define U_IMM_HI 31
#define U_IMM_LO 12

#define J_IMM_20_BIT 31
#define J_IMM_10_1_HI 30
#define J_IMM_10_1_LO 21
#define J_IMM_11_BIT 20
#define J_IMM_19_12_HI 19
#define J_IMM_19_12_LO 12
#define J_IMM_WIDTH 21

// Shift-immediate instructions: imm[10] (instruction bit 30) selects srai over
// srli; the bits above the shift amount must otherwise be zero.
#define SHIFT_IMM_ARITHMETIC_BIT 10

#define INSTRUCTION_WIDTH_BYTES 4
#define INSTRUCTION_WIDTH_BITS (INSTRUCTION_WIDTH_BYTES * 8)

struct Instruction {
  uint32_t raw;
  uint8_t opcode;
  uint8_t rd;
  uint8_t funct3;
  uint8_t rs1;
  uint8_t rs2;
  uint8_t funct7;
  int32_t imm;
};

enum Opcode {
  OPCODE_LOAD = 0x03,
  OPCODE_OP_IMM = 0x13,
  OPCODE_STORE = 0x23,
  OPCODE_OP = 0x33,
  OPCODE_LUI = 0x37,
  OPCODE_BRANCH = 0x63,
  OPCODE_JAL = 0x6F,
  OPCODE_JALR = 0x67,
  OPCODE_AUIPC = 0x17,
  OPCODE_OP_IMM_32 = 0x1B,
  OPCODE_OP_32 = 0x3B,
  OPCODE_SYSTEM = 0x73,
  OPCODE_FENCE = 0x0F,
  OPCODE_AMO = 0x2F
};

enum Funct3_OP {
  FUNCT3_ADD_SUB = 0x0,
  FUNCT3_SLL = 0x1,
  FUNCT3_SLT = 0x2,
  FUNCT3_SLTU = 0x3,
  FUNCT3_XOR = 0x4,
  FUNCT3_SRL_SRA = 0x5,
  FUNCT3_OR = 0x6,
  FUNCT3_AND = 0x7,
};

enum Funct3_M {
  FUNCT3_MUL = 0x0,
  FUNCT3_MULH = 0x1,
  FUNCT3_MULHSU = 0x2,
  FUNCT3_MULHU = 0x3,
  FUNCT3_DIV = 0x4,
  FUNCT3_DIVU = 0x5,
  FUNCT3_REM = 0x6,
  FUNCT3_REMU = 0x7
};

enum Funct3_Branch {
  FUNCT3_BEQ = 0x0,
  FUNCT3_BNE = 0x1,
  FUNCT3_BLT = 0x4,
  FUNCT3_BGE = 0x5,
  FUNCT3_BLTU = 0x6,
  FUNCT3_BGEU = 0x7
};

enum Funct3_LOAD {
  FUNCT3_LB = 0x0,
  FUNCT3_LH = 0x1,
  FUNCT3_LD = 0x3,
  FUNCT3_LW = 0x2,
  FUNCT3_LBU = 0x4,
  FUNCT3_LHU = 0x5,
  FUNCT3_LWU = 0x6
};

enum Funct3_STORE {
  FUNCT3_SB = 0x0,
  FUNCT3_SH = 0x1,
  FUNCT3_SW = 0x2,
  FUNCT3_SD = 0x3
};

enum Funct3_SYSTEM {
  FUNCT3_CSRRW = 0x1,
  FUNCT3_CSRRS = 0x2,
  FUNCT3_CSRRC = 0x3,
  FUNCT3_CSRRWI = 0x5,
  FUNCT3_CSRRSI = 0x6,
  FUNCT3_CSRRCI = 0x7
};

enum Funct3_FENCE { FUNCT3_FENCE = 0x0, FUNCT3_FENCE_I = 0x1 };

enum Funct3_JALR { FUNCT3_JALR = 0x0 };

enum Funct3_AMO { FUNCT3_AMO_W = 2, FUNCT3_AMO_D = 3 };
enum AMO_Size { AMO_SIZE_W = 4, AMO_SIZE_D = 8 };

enum Funct5_AMO {
  FUNCT5_AMO_ADD = 0x00,
  FUNCT5_AMO_SWAP = 0x01,
  FUNCT5_AMO_LR = 0x02,
  FUNCT5_AMO_SC = 0x03,
  FUNCT5_AMO_XOR = 0x04,
  FUNCT5_AMO_OR = 0x08,
  FUNCT5_AMO_AND = 0x0C,
  FUNCT5_AMO_MIN = 0x10,
  FUNCT5_AMO_MAX = 0x14,
  FUNCT5_AMO_MINU = 0x18,
  FUNCT5_AMO_MAXU = 0x1C
};

enum Funct7 {
  FUNCT7_ADD = 0x00,
  FUNCT7_MUL = 0x01,
  FUNCT7_SUB = 0x20,
};

struct Instruction instruction_decode(uint32_t raw);

#endif // RISCV_DECODER_H
