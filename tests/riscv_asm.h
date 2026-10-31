#ifndef TESTS_RISCV_ASM_H
#define TESTS_RISCV_ASM_H

// Instruction encoders for tests. Operands follow assembler order, registers
// are plain numbers: LW(1, -4, 2) is `lw x1, -4(x2)`, CSRR(1, MEPC) is
// `csrr x1, mepc`. Out-of-range fields are not checked, so the same macros
// also build the deliberately invalid encodings the tests need.

#include <stdint.h>

#define ASM_LOAD 0x03u
#define ASM_LOAD_FP 0x07u
#define ASM_MISC_MEM 0x0Fu
#define ASM_OP_IMM 0x13u
#define ASM_AUIPC 0x17u
#define ASM_OP_IMM_32 0x1Bu
#define ASM_STORE 0x23u
#define ASM_STORE_FP 0x27u
#define ASM_AMO 0x2Fu
#define ASM_OP 0x33u
#define ASM_LUI 0x37u
#define ASM_OP_32 0x3Bu
#define ASM_OP_FP 0x53u
#define ASM_MADD 0x43u
#define ASM_MSUB 0x47u
#define ASM_NMSUB 0x4Bu
#define ASM_NMADD 0x4Fu
#define ASM_BRANCH 0x63u
#define ASM_JALR 0x67u
#define ASM_JAL 0x6Fu
#define ASM_SYSTEM 0x73u

#define ASM_BITS(value, hi, lo)                                                \
  ((((uint32_t)(value)) >> (lo)) & ((1u << ((hi) - (lo) + 1)) - 1))

// Base formats.
#define RV_R(funct7, rs2, rs1, funct3, rd, opcode)                             \
  (((uint32_t)(funct7) << 25) | ((uint32_t)(rs2) << 20) |                      \
   ((uint32_t)(rs1) << 15) | ((uint32_t)(funct3) << 12) |                      \
   ((uint32_t)(rd) << 7) | (uint32_t)(opcode))
#define RV_I(imm, rs1, funct3, rd, opcode)                                     \
  ((ASM_BITS(imm, 11, 0) << 20) | ((uint32_t)(rs1) << 15) |                    \
   ((uint32_t)(funct3) << 12) | ((uint32_t)(rd) << 7) | (uint32_t)(opcode))
#define RV_S(imm, rs2, rs1, funct3, opcode)                                    \
  ((ASM_BITS(imm, 11, 5) << 25) | ((uint32_t)(rs2) << 20) |                    \
   ((uint32_t)(rs1) << 15) | ((uint32_t)(funct3) << 12) |                      \
   (ASM_BITS(imm, 4, 0) << 7) | (uint32_t)(opcode))
#define RV_B(imm, rs2, rs1, funct3, opcode)                                    \
  ((ASM_BITS(imm, 12, 12) << 31) | (ASM_BITS(imm, 10, 5) << 25) |              \
   ((uint32_t)(rs2) << 20) | ((uint32_t)(rs1) << 15) |                         \
   ((uint32_t)(funct3) << 12) | (ASM_BITS(imm, 4, 1) << 8) |                   \
   (ASM_BITS(imm, 11, 11) << 7) | (uint32_t)(opcode))
#define RV_U(imm20, rd, opcode)                                                \
  ((ASM_BITS(imm20, 19, 0) << 12) | ((uint32_t)(rd) << 7) | (uint32_t)(opcode))
#define RV_J(imm, rd, opcode)                                                  \
  ((ASM_BITS(imm, 20, 20) << 31) | (ASM_BITS(imm, 10, 1) << 21) |              \
   (ASM_BITS(imm, 11, 11) << 20) | (ASM_BITS(imm, 19, 12) << 12) |             \
   ((uint32_t)(rd) << 7) | (uint32_t)(opcode))

