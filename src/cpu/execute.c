#include "riscv/cpu/execute.h"
#include "riscv/cpu/fpu.h"
#include "riscv/cpu/mmu.h"
#include "riscv/cpu/trap.h"
#include "riscv/util/bits.h"
#include "riscv/util/float.h"
#include <stdbool.h>
#include <stddef.h>

#define FUNCT12_ECALL 0x000
#define FUNCT12_EBREAK 0x001
#define FUNCT12_SRET 0x102
#define FUNCT12_WFI 0x105
#define FUNCT12_MRET 0x302

static enum ExecuteResult trap(Cpu *cpu, uxlen_t cause, uxlen_t tval,
                               uxlen_t *next_pc) {
  *next_pc = cpu_raise_exception(cpu, cause, tval);
  return EXECUTE_TRAP;
}

static enum ExecuteResult illegal_instruction(Cpu *cpu,
                                              const struct Instruction *inst,
                                              uxlen_t *next_pc) {
  return trap(cpu, TRAP_ILLEGAL_INSTRUCTION, inst->raw, next_pc);
}

static enum ExecuteResult translate(Cpu *cpu, uxlen_t addr,
                                    enum MmuAccess access, uint64_t *paddr,
                                    uxlen_t *next_pc) {
  enum MmuResult mmu_res = mmu_translate(cpu, addr, access, paddr);
  if (mmu_res != MMU_OK)
    return trap(cpu, mmu_fault_cause(mmu_res, access), addr, next_pc);
  return EXECUTE_OK;
}

static enum ExecuteResult access_fault(Cpu *cpu, enum MmuAccess access,
                                       uxlen_t addr, uxlen_t *next_pc) {
  return trap(cpu, mmu_fault_cause(MMU_ACCESS_FAULT, access), addr, next_pc);
}

struct PhysicalAccess {
  uint64_t paddr[2];
  unsigned size, first;
};

static inline uint8_t funct5_extract(uint8_t funct7) { return funct7 >> 2; }

static inline uint8_t fmt_extract(uint8_t funct7) { return bits(funct7, 1, 0); }

static bool crosses_page(const struct PhysicalAccess *pa) {
  return pa->first < pa->size;
}

static uint64_t byte_address(const struct PhysicalAccess *pa, unsigned i) {
  return i < pa->first ? pa->paddr[0] + i : pa->paddr[1] + (i - pa->first);
}

static enum ExecuteResult translate_access(Cpu *cpu, uxlen_t vaddr,
                                           unsigned size, enum MmuAccess access,
                                           struct PhysicalAccess *pa,
                                           uxlen_t *next_pc) {
  unsigned to_page_end = mmu_bytes_to_page_end(vaddr);
  pa->size = size;
  pa->first = to_page_end < size ? to_page_end : size;
  enum ExecuteResult result =
      translate(cpu, vaddr, access, &pa->paddr[0], next_pc);
  if (result != EXECUTE_OK || !crosses_page(pa))
    return result;
  return translate(cpu, vaddr + pa->first, access, &pa->paddr[1], next_pc);
}

static uxlen_t alu(uint8_t funct3, bool alternate, uxlen_t a, uxlen_t b) {
  unsigned shamt = b & SHAMT_MASK;
  switch (funct3) {
  case FUNCT3_ADD_SUB:
    return alternate ? a - b : a + b;
  case FUNCT3_SLL:
    return a << shamt;
  case FUNCT3_SLT:
    return (sxlen_t)a < (sxlen_t)b;
  case FUNCT3_SLTU:
    return a < b;
  case FUNCT3_XOR:
    return a ^ b;
  case FUNCT3_SRL_SRA:
    return alternate ? (uxlen_t)((sxlen_t)a >> shamt) : a >> shamt;
  case FUNCT3_OR:
    return a | b;
  case FUNCT3_AND:
    return a & b;
  }
  return 0;
}

#if XLEN == 64
__extension__ typedef __int128 sdouble_t;
__extension__ typedef unsigned __int128 udouble_t;
#else
typedef int64_t sdouble_t;
typedef uint64_t udouble_t;
#endif

#define SXLEN_MIN ((sxlen_t)((uxlen_t)1 << (XLEN - 1)))

static uxlen_t alu_m(uint8_t funct3, uxlen_t a, uxlen_t b) {
  sxlen_t sa = (sxlen_t)a, sb = (sxlen_t)b;
  bool overflow = sa == SXLEN_MIN && sb == -1;
  switch (funct3) {
  case FUNCT3_MUL:
    return a * b;
  case FUNCT3_MULH:
    return (uxlen_t)(((sdouble_t)sa * sb) >> XLEN);
  case FUNCT3_MULHSU:
    return (uxlen_t)(((sdouble_t)sa * (sdouble_t)b) >> XLEN);
  case FUNCT3_MULHU:
    return (uxlen_t)(((udouble_t)a * b) >> XLEN);
  case FUNCT3_DIV:
    return b == 0 ? (uxlen_t)-1 : overflow ? a : (uxlen_t)(sa / sb);
  case FUNCT3_DIVU:
    return b == 0 ? (uxlen_t)-1 : a / b;
  case FUNCT3_REM:
    return b == 0 ? a : overflow ? 0 : (uxlen_t)(sa % sb);
  case FUNCT3_REMU:
    return b == 0 ? a : a % b;
  }
  return 0;
}

#if XLEN == 64
static int32_t alu_32(uint8_t funct3, bool alternate, uint32_t a, uint32_t b) {
  unsigned shamt = b & SHAMT_MASK_32;
  switch (funct3) {
  case FUNCT3_ADD_SUB:
    return (int32_t)(alternate ? a - b : a + b);
  case FUNCT3_SLL:
    return (int32_t)(a << shamt);
  case FUNCT3_SRL_SRA:
    return alternate ? (int32_t)a >> shamt : (int32_t)(a >> shamt);
  }
  return 0;
}

static int32_t alu_m_32(uint8_t funct3, uint32_t a, uint32_t b) {
  int32_t sa = (int32_t)a, sb = (int32_t)b;
  bool overflow = sa == INT32_MIN && sb == -1;
  switch (funct3) {
  case FUNCT3_MUL:
    return (int32_t)(a * b);
  case FUNCT3_DIV:
    return b == 0 ? -1 : overflow ? sa : sa / sb;
  case FUNCT3_DIVU:
    return b == 0 ? -1 : (int32_t)(a / b);
  case FUNCT3_REM:
    return b == 0 ? sa : overflow ? 0 : sa % sb;
  case FUNCT3_REMU:
    return (int32_t)(b == 0 ? a : a % b);
  }
  return 0;
}

