#ifndef RISCV_CPU_H
#define RISCV_CPU_H

#include "riscv/bus.h"
#include "riscv/csr.h"
#include "riscv/privilege.h"
#include "riscv/registers.h"

typedef struct Cpu Cpu;

#define PC_INCREMENT 4

Cpu *cpu_create(Bus *bus);
Registers *cpu_get_registers(Cpu *cpu);
uxlen_t cpu_get_pc(Cpu *cpu);
void cpu_set_pc(Cpu *cpu, uxlen_t pc);
Bus *cpu_get_bus(Cpu *cpu);
Csr *cpu_get_csr(Cpu *cpu);
enum Privilege cpu_get_privilege(Cpu *cpu);
void cpu_set_privilege(Cpu *cpu, enum Privilege privilege);
uxlen_t cpu_get_amo_reservation(Cpu *cpu);
void cpu_set_amo_reservation(Cpu *cpu, uxlen_t amo_reservation);
void cpu_destroy(Cpu *cpu);
void cpu_reset(Cpu *cpu);

#endif // RISCV_CPU_H
