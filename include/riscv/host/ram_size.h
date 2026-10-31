#ifndef RISCV_HOST_RAM_SIZE_H
#define RISCV_HOST_RAM_SIZE_H

#include <stdbool.h>
#include <stdint.h>

#define RAM_SIZE_DEFAULT (128ull << 20)

bool ram_size_parse(const char *text, uint64_t *bytes);

#endif // RISCV_HOST_RAM_SIZE_H