static bool has_w_form(uint8_t funct3) {
  return funct3 == FUNCT3_ADD_SUB || funct3 == FUNCT3_SLL ||
         funct3 == FUNCT3_SRL_SRA;
}

static bool has_m_w_form(uint8_t funct3) {
  return funct3 == FUNCT3_MUL || funct3 >= FUNCT3_DIV;
}

static void write_w_result(Cpu *cpu, uint8_t rd, int32_t result) {
  registers_write(cpu_get_registers(cpu), rd, (uxlen_t)(int64_t)result);
}
#endif

static bool shift_imm_valid(uint8_t funct3, int32_t imm, unsigned shamt_len) {
  uxlen_t above_shamt = bits(imm, I_IMM_WIDTH - 1, shamt_len);
  switch (funct3) {
  case FUNCT3_SLL:
    return above_shamt == 0;
  case FUNCT3_SRL_SRA:
    return (above_shamt & ~((1u << SHIFT_IMM_ARITHMETIC_BIT) >> shamt_len)) ==
           0;
  default:
    return true;
  }
}

static bool op_imm_alternate(const struct Instruction *inst) {
  return inst->funct3 == FUNCT3_SRL_SRA &&
         bits(inst->imm, SHIFT_IMM_ARITHMETIC_BIT, SHIFT_IMM_ARITHMETIC_BIT);
}

static enum ExecuteResult
execute_op_imm(Cpu *cpu, const struct Instruction *inst, uxlen_t *next_pc) {
  if (!shift_imm_valid(inst->funct3, inst->imm, SHAMT_LEN))
    return illegal_instruction(cpu, inst, next_pc);

  Registers *regs = cpu_get_registers(cpu);
  uxlen_t rs1_val = registers_read(regs, inst->rs1);
  uxlen_t result =
      alu(inst->funct3, op_imm_alternate(inst), rs1_val, inst->imm);
  registers_write(regs, inst->rd, result);
  return EXECUTE_OK;
}

#if XLEN == 64
static enum ExecuteResult
execute_op_imm_32(Cpu *cpu, const struct Instruction *inst, uxlen_t *next_pc) {
  if (!has_w_form(inst->funct3) ||
      !shift_imm_valid(inst->funct3, inst->imm, SHAMT_LEN_32))
    return illegal_instruction(cpu, inst, next_pc);

  uint32_t rs1_val = registers_read(cpu_get_registers(cpu), inst->rs1);
  write_w_result(
      cpu, inst->rd,
      alu_32(inst->funct3, op_imm_alternate(inst), rs1_val, inst->imm));
  return EXECUTE_OK;
}
#endif

static bool op_funct7_valid(uint8_t funct3, uint8_t funct7) {
  switch (funct7) {
  case FUNCT7_ADD:
    return true;
  case FUNCT7_SUB:
    return funct3 == FUNCT3_ADD_SUB || funct3 == FUNCT3_SRL_SRA;
  case FUNCT7_MUL:
    return true;
  default:
    return false;
  }
}

static enum ExecuteResult execute_op(Cpu *cpu, const struct Instruction *inst,
                                     uxlen_t *next_pc) {
  if (!op_funct7_valid(inst->funct3, inst->funct7))
    return illegal_instruction(cpu, inst, next_pc);

  Registers *regs = cpu_get_registers(cpu);
  uxlen_t rs1_val = registers_read(regs, inst->rs1);
  uxlen_t rs2_val = registers_read(regs, inst->rs2);
  uxlen_t result =
      inst->funct7 == FUNCT7_MUL
          ? alu_m(inst->funct3, rs1_val, rs2_val)
          : alu(inst->funct3, inst->funct7 == FUNCT7_SUB, rs1_val, rs2_val);
  registers_write(regs, inst->rd, result);
  return EXECUTE_OK;
}

static bool fp_enabled(Cpu *cpu) { return csr_fp_enabled(cpu_get_csr(cpu)); }

static uint64_t read_f(Cpu *cpu, uint8_t index) {
  return fp_registers_read(cpu_get_fp_registers(cpu), index);
}

static void write_f(Cpu *cpu, uint8_t index, uint64_t value) {
  fp_registers_write(cpu_get_fp_registers(cpu), index, value);
  csr_set_fp_dirty(cpu_get_csr(cpu));
}

static uint32_t read_single(Cpu *cpu, uint8_t index) {
  return unbox_single(read_f(cpu, index));
}

static void write_single(Cpu *cpu, uint8_t index, uint32_t value) {
  write_f(cpu, index, nan_box(value));
}

static bool resolve_rm(Cpu *cpu, uint8_t funct3, enum FpRounding *rm) {
  uint8_t mode =
      funct3 == FUNCT3_RM_DYN ? csr_get_frm(cpu_get_csr(cpu)) : funct3;
  if (mode > FP_RMM)
    return false;
  *rm = (enum FpRounding)mode;
  return true;
}

static uint64_t read_operand(Cpu *cpu, uint8_t fmt, uint8_t index) {
  return fmt == FMT_S ? read_single(cpu, index) : read_f(cpu, index);
}

static void write_result(Cpu *cpu, uint8_t fmt, uint8_t index, uint64_t value) {
  if (fmt == FMT_S)
    write_single(cpu, index, value);
  else
    write_f(cpu, index, value);
}

static void accrue(Cpu *cpu, uint8_t flags) {
  csr_accrue_fflags(cpu_get_csr(cpu), flags);
}

static enum FpOp arith_op(uint8_t funct5) {
  switch (funct5) {
  case FUNCT5_FSUB:
    return FP_SUB;
  case FUNCT5_FMUL:
    return FP_MUL;
  case FUNCT5_FDIV:
    return FP_DIV;
  }
  return FP_ADD;
}

static enum ExecuteResult execute_arith(Cpu *cpu,
                                        const struct Instruction *inst,
                                        uint8_t fmt, uxlen_t *next_pc) {
  enum FpRounding rm;
  if (!resolve_rm(cpu, inst->funct3, &rm))
    return illegal_instruction(cpu, inst, next_pc);

  enum FpOp op = arith_op(funct5_extract(inst->funct7));
  uint64_t a = read_operand(cpu, fmt, inst->rs1);
  uint64_t b = read_operand(cpu, fmt, inst->rs2);
  uint8_t flags;
  uint64_t result = fmt == FMT_S ? fpu_arith_s(op, a, b, rm, &flags)
                                 : fpu_arith_d(op, a, b, rm, &flags);
  write_result(cpu, fmt, inst->rd, result);
  accrue(cpu, flags);
  return EXECUTE_OK;
}

