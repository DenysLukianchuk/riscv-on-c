#ifndef RISCV_DEVICE_TREE_H
#define RISCV_DEVICE_TREE_H

#include "riscv/board.h"
#include "riscv/fdt.h"

void device_tree_describe(Fdt *fdt, const struct BoardDevices *devices);

#endif // RISCV_DEVICE_TREE_H
