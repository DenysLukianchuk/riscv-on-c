#ifndef RISCV_DEVICES_CLINT_H
#define RISCV_DEVICES_CLINT_H

#include "riscv/system/device.h"
#include "riscv/xlen.h"
#include <stdbool.h>

typedef struct Clint Clint;

Clint *clint_create(void);
void clint_destroy(Clint *clint);
enum BusStatus clint_read(Clint *clint, uxlen_t offset, unsigned size,
                          uint64_t *value);
enum BusStatus clint_write(Clint *clint, uxlen_t offset, unsigned size,
                           uint64_t value);
void clint_tick(Clint *clint, uint64_t ticks);
bool clint_timer_pending(const Clint *clint);
bool clint_software_pending(const Clint *clint);

struct BusDevice clint_bus_device(Clint *clint, uxlen_t base);
uint64_t clint_mtime(const Clint *clint);

#endif // RISCV_DEVICES_CLINT_H