static enum ExecuteResult execute_fsqrt(Cpu *cpu,
                                        const struct Instruction *inst,
                                        uint8_t fmt, uxlen_t *next_pc) {
  enum FpRounding rm;
  if (inst->rs2 != 0 || !resolve_rm(cpu, inst->funct3, &rm))
    return illegal_instruction(cpu, inst, next_pc);

  uint64_t a = read_operand(cpu, fmt, inst->rs1);
  uint8_t flags;
  uint64_t result =
      fmt == FMT_S ? fpu_sqrt_s(a, rm, &flags) : fpu_sqrt_d(a, rm, &flags);
  write_result(cpu, fmt, inst->rd, result);
  accrue(cpu, flags);
  return EXECUTE_OK;
}

static enum ExecuteResult execute_fsgnj(Cpu *cpu,
                                        const struct Instruction *inst,
                                        uint8_t fmt, uxlen_t *next_pc) {
  if (inst->funct3 > SIGN_OF_A_XOR_B)
    return illegal_instruction(cpu, inst, next_pc);

  uint64_t result =
      sign_inject(inst->funct3, read_operand(cpu, fmt, inst->rs1),
                  read_operand(cpu, fmt, inst->rs2),
                  fmt == FMT_S ? SINGLE_SIGN_BIT : DOUBLE_SIGN_BIT);
  write_result(cpu, fmt, inst->rd, result);
  return EXECUTE_OK;
}

static enum ExecuteResult execute_fmin_fmax(Cpu *cpu,
                                            const struct Instruction *inst,
                                            uint8_t fmt, uxlen_t *next_pc) {
  if (inst->funct3 > FUNCT3_FMAX)
    return illegal_instruction(cpu, inst, next_pc);

  bool max = inst->funct3 == FUNCT3_FMAX;
  uint64_t a = read_operand(cpu, fmt, inst->rs1);
  uint64_t b = read_operand(cpu, fmt, inst->rs2);
  uint8_t flags;
  uint64_t result = fmt == FMT_S ? fpu_min_max_s(a, b, max, &flags)
                                 : fpu_min_max_d(a, b, max, &flags);
  write_result(cpu, fmt, inst->rd, result);
  accrue(cpu, flags);
  return EXECUTE_OK;
}

static enum ExecuteResult execute_fcmp(Cpu *cpu, const struct Instruction *inst,
                                       uint8_t fmt, uxlen_t *next_pc) {
  if (inst->funct3 > FP_EQ)
    return illegal_instruction(cpu, inst, next_pc);

  enum FpCompare cmp = inst->funct3;
  uint64_t a = read_operand(cpu, fmt, inst->rs1);
  uint64_t b = read_operand(cpu, fmt, inst->rs2);
  uint8_t flags;
  bool result = fmt == FMT_S ? fpu_compare_s(cmp, a, b, &flags)
                             : fpu_compare_d(cmp, a, b, &flags);
  registers_write(cpu_get_registers(cpu), inst->rd, result);
  accrue(cpu, flags);
  return EXECUTE_OK;
}

static enum ExecuteResult execute_fcvt_fp(Cpu *cpu,
                                          const struct Instruction *inst,
                                          uint8_t fmt, uxlen_t *next_pc) {
  enum FpRounding rm;
  uint8_t source = fmt == FMT_S ? FMT_D : FMT_S;
  if (inst->rs2 != source || !resolve_rm(cpu, inst->funct3, &rm))
    return illegal_instruction(cpu, inst, next_pc);

  uint64_t a = read_operand(cpu, source, inst->rs1);
  uint8_t flags;
  uint64_t result =
      fmt == FMT_S ? fpu_d_to_s(a, rm, &flags) : fpu_s_to_d(a, &flags);
  write_result(cpu, fmt, inst->rd, result);
  accrue(cpu, flags);
  return EXECUTE_OK;
}

static bool int_type(uint8_t rs2, enum FpInt *type) {
  if (rs2 > FP_INT_LU || (XLEN == 32 && rs2 >= FP_INT_L))
    return false;
  *type = (enum FpInt)rs2;
  return true;
}

static enum ExecuteResult execute_fcvt_to_int(Cpu *cpu,
                                              const struct Instruction *inst,
                                              uint8_t fmt, uxlen_t *next_pc) {
  enum FpRounding rm;
  enum FpInt type;
  if (!int_type(inst->rs2, &type) || !resolve_rm(cpu, inst->funct3, &rm))
    return illegal_instruction(cpu, inst, next_pc);

  uint64_t a = read_operand(cpu, fmt, inst->rs1);
  uint8_t flags;
  uint64_t result = fmt == FMT_S ? fpu_s_to_int(a, type, rm, &flags)
                                 : fpu_d_to_int(a, type, rm, &flags);
  registers_write(cpu_get_registers(cpu), inst->rd, (uxlen_t)result);
  accrue(cpu, flags);
  return EXECUTE_OK;
}

static enum ExecuteResult execute_fcvt_from_int(Cpu *cpu,
                                                const struct Instruction *inst,
                                                uint8_t fmt, uxlen_t *next_pc) {
  enum FpRounding rm;
  enum FpInt type;
  if (!int_type(inst->rs2, &type) || !resolve_rm(cpu, inst->funct3, &rm))
    return illegal_instruction(cpu, inst, next_pc);

  uint64_t value = registers_read(cpu_get_registers(cpu), inst->rs1);
  uint8_t flags;
  uint64_t result = fmt == FMT_S ? fpu_int_to_s(value, type, rm, &flags)
                                 : fpu_int_to_d(value, type, rm, &flags);
  write_result(cpu, fmt, inst->rd, result);
  accrue(cpu, flags);
  return EXECUTE_OK;
}

