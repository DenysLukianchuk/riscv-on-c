#include "riscv/cpu/execute.h"
#include "riscv/cpu/mmu.h"
#include "riscv/cpu/trap.h"
#include "riscv/util/bits.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#define PC_INCREMENT 4

#define INSTRUCTION_ECALL 0x00000073
#define INSTRUCTION_EBREAK 0x00100073
#define INSTRUCTION_MRET 0x30200073
#define INSTRUCTION_SRET 0x10200073
#define INSTRUCTION_WFI 0x10500073

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
  uint64_t phys_addr;
  enum MmuResult mmu_res = mmu_translate(cpu, addr, access, &phys_addr);
  if (mmu_res != MMU_OK) {
    return trap(cpu, mmu_fault_cause(mmu_res, access), addr, next_pc);
  }
  *paddr = phys_addr;
  return EXECUTE_OK;
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
  if (target % 4)
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
  if (target % 4)
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

static enum ExecuteResult execute_load(Cpu *cpu, const struct Instruction *inst,
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
  uxlen_t tval;
  if (load_trigger_hit(cpu, size, addr, NULL, &tval)) {
    return trap(cpu, TRAP_BREAKPOINT, tval, next_pc);
  }

  uint64_t phys_addr;
  enum ExecuteResult translate_result =
      translate(cpu, addr, MMU_LOAD, &phys_addr, next_pc);
  if (translate_result != EXECUTE_OK)
    return translate_result;

  uint64_t val;
  enum BusStatus res = bus_read(cpu_get_bus(cpu), phys_addr, size, &val);
  if (res != BUS_OK) {
    return trap(cpu, TRAP_LOAD_ACCESS_FAULT, addr, next_pc);
  }

  uxlen_t data = (uxlen_t)val;
  if (load_trigger_hit(cpu, size, addr, &data, &tval))
    return trap(cpu, TRAP_BREAKPOINT, tval, next_pc);

  if (inst->funct3 == FUNCT3_LBU || inst->funct3 == FUNCT3_LHU ||
      inst->funct3 == FUNCT3_LWU || inst->funct3 == FUNCT3_LD)
    val = (uxlen_t)val;
  else
    val = sign_extend((uxlen_t)val, size * BITS_PER_BYTE);

  registers_write(cpu_get_registers(cpu), inst->rd, val);
  return EXECUTE_OK;
}

static enum ExecuteResult
execute_store(Cpu *cpu, const struct Instruction *inst, uxlen_t *next_pc) {
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
  uxlen_t tval;
  if (store_trigger_hit(cpu, size, addr, NULL, &tval)) {
    return trap(cpu, TRAP_BREAKPOINT, tval, next_pc);
  }
  uint64_t val = registers_read(cpu_get_registers(cpu), inst->rs2);
  uxlen_t data = truncate_to_size(val, size);
  if (store_trigger_hit(cpu, size, addr, &data, &tval)) {
    return trap(cpu, TRAP_BREAKPOINT, tval, next_pc);
  }

  uint64_t phys_addr;
  enum ExecuteResult translate_result =
      translate(cpu, addr, MMU_STORE, &phys_addr, next_pc);
  if (translate_result != EXECUTE_OK)
    return translate_result;

  enum BusStatus res = bus_write(cpu_get_bus(cpu), phys_addr, size, val);
  if (res != BUS_OK) {
    return trap(cpu, TRAP_STORE_ACCESS_FAULT, addr, next_pc);
  }
  return EXECUTE_OK;
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

static enum ExecuteResult
execute_system(Cpu *cpu, const struct Instruction *inst, uxlen_t *next_pc) {
  enum Privilege privilege = cpu_get_privilege(cpu);
  if (inst->raw == INSTRUCTION_ECALL) {
    return trap(cpu, TRAP_ECALL_FROM_U + privilege, 0, next_pc);
  } else if (inst->raw == INSTRUCTION_EBREAK) {
    return trap(cpu, TRAP_BREAKPOINT, cpu_get_pc(cpu), next_pc);
  } else if (privilege >= PRIV_M && inst->raw == INSTRUCTION_MRET) {
    *next_pc = cpu_return_from_trap(cpu, PRIV_M);
    return EXECUTE_OK;
  } else if (privilege >= PRIV_S && inst->raw == INSTRUCTION_SRET) {
    *next_pc = cpu_return_from_trap(cpu, PRIV_S);
    return EXECUTE_OK;
  } else if (inst->raw == INSTRUCTION_WFI) {
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
    return trap(cpu, TRAP_LOAD_ACCESS_FAULT, rs1_addr, next_pc);

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
    return trap(cpu, TRAP_STORE_ACCESS_FAULT, rs1_addr, next_pc);

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
  uint8_t funct5 = bits(inst->funct7, 6, 2);
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
    return trap(cpu, TRAP_STORE_ACCESS_FAULT, addr, next_pc);

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
    return trap(cpu, TRAP_STORE_ACCESS_FAULT, addr, next_pc);

  registers_write(regs, inst->rd, (uxlen_t)old);
  return EXECUTE_OK;
}

enum ExecuteResult cpu_execute(Cpu *cpu, const struct Instruction *inst) {
  uxlen_t next_pc = cpu_get_pc(cpu) + PC_INCREMENT;
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
  case OPCODE_STORE:
    status = execute_store(cpu, inst, &next_pc);
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
