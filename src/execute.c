#include "riscv/execute.h"
#include "riscv/bits.h"
#include "riscv/trap.h"
#include <stdbool.h>

static inline uxlen_t advance_pc_address(Cpu *cpu) {
  return cpu_get_pc(cpu) + PC_INCREMENT;
}

static uxlen_t raise_exception(Cpu *cpu, uxlen_t cause, uxlen_t tval) {
  return trap_raise(cpu_get_csr(cpu), cpu_get_pc(cpu), cause, tval);
}

static uxlen_t return_exception(Cpu *cpu) {
  return trap_return(cpu_get_csr(cpu));
}

static uxlen_t execute_illegal_instruction(Cpu *cpu, uxlen_t tval) {
  return raise_exception(cpu, TRAP_ILLEGAL_INSTRUCTION, tval);
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

static bool has_w_form(uint8_t funct3) {
  return funct3 == FUNCT3_ADD_SUB || funct3 == FUNCT3_SLL ||
         funct3 == FUNCT3_SRL_SRA;
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

static uxlen_t execute_op_imm(Cpu *cpu, const struct Instruction *inst) {
  if (!shift_imm_valid(inst->funct3, inst->imm, SHAMT_LEN))
    return execute_illegal_instruction(cpu, inst->raw);

  Registers *regs = cpu_get_registers(cpu);
  uxlen_t rs1_val = registers_read(regs, inst->rs1);
  uxlen_t result =
      alu(inst->funct3, op_imm_alternate(inst), rs1_val, inst->imm);
  registers_write(regs, inst->rd, result);
  return advance_pc_address(cpu);
}

#if XLEN == 64
static uxlen_t execute_op_imm_32(Cpu *cpu, const struct Instruction *inst) {
  if (!has_w_form(inst->funct3) ||
      !shift_imm_valid(inst->funct3, inst->imm, SHAMT_LEN_32))
    return execute_illegal_instruction(cpu, inst->raw);

  uint32_t rs1_val = registers_read(cpu_get_registers(cpu), inst->rs1);
  write_w_result(
      cpu, inst->rd,
      alu_32(inst->funct3, op_imm_alternate(inst), rs1_val, inst->imm));
  return advance_pc_address(cpu);
}
#endif

static bool op_funct7_valid(uint8_t funct3, uint8_t funct7) {
  switch (funct7) {
  case FUNCT7_ADD:
    return true;
  case FUNCT7_SUB:
    return funct3 == FUNCT3_ADD_SUB || funct3 == FUNCT3_SRL_SRA;
  default:
    return false;
  }
}

static uxlen_t execute_op(Cpu *cpu, const struct Instruction *inst) {
  if (!op_funct7_valid(inst->funct3, inst->funct7))
    return execute_illegal_instruction(cpu, inst->raw);

  Registers *regs = cpu_get_registers(cpu);
  uxlen_t rs1_val = registers_read(regs, inst->rs1);
  uxlen_t rs2_val = registers_read(regs, inst->rs2);
  uxlen_t result =
      alu(inst->funct3, inst->funct7 == FUNCT7_SUB, rs1_val, rs2_val);
  registers_write(regs, inst->rd, result);
  return advance_pc_address(cpu);
}

#if XLEN == 64
static uxlen_t execute_op_32(Cpu *cpu, const struct Instruction *inst) {
  if (!has_w_form(inst->funct3) || !op_funct7_valid(inst->funct3, inst->funct7))
    return execute_illegal_instruction(cpu, inst->raw);

  Registers *regs = cpu_get_registers(cpu);
  uint32_t rs1_val = registers_read(regs, inst->rs1);
  uint32_t rs2_val = registers_read(regs, inst->rs2);
  write_w_result(
      cpu, inst->rd,
      alu_32(inst->funct3, inst->funct7 == FUNCT7_SUB, rs1_val, rs2_val));
  return advance_pc_address(cpu);
}
#endif

static uxlen_t execute_lui(Cpu *cpu, const struct Instruction *inst) {
  registers_write(cpu_get_registers(cpu), inst->rd, inst->imm);
  return advance_pc_address(cpu);
}

static uxlen_t execute_auipc(Cpu *cpu, const struct Instruction *inst) {
  registers_write(cpu_get_registers(cpu), inst->rd,
                  cpu_get_pc(cpu) + inst->imm);
  return advance_pc_address(cpu);
}

static uxlen_t execute_jump_base(Cpu *cpu, const struct Instruction *inst,
                                 uxlen_t target) {
  if (target % 4) {
    return raise_exception(cpu, TRAP_INSTRUCTION_ADDRESS_MISALIGNED, target);
  }
  uxlen_t next_pc = cpu_get_pc(cpu) + PC_INCREMENT;
  registers_write(cpu_get_registers(cpu), inst->rd, next_pc);
  return target;
}

static uxlen_t execute_jal(Cpu *cpu, const struct Instruction *inst) {
  return execute_jump_base(cpu, inst, cpu_get_pc(cpu) + inst->imm);
}

static uxlen_t execute_jalr(Cpu *cpu, const struct Instruction *inst) {
  if (inst->funct3 != FUNCT3_JALR) {
    return raise_exception(cpu, TRAP_ILLEGAL_INSTRUCTION, inst->raw);
  }
  return execute_jump_base(
      cpu, inst,
      clear_bits(registers_read(cpu_get_registers(cpu), inst->rs1) + inst->imm,
                 0, 0));
}

static uxlen_t execute_branch(Cpu *cpu, const struct Instruction *inst) {
  uxlen_t rs1_val = registers_read(cpu_get_registers(cpu), inst->rs1);
  uxlen_t rs2_val = registers_read(cpu_get_registers(cpu), inst->rs2);
  uxlen_t next_pc = advance_pc_address(cpu);
  switch (inst->funct3) {
  case FUNCT3_BEQ:
    if (rs1_val == rs2_val)
      next_pc = next_pc - PC_INCREMENT + inst->imm;
    break;
  case FUNCT3_BNE:
    if (rs1_val != rs2_val)
      next_pc = next_pc - PC_INCREMENT + inst->imm;
    break;
  case FUNCT3_BLT:
    if ((sxlen_t)rs1_val < (sxlen_t)rs2_val)
      next_pc = next_pc - PC_INCREMENT + inst->imm;
    break;
  case FUNCT3_BGE:
    if ((sxlen_t)rs1_val >= (sxlen_t)rs2_val)
      next_pc = next_pc - PC_INCREMENT + inst->imm;
    break;
  case FUNCT3_BLTU:
    if (rs1_val < rs2_val)
      next_pc = next_pc - PC_INCREMENT + inst->imm;
    break;
  case FUNCT3_BGEU:
    if (rs1_val >= rs2_val)
      next_pc = next_pc - PC_INCREMENT + inst->imm;
    break;
  default:
    next_pc = execute_illegal_instruction(cpu, inst->raw);
  }
  if (next_pc % 4) {
    return raise_exception(cpu, TRAP_INSTRUCTION_ADDRESS_MISALIGNED, next_pc);
  }
  return next_pc;
}

static uxlen_t execute_load(Cpu *cpu, const struct Instruction *inst) {
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
    return raise_exception(cpu, TRAP_ILLEGAL_INSTRUCTION, inst->raw);
  }
  uint64_t val;
  uint8_t res = bus_read(cpu_get_bus(cpu), addr, size, &val);
  if (res != 0) {
    return trap_raise(cpu_get_csr(cpu), cpu_get_pc(cpu), TRAP_LOAD_ACCESS_FAULT,
                      addr);
  }

  if (inst->funct3 == FUNCT3_LBU || inst->funct3 == FUNCT3_LHU ||
      inst->funct3 == FUNCT3_LWU || inst->funct3 == FUNCT3_LD)
    val = (uxlen_t)val;
  else
    val = sign_extend((uxlen_t)val, size * BITS_PER_BYTE);

  registers_write(cpu_get_registers(cpu), inst->rd, val);
  return advance_pc_address(cpu);
}

static uxlen_t execute_store(Cpu *cpu, const struct Instruction *inst) {
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
    return raise_exception(cpu, TRAP_ILLEGAL_INSTRUCTION, inst->raw);
  }
  uint64_t val = registers_read(cpu_get_registers(cpu), inst->rs2);
  uint8_t res = bus_write(cpu_get_bus(cpu), addr, val, size);
  if (res != 0) {
    return trap_raise(cpu_get_csr(cpu), cpu_get_pc(cpu),
                      TRAP_STORE_ACCESS_FAULT, addr);
  }
  return advance_pc_address(cpu);
}

