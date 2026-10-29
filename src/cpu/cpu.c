#include "riscv/cpu/cpu.h"
#include "riscv/cpu/execute.h"
#include <stdlib.h>

struct Cpu {
  Registers *regs;
  uxlen_t pc;
  Bus *bus;
  Csr *csr;
  enum Privilege privilege;
  uxlen_t amo_reservation;
};

Cpu *cpu_create(Bus *bus) {
  Cpu *cpu = (Cpu *)malloc(sizeof(Cpu));
  cpu->regs = registers_create();
  cpu->pc = 0;
  cpu->bus = bus;
  cpu->csr = csr_create();
  cpu->privilege = PRIV_M;
  cpu->amo_reservation = 0;
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
uxlen_t cpu_get_amo_reservation(Cpu *cpu) { return cpu->amo_reservation; }
void cpu_set_amo_reservation(Cpu *cpu, uxlen_t amo_reservation) {
  cpu->amo_reservation = amo_reservation;
}

void cpu_reset(Cpu *cpu) {
  cpu->pc = 0;
  cpu->privilege = PRIV_M;
  cpu->amo_reservation = 0;
  registers_reset(cpu->regs);
  csr_reset(cpu->csr);
}
