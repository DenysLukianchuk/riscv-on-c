#include "riscv/cpu.h"
#include "riscv/execute.h"
#include <stdlib.h>

struct Cpu {
  Registers *regs;
  uxlen_t pc;
  Bus *bus;
  Csr *csr;
  enum Privilege privilege;
};

Cpu *cpu_create(Bus *bus) {
  Cpu *cpu = (Cpu *)malloc(sizeof(Cpu));
  cpu->regs = registers_create();
  cpu->pc = 0;
  cpu->bus = bus;
  cpu->csr = csr_create();
  cpu->privilege = PRIV_M;
  return cpu;
}

void cpu_destroy(Cpu *cpu) {
  registers_destroy(cpu->regs);
  csr_destroy(cpu->csr);
  free(cpu);
}

Registers *cpu_get_registers(Cpu *cpu) { return cpu->regs; }
uxlen_t cpu_get_pc(Cpu *cpu) { return cpu->pc; }
void cpu_set_pc(Cpu *cpu, uxlen_t pc) { cpu->pc = pc; }
Bus *cpu_get_bus(Cpu *cpu) { return cpu->bus; }
Csr *cpu_get_csr(Cpu *cpu) { return cpu->csr; }
enum Privilege cpu_get_privilege(Cpu *cpu) { return cpu->privilege; }
void cpu_set_privilege(Cpu *cpu, enum Privilege privilege) {
  cpu->privilege = privilege;
}
