#include "riscv/cpu/hart.h"
#include "riscv/cpu/execute.h"
#include "riscv/cpu/mmu.h"
#include "riscv/cpu/trap.h"
#include "riscv/system/bus.h"
#include "riscv/util/bits.h"
#include <stddef.h>

static bool fetch_half(Cpu *cpu, uxlen_t vaddr, uint16_t *half) {
  uint64_t phys_addr;
  enum MmuResult mmu_res = mmu_translate(cpu, vaddr, MMU_FETCH, &phys_addr);
  if (mmu_res != MMU_OK) {
    cpu_set_pc(cpu, cpu_raise_exception(
                        cpu, mmu_fault_cause(mmu_res, MMU_FETCH), vaddr));
    return false;
  }

  uint64_t read_val;
  if (bus_read(cpu_get_bus(cpu), phys_addr, COMPRESSED_WIDTH_BYTES,
               &read_val) != BUS_OK) {
    cpu_set_pc(cpu,
               cpu_raise_exception(cpu, TRAP_INSTRUCTION_ACCESS_FAULT, vaddr));
    return false;
  }
  *half = (uint16_t)read_val;
  return true;
}

static bool fetch_word(Cpu *cpu, uxlen_t pc, uint32_t *instruction) {
  if (mmu_bytes_to_page_end(pc) < INSTRUCTION_WIDTH_BYTES)
    return false;
  uint64_t phys_addr;
  if (mmu_translate(cpu, pc, MMU_FETCH, &phys_addr) != MMU_OK)
    return false;
  uint64_t read_val;
  if (bus_read(cpu_get_bus(cpu), phys_addr, INSTRUCTION_WIDTH_BYTES,
               &read_val) != BUS_OK)
    return false;
  *instruction = (uint32_t)read_val;
  return true;
}

static bool fetch_halves(Cpu *cpu, uxlen_t pc, uint32_t *instruction) {
  uint16_t low;
  if (!fetch_half(cpu, pc, &low))
    return false;
  *instruction = low;
  if (instruction_is_compressed(low))
    return true;
  uint16_t high;
  if (!fetch_half(cpu, pc + COMPRESSED_WIDTH_BYTES, &high))
    return false;
  *instruction |= (uint32_t)high << (COMPRESSED_WIDTH_BYTES * BITS_PER_BYTE);
  return true;
}

static bool fetch(Cpu *cpu, uxlen_t pc, uint32_t *instruction) {
  if (!fetch_word(cpu, pc, instruction))
    return fetch_halves(cpu, pc, instruction);
  if (instruction_is_compressed(*instruction))
    *instruction = (uint16_t)*instruction;
  return true;
}

static void step_instruction(Cpu *cpu) {
  uxlen_t pc = cpu_get_pc(cpu);
  Csr *csr = cpu_get_csr(cpu);
  enum Privilege privilege = cpu_get_privilege(cpu);
  uxlen_t tval;

  unsigned code;
  if (csr_pending_interrupt(csr, privilege, &code)) {
    cpu_set_pc(cpu, cpu_raise_interrupt(cpu, code));
    return;
  }

  if (csr_breakpoint_on_execute(csr, privilege, pc, NULL, &tval)) {
    cpu_set_pc(cpu, cpu_raise_exception(cpu, TRAP_BREAKPOINT, tval));
    return;
  }

  uint32_t instruction;
  if (!fetch(cpu, pc, &instruction))
    return;

  uxlen_t data = (uxlen_t)instruction;
  if (csr_breakpoint_on_execute(csr, privilege, pc, &data, &tval)) {
    cpu_set_pc(cpu, cpu_raise_exception(cpu, TRAP_BREAKPOINT, tval));
    return;
  }
  struct Instruction inst = instruction_decode(instruction);
  csr_begin_instruction(csr);
  enum ExecuteResult execute_result = cpu_execute(cpu, &inst);
  csr_end_instruction(csr, execute_result == EXECUTE_OK);
}

void cpu_step(Cpu *cpu) {
  Csr *csr = cpu_get_csr(cpu);
  enum Privilege privilege = cpu_get_privilege(cpu);
  bool counted = csr_breakpoints_enabled(csr, privilege);
  if (csr_breakpoint_on_icount(csr, privilege))
    cpu_set_pc(cpu, cpu_raise_exception(cpu, TRAP_BREAKPOINT, 0));
  else
    step_instruction(cpu);
  if (counted)
    csr_count_icount(csr, privilege);
}
