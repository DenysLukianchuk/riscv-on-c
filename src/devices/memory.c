#include "riscv/memory.h"
#include "riscv/bits.h"
#include <assert.h>
#include <stdlib.h>

#define ENSURE_VALID_SIZE(size)                                                \
  assert(size == 1 || size == 2 || size == 4 || size == 8)

struct Memory {
  uint8_t *data;
  uxlen_t size;
};

Memory *memory_create(uxlen_t size) {
  Memory *mem = (Memory *)malloc(sizeof(Memory));
  mem->data = (uint8_t *)malloc(size * sizeof(uint8_t));
  mem->size = size;
  return mem;
}

uxlen_t memory_get_size(Memory *mem) { return mem->size; }

void memory_write(Memory *mem, uxlen_t addr, unsigned size, uint64_t value) {
  assert(addr + size - 1 < mem->size);
  ENSURE_VALID_SIZE(size);
  for (unsigned i = 0; i < size; i++) {
    mem->data[addr + i] = (value >> (BITS_PER_BYTE * i)) & 0xFF;
  }
}

uint64_t memory_read(Memory *mem, uxlen_t addr, unsigned size) {
  assert(addr + size - 1 < mem->size);
  ENSURE_VALID_SIZE(size);
  uint64_t value = 0;
  for (unsigned i = 0; i < size; i++) {
    value |= (uint64_t)(mem->data[addr + i]) << (BITS_PER_BYTE * i);
  }
  return value;
}

void memory_destroy(Memory *mem) {
  free(mem->data);
  free(mem);
}

// The bus only passes accesses that lie wholly inside the memory.
static enum BusStatus bus_read(void *mem, uxlen_t offset, unsigned size,
                               uint64_t *value) {
  *value = memory_read(mem, offset, size);
  return BUS_OK;
}

static enum BusStatus bus_write(void *mem, uxlen_t offset, unsigned size,
                                uint64_t value) {
  memory_write(mem, offset, size, value);
  return BUS_OK;
}

struct BusDevice memory_bus_device(Memory *mem, uxlen_t base) {
  return (struct BusDevice){.base = base,
                            .size = mem->size,
                            .device = mem,
                            .read = bus_read,
                            .write = bus_write};
}