static enum ExecuteResult execute_fmv_x_fclass(Cpu *cpu,
                                               const struct Instruction *inst,
                                               uint8_t fmt, uxlen_t *next_pc) {
  if (inst->rs2 != 0)
    return illegal_instruction(cpu, inst, next_pc);

  uxlen_t result;
  if (inst->funct3 == FUNCT3_FMV && fmt == FMT_S)
    result = sign_extend((uint32_t)read_f(cpu, inst->rs1), 32);
#if XLEN == 64
  else if (inst->funct3 == FUNCT3_FMV && fmt == FMT_D)
    result = read_f(cpu, inst->rs1);
#endif
  else if (inst->funct3 == FUNCT3_FCLASS && fmt == FMT_S)
    result =
        fclass(read_single(cpu, inst->rs1), SINGLE_EXP_BITS, SINGLE_MANT_BITS);
  else if (inst->funct3 == FUNCT3_FCLASS && fmt == FMT_D)
    result = fclass(read_f(cpu, inst->rs1), DOUBLE_EXP_BITS, DOUBLE_MANT_BITS);
  else
    return illegal_instruction(cpu, inst, next_pc);

  registers_write(cpu_get_registers(cpu), inst->rd, result);
  return EXECUTE_OK;
}

static enum ExecuteResult execute_fmv_f(Cpu *cpu,
                                        const struct Instruction *inst,
                                        uint8_t fmt, uxlen_t *next_pc) {
  if (inst->rs2 != 0 || inst->funct3 != FUNCT3_FMV)
    return illegal_instruction(cpu, inst, next_pc);

  uxlen_t value = registers_read(cpu_get_registers(cpu), inst->rs1);
  if (fmt == FMT_S)
    write_single(cpu, inst->rd, (uint32_t)value);
#if XLEN == 64
  else if (fmt == FMT_D)
    write_f(cpu, inst->rd, value);
#endif
  else
    return illegal_instruction(cpu, inst, next_pc);
  return EXECUTE_OK;
}

static bool fp_format_valid(Cpu *cpu, uint8_t fmt) {
  return fp_enabled(cpu) && (fmt == FMT_S || fmt == FMT_D);
}

static enum ExecuteResult
execute_fp_op(Cpu *cpu, const struct Instruction *inst, uxlen_t *next_pc) {
  uint8_t fmt = fmt_extract(inst->funct7);
  if (!fp_format_valid(cpu, fmt))
    return illegal_instruction(cpu, inst, next_pc);

  switch (funct5_extract(inst->funct7)) {
  case FUNCT5_FADD:
  case FUNCT5_FSUB:
  case FUNCT5_FMUL:
  case FUNCT5_FDIV:
    return execute_arith(cpu, inst, fmt, next_pc);
  case FUNCT5_FSQRT:
    return execute_fsqrt(cpu, inst, fmt, next_pc);
  case FUNCT5_FSGNJ:
    return execute_fsgnj(cpu, inst, fmt, next_pc);
  case FUNCT5_FMIN_FMAX:
    return execute_fmin_fmax(cpu, inst, fmt, next_pc);
  case FUNCT5_FCVT_FP:
    return execute_fcvt_fp(cpu, inst, fmt, next_pc);
  case FUNCT5_FCMP:
    return execute_fcmp(cpu, inst, fmt, next_pc);
  case FUNCT5_FCVT_TO_INT:
    return execute_fcvt_to_int(cpu, inst, fmt, next_pc);
  case FUNCT5_FCVT_FROM_INT:
    return execute_fcvt_from_int(cpu, inst, fmt, next_pc);
  case FUNCT5_FMV_X_FCLASS:
    return execute_fmv_x_fclass(cpu, inst, fmt, next_pc);
  case FUNCT5_FMV_F_X:
    return execute_fmv_f(cpu, inst, fmt, next_pc);
  default:
    return illegal_instruction(cpu, inst, next_pc);
  }
}

// R4 format: rs3 sits where funct5 would.
static enum ExecuteResult execute_fma(Cpu *cpu, const struct Instruction *inst,
                                      uxlen_t *next_pc) {
  uint8_t fmt = fmt_extract(inst->funct7);
  enum FpRounding rm;
  if (!fp_format_valid(cpu, fmt) || !resolve_rm(cpu, inst->funct3, &rm))
    return illegal_instruction(cpu, inst, next_pc);

  uint64_t sign = fmt == FMT_S ? SINGLE_SIGN_BIT : DOUBLE_SIGN_BIT;
  uint64_t a = read_operand(cpu, fmt, inst->rs1);
  uint64_t b = read_operand(cpu, fmt, inst->rs2);
  uint64_t c = read_operand(cpu, fmt, funct5_extract(inst->funct7));
  if (inst->opcode == OPCODE_NMSUB || inst->opcode == OPCODE_NMADD)
    a ^= sign;
  if (inst->opcode == OPCODE_MSUB || inst->opcode == OPCODE_NMADD)
    c ^= sign;

  uint8_t flags;
  uint64_t result = fmt == FMT_S ? fpu_fma_s(a, b, c, rm, &flags)
                                 : fpu_fma_d(a, b, c, rm, &flags);
  write_result(cpu, fmt, inst->rd, result);
  accrue(cpu, flags);
  return EXECUTE_OK;
}

#if XLEN == 64
static bool op_32_valid(uint8_t funct3, uint8_t funct7) {
  if (funct7 == FUNCT7_MUL)
    return has_m_w_form(funct3);
  return has_w_form(funct3) && op_funct7_valid(funct3, funct7);
}

static enum ExecuteResult
execute_op_32(Cpu *cpu, const struct Instruction *inst, uxlen_t *next_pc) {
  if (!op_32_valid(inst->funct3, inst->funct7))
    return illegal_instruction(cpu, inst, next_pc);

  Registers *regs = cpu_get_registers(cpu);
  uint32_t rs1_val = registers_read(regs, inst->rs1);
  uint32_t rs2_val = registers_read(regs, inst->rs2);
  int32_t result =
      inst->funct7 == FUNCT7_MUL
          ? alu_m_32(inst->funct3, rs1_val, rs2_val)
          : alu_32(inst->funct3, inst->funct7 == FUNCT7_SUB, rs1_val, rs2_val);
  write_w_result(cpu, inst->rd, result);
  return EXECUTE_OK;
}
#endif

static enum ExecuteResult execute_lui(Cpu *cpu,
                                      const struct Instruction *inst) {
  registers_write(cpu_get_registers(cpu), inst->rd, inst->imm);
  return EXECUTE_OK;
}

static enum ExecuteResult execute_auipc(Cpu *cpu,
                                        const struct Instruction *inst) {
  registers_write(cpu_get_registers(cpu), inst->rd,
                  cpu_get_pc(cpu) + inst->imm);
  return EXECUTE_OK;
}