// RV32I / RV64I.
#define LUI(rd, imm20) RV_U(imm20, rd, ASM_LUI)
#define AUIPC(rd, imm20) RV_U(imm20, rd, ASM_AUIPC)
#define JAL(rd, imm) RV_J(imm, rd, ASM_JAL)
#define JALR(rd, imm, rs1) RV_I(imm, rs1, 0, rd, ASM_JALR)

#define BEQ(rs1, rs2, imm) RV_B(imm, rs2, rs1, 0, ASM_BRANCH)
#define BNE(rs1, rs2, imm) RV_B(imm, rs2, rs1, 1, ASM_BRANCH)
#define BLT(rs1, rs2, imm) RV_B(imm, rs2, rs1, 4, ASM_BRANCH)
#define BGE(rs1, rs2, imm) RV_B(imm, rs2, rs1, 5, ASM_BRANCH)
#define BLTU(rs1, rs2, imm) RV_B(imm, rs2, rs1, 6, ASM_BRANCH)
#define BGEU(rs1, rs2, imm) RV_B(imm, rs2, rs1, 7, ASM_BRANCH)

#define LB(rd, imm, rs1) RV_I(imm, rs1, 0, rd, ASM_LOAD)
#define LH(rd, imm, rs1) RV_I(imm, rs1, 1, rd, ASM_LOAD)
#define LW(rd, imm, rs1) RV_I(imm, rs1, 2, rd, ASM_LOAD)
#define LD(rd, imm, rs1) RV_I(imm, rs1, 3, rd, ASM_LOAD)
#define LBU(rd, imm, rs1) RV_I(imm, rs1, 4, rd, ASM_LOAD)
#define LHU(rd, imm, rs1) RV_I(imm, rs1, 5, rd, ASM_LOAD)
#define LWU(rd, imm, rs1) RV_I(imm, rs1, 6, rd, ASM_LOAD)
#define SB(rs2, imm, rs1) RV_S(imm, rs2, rs1, 0, ASM_STORE)
#define SH(rs2, imm, rs1) RV_S(imm, rs2, rs1, 1, ASM_STORE)
#define SW(rs2, imm, rs1) RV_S(imm, rs2, rs1, 2, ASM_STORE)
#define SD(rs2, imm, rs1) RV_S(imm, rs2, rs1, 3, ASM_STORE)

#define ADDI(rd, rs1, imm) RV_I(imm, rs1, 0, rd, ASM_OP_IMM)
#define SLTI(rd, rs1, imm) RV_I(imm, rs1, 2, rd, ASM_OP_IMM)
#define SLTIU(rd, rs1, imm) RV_I(imm, rs1, 3, rd, ASM_OP_IMM)
#define XORI(rd, rs1, imm) RV_I(imm, rs1, 4, rd, ASM_OP_IMM)
#define ORI(rd, rs1, imm) RV_I(imm, rs1, 6, rd, ASM_OP_IMM)
#define ANDI(rd, rs1, imm) RV_I(imm, rs1, 7, rd, ASM_OP_IMM)
#define SLLI(rd, rs1, shamt) RV_I(shamt, rs1, 1, rd, ASM_OP_IMM)
#define SRLI(rd, rs1, shamt) RV_I(shamt, rs1, 5, rd, ASM_OP_IMM)
#define SRAI(rd, rs1, shamt) RV_I(0x400 | (shamt), rs1, 5, rd, ASM_OP_IMM)

#define ADD(rd, rs1, rs2) RV_R(0x00, rs2, rs1, 0, rd, ASM_OP)
#define SUB(rd, rs1, rs2) RV_R(0x20, rs2, rs1, 0, rd, ASM_OP)
#define SLL(rd, rs1, rs2) RV_R(0x00, rs2, rs1, 1, rd, ASM_OP)
#define SLT(rd, rs1, rs2) RV_R(0x00, rs2, rs1, 2, rd, ASM_OP)
#define SLTU(rd, rs1, rs2) RV_R(0x00, rs2, rs1, 3, rd, ASM_OP)
#define XOR(rd, rs1, rs2) RV_R(0x00, rs2, rs1, 4, rd, ASM_OP)
#define SRL(rd, rs1, rs2) RV_R(0x00, rs2, rs1, 5, rd, ASM_OP)
#define SRA(rd, rs1, rs2) RV_R(0x20, rs2, rs1, 5, rd, ASM_OP)
#define OR(rd, rs1, rs2) RV_R(0x00, rs2, rs1, 6, rd, ASM_OP)
#define AND(rd, rs1, rs2) RV_R(0x00, rs2, rs1, 7, rd, ASM_OP)

