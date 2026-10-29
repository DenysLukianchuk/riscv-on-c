#ifndef RISCV_SYSTEM_BUS_H
#define RISCV_SYSTEM_BUS_H

#include "riscv/system/device.h"
#include <stdbool.h>

typedef struct Bus Bus;

Bus *bus_create(void);
void bus_destroy(Bus *bus);
bool bus_attach(Bus *bus, struct BusDevice device);
enum BusStatus bus_read(Bus *bus, uxlen_t addr, unsigned size, uint64_t *value);
enum BusStatus bus_write(Bus *bus, uxlen_t addr, unsigned size, uint64_t value);

#endif // RISCV_SYSTEM_BUS_H
