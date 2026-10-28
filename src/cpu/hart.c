#include "riscv/hart.h"
#include "riscv/bus.h"
#include "riscv/execute.h"
#include "riscv/trap.h"
#include <stddef.h>

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
  uint64_t instruction;
  int res =
      bus_read(cpu_get_bus(cpu), pc, INSTRUCTION_WIDTH_BYTES, &instruction);
  if (res != 0) {
    cpu_set_pc(cpu,
               cpu_raise_exception(cpu, TRAP_INSTRUCTION_ACCESS_FAULT, pc));
    return;
  }
  uxlen_t data = (uxlen_t)instruction;
  if (csr_breakpoint_on_execute(csr, privilege, pc, &data, &tval)) {
    cpu_set_pc(cpu, cpu_raise_exception(cpu, TRAP_BREAKPOINT, tval));
    return;
  }
  struct Instruction inst = instruction_decode(instruction);
  csr_begin_instruction(csr);
  int execute_result = cpu_execute(cpu, &inst);
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
