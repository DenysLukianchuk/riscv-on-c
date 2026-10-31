#include "riscv/system/machine.h"
#include "riscv/cpu/hart.h"
#include "riscv/cpu/trap_cause.h"
#include "riscv/devices/clint.h"
#include "riscv/devices/finisher.h"
#include "riscv/devices/plic.h"
#include "riscv/devices/uart.h"
#include "riscv/system/board.h"
#include "riscv/system/device_tree.h"
#include <assert.h>
#include <stdlib.h>

#define MTIP INTERRUPT_BIT(INTERRUPT_M_TIMER)
#define MSIP INTERRUPT_BIT(INTERRUPT_M_SOFTWARE)
#define MEIP INTERRUPT_BIT(INTERRUPT_M_EXTERNAL)

#define A0 10
#define A1 11

struct Machine {
  Memory *memory;
  Clint *clint;
  Uart *uart;
  Plic *plic;
  Finisher *finisher;
  Bus *bus;
  Cpu *cpu;
  Fdt *fdt;
  bool host_time;
};

static void attach(Bus *bus, struct BusDevice device) {
  bool attached = bus_attach(bus, device);
  assert(attached && "devices overlap");
  (void)attached;
}

Machine *machine_create(uxlen_t ram_size, UartOutputFn console, void *context) {
  Memory *memory = memory_create(ram_size);
  if (!memory)
    return NULL;
  Machine *machine = (Machine *)malloc(sizeof(Machine));
  machine->memory = memory;
  machine->host_time = false;
  machine->clint = clint_create();
  machine->plic = plic_create();
  machine->bus = bus_create();
  machine->uart = uart_create(console, context);
  machine->finisher = finisher_create();
  machine->fdt = fdt_create();
  const struct BoardDevices devices = {
      .ram = memory_bus_device(machine->memory, RAM_BASE),
      .clint = clint_bus_device(machine->clint, CLINT_BASE),
      .plic = plic_bus_device(machine->plic, PLIC_BASE),
      .uart = uart_bus_device(machine->uart, UART_BASE),
      .finisher = finisher_bus_device(machine->finisher, FINISHER_BASE),
  };
  attach(machine->bus, devices.ram);
  attach(machine->bus, devices.clint);
  attach(machine->bus, devices.plic);
  attach(machine->bus, devices.uart);
  attach(machine->bus, devices.finisher);
  machine->cpu = cpu_create(machine->bus);
  device_tree_describe(machine->fdt, &devices);
  return machine;
}

static uxlen_t device_tree_address(Machine *machine, uint32_t size) {
  uxlen_t end = RAM_BASE + memory_get_size(machine->memory);
  uxlen_t start = (end - size) & ~7;
  return start;
}

void machine_boot(Machine *machine, uxlen_t entry) {
  cpu_reset(machine->cpu);
  cpu_set_pc(machine->cpu, entry);

  uint32_t size;
  const uint8_t *blob = machine_device_tree(machine, &size);
  uxlen_t start = device_tree_address(machine, size);
  for (uint32_t i = 0; i < size; i++) {
    bus_write(machine->bus, start + i, 1, blob[i]);
  }
  Registers *regs = cpu_get_registers(machine->cpu);
  registers_write(regs, A0, 0);
  registers_write(regs, A1, start);
}

void machine_destroy(Machine *machine) {
  cpu_destroy(machine->cpu);
  bus_destroy(machine->bus);
  uart_destroy(machine->uart);
  plic_destroy(machine->plic);
  clint_destroy(machine->clint);
  finisher_destroy(machine->finisher);
  memory_destroy(machine->memory);
  fdt_destroy(machine->fdt);
  free(machine);
}

Cpu *machine_get_cpu(Machine *machine) { return machine->cpu; }

Bus *machine_get_bus(Machine *machine) { return machine->bus; }

static void update_interrupt_lines(Machine *machine) {
  Csr *csr = cpu_get_csr(machine->cpu);
  csr_set_interrupt_pending(csr, MTIP, clint_timer_pending(machine->clint));
  csr_set_time(csr, clint_mtime(machine->clint));
  csr_set_interrupt_pending(csr, MSIP, clint_software_pending(machine->clint));
  plic_set_source(machine->plic, UART_IRQ,
                  uart_interrupt_pending(machine->uart));
  csr_set_interrupt_pending(csr, MEIP,
                            plic_interrupt_pending(machine->plic, M_CONTEXT));
  csr_set_seip_line(csr, plic_interrupt_pending(machine->plic, S_CONTEXT));
}

void machine_step(Machine *machine) {
  if (machine_finished(machine))
    return;
  update_interrupt_lines(machine);
  cpu_step(machine->cpu);
  if (!machine->host_time)
    clint_tick(machine->clint, 1);
}

void machine_set_time(Machine *machine, uint64_t mtime) {
  machine->host_time = true;
  if (mtime > clint_mtime(machine->clint))
    clint_set_mtime(machine->clint, mtime);
}

uint64_t machine_next_timer(Machine *machine) {
  return clint_mtimecmp(machine->clint);
}

// wfi waits for an interrupt enabled in mie, whatever mstatus says.
bool machine_idle(Machine *machine) {
  if (!cpu_waiting(machine->cpu))
    return false;
  update_interrupt_lines(machine);
  Csr *csr = cpu_get_csr(machine->cpu);
  uxlen_t pending = 0, enabled = 0;
  csr_read(csr, CSR_REG_MIP, PRIV_M, &pending);
  csr_read(csr, CSR_REG_MIE, PRIV_M, &enabled);
  return (pending & enabled) == 0;
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

const uint8_t *machine_device_tree(Machine *machine, uint32_t *size) {
  return fdt_finish(machine->fdt, size);
}

bool machine_console_ready(Machine *machine) {
  return uart_can_receive(machine->uart);
}
