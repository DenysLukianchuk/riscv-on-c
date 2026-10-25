#ifndef RISCV_EXECUTE_H
#define RISCV_EXECUTE_H

#include "riscv/cpu.h"
#include "riscv/decoder.h"

#define EXECUTE_OK 0
#define EXECUTE_TRAP 1

int cpu_execute(Cpu *cpu, const struct Instruction *inst);

#endif // RISCV_EXECUTE_H
