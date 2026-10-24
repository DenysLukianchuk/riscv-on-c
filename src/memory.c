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

void memory_write(Memory *mem, uxlen_t addr, uint64_t value, unsigned size) {
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
