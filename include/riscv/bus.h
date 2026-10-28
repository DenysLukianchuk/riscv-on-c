#ifndef RISCV_BUS_H
#define RISCV_BUS_H

#include "riscv/device.h"
#include <stdbool.h>

#define RAM_BASE 0x80000000

typedef struct Bus Bus;

Bus *bus_create(void);
void bus_destroy(Bus *bus);
bool bus_attach(Bus *bus, struct BusDevice device);
enum BusStatus bus_read(Bus *bus, uxlen_t addr, unsigned size, uint64_t *value);
enum BusStatus bus_write(Bus *bus, uxlen_t addr, unsigned size, uint64_t value);

#endif // RISCV_BUS_H
