#ifndef RISCV_EXECUTE_H
#define RISCV_EXECUTE_H

#include "riscv/cpu.h"
#include "riscv/decoder.h"

void cpu_execute(Cpu *cpu, const struct Instruction *inst);

#endif // RISCV_EXECUTE_H
