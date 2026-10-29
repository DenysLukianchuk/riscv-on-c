#ifndef RISCV_SYSTEM_DEVICE_H
#define RISCV_SYSTEM_DEVICE_H

#include "riscv/xlen.h"

enum BusStatus { BUS_OK = 0, BUS_ERROR = 1 };

typedef enum BusStatus (*BusReadFn)(void *device, uxlen_t offset, unsigned size,
                                    uint64_t *value);
typedef enum BusStatus (*BusWriteFn)(void *device, uxlen_t offset,
                                     unsigned size, uint64_t value);

// A window of addresses on the bus. Accesses that lie wholly inside it go to
// read and write with the offset from base.
struct BusDevice {
  uxlen_t base, size;
  void *device;
  BusReadFn read;
  BusWriteFn write;
};

#endif // RISCV_SYSTEM_DEVICE_H
