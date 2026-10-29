#ifndef RISCV_SYSTEM_BOARD_H
#define RISCV_SYSTEM_BOARD_H

#include "riscv/system/device.h"

// The memory map follows QEMU's virt board.
#define RAM_BASE 0x80000000
#define FINISHER_BASE 0x00100000
#define CLINT_BASE 0x02000000
#define PLIC_BASE 0x0c000000
#define UART_BASE 0x10000000

#define UART_IRQ 10

// PLIC contexts of hart 0.
#define M_CONTEXT 0
#define S_CONTEXT 1

struct BoardDevices {
  struct BusDevice ram, clint, plic, uart, finisher;
};

#endif // RISCV_SYSTEM_BOARD_H
