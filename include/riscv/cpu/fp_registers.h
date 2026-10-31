#ifndef RISCV_CPU_FP_REGISTERS_H
#define RISCV_CPU_FP_REGISTERS_H

#include <stdint.h>

#define FP_REG_COUNT 32

typedef struct FpRegisters FpRegisters;

FpRegisters *fp_registers_create(void);

void fp_registers_destroy(FpRegisters *regs);

void fp_registers_write(FpRegisters *regs, uint8_t index, uint64_t value);

uint64_t fp_registers_read(FpRegisters *regs, uint8_t index);

void fp_registers_reset(FpRegisters *regs);

#endif // RISCV_CPU_FP_REGISTERS_H