#define ADDIW(rd, rs1, imm) RV_I(imm, rs1, 0, rd, ASM_OP_IMM_32)
#define SLLIW(rd, rs1, shamt) RV_I(shamt, rs1, 1, rd, ASM_OP_IMM_32)
#define SRLIW(rd, rs1, shamt) RV_I(shamt, rs1, 5, rd, ASM_OP_IMM_32)
#define SRAIW(rd, rs1, shamt) RV_I(0x400 | (shamt), rs1, 5, rd, ASM_OP_IMM_32)
#define ADDW(rd, rs1, rs2) RV_R(0x00, rs2, rs1, 0, rd, ASM_OP_32)
#define SUBW(rd, rs1, rs2) RV_R(0x20, rs2, rs1, 0, rd, ASM_OP_32)
#define SLLW(rd, rs1, rs2) RV_R(0x00, rs2, rs1, 1, rd, ASM_OP_32)
#define SRLW(rd, rs1, rs2) RV_R(0x00, rs2, rs1, 5, rd, ASM_OP_32)
#define SRAW(rd, rs1, rs2) RV_R(0x20, rs2, rs1, 5, rd, ASM_OP_32)

// F and D loads and stores: rd and rs2 name f registers, rs1 an x register.
#define FLW(rd, imm, rs1) RV_I(imm, rs1, 2, rd, ASM_LOAD_FP)
#define FLD(rd, imm, rs1) RV_I(imm, rs1, 3, rd, ASM_LOAD_FP)
#define FSW(rs2, imm, rs1) RV_S(imm, rs2, rs1, 2, ASM_STORE_FP)
#define FSD(rs2, imm, rs1) RV_S(imm, rs2, rs1, 3, ASM_STORE_FP)

