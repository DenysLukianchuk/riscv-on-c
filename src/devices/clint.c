#include "riscv/devices/clint.h"
#include <stdlib.h>

#define CLINT_MSIP 0x0000
#define CLINT_MTIMECMP 0x4000
#define CLINT_MTIME 0xBFF8
#define CLINT_SIZE 0x10000

#define LOW_HALF 0xFFFFFFFFull
#define WIDE_SIZE 8
#define HALF_SIZE 4

struct Clint {
  uint32_t msip;
  uint64_t mtimecmp;
  uint64_t mtime;
};

Clint *clint_create(void) {
  Clint *clint = (Clint *)malloc(sizeof(Clint));
  clint->msip = 0;
  // No timer interrupt until software sets mtimecmp.
  clint->mtimecmp = UINT64_MAX;
  clint->mtime = 0;
  return clint;
}

void clint_destroy(Clint *clint) { free(clint); }

static bool inside(uxlen_t offset, uxlen_t base) {
  return offset >= base && offset < base + WIDE_SIZE;
}

// A 64-bit register is reached whole or as two aligned 32-bit halves; at is
// the offset inside the register.
static enum BusStatus read_wide(uint64_t reg, uxlen_t at, unsigned size,
                                uint64_t *value) {
  if (at == 0 && size == WIDE_SIZE)
    *value = reg;
  else if (at == 0 && size == HALF_SIZE)
    *value = reg & LOW_HALF;
  else if (at == HALF_SIZE && size == HALF_SIZE)
    *value = reg >> 32;
  else
    return BUS_ERROR;
  return BUS_OK;
}

static enum BusStatus write_wide(uint64_t *reg, uxlen_t at, unsigned size,
                                 uint64_t value) {
  if (at == 0 && size == WIDE_SIZE)
    *reg = value;
  else if (at == 0 && size == HALF_SIZE)
    *reg = (*reg & ~LOW_HALF) | (value & LOW_HALF);
  else if (at == HALF_SIZE && size == HALF_SIZE)
    *reg = (*reg & LOW_HALF) | ((value & LOW_HALF) << 32);
  else
    return BUS_ERROR;
  return BUS_OK;
}

enum BusStatus clint_read(Clint *clint, uxlen_t offset, unsigned size,
                          uint64_t *value) {
  if (offset == CLINT_MSIP && size == HALF_SIZE) {
    *value = clint->msip;
    return BUS_OK;
  }
  if (inside(offset, CLINT_MTIMECMP))
    return read_wide(clint->mtimecmp, offset - CLINT_MTIMECMP, size, value);
  if (inside(offset, CLINT_MTIME))
    return read_wide(clint->mtime, offset - CLINT_MTIME, size, value);
  return BUS_ERROR;
}

enum BusStatus clint_write(Clint *clint, uxlen_t offset, unsigned size,
                           uint64_t value) {
  if (offset == CLINT_MSIP && size == HALF_SIZE) {
    clint->msip = value & 1;
    return BUS_OK;
  }
  if (inside(offset, CLINT_MTIMECMP))
    return write_wide(&clint->mtimecmp, offset - CLINT_MTIMECMP, size, value);
  if (inside(offset, CLINT_MTIME))
    return write_wide(&clint->mtime, offset - CLINT_MTIME, size, value);
  return BUS_ERROR;
}

void clint_tick(Clint *clint, uint64_t ticks) { clint->mtime += ticks; }

bool clint_timer_pending(const Clint *clint) {
  return clint->mtime >= clint->mtimecmp;
}

bool clint_software_pending(const Clint *clint) { return clint->msip != 0; }

static enum BusStatus bus_read(void *clint, uxlen_t offset, unsigned size,
                               uint64_t *value) {
  return clint_read(clint, offset, size, value);
}

static enum BusStatus bus_write(void *clint, uxlen_t offset, unsigned size,
                                uint64_t value) {
  return clint_write(clint, offset, size, value);
}

struct BusDevice clint_bus_device(Clint *clint, uxlen_t base) {
  return (struct BusDevice){.base = base,
                            .size = CLINT_SIZE,
                            .device = clint,
                            .read = bus_read,
                            .write = bus_write};
}
