#include "riscv/cpu/registers.h"
#include <assert.h>
#include <stdlib.h>

struct Registers {
  uxlen_t regs[REG_COUNT];
};

Registers *registers_create(void) {
  Registers *regs = (Registers *)malloc(sizeof(Registers));
  registers_reset(regs);
  return regs;
}

void registers_reset(Registers *regs) {
  for (int i = 0; i < REG_COUNT; i++) {
    regs->regs[i] = 0;
  }
}

void registers_destroy(Registers *regs) { free(regs); }

void registers_write(Registers *regs, uint8_t index, uxlen_t value) {
  assert(index < REG_COUNT);
  if (index == 0) {
    return;
  }
  regs->regs[index] = value;
}

uxlen_t registers_read(Registers *regs, uint8_t index) {
  assert(index < REG_COUNT);
  return regs->regs[index];
}