// OP-FP: funct7 is funct5 followed by the format, 0 for single and 1 for
// double. rd and rs1 name f or x registers depending on the instruction.
#define FSGNJ_S(rd, rs1, rs2) RV_R(0x10, rs2, rs1, 0, rd, ASM_OP_FP)
#define FSGNJN_S(rd, rs1, rs2) RV_R(0x10, rs2, rs1, 1, rd, ASM_OP_FP)
#define FSGNJX_S(rd, rs1, rs2) RV_R(0x10, rs2, rs1, 2, rd, ASM_OP_FP)
#define FSGNJ_D(rd, rs1, rs2) RV_R(0x11, rs2, rs1, 0, rd, ASM_OP_FP)
#define FSGNJN_D(rd, rs1, rs2) RV_R(0x11, rs2, rs1, 1, rd, ASM_OP_FP)
#define FSGNJX_D(rd, rs1, rs2) RV_R(0x11, rs2, rs1, 2, rd, ASM_OP_FP)
#define FMV_X_W(rd, rs1) RV_R(0x70, 0, rs1, 0, rd, ASM_OP_FP)
#define FCLASS_S(rd, rs1) RV_R(0x70, 0, rs1, 1, rd, ASM_OP_FP)
#define FMV_W_X(rd, rs1) RV_R(0x78, 0, rs1, 0, rd, ASM_OP_FP)
#define FMV_X_D(rd, rs1) RV_R(0x71, 0, rs1, 0, rd, ASM_OP_FP)
#define FCLASS_D(rd, rs1) RV_R(0x71, 0, rs1, 1, rd, ASM_OP_FP)
#define FMV_D_X(rd, rs1) RV_R(0x79, 0, rs1, 0, rd, ASM_OP_FP)
// Arithmetic: funct3 is the rounding mode rm.
#define FADD_S(rd, rs1, rs2, rm) RV_R(0x00, rs2, rs1, rm, rd, ASM_OP_FP)
#define FSUB_S(rd, rs1, rs2, rm) RV_R(0x04, rs2, rs1, rm, rd, ASM_OP_FP)
#define FADD_D(rd, rs1, rs2, rm) RV_R(0x01, rs2, rs1, rm, rd, ASM_OP_FP)
#define FSUB_D(rd, rs1, rs2, rm) RV_R(0x05, rs2, rs1, rm, rd, ASM_OP_FP)
#define FMUL_S(rd, rs1, rs2, rm) RV_R(0x08, rs2, rs1, rm, rd, ASM_OP_FP)
#define FMUL_D(rd, rs1, rs2, rm) RV_R(0x09, rs2, rs1, rm, rd, ASM_OP_FP)
#define FDIV_S(rd, rs1, rs2, rm) RV_R(0x0C, rs2, rs1, rm, rd, ASM_OP_FP)
#define FDIV_D(rd, rs1, rs2, rm) RV_R(0x0D, rs2, rs1, rm, rd, ASM_OP_FP)
#define FSQRT_S(rd, rs1, rm) RV_R(0x2C, 0, rs1, rm, rd, ASM_OP_FP)
#define FSQRT_D(rd, rs1, rm) RV_R(0x2D, 0, rs1, rm, rd, ASM_OP_FP)
#define FMIN_S(rd, rs1, rs2) RV_R(0x14, rs2, rs1, 0, rd, ASM_OP_FP)
#define FMAX_S(rd, rs1, rs2) RV_R(0x14, rs2, rs1, 1, rd, ASM_OP_FP)
#define FMIN_D(rd, rs1, rs2) RV_R(0x15, rs2, rs1, 0, rd, ASM_OP_FP)
#define FMAX_D(rd, rs1, rs2) RV_R(0x15, rs2, rs1, 1, rd, ASM_OP_FP)
// Comparisons write 0 or 1 to the x register rd.
#define FLE_S(rd, rs1, rs2) RV_R(0x50, rs2, rs1, 0, rd, ASM_OP_FP)
#define FLT_S(rd, rs1, rs2) RV_R(0x50, rs2, rs1, 1, rd, ASM_OP_FP)
#define FEQ_S(rd, rs1, rs2) RV_R(0x50, rs2, rs1, 2, rd, ASM_OP_FP)
#define FLE_D(rd, rs1, rs2) RV_R(0x51, rs2, rs1, 0, rd, ASM_OP_FP)
#define FLT_D(rd, rs1, rs2) RV_R(0x51, rs2, rs1, 1, rd, ASM_OP_FP)
#define FEQ_D(rd, rs1, rs2) RV_R(0x51, rs2, rs1, 2, rd, ASM_OP_FP)
// Conversions. To an integer: rd is an x register and rs2 picks the type,
// 0 int32, 1 uint32, 2 int64, 3 uint64. From an integer: rs1 is an x register.
#define FCVT_W_S(rd, rs1, rm) RV_R(0x60, 0, rs1, rm, rd, ASM_OP_FP)
#define FCVT_WU_S(rd, rs1, rm) RV_R(0x60, 1, rs1, rm, rd, ASM_OP_FP)
#define FCVT_L_S(rd, rs1, rm) RV_R(0x60, 2, rs1, rm, rd, ASM_OP_FP)
#define FCVT_LU_S(rd, rs1, rm) RV_R(0x60, 3, rs1, rm, rd, ASM_OP_FP)
#define FCVT_W_D(rd, rs1, rm) RV_R(0x61, 0, rs1, rm, rd, ASM_OP_FP)
#define FCVT_WU_D(rd, rs1, rm) RV_R(0x61, 1, rs1, rm, rd, ASM_OP_FP)
#define FCVT_L_D(rd, rs1, rm) RV_R(0x61, 2, rs1, rm, rd, ASM_OP_FP)
#define FCVT_LU_D(rd, rs1, rm) RV_R(0x61, 3, rs1, rm, rd, ASM_OP_FP)
#define FCVT_S_W(rd, rs1, rm) RV_R(0x68, 0, rs1, rm, rd, ASM_OP_FP)
#define FCVT_S_WU(rd, rs1, rm) RV_R(0x68, 1, rs1, rm, rd, ASM_OP_FP)
#define FCVT_S_L(rd, rs1, rm) RV_R(0x68, 2, rs1, rm, rd, ASM_OP_FP)
#define FCVT_S_LU(rd, rs1, rm) RV_R(0x68, 3, rs1, rm, rd, ASM_OP_FP)
#define FCVT_D_W(rd, rs1, rm) RV_R(0x69, 0, rs1, rm, rd, ASM_OP_FP)
#define FCVT_D_WU(rd, rs1, rm) RV_R(0x69, 1, rs1, rm, rd, ASM_OP_FP)
#define FCVT_D_L(rd, rs1, rm) RV_R(0x69, 2, rs1, rm, rd, ASM_OP_FP)
#define FCVT_D_LU(rd, rs1, rm) RV_R(0x69, 3, rs1, rm, rd, ASM_OP_FP)
#define FCVT_S_D(rd, rs1, rm) RV_R(0x20, 1, rs1, rm, rd, ASM_OP_FP)
#define FCVT_D_S(rd, rs1, rm) RV_R(0x21, 0, rs1, rm, rd, ASM_OP_FP)
// Fused multiply-add, R4 format: rs3 and fmt (0 single, 1 double) fill the
// funct7 field.
#define RV_R4(rs3, fmt, rs2, rs1, rm, rd, opcode)                              \
  RV_R((uint32_t)(rs3) << 2 | (fmt), rs2, rs1, rm, rd, opcode)
