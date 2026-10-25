#include "riscv/execute.h"
#include "riscv/bits.h"
#include "riscv/trap.h"
#include <stdbool.h>

static inline uxlen_t advance_pc_address(Cpu *cpu) {
  return cpu_get_pc(cpu) + PC_INCREMENT;
}

static int retire(uxlen_t pc, uxlen_t *next_pc) {
  *next_pc = pc;
  return EXECUTE_OK;
}

static int trap(Cpu *cpu, uxlen_t cause, uxlen_t tval, uxlen_t *next_pc) {
  *next_pc = cpu_raise_exception(cpu, cause, tval);
  return EXECUTE_TRAP;
}

static int illegal_instruction(Cpu *cpu, const struct Instruction *inst,
                               uxlen_t *next_pc) {
  return trap(cpu, TRAP_ILLEGAL_INSTRUCTION, inst->raw, next_pc);
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

static int execute_op_imm(Cpu *cpu, const struct Instruction *inst,
                          uxlen_t *next_pc) {
  if (!shift_imm_valid(inst->funct3, inst->imm, SHAMT_LEN))
    return illegal_instruction(cpu, inst, next_pc);

  Registers *regs = cpu_get_registers(cpu);
  uxlen_t rs1_val = registers_read(regs, inst->rs1);
  uxlen_t result =
      alu(inst->funct3, op_imm_alternate(inst), rs1_val, inst->imm);
  registers_write(regs, inst->rd, result);
  return retire(advance_pc_address(cpu), next_pc);
}

#if XLEN == 64
static int execute_op_imm_32(Cpu *cpu, const struct Instruction *inst,
                             uxlen_t *next_pc) {
  if (!has_w_form(inst->funct3) ||
      !shift_imm_valid(inst->funct3, inst->imm, SHAMT_LEN_32))
    return illegal_instruction(cpu, inst, next_pc);

  uint32_t rs1_val = registers_read(cpu_get_registers(cpu), inst->rs1);
  write_w_result(
      cpu, inst->rd,
      alu_32(inst->funct3, op_imm_alternate(inst), rs1_val, inst->imm));
  return retire(advance_pc_address(cpu), next_pc);
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

static int execute_op(Cpu *cpu, const struct Instruction *inst,
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
  return retire(advance_pc_address(cpu), next_pc);
}

#if XLEN == 64
static bool op_32_valid(uint8_t funct3, uint8_t funct7) {
  if (funct7 == FUNCT7_MUL)
    return has_m_w_form(funct3);
  return has_w_form(funct3) && op_funct7_valid(funct3, funct7);
}

static int execute_op_32(Cpu *cpu, const struct Instruction *inst,
                         uxlen_t *next_pc) {
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
  return retire(advance_pc_address(cpu), next_pc);
}
#endif

static int execute_lui(Cpu *cpu, const struct Instruction *inst,
                       uxlen_t *next_pc) {
  registers_write(cpu_get_registers(cpu), inst->rd, inst->imm);
  return retire(advance_pc_address(cpu), next_pc);
}

static int execute_auipc(Cpu *cpu, const struct Instruction *inst,
                         uxlen_t *next_pc) {
  registers_write(cpu_get_registers(cpu), inst->rd,
                  cpu_get_pc(cpu) + inst->imm);
  return retire(advance_pc_address(cpu), next_pc);
}

static int execute_jump_base(Cpu *cpu, const struct Instruction *inst,
                             uxlen_t target, uxlen_t *next_pc) {
  if (target % 4)
    return trap(cpu, TRAP_INSTRUCTION_ADDRESS_MISALIGNED, target, next_pc);
  registers_write(cpu_get_registers(cpu), inst->rd, advance_pc_address(cpu));
  return retire(target, next_pc);
}

static int execute_jal(Cpu *cpu, const struct Instruction *inst,
                       uxlen_t *next_pc) {
  return execute_jump_base(cpu, inst, cpu_get_pc(cpu) + inst->imm, next_pc);
}

static int execute_jalr(Cpu *cpu, const struct Instruction *inst,
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

static int execute_branch(Cpu *cpu, const struct Instruction *inst,
                          uxlen_t *next_pc) {
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
    return retire(advance_pc_address(cpu), next_pc);
  uxlen_t target = cpu_get_pc(cpu) + inst->imm;
  if (target % 4)
    return trap(cpu, TRAP_INSTRUCTION_ADDRESS_MISALIGNED, target, next_pc);
  return retire(target, next_pc);
}

static int execute_load(Cpu *cpu, const struct Instruction *inst,
                        uxlen_t *next_pc) {
  uxlen_t addr = registers_read(cpu_get_registers(cpu), inst->rs1) + inst->imm;
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
  uint8_t res = bus_read(cpu_get_bus(cpu), addr, size, &val);
  if (res != 0) {
    return trap(cpu, TRAP_LOAD_ACCESS_FAULT, addr, next_pc);
  }

  if (inst->funct3 == FUNCT3_LBU || inst->funct3 == FUNCT3_LHU ||
      inst->funct3 == FUNCT3_LWU || inst->funct3 == FUNCT3_LD)
    val = (uxlen_t)val;
  else
    val = sign_extend((uxlen_t)val, size * BITS_PER_BYTE);

  registers_write(cpu_get_registers(cpu), inst->rd, val);
  return retire(advance_pc_address(cpu), next_pc);
}

static int execute_store(Cpu *cpu, const struct Instruction *inst,
                         uxlen_t *next_pc) {
  uxlen_t addr = registers_read(cpu_get_registers(cpu), inst->rs1) + inst->imm;
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
  uint8_t res = bus_write(cpu_get_bus(cpu), addr, val, size);
  if (res != 0) {
    return trap(cpu, TRAP_STORE_ACCESS_FAULT, addr, next_pc);
  }
  return retire(advance_pc_address(cpu), next_pc);
}

static int execute_system(Cpu *cpu, const struct Instruction *inst,
                          uxlen_t *next_pc) {
  uint8_t privilege = cpu_get_privilege(cpu);
  if (inst->raw == TRAP_INSTRUCTION_ECALL)
    return trap(cpu, ECALL_BASE + privilege, 0, next_pc);
  else if (inst->raw == TRAP_INSTRUCTION_EBREAK)
    return trap(cpu, TRAP_BREAKPOINT, cpu_get_pc(cpu), next_pc);
  else if (privilege == PRIV_M && inst->raw == TRAP_INSTRUCTION_MRET)
    return retire(cpu_return_from_trap(cpu), next_pc);

  uint32_t csr_addr = (uint32_t)(inst->imm & 0xFFF);
  Csr *csr = cpu_get_csr(cpu);
  Registers *regs = cpu_get_registers(cpu);
  uxlen_t csr_val = 0;

  if (privilege < bits(csr_addr, 9, 8))
    return illegal_instruction(cpu, inst, next_pc);

  if (csr_read(csr, csr_addr, &csr_val) != CSR_OK) {
    return illegal_instruction(cpu, inst, next_pc);
  }
  uxlen_t rs1_val = registers_read(regs, inst->rs1);
  uxlen_t new_csr_val = csr_val;
  uint8_t need_write = 1;

  switch (inst->funct3) {
  case FUNCT3_CSRRW:
    new_csr_val = rs1_val;
    break;
  case FUNCT3_CSRRS:
    new_csr_val = csr_val | rs1_val;
    need_write = (inst->rs1 != 0);
    break;
  case FUNCT3_CSRRC:
    new_csr_val = csr_val & ~rs1_val;
    need_write = (inst->rs1 != 0);
    break;
  case FUNCT3_CSRRWI:
    new_csr_val = inst->rs1;
    break;
  case FUNCT3_CSRRSI:
    new_csr_val = csr_val | inst->rs1;
    need_write = (inst->rs1 != 0);
    break;
  case FUNCT3_CSRRCI:
    new_csr_val = csr_val & ~inst->rs1;
    need_write = (inst->rs1 != 0);
    break;
  default:
    return illegal_instruction(cpu, inst, next_pc);
  }
  if (need_write && csr_write(csr, csr_addr, new_csr_val) != CSR_OK) {
    return illegal_instruction(cpu, inst, next_pc);
  }
  registers_write(regs, inst->rd, csr_val);

  return retire(advance_pc_address(cpu), next_pc);
}

static int execute_fence(Cpu *cpu, const struct Instruction *inst,
                         uxlen_t *next_pc) {
  if (inst->funct3 != FUNCT3_FENCE && inst->funct3 != FUNCT3_FENCE_I) {
    return illegal_instruction(cpu, inst, next_pc);
  }
  return retire(advance_pc_address(cpu), next_pc);
}

int cpu_execute(Cpu *cpu, const struct Instruction *inst) {
  uxlen_t next_pc = 0;
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
  case OPCODE_LUI:
    status = execute_lui(cpu, inst, &next_pc);
    break;
  case OPCODE_AUIPC:
    status = execute_auipc(cpu, inst, &next_pc);
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
  case OPCODE_STORE:
    status = execute_store(cpu, inst, &next_pc);
    break;
  case OPCODE_SYSTEM:
    status = execute_system(cpu, inst, &next_pc);
    break;
  case OPCODE_FENCE:
    status = execute_fence(cpu, inst, &next_pc);
    break;
  default:
    status = illegal_instruction(cpu, inst, &next_pc);
    break;
  }
  cpu_set_pc(cpu, next_pc);
  return status;
}
