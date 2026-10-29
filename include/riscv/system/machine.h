#ifndef RISCV_SYSTEM_MACHINE_H
#define RISCV_SYSTEM_MACHINE_H

#include "riscv/cpu/cpu.h"
#include "riscv/devices/memory.h"
#include "riscv/devices/uart.h"
#include "riscv/system/bus.h"

typedef struct Machine Machine;

Machine *machine_create(uxlen_t ram_size, UartOutputFn console, void *context);
void machine_boot(Machine *machine, uxlen_t entry);
void machine_destroy(Machine *machine);

Cpu *machine_get_cpu(Machine *machine);
Bus *machine_get_bus(Machine *machine);

void machine_step(Machine *machine);

void machine_console_input(Machine *machine, uint8_t byte);

bool machine_finished(Machine *machine);
int machine_exit_code(Machine *machine);
const uint8_t *machine_device_tree(Machine *machine, uint32_t *size);

#endif // RISCV_SYSTEM_MACHINE_H
