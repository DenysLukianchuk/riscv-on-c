#include "riscv/bus.h"
#include <stdlib.h>

struct Bus {
  Memory *memory;
};

Bus *bus_create(Memory *memory) {
  Bus *bus = (Bus *)malloc(sizeof(Bus));
  bus->memory = memory;
  return bus;
}

void bus_destroy(Bus *bus) { free(bus); }

enum BusStatus bus_read(Bus *bus, uxlen_t addr, unsigned size,
                        uint64_t *value) {
  uxlen_t offset = addr - RAM_BASE;
  if (addr < RAM_BASE) {
    return BUS_ACCESS_FAULT;
  }
  if (offset + size - 1 >= memory_get_size(bus->memory)) {
    return BUS_ACCESS_FAULT;
  }
  *value = memory_read(bus->memory, offset, size);
  return BUS_OK;
}

enum BusStatus bus_write(Bus *bus, uxlen_t addr, unsigned size,
                         uint64_t value) {
  uxlen_t offset = addr - RAM_BASE;
  if (addr < RAM_BASE) {
    return BUS_ACCESS_FAULT;
  }
  if (offset + size - 1 >= memory_get_size(bus->memory)) {
    return BUS_ACCESS_FAULT;
  }
  memory_write(bus->memory, offset, size, value);
  return BUS_OK;
}