static enum ExecuteResult execute_jump_base(Cpu *cpu,
                                            const struct Instruction *inst,
                                            uxlen_t target, uxlen_t *next_pc) {
  if (target % COMPRESSED_WIDTH_BYTES)
    return trap(cpu, TRAP_INSTRUCTION_ADDRESS_MISALIGNED, target, next_pc);
  registers_write(cpu_get_registers(cpu), inst->rd, *next_pc);
  *next_pc = target;
  return EXECUTE_OK;
}

static enum ExecuteResult execute_jal(Cpu *cpu, const struct Instruction *inst,
                                      uxlen_t *next_pc) {
  return execute_jump_base(cpu, inst, cpu_get_pc(cpu) + inst->imm, next_pc);
}

static enum ExecuteResult execute_jalr(Cpu *cpu, const struct Instruction *inst,
                                       uxlen_t *next_pc) {
  if (inst->funct3 != FUNCT3_JALR) {
    return illegal_instruction(cpu, inst, next_pc);
  }
  return execute_jump_base(
      cpu, inst,
      clear_bits(registers_read(cpu_get_registers(cpu), inst->rs1) + inst->imm,
                 0, 0),
      next_pc);
}

static enum ExecuteResult
execute_branch(Cpu *cpu, const struct Instruction *inst, uxlen_t *next_pc) {
  uxlen_t rs1_val = registers_read(cpu_get_registers(cpu), inst->rs1);
  uxlen_t rs2_val = registers_read(cpu_get_registers(cpu), inst->rs2);
  bool taken;
  switch (inst->funct3) {
  case FUNCT3_BEQ:
    taken = rs1_val == rs2_val;
    break;
  case FUNCT3_BNE:
    taken = rs1_val != rs2_val;
    break;
  case FUNCT3_BLT:
    taken = (sxlen_t)rs1_val < (sxlen_t)rs2_val;
    break;
  case FUNCT3_BGE:
    taken = (sxlen_t)rs1_val >= (sxlen_t)rs2_val;
    break;
  case FUNCT3_BLTU:
    taken = rs1_val < rs2_val;
    break;
  case FUNCT3_BGEU:
    taken = rs1_val >= rs2_val;
    break;
  default:
    return illegal_instruction(cpu, inst, next_pc);
  }
  if (!taken)
    return EXECUTE_OK;
  uxlen_t target = cpu_get_pc(cpu) + inst->imm;
  if (target % COMPRESSED_WIDTH_BYTES)
    return trap(cpu, TRAP_INSTRUCTION_ADDRESS_MISALIGNED, target, next_pc);
  *next_pc = target;
  return EXECUTE_OK;
}

static inline bool load_trigger_hit(Cpu *cpu, uint8_t size, uxlen_t addr,
                                    const uxlen_t *data, uxlen_t *tval) {
  return csr_breakpoint_on_load(cpu_get_csr(cpu), cpu_get_privilege(cpu), addr,
                                size, data, tval);
}

static inline bool store_trigger_hit(Cpu *cpu, uint8_t size, uxlen_t addr,
                                     const uxlen_t *data, uxlen_t *tval) {
  return csr_breakpoint_on_store(cpu_get_csr(cpu), cpu_get_privilege(cpu), addr,
                                 size, data, tval);
}

static enum ExecuteResult read_virtual(Cpu *cpu, uxlen_t vaddr, unsigned size,
                                       uint64_t *value, enum MmuAccess access,
                                       uxlen_t *next_pc) {
  struct PhysicalAccess pa;
  enum ExecuteResult result =
      translate_access(cpu, vaddr, size, access, &pa, next_pc);
  if (result != EXECUTE_OK)
    return result;

  Bus *bus = cpu_get_bus(cpu);
  if (!crosses_page(&pa)) {
    if (bus_read(bus, pa.paddr[0], size, value) != BUS_OK)
      return access_fault(cpu, access, vaddr, next_pc);
    return EXECUTE_OK;
  }
  // The parts can be any size, which the bus does not take: go byte by byte.
  *value = 0;
  for (unsigned i = 0; i < size; i++) {
    uint64_t byte;
    if (bus_read(bus, byte_address(&pa, i), 1, &byte) != BUS_OK)
      return access_fault(cpu, access, vaddr + i, next_pc);
    *value |= byte << (BITS_PER_BYTE * i);
  }
  return EXECUTE_OK;
}

static uint8_t fp_access_size(uint8_t funct3) {
  switch (funct3) {
  case FUNCT3_FP_W:
    return 4;
  case FUNCT3_FP_D:
    return 8;
  default:
    return 0;
  }
}

static enum ExecuteResult load_memory(Cpu *cpu, const struct Instruction *inst,
                                      uint8_t size, uint64_t *value,
                                      uxlen_t *next_pc) {
  uxlen_t addr = registers_read(cpu_get_registers(cpu), inst->rs1) + inst->imm;
  uxlen_t tval;
  if (load_trigger_hit(cpu, size, addr, NULL, &tval)) {
    return trap(cpu, TRAP_BREAKPOINT, tval, next_pc);
  }

  enum ExecuteResult read_result =
      read_virtual(cpu, addr, size, value, MMU_LOAD, next_pc);
  if (read_result != EXECUTE_OK)
    return read_result;

  uxlen_t data = (uxlen_t)*value;
  if (load_trigger_hit(cpu, size, addr, &data, &tval))
    return trap(cpu, TRAP_BREAKPOINT, tval, next_pc);
  return EXECUTE_OK;
}

static enum ExecuteResult execute_load(Cpu *cpu, const struct Instruction *inst,
                                       uxlen_t *next_pc) {
  uint8_t size = 0;
  switch (inst->funct3) {
  case FUNCT3_LB:
  case FUNCT3_LBU:
    size = 1;
    break;
  case FUNCT3_LH:
  case FUNCT3_LHU:
    size = 2;
    break;
  case FUNCT3_LW:
#if XLEN == 64
  case FUNCT3_LWU:
#endif
    size = 4;
    break;
#if XLEN == 64
  case FUNCT3_LD:
    size = 8;
    break;
#endif
  default:
    return illegal_instruction(cpu, inst, next_pc);
  }

  uint64_t val;
  enum ExecuteResult load_result = load_memory(cpu, inst, size, &val, next_pc);
  if (load_result != EXECUTE_OK)
    return load_result;

  if (inst->funct3 == FUNCT3_LBU || inst->funct3 == FUNCT3_LHU ||
      inst->funct3 == FUNCT3_LWU || inst->funct3 == FUNCT3_LD)
    val = (uxlen_t)val;
  else
    val = sign_extend((uxlen_t)val, size * BITS_PER_BYTE);

  registers_write(cpu_get_registers(cpu), inst->rd, val);
  return EXECUTE_OK;
}

