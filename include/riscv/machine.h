#ifndef RISCV_MACHINE_H
#define RISCV_MACHINE_H

#include "riscv/bus.h"
#include "riscv/cpu.h"
#include "riscv/memory.h"
#include "riscv/uart.h"

typedef struct Machine Machine;

Machine *machine_create(uxlen_t ram_size, UartOutputFn console, void *context);
void machine_destroy(Machine *machine);

Cpu *machine_get_cpu(Machine *machine);
Bus *machine_get_bus(Machine *machine);

void machine_step(Machine *machine);

void machine_console_input(Machine *machine, uint8_t byte);

bool machine_finished(Machine *machine);
int machine_exit_code(Machine *machine);

#endif // RISCV_MACHINE_H
