#include "riscv/hart.h"
#include "riscv/bus.h"
#include "riscv/execute.h"
#include "riscv/trap.h"

void cpu_step(Cpu *cpu) {
  uxlen_t pc = cpu_get_pc(cpu);
  uint64_t instruction;
  int res =
      bus_read(cpu_get_bus(cpu), pc, INSTRUCTION_WIDTH_BYTES, &instruction);
  if (res != 0) {
    cpu_set_pc(cpu,
               cpu_raise_exception(cpu, TRAP_INSTRUCTION_ACCESS_FAULT, pc));
    return;
  }
  struct Instruction inst = instruction_decode(instruction);
  if (cpu_execute(cpu, &inst) == EXECUTE_OK)
    csr_count_instret(cpu_get_csr(cpu));
  csr_count_cycle(cpu_get_csr(cpu));
}
