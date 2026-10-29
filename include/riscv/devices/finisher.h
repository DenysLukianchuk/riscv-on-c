#ifndef RISCV_DEVICES_FINISHER_H
#define RISCV_DEVICES_FINISHER_H

#include "riscv/system/device.h"
#include "riscv/xlen.h"
#include <stdbool.h>

typedef struct Finisher Finisher;

Finisher *finisher_create(void);
void finisher_destroy(Finisher *finisher);
enum BusStatus finisher_read(Finisher *finisher, uxlen_t offset, unsigned size,
                             uint64_t *value);
enum BusStatus finisher_write(Finisher *finisher, uxlen_t offset, unsigned size,
                              uint64_t value);
bool finisher_finished(const Finisher *finisher);
int finisher_exit_code(const Finisher *finisher);
struct BusDevice finisher_bus_device(Finisher *finisher, uxlen_t base);

#endif // RISCV_DEVICES_FINISHER_H