#define FMADD_S(rd, rs1, rs2, rs3, rm) RV_R4(rs3, 0, rs2, rs1, rm, rd, ASM_MADD)
#define FMSUB_S(rd, rs1, rs2, rs3, rm) RV_R4(rs3, 0, rs2, rs1, rm, rd, ASM_MSUB)
#define FNMSUB_S(rd, rs1, rs2, rs3, rm)                                        \
  RV_R4(rs3, 0, rs2, rs1, rm, rd, ASM_NMSUB)
#define FNMADD_S(rd, rs1, rs2, rs3, rm)                                        \
  RV_R4(rs3, 0, rs2, rs1, rm, rd, ASM_NMADD)
#define FMADD_D(rd, rs1, rs2, rs3, rm) RV_R4(rs3, 1, rs2, rs1, rm, rd, ASM_MADD)

#define NOP ADDI(0, 0, 0)
#define LI(rd, imm) ADDI(rd, 0, imm)

// Fence predecessor / successor sets.
#define FENCE_R 0x2
#define FENCE_W 0x1
#define FENCE(pred, succ) RV_I(((pred) << 4) | (succ), 0, 0, 0, ASM_MISC_MEM)
#define FENCE_I RV_I(0, 0, 1, 0, ASM_MISC_MEM)

#define ECALL 0x00000073u
#define EBREAK 0x00100073u
#define SRET 0x10200073u
#define WFI 0x10500073u
// sfence.vma rs1, rs2: funct7 0001001, funct3 000, rd 0, opcode SYSTEM.
#define SFENCE_VMA(rs1, rs2)                                                   \
  (0x12000073u | (uint32_t)(rs2) << 20 | (uint32_t)(rs1) << 15)
#define MRET 0x30200073u

// The all-zeros word is defined to be an illegal instruction.
#define ILLEGAL_INSTRUCTION 0x00000000u

