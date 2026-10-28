#include "riscv/machine.h"
#include "riscv/clint.h"
#include "riscv/finisher.h"
#include "riscv/hart.h"
#include "riscv/plic.h"
#include "riscv/trap_cause.h"
#include "riscv/uart.h"
#include <assert.h>
#include <stdlib.h>

// The memory map follows QEMU's virt board.
#define FINISHER_BASE 0x00100000
#define CLINT_BASE 0x02000000
#define PLIC_BASE 0x0c000000
#define UART_BASE 0x10000000

#define UART_IRQ 10

#define MTIP INTERRUPT_BIT(INTERRUPT_M_TIMER)
#define MSIP INTERRUPT_BIT(INTERRUPT_M_SOFTWARE)
#define MEIP INTERRUPT_BIT(INTERRUPT_M_EXTERNAL)

#define M_CONTEXT 0
#define S_CONTEXT 1

struct Machine {
  Memory *memory;
  Clint *clint;
  Uart *uart;
  Plic *plic;
  Finisher *finisher;
  Bus *bus;
  Cpu *cpu;
};

static void attach(Bus *bus, struct BusDevice device) {
  bool attached = bus_attach(bus, device);
  assert(attached && "devices overlap");
  (void)attached;
}

Machine *machine_create(uxlen_t ram_size, UartOutputFn console, void *context) {
  Machine *machine = (Machine *)malloc(sizeof(Machine));
  machine->memory = memory_create(ram_size);
  machine->clint = clint_create();
  machine->plic = plic_create();
  machine->bus = bus_create();
  machine->uart = uart_create(console, context);
  machine->finisher = finisher_create();
  attach(machine->bus, memory_bus_device(machine->memory, RAM_BASE));
  attach(machine->bus, clint_bus_device(machine->clint, CLINT_BASE));
  attach(machine->bus, plic_bus_device(machine->plic, PLIC_BASE));
  attach(machine->bus, uart_bus_device(machine->uart, UART_BASE));
  attach(machine->bus, finisher_bus_device(machine->finisher, FINISHER_BASE));
  machine->cpu = cpu_create(machine->bus);
  return machine;
}

void machine_destroy(Machine *machine) {
  cpu_destroy(machine->cpu);
  bus_destroy(machine->bus);
  uart_destroy(machine->uart);
  plic_destroy(machine->plic);
  clint_destroy(machine->clint);
  finisher_destroy(machine->finisher);
  memory_destroy(machine->memory);
  free(machine);
}

Cpu *machine_get_cpu(Machine *machine) { return machine->cpu; }

Bus *machine_get_bus(Machine *machine) { return machine->bus; }

void machine_step(Machine *machine) {
  if (machine_finished(machine))
    return;
  Csr *csr = cpu_get_csr(machine->cpu);
  csr_set_interrupt_pending(csr, MTIP, clint_timer_pending(machine->clint));
  csr_set_interrupt_pending(csr, MSIP, clint_software_pending(machine->clint));
  plic_set_source(machine->plic, UART_IRQ,
                  uart_interrupt_pending(machine->uart));
  csr_set_interrupt_pending(csr, MEIP,
                            plic_interrupt_pending(machine->plic, M_CONTEXT));
  csr_set_seip_line(csr, plic_interrupt_pending(machine->plic, S_CONTEXT));
  cpu_step(machine->cpu);
  clint_tick(machine->clint, 1);
}

void machine_console_input(Machine *machine, uint8_t byte) {
  uart_receive(machine->uart, byte);
}

bool machine_finished(Machine *machine) {
  return finisher_finished(machine->finisher);
}

int machine_exit_code(Machine *machine) {
  return finisher_exit_code(machine->finisher);
}