static uxlen_t execute_system(Cpu *cpu, const struct Instruction *inst) {
  if (inst->raw == TRAP_INSTRUCTION_ECALL)
    return raise_exception(cpu, TRAP_ECALL, 0);
  else if (inst->raw == TRAP_INSTRUCTION_EBREAK)
    return raise_exception(cpu, TRAP_BREAKPOINT, cpu_get_pc(cpu));
  else if (inst->raw == TRAP_INSTRUCTION_MRET)
    return return_exception(cpu);

  uint32_t csr_addr = (uint32_t)(inst->imm & 0xFFF);
  Csr *csr = cpu_get_csr(cpu);
  Registers *regs = cpu_get_registers(cpu);
  uxlen_t csr_val = 0;
  if (csr_read(csr, csr_addr, &csr_val) != CSR_OK) {
    return raise_exception(cpu, TRAP_ILLEGAL_INSTRUCTION, inst->raw);
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
    return raise_exception(cpu, TRAP_ILLEGAL_INSTRUCTION, inst->raw);
  }
  if (need_write && csr_write(csr, csr_addr, new_csr_val) != CSR_OK) {
    return raise_exception(cpu, TRAP_ILLEGAL_INSTRUCTION, inst->raw);
  }
  registers_write(regs, inst->rd, csr_val);

  return advance_pc_address(cpu);
}

