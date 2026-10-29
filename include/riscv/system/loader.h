#ifndef RISCV_SYSTEM_LOADER_H
#define RISCV_SYSTEM_LOADER_H

#include "riscv/system/bus.h"
#include <stddef.h>

#define LOADER_OK 0
#define LOADER_ERR -1

int loader_load_elf(Bus *bus, const uint8_t *image, size_t size,
                    uxlen_t *entry);

#endif // RISCV_SYSTEM_LOADER_H