static enum ExecuteResult
execute_fp_load(Cpu *cpu, const struct Instruction *inst, uxlen_t *next_pc) {
  uint8_t size = fp_access_size(inst->funct3);
  if (!fp_enabled(cpu) || size == 0)
    return illegal_instruction(cpu, inst, next_pc);

  uint64_t val;
  enum ExecuteResult load_result = load_memory(cpu, inst, size, &val, next_pc);
  if (load_result != EXECUTE_OK)
    return load_result;

  if (inst->funct3 == FUNCT3_FP_W)
    write_single(cpu, inst->rd, (uint32_t)val);
  else
    write_f(cpu, inst->rd, val);
  return EXECUTE_OK;
}

static enum ExecuteResult write_virtual(Cpu *cpu, uxlen_t vaddr, unsigned size,
                                        uint64_t value, enum MmuAccess access,
                                        uxlen_t *next_pc) {
  struct PhysicalAccess pa;
  enum ExecuteResult result =
      translate_access(cpu, vaddr, size, access, &pa, next_pc);
  if (result != EXECUTE_OK)
    return result;

  Bus *bus = cpu_get_bus(cpu);
  if (!crosses_page(&pa)) {
    if (bus_write(bus, pa.paddr[0], size, value) != BUS_OK)
      return access_fault(cpu, access, vaddr, next_pc);
    return EXECUTE_OK;
  }
  for (unsigned i = 0; i < size; i++) {
    uint8_t byte = value >> (BITS_PER_BYTE * i);
    if (bus_write(bus, byte_address(&pa, i), 1, byte) != BUS_OK)
      return access_fault(cpu, access, vaddr + i, next_pc);
  }
  return EXECUTE_OK;
}

static enum ExecuteResult store_memory(Cpu *cpu, const struct Instruction *inst,
                                       unsigned size, uint64_t value,
                                       uxlen_t *next_pc) {
  uxlen_t addr = registers_read(cpu_get_registers(cpu), inst->rs1) + inst->imm;
  uxlen_t tval;
  if (store_trigger_hit(cpu, size, addr, NULL, &tval)) {
    return trap(cpu, TRAP_BREAKPOINT, tval, next_pc);
  }
  uxlen_t data = truncate_to_size(value, size);
  if (store_trigger_hit(cpu, size, addr, &data, &tval)) {
    return trap(cpu, TRAP_BREAKPOINT, tval, next_pc);
  }
  return write_virtual(cpu, addr, size, value, MMU_STORE, next_pc);
}

static enum ExecuteResult
execute_store(Cpu *cpu, const struct Instruction *inst, uxlen_t *next_pc) {
  uint8_t size = 0;
  switch (inst->funct3) {
  case FUNCT3_SB:
    size = 1;
    break;
  case FUNCT3_SH:
    size = 2;
    break;
  case FUNCT3_SW:
    size = 4;
    break;
#if XLEN == 64
  case FUNCT3_SD:
    size = 8;
    break;
#endif
  default:
    return illegal_instruction(cpu, inst, next_pc);
  }

  uint64_t val = registers_read(cpu_get_registers(cpu), inst->rs2);
  return store_memory(cpu, inst, size, val, next_pc);
}

static enum ExecuteResult
execute_fp_store(Cpu *cpu, const struct Instruction *inst, uxlen_t *next_pc) {
  uint8_t size = fp_access_size(inst->funct3);
  if (!fp_enabled(cpu) || size == 0)
    return illegal_instruction(cpu, inst, next_pc);

  return store_memory(cpu, inst, size, read_f(cpu, inst->rs2), next_pc);
}

static bool allowed_below_m(Cpu *cpu, enum Privilege privilege,
                            uxlen_t trap_bit) {
  uxlen_t mstatus;
  csr_read(cpu_get_csr(cpu), CSR_REG_MSTATUS, PRIV_M, &mstatus);
  return privilege == PRIV_M || (privilege == PRIV_S && !(mstatus & trap_bit));
}

static enum ExecuteResult execute_wfi(Cpu *cpu, const struct Instruction *inst,
                                      enum Privilege privilege,
                                      uxlen_t *next_pc) {
  if (!allowed_below_m(cpu, privilege, MSTATUS_TW_BIT))
    return illegal_instruction(cpu, inst, next_pc);
  cpu_set_waiting(cpu, true);
  return EXECUTE_OK;
}

static enum ExecuteResult execute_sfence_vma(Cpu *cpu,
                                             const struct Instruction *inst,
                                             enum Privilege privilege,
                                             uxlen_t *next_pc) {
  if (!allowed_below_m(cpu, privilege, MSTATUS_TVM_BIT))
    return illegal_instruction(cpu, inst, next_pc);
  return EXECUTE_OK;
}

static bool is_sfence_vma(const struct Instruction *inst) {
  return inst->funct3 == FUNCT3_PRIV && inst->funct7 == FUNCT7_SFENCE_VMA &&
         inst->rd == 0;
}

static bool is_priv(const struct Instruction *inst, int32_t funct12) {
  return inst->funct3 == FUNCT3_PRIV && inst->rd == 0 && inst->rs1 == 0 &&
         inst->imm == funct12;
}

