#include "riscv/machine.h"
#include "riscv/clint.h"
#include "riscv/hart.h"
#include "riscv/trap_cause.h"
#include "riscv/uart.h"
#include <stdlib.h>

// The memory map follows QEMU's virt board.
#define CLINT_BASE 0x02000000
#define UART_BASE 0x10000000

#define MTIP INTERRUPT_BIT(INTERRUPT_M_TIMER)
#define MSIP INTERRUPT_BIT(INTERRUPT_M_SOFTWARE)

struct Machine {
  Memory *memory;
  Clint *clint;
  Uart *uart;
  Bus *bus;
  Cpu *cpu;
};

Machine *machine_create(uxlen_t ram_size, UartOutputFn console, void *context) {
  Machine *machine = (Machine *)malloc(sizeof(Machine));
  machine->memory = memory_create(ram_size);
  machine->clint = clint_create();
  machine->bus = bus_create();
  machine->uart = uart_create(console, context);
  bus_attach(machine->bus, memory_bus_device(machine->memory, RAM_BASE));
  bus_attach(machine->bus, clint_bus_device(machine->clint, CLINT_BASE));
  bus_attach(machine->bus, uart_bus_device(machine->uart, UART_BASE));
  machine->cpu = cpu_create(machine->bus);
  return machine;
}

void machine_destroy(Machine *machine) {
  cpu_destroy(machine->cpu);
  bus_destroy(machine->bus);
  uart_destroy(machine->uart);
  clint_destroy(machine->clint);
  memory_destroy(machine->memory);
  free(machine);
}

Cpu *machine_get_cpu(Machine *machine) { return machine->cpu; }

Bus *machine_get_bus(Machine *machine) { return machine->bus; }

void machine_step(Machine *machine) {
  Csr *csr = cpu_get_csr(machine->cpu);
  csr_set_interrupt_pending(csr, MTIP, clint_timer_pending(machine->clint));
  csr_set_interrupt_pending(csr, MSIP, clint_software_pending(machine->clint));
  cpu_step(machine->cpu);
  clint_tick(machine->clint, 1);
}

void machine_console_input(Machine *machine, uint8_t byte) {
  uart_receive(machine->uart, byte);
}
