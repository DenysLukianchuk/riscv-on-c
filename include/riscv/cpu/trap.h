#ifndef RISCV_CPU_TRAP_H
#define RISCV_CPU_TRAP_H

#include "riscv/cpu/cpu.h"
#include "riscv/cpu/csr.h"
#include "riscv/cpu/trap_cause.h"

uxlen_t cpu_raise_exception(Cpu *cpu, uxlen_t cause, uxlen_t tval);
uxlen_t cpu_return_from_trap(Cpu *cpu, enum Privilege xret);
uxlen_t cpu_raise_interrupt(Cpu *cpu, unsigned code);

#endif // RISCV_CPU_TRAP_H