static enum ExecuteResult
execute_system(Cpu *cpu, const struct Instruction *inst, uxlen_t *next_pc) {
  enum Privilege privilege = cpu_get_privilege(cpu);
  if (is_priv(inst, FUNCT12_ECALL)) {
    return trap(cpu, TRAP_ECALL_FROM_U + privilege, 0, next_pc);
  } else if (is_priv(inst, FUNCT12_EBREAK)) {
    return trap(cpu, TRAP_BREAKPOINT, cpu_get_pc(cpu), next_pc);
  } else if (privilege >= PRIV_M && is_priv(inst, FUNCT12_MRET)) {
    *next_pc = cpu_return_from_trap(cpu, PRIV_M);
    return EXECUTE_OK;
  } else if (privilege >= PRIV_S && is_priv(inst, FUNCT12_SRET)) {
    *next_pc = cpu_return_from_trap(cpu, PRIV_S);
    return EXECUTE_OK;
  } else if (is_priv(inst, FUNCT12_WFI)) {
    return execute_wfi(cpu, inst, privilege, next_pc);
  } else if (is_sfence_vma(inst)) {
    return execute_sfence_vma(cpu, inst, privilege, next_pc);
  }

  uint32_t csr_addr = (uint32_t)(inst->imm & 0xFFF);
  Csr *csr = cpu_get_csr(cpu);
  Registers *regs = cpu_get_registers(cpu);

  uxlen_t rs1_val = registers_read(regs, inst->rs1);
  uxlen_t value, mask;
  bool write = inst->rs1 != 0;

  switch (inst->funct3) {
  case FUNCT3_CSRRW:
    value = rs1_val;
    mask = ~(uxlen_t)0;
    write = true;
    break;
  case FUNCT3_CSRRS:
    value = ~(uxlen_t)0;
    mask = rs1_val;
    break;
  case FUNCT3_CSRRC:
    value = 0;
    mask = rs1_val;
    break;
  case FUNCT3_CSRRWI:
    value = inst->rs1;
    mask = ~(uxlen_t)0;
    write = true;
    break;
  case FUNCT3_CSRRSI:
    value = ~(uxlen_t)0;
    mask = inst->rs1;
    break;
  case FUNCT3_CSRRCI:
    value = 0;
    mask = inst->rs1;
    break;
  default:
    return illegal_instruction(cpu, inst, next_pc);
  }

  uxlen_t old;
  if (csr_modify(csr, csr_addr, privilege, value, mask, write, &old) != CSR_OK)
    return illegal_instruction(cpu, inst, next_pc);
  registers_write(regs, inst->rd, old);

  return EXECUTE_OK;
}

static enum ExecuteResult
execute_fence(Cpu *cpu, const struct Instruction *inst, uxlen_t *next_pc) {
  if (inst->funct3 != FUNCT3_FENCE && inst->funct3 != FUNCT3_FENCE_I) {
    return illegal_instruction(cpu, inst, next_pc);
  }
  return EXECUTE_OK;
}

static enum ExecuteResult amo_lr(Cpu *cpu, const struct Instruction *inst,
                                 uint8_t size, uxlen_t *next_pc) {
  uxlen_t rs1_addr = registers_read(cpu_get_registers(cpu), inst->rs1);
  uxlen_t tval;
  if (load_trigger_hit(cpu, size, rs1_addr, NULL, &tval)) {
    return trap(cpu, TRAP_BREAKPOINT, tval, next_pc);
  }
  if (rs1_addr % size != 0)
    return trap(cpu, TRAP_LOAD_ADDRESS_MISALIGNED, rs1_addr, next_pc);

  uint64_t phys_addr;
  enum ExecuteResult translate_result =
      translate(cpu, rs1_addr, MMU_LOAD, &phys_addr, next_pc);
  if (translate_result != EXECUTE_OK)
    return translate_result;

  uint64_t value;
  if (bus_read(cpu_get_bus(cpu), phys_addr, size, &value) != BUS_OK)
    return access_fault(cpu, MMU_LOAD, rs1_addr, next_pc);

  uxlen_t data = (uxlen_t)value;
  if (load_trigger_hit(cpu, size, rs1_addr, &data, &tval)) {
    return trap(cpu, TRAP_BREAKPOINT, tval, next_pc);
  }
  if (size == AMO_SIZE_W)
    value = (int32_t)value;
  cpu_set_amo_reservation(cpu, rs1_addr);
  registers_write(cpu_get_registers(cpu), inst->rd, value);
  return EXECUTE_OK;
}

enum ScResult { SC_SUCCESS = 0, SC_FAILURE = 1 };

static enum ExecuteResult amo_sc(Cpu *cpu, const struct Instruction *inst,
                                 uint8_t size, uxlen_t *next_pc) {
  uxlen_t rs1_addr = registers_read(cpu_get_registers(cpu), inst->rs1);
  if (rs1_addr % size != 0)
    return trap(cpu, TRAP_STORE_ADDRESS_MISALIGNED, rs1_addr, next_pc);
  if (cpu_get_amo_reservation(cpu) != rs1_addr) {
    registers_write(cpu_get_registers(cpu), inst->rd, SC_FAILURE);
    cpu_set_amo_reservation(cpu, 0);
    return EXECUTE_OK;
  }
  uxlen_t tval;
  if (store_trigger_hit(cpu, size, rs1_addr, NULL, &tval)) {
    return trap(cpu, TRAP_BREAKPOINT, tval, next_pc);
  }
  uxlen_t rs2_val = registers_read(cpu_get_registers(cpu), inst->rs2);

  uxlen_t data = truncate_to_size(rs2_val, size);
  if (store_trigger_hit(cpu, size, rs1_addr, &data, &tval)) {
    return trap(cpu, TRAP_BREAKPOINT, tval, next_pc);
  }

  uint64_t phys_addr;
  enum ExecuteResult translate_result =
      translate(cpu, rs1_addr, MMU_STORE, &phys_addr, next_pc);
  if (translate_result != EXECUTE_OK)
    return translate_result;

  if (bus_write(cpu_get_bus(cpu), phys_addr, size, rs2_val) != BUS_OK)
    return access_fault(cpu, MMU_STORE, rs1_addr, next_pc);

  registers_write(cpu_get_registers(cpu), inst->rd, SC_SUCCESS);
  cpu_set_amo_reservation(cpu, 0);
  return EXECUTE_OK;
}

static int64_t alu_amo(uxlen_t funct5, int64_t rs1_val, int64_t rs2_val) {
  switch (funct5) {
  case FUNCT5_AMO_ADD:
    return rs1_val + rs2_val;
  case FUNCT5_AMO_SWAP:
    return rs2_val;
  case FUNCT5_AMO_XOR:
    return rs1_val ^ rs2_val;
  case FUNCT5_AMO_OR:
    return rs1_val | rs2_val;
  case FUNCT5_AMO_AND:
    return rs1_val & rs2_val;
  case FUNCT5_AMO_MIN:
    return (rs1_val < rs2_val) ? rs1_val : rs2_val;
  case FUNCT5_AMO_MAX:
    return (rs1_val > rs2_val) ? rs1_val : rs2_val;
  case FUNCT5_AMO_MINU:
    return ((uint64_t)rs1_val < (uint64_t)rs2_val) ? rs1_val : rs2_val;
  case FUNCT5_AMO_MAXU:
    return ((uint64_t)rs1_val > (uint64_t)rs2_val) ? rs1_val : rs2_val;
  }
  return 0; // Should not reach here if funct5 is valid
}

