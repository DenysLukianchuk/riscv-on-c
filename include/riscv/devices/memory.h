#ifndef RISCV_DEVICES_MEMORY_H
#define RISCV_DEVICES_MEMORY_H

#include "riscv/system/device.h"
#include "riscv/xlen.h"

typedef struct Memory Memory;

Memory *memory_create(uxlen_t size);
uxlen_t memory_get_size(Memory *mem);
uint64_t memory_read(Memory *mem, uxlen_t addr, unsigned size);
void memory_write(Memory *mem, uxlen_t addr, unsigned size, uint64_t value);
void memory_destroy(Memory *mem);

struct BusDevice memory_bus_device(Memory *mem, uxlen_t base);

#endif // RISCV_DEVICES_MEMORY_H
