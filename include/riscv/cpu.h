#ifndef RISCV_CPU_H
#define RISCV_CPU_H

#include "riscv/bus.h"
#include "riscv/csr.h"
#include "riscv/registers.h"

typedef struct Cpu Cpu;

#define PC_INCREMENT 4

enum Privilege { PRIV_U = 0, PRIV_S = 1, PRIV_M = 3 };

Cpu *cpu_create(Bus *bus);
Registers *cpu_get_registers(Cpu *cpu);
uxlen_t cpu_get_pc(Cpu *cpu);
void cpu_set_pc(Cpu *cpu, uxlen_t pc);
Bus *cpu_get_bus(Cpu *cpu);
Csr *cpu_get_csr(Cpu *cpu);
enum Privilege cpu_get_privilege(Cpu *cpu);
void cpu_set_privilege(Cpu *cpu, enum Privilege privilege);
void cpu_destroy(Cpu *cpu);

#endif // RISCV_CPU_H
