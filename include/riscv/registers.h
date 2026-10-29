#ifndef RISCV_REGISTERS_H
#define RISCV_REGISTERS_H

#include "riscv/xlen.h"

#define REG_COUNT 32

#define WIDTH XLEN

typedef struct Registers Registers;

Registers *registers_create(void);

void registers_destroy(Registers *regs);

void registers_write(Registers *regs, uint8_t index, uxlen_t value);

uxlen_t registers_read(Registers *regs, uint8_t index);

void registers_reset(Registers *regs);

#endif // RISCV_REGISTERS_H