// Zicsr.
#define CSRRW(rd, csr, rs1) RV_I(csr, rs1, 1, rd, ASM_SYSTEM)
#define CSRRS(rd, csr, rs1) RV_I(csr, rs1, 2, rd, ASM_SYSTEM)
#define CSRRC(rd, csr, rs1) RV_I(csr, rs1, 3, rd, ASM_SYSTEM)
#define CSRRWI(rd, csr, uimm) RV_I(csr, uimm, 5, rd, ASM_SYSTEM)
#define CSRRSI(rd, csr, uimm) RV_I(csr, uimm, 6, rd, ASM_SYSTEM)
#define CSRRCI(rd, csr, uimm) RV_I(csr, uimm, 7, rd, ASM_SYSTEM)
#define CSRR(rd, csr) CSRRS(rd, csr, 0)
#define CSRW(csr, rs1) CSRRW(0, csr, rs1)
#define CSRWI(csr, uimm) CSRRWI(0, csr, uimm)

// M extension.
#define MUL(rd, rs1, rs2) RV_R(0x01, rs2, rs1, 0, rd, ASM_OP)
#define MULH(rd, rs1, rs2) RV_R(0x01, rs2, rs1, 1, rd, ASM_OP)
#define MULHSU(rd, rs1, rs2) RV_R(0x01, rs2, rs1, 2, rd, ASM_OP)
#define MULHU(rd, rs1, rs2) RV_R(0x01, rs2, rs1, 3, rd, ASM_OP)
#define DIV(rd, rs1, rs2) RV_R(0x01, rs2, rs1, 4, rd, ASM_OP)
#define DIVU(rd, rs1, rs2) RV_R(0x01, rs2, rs1, 5, rd, ASM_OP)
#define REM(rd, rs1, rs2) RV_R(0x01, rs2, rs1, 6, rd, ASM_OP)
#define REMU(rd, rs1, rs2) RV_R(0x01, rs2, rs1, 7, rd, ASM_OP)
#define MULW(rd, rs1, rs2) RV_R(0x01, rs2, rs1, 0, rd, ASM_OP_32)
#define DIVW(rd, rs1, rs2) RV_R(0x01, rs2, rs1, 4, rd, ASM_OP_32)
#define DIVUW(rd, rs1, rs2) RV_R(0x01, rs2, rs1, 5, rd, ASM_OP_32)
#define REMW(rd, rs1, rs2) RV_R(0x01, rs2, rs1, 6, rd, ASM_OP_32)
#define REMUW(rd, rs1, rs2) RV_R(0x01, rs2, rs1, 7, rd, ASM_OP_32)

// A extension: funct5 values and the .w / .d widths (funct3).
#define AMO_ADD 0x00
#define AMO_SWAP 0x01
#define AMO_LR 0x02
#define AMO_SC 0x03
#define AMO_XOR 0x04
#define AMO_OR 0x08
#define AMO_AND 0x0C
#define AMO_MIN 0x10
#define AMO_MAX 0x14
#define AMO_MINU 0x18
#define AMO_MAXU 0x1C
#define AMO_WIDTH_W 2
#define AMO_WIDTH_D 3

#define RV_AMO(funct5, width, rd, rs2, rs1)                                    \
  RV_R((funct5) << 2, rs2, rs1, width, rd, ASM_AMO)
#define AMO_W(funct5, rd, rs2, rs1) RV_AMO(funct5, AMO_WIDTH_W, rd, rs2, rs1)
#define AMO_D(funct5, rd, rs2, rs1) RV_AMO(funct5, AMO_WIDTH_D, rd, rs2, rs1)
#define LR_W(rd, rs1) AMO_W(AMO_LR, rd, 0, rs1)
#define SC_W(rd, rs2, rs1) AMO_W(AMO_SC, rd, rs2, rs1)
#define LR_D(rd, rs1) AMO_D(AMO_LR, rd, 0, rs1)
#define SC_D(rd, rs2, rs1) AMO_D(AMO_SC, rd, rs2, rs1)

#endif // TESTS_RISCV_ASM_H
