#ifndef RISCV_PLIC_H
#define RISCV_PLIC_H

#include "riscv/device.h"
#include "riscv/xlen.h"
#include <stdbool.h>

typedef struct Plic Plic;
Plic *plic_create(void);
void plic_destroy(Plic *plic);
enum BusStatus plic_read(Plic *plic, uxlen_t offset, unsigned size,
                         uint64_t *value);
enum BusStatus plic_write(Plic *plic, uxlen_t offset, unsigned size,
                          uint64_t value);
void plic_set_source(Plic *plic, unsigned source, bool raised);
bool plic_interrupt_pending(const Plic *plic, unsigned context);
struct BusDevice plic_bus_device(Plic *plic, uxlen_t base);

#endif // RISCV_PLIC_H
