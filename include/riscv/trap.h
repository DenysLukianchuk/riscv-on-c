#ifndef RISCV_TRAP_H
#define RISCV_TRAP_H

#include "riscv/cpu.h"
#include "riscv/csr.h"

enum Trap {
  TRAP_INSTRUCTION_ADDRESS_MISALIGNED = 0x0,
  TRAP_INSTRUCTION_ACCESS_FAULT = 0x1,
  TRAP_ILLEGAL_INSTRUCTION = 0x2,
  TRAP_BREAKPOINT = 0x3,
  TRAP_LOAD_ACCESS_FAULT = 0x5,
  TRAP_STORE_ACCESS_FAULT = 0x7,
};

#define TRAP_INSTRUCTION_ECALL 0x00000073
#define TRAP_INSTRUCTION_EBREAK 0x00100073
#define TRAP_INSTRUCTION_MRET 0x30200073

#define ECALL_BASE 0x08

uxlen_t trap_raise(Csr *csr, uxlen_t pc, uint8_t mpp, uxlen_t cause,
                   uxlen_t tval);
uxlen_t trap_return(Csr *csr);
uint8_t trap_get_mpp(Csr *csr);

uxlen_t cpu_raise_exception(Cpu *cpu, uxlen_t cause, uxlen_t tval);
uxlen_t cpu_return_from_trap(Cpu *cpu);

#endif // RISCV_TRAP_H
