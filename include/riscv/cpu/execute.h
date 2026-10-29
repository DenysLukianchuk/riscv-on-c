#ifndef RISCV_CPU_EXECUTE_H
#define RISCV_CPU_EXECUTE_H

#include "riscv/cpu/cpu.h"
#include "riscv/cpu/decoder.h"

enum ExecuteResult { EXECUTE_OK, EXECUTE_TRAP };

enum ExecuteResult cpu_execute(Cpu *cpu, const struct Instruction *inst);

#endif // RISCV_CPU_EXECUTE_H
