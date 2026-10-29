#ifndef RISCV_SYSTEM_DEVICE_TREE_H
#define RISCV_SYSTEM_DEVICE_TREE_H

#include "riscv/system/board.h"
#include "riscv/util/fdt.h"

void device_tree_describe(Fdt *fdt, const struct BoardDevices *devices);

#endif // RISCV_SYSTEM_DEVICE_TREE_H
