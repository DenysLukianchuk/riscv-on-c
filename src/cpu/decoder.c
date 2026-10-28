#include "riscv/decoder.h"
#include "riscv/bits.h"

static inline void extract_rd(struct Instruction *inst, uint32_t raw) {
  switch (inst->opcode) {
  case OPCODE_STORE:
  case OPCODE_BRANCH:
    inst->rd = 0;
    break;
  default:
    inst->rd = bits(raw, RD_HI, RD_LO);
    break;
  }
}

static inline void extract_funct3(struct Instruction *inst, uint32_t raw) {
  switch (inst->opcode) {
  case OPCODE_LUI:
  case OPCODE_AUIPC:
  case OPCODE_JAL:
    inst->funct3 = 0;
    break;
  default:
    inst->funct3 = bits(raw, FUNCT3_HI, FUNCT3_LO);
    break;
  }
}

static inline void extract_rs1(struct Instruction *inst, uint32_t raw) {
  switch (inst->opcode) {
  case OPCODE_LUI:
  case OPCODE_AUIPC:
  case OPCODE_JAL:
    inst->rs1 = 0;
    break;
  default:
    inst->rs1 = bits(raw, RS1_HI, RS1_LO);
    break;
  }
}

static inline void extract_rs2(struct Instruction *inst, uint32_t raw) {
  switch (inst->opcode) {
  case OPCODE_LUI:
  case OPCODE_AUIPC:
  case OPCODE_JAL:
    inst->rs2 = 0;
    break;
  default:
    inst->rs2 = bits(raw, RS2_HI, RS2_LO);
    break;
  }
}

static inline void extract_funct7(struct Instruction *inst, uint32_t raw) {
  switch (inst->opcode) {
  case OPCODE_LUI:
  case OPCODE_AUIPC:
  case OPCODE_JAL:
    inst->funct7 = 0;
    break;
  default:
    inst->funct7 = bits(raw, FUNCT7_HI, FUNCT7_LO);
    break;
  }
}

static inline void extract_imm(struct Instruction *inst, uint32_t raw) {

  switch (inst->opcode) {
  case OPCODE_STORE:
    inst->imm = sign_extend((bits(raw, S_IMM_11_5_HI, S_IMM_11_5_LO) << 5) |
                                bits(raw, S_IMM_4_0_HI, S_IMM_4_0_LO),
                            S_IMM_WIDTH);
    break;
  case OPCODE_BRANCH:
    inst->imm = sign_extend((bits(raw, B_IMM_12_BIT, B_IMM_12_BIT) << 12) |
                                (bits(raw, B_IMM_10_5_HI, B_IMM_10_5_LO) << 5) |
                                (bits(raw, B_IMM_4_1_HI, B_IMM_4_1_LO) << 1) |
                                (bits(raw, B_IMM_11_BIT, B_IMM_11_BIT) << 11),
                            B_IMM_WIDTH);
    break;
  case OPCODE_LUI:
  case OPCODE_AUIPC:
    inst->imm = bits(raw, U_IMM_HI, U_IMM_LO) << U_IMM_LO;
    break;
  case OPCODE_JAL:
    inst->imm =
        sign_extend((bits(raw, J_IMM_20_BIT, J_IMM_20_BIT) << 20) |
                        (bits(raw, J_IMM_10_1_HI, J_IMM_10_1_LO) << 1) |
                        (bits(raw, J_IMM_11_BIT, J_IMM_11_BIT) << 11) |
                        (bits(raw, J_IMM_19_12_HI, J_IMM_19_12_LO) << 12),
                    J_IMM_WIDTH);
    break;
  default:
    inst->imm = sign_extend(bits(raw, I_IMM_HI, I_IMM_LO), I_IMM_WIDTH);
    break;
  }
}

struct Instruction instruction_decode(uint32_t raw) {
  struct Instruction inst;
  inst.raw = raw;
  inst.opcode = bits(raw, OPCODE_HI, OPCODE_LO);
  extract_rd(&inst, raw);
  extract_funct3(&inst, raw);
  extract_rs1(&inst, raw);
  extract_rs2(&inst, raw);
  extract_funct7(&inst, raw);
  extract_imm(&inst, raw);
  return inst;
}