static uxlen_t execute_fence(Cpu *cpu, const struct Instruction *inst) {
  if (inst->funct3 != FUNCT3_FENCE && inst->funct3 != FUNCT3_FENCE_I) {
    return raise_exception(cpu, TRAP_ILLEGAL_INSTRUCTION, inst->raw);
  }
  return advance_pc_address(cpu);
}

void cpu_execute(Cpu *cpu, const struct Instruction *inst) {
  uxlen_t pc;
  switch (inst->opcode) {
  case OPCODE_OP_IMM:
    pc = execute_op_imm(cpu, inst);
    break;
#if XLEN == 64
  case OPCODE_OP_IMM_32:
    pc = execute_op_imm_32(cpu, inst);
    break;
  case OPCODE_OP_32:
    pc = execute_op_32(cpu, inst);
    break;
#endif
  case OPCODE_OP:
    pc = execute_op(cpu, inst);
    break;
  case OPCODE_LUI:
    pc = execute_lui(cpu, inst);
    break;
  case OPCODE_AUIPC:
    pc = execute_auipc(cpu, inst);
    break;
  case OPCODE_JAL:
    pc = execute_jal(cpu, inst);
    break;
  case OPCODE_JALR:
    pc = execute_jalr(cpu, inst);
    break;
  case OPCODE_BRANCH:
    pc = execute_branch(cpu, inst);
    break;
  case OPCODE_LOAD:
    pc = execute_load(cpu, inst);
    break;
  case OPCODE_STORE:
    pc = execute_store(cpu, inst);
    break;
  case OPCODE_SYSTEM:
    pc = execute_system(cpu, inst);
    break;
  case OPCODE_FENCE:
    pc = execute_fence(cpu, inst);
    break;
  default:
    pc = raise_exception(cpu, TRAP_ILLEGAL_INSTRUCTION, inst->raw);
    break;
  }
  cpu_set_pc(cpu, pc);
}
