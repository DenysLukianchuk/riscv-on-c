#ifndef RISCV_TRAP_H
#define RISCV_TRAP_H

#include "riscv/cpu.h"
#include "riscv/csr.h"
#include "riscv/trap_cause.h"

#define TRAP_INSTRUCTION_ECALL 0x00000073
#define TRAP_INSTRUCTION_EBREAK 0x00100073
#define TRAP_INSTRUCTION_MRET 0x30200073
#define TRAP_INSTRUCTION_SRET 0x10200073
#define TRAP_INSTRUCTION_WFI 0x10500073

uxlen_t cpu_raise_exception(Cpu *cpu, uxlen_t cause, uxlen_t tval);
uxlen_t cpu_return_from_trap(Cpu *cpu, enum Privilege xret);
uxlen_t cpu_raise_interrupt(Cpu *cpu, unsigned code);

#endif // RISCV_TRAP_H
