#include "riscv/cpu/fp_registers.h"
#include <assert.h>
#include <stdlib.h>

struct FpRegisters {
  uint64_t regs[FP_REG_COUNT];
};

FpRegisters *fp_registers_create(void) {
  FpRegisters *regs = (FpRegisters *)malloc(sizeof(FpRegisters));
  fp_registers_reset(regs);
  return regs;
}

void fp_registers_reset(FpRegisters *regs) {
  for (int i = 0; i < FP_REG_COUNT; i++) {
    regs->regs[i] = 0;
  }
}

void fp_registers_destroy(FpRegisters *regs) { free(regs); }

void fp_registers_write(FpRegisters *regs, uint8_t index, uint64_t value) {
  assert(index < FP_REG_COUNT);
  regs->regs[index] = value;
}

uint64_t fp_registers_read(FpRegisters *regs, uint8_t index) {
  assert(index < FP_REG_COUNT);
  return regs->regs[index];
}
