#include "riscv/hart.h"
#include "riscv/bus.h"
#include "riscv/csr.h"
#include "riscv/execute.h"
#include "riscv/trap.h"

void cpu_step(Cpu *cpu) {
  uxlen_t pc = cpu_get_pc(cpu);
  uint64_t instruction;
  int res =
      bus_read(cpu_get_bus(cpu), pc, INSTRUCTION_WIDTH_BYTES, &instruction);
  if (res != 0) {
    trap_raise(cpu_get_csr(cpu), cpu_get_pc(cpu), TRAP_INSTRUCTION_ACCESS_FAULT,
               pc);
    csr_read(cpu_get_csr(cpu), CSR_REG_MTVEC, &pc);
    cpu_set_pc(cpu, pc & ~0x1);
    return;
  }
  struct Instruction inst = instruction_decode(instruction);
  cpu_execute(cpu, &inst);
}
