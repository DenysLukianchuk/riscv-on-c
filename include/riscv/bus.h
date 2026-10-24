#ifndef RISCV_BUS_H
#define RISCV_BUS_H

#include "riscv/memory.h"

#define RAM_BASE 0x80000000

enum BusStatus { BUS_OK = 0, BUS_ACCESS_FAULT = 1 };

typedef struct Bus Bus;

Bus *bus_create(Memory *memory);
void bus_destroy(Bus *bus);
enum BusStatus bus_read(Bus *bus, uxlen_t addr, unsigned size, uint64_t *value);
enum BusStatus bus_write(Bus *bus, uxlen_t addr, uint64_t value, unsigned size);

#endif // RISCV_BUS_H