static bool amo_size(uint8_t funct3, uint8_t *size) {
  switch (funct3) {
  case FUNCT3_AMO_W:
    *size = AMO_SIZE_W;
    return true;
#if XLEN == 64
  case FUNCT3_AMO_D:
    *size = AMO_SIZE_D;
    return true;
#endif
  default:
    return false;
  }
}

static bool is_amo_funct5_valid(uint8_t funct5) {
  switch (funct5) {
  case FUNCT5_AMO_ADD:
  case FUNCT5_AMO_SWAP:
  case FUNCT5_AMO_LR:
  case FUNCT5_AMO_SC:
  case FUNCT5_AMO_XOR:
  case FUNCT5_AMO_OR:
  case FUNCT5_AMO_AND:
  case FUNCT5_AMO_MIN:
  case FUNCT5_AMO_MAX:
  case FUNCT5_AMO_MINU:
  case FUNCT5_AMO_MAXU:
    return true;
  default:
    return false;
  }
}

static int64_t amo_operand(uint64_t value, uint8_t size) {
  return size == AMO_SIZE_W ? (int32_t)value : (int64_t)value;
}

static inline uxlen_t lower_tval(uxlen_t a, uxlen_t b) { return a < b ? a : b; }

static enum ExecuteResult execute_amo(Cpu *cpu, const struct Instruction *inst,
                                      uxlen_t *next_pc) {
  uint8_t size;
  if (!amo_size(inst->funct3, &size))
    return illegal_instruction(cpu, inst, next_pc);
  uint8_t funct5 = funct5_extract(inst->funct7);
  if (!is_amo_funct5_valid(funct5))
    return illegal_instruction(cpu, inst, next_pc);

  if (funct5 == FUNCT5_AMO_LR) {
    return amo_lr(cpu, inst, size, next_pc);
  } else if (funct5 == FUNCT5_AMO_SC)
    return amo_sc(cpu, inst, size, next_pc);

  Registers *regs = cpu_get_registers(cpu);
  uxlen_t addr = registers_read(regs, inst->rs1);
  // Evaluate both so every matching trigger gets its hit bit
  uxlen_t load_tval = (uxlen_t)-1;
  uxlen_t store_tval = (uxlen_t)-1;
  bool load = load_trigger_hit(cpu, size, addr, NULL, &load_tval);
  bool store = store_trigger_hit(cpu, size, addr, NULL, &store_tval);
  if (load || store)
    return trap(cpu, TRAP_BREAKPOINT, lower_tval(load_tval, store_tval),
                next_pc);
  if (addr % size != 0)
    return trap(cpu, TRAP_STORE_ADDRESS_MISALIGNED, addr, next_pc);

  uint64_t phys_addr;
  enum ExecuteResult translate_result =
      translate(cpu, addr, MMU_STORE, &phys_addr, next_pc);
  if (translate_result != EXECUTE_OK)
    return translate_result;

  Bus *bus = cpu_get_bus(cpu);
  uint64_t loaded;
  // The read half faults as a store too.
  if (bus_read(bus, phys_addr, size, &loaded) != BUS_OK)
    return access_fault(cpu, MMU_STORE, addr, next_pc);

  uxlen_t tval;
  uxlen_t data = (uxlen_t)loaded;
  if (load_trigger_hit(cpu, size, addr, &data, &tval))
    return trap(cpu, TRAP_BREAKPOINT, tval, next_pc);

  int64_t old = amo_operand(loaded, size);
  int64_t operand = amo_operand(registers_read(regs, inst->rs2), size);
  int64_t result;

  result = alu_amo(funct5, old, operand);

  data = truncate_to_size(result, size);
  if (store_trigger_hit(cpu, size, addr, &data, &tval))
    return trap(cpu, TRAP_BREAKPOINT, tval, next_pc);

  if (bus_write(bus, phys_addr, size, (uint64_t)result) != BUS_OK)
    return access_fault(cpu, MMU_STORE, addr, next_pc);

  registers_write(regs, inst->rd, (uxlen_t)old);
  return EXECUTE_OK;
}

enum ExecuteResult cpu_execute(Cpu *cpu, const struct Instruction *inst) {
  uxlen_t next_pc = cpu_get_pc(cpu) + inst->length;
  int status;
  switch (inst->opcode) {
  case OPCODE_OP_IMM:
    status = execute_op_imm(cpu, inst, &next_pc);
    break;
#if XLEN == 64
  case OPCODE_OP_IMM_32:
    status = execute_op_imm_32(cpu, inst, &next_pc);
    break;
  case OPCODE_OP_32:
    status = execute_op_32(cpu, inst, &next_pc);
    break;
#endif
  case OPCODE_OP:
    status = execute_op(cpu, inst, &next_pc);
    break;
  case OPCODE_OP_FP:
    status = execute_fp_op(cpu, inst, &next_pc);
    break;
  case OPCODE_LUI:
    status = execute_lui(cpu, inst);
    break;
  case OPCODE_AUIPC:
    status = execute_auipc(cpu, inst);
    break;
  case OPCODE_JAL:
    status = execute_jal(cpu, inst, &next_pc);
    break;
  case OPCODE_JALR:
    status = execute_jalr(cpu, inst, &next_pc);
    break;
  case OPCODE_BRANCH:
    status = execute_branch(cpu, inst, &next_pc);
    break;
  case OPCODE_LOAD:
    status = execute_load(cpu, inst, &next_pc);
    break;
  case OPCODE_LOAD_FP:
    status = execute_fp_load(cpu, inst, &next_pc);
    break;
  case OPCODE_STORE:
    status = execute_store(cpu, inst, &next_pc);
    break;
  case OPCODE_MADD:
  case OPCODE_MSUB:
  case OPCODE_NMSUB:
  case OPCODE_NMADD:
    status = execute_fma(cpu, inst, &next_pc);
    break;
  case OPCODE_STORE_FP:
    status = execute_fp_store(cpu, inst, &next_pc);
    break;
  case OPCODE_SYSTEM:
    status = execute_system(cpu, inst, &next_pc);
    break;
  case OPCODE_FENCE:
    status = execute_fence(cpu, inst, &next_pc);
    break;
  case OPCODE_AMO:
    status = execute_amo(cpu, inst, &next_pc);
    break;
  default:
    status = illegal_instruction(cpu, inst, &next_pc);
    break;
  }
  cpu_set_pc(cpu, next_pc);
  return status;
}
