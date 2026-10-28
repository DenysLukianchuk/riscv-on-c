#include "riscv/finisher.h"
#include <stdlib.h>

// QEMU's sifive_test: one write-only word. Its low half says what to do, and
// on a failure its high half carries the exit code.
#define FINISHER_PASS 0x5555
#define FINISHER_FAIL 0x3333
#define STATUS_MASK 0xFFFF
#define CODE_SHIFT 16
#define REGISTER_SIZE 4

#define FINISHER_SIZE 0x1000

struct Finisher {
  bool finished;
  int exit_code;
};

Finisher *finisher_create(void) {
  Finisher *finisher = (Finisher *)malloc(sizeof(Finisher));
  *finisher = (Finisher){0};
  return finisher;
}

void finisher_destroy(Finisher *finisher) { free(finisher); }

enum BusStatus finisher_read(Finisher *finisher, uxlen_t offset, unsigned size,
                             uint64_t *value) {
  (void)finisher;
  (void)offset;
  if (size != REGISTER_SIZE)
    return BUS_ACCESS_FAULT;
  *value = 0;
  return BUS_OK;
}

enum BusStatus finisher_write(Finisher *finisher, uxlen_t offset, unsigned size,
                              uint64_t value) {
  if (size != REGISTER_SIZE)
    return BUS_ACCESS_FAULT;
  if (offset != 0)
    return BUS_OK;
  switch (value & STATUS_MASK) {
  case FINISHER_PASS:
    finisher->exit_code = 0;
    finisher->finished = true;
    break;
  case FINISHER_FAIL:
    finisher->exit_code = (int)(value >> CODE_SHIFT & STATUS_MASK);
    finisher->finished = true;
    break;
  }
  return BUS_OK;
}

bool finisher_finished(const Finisher *finisher) { return finisher->finished; }

int finisher_exit_code(const Finisher *finisher) { return finisher->exit_code; }

static enum BusStatus bus_read(void *finisher, uxlen_t offset, unsigned size,
                               uint64_t *value) {
  return finisher_read(finisher, offset, size, value);
}

static enum BusStatus bus_write(void *finisher, uxlen_t offset, unsigned size,
                                uint64_t value) {
  return finisher_write(finisher, offset, size, value);
}

struct BusDevice finisher_bus_device(Finisher *finisher, uxlen_t base) {
  return (struct BusDevice){.base = base,
                            .size = FINISHER_SIZE,
                            .device = finisher,
                            .read = bus_read,
                            .write = bus_write};
}
