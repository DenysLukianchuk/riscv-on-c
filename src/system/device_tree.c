#include "riscv/system/device_tree.h"
#include "riscv/cpu/trap_cause.h"
#include <stddef.h>

#if XLEN == 64
#define ISA "rv64imac_zicsr_zifencei"
#define ISA_BASE "rv64i"
#define MMU_TYPE "riscv,sv39"
#else
#define ISA "rv32imac_zicsr_zifencei"
#define ISA_BASE "rv32i"
#define MMU_TYPE "riscv,sv32"
#endif
#define TIMEBASE_FREQUENCY 10000000
#define FINISHER_POWEROFF 0x5555

#define UART_CLOCK_FREQUENCY 3686400
#define PLIC_SOURCES 31

enum Phandle { INTC_PHANDLE = 1, PLIC_PHANDLE, FINISHER_PHANDLE };

#define put_strings(fdt, name, list) fdt_property(fdt, name, list, sizeof list)

static void put_reg(Fdt *fdt, struct BusDevice device) {
  uint64_t base = device.base, size = device.size;
  const uint32_t cells[] = {base >> 32, (uint32_t)base, size >> 32,
                            (uint32_t)size};
  fdt_property_cells(fdt, "reg", cells, 4);
}

static void put_hart_lines(Fdt *fdt, uint32_t first, uint32_t second) {
  const uint32_t cells[] = {INTC_PHANDLE, first, INTC_PHANDLE, second};
  fdt_property_cells(fdt, "interrupts-extended", cells, 4);
}

static void put_flag(Fdt *fdt, const char *name) {
  fdt_property(fdt, name, NULL, 0);
}

static void describe_memory(Fdt *fdt, const struct BoardDevices *devices) {
  fdt_begin_node(fdt, "memory@80000000");
  fdt_property_string(fdt, "device_type", "memory");
  put_reg(fdt, devices->ram);
  fdt_end_node(fdt);
}

static void describe_cpus(Fdt *fdt) {
  fdt_begin_node(fdt, "cpus");
  fdt_property_u32(fdt, "#address-cells", 1);
  fdt_property_u32(fdt, "#size-cells", 0);
  fdt_property_u32(fdt, "timebase-frequency", TIMEBASE_FREQUENCY);

  fdt_begin_node(fdt, "cpu@0");
  fdt_property_string(fdt, "device_type", "cpu");
  fdt_property_u32(fdt, "reg", 0);
  fdt_property_string(fdt, "compatible", "riscv");
  fdt_property_string(fdt, "riscv,isa", ISA);
  fdt_property_string(fdt, "riscv,isa-base", ISA_BASE);
  put_strings(fdt, "riscv,isa-extensions",
              "i\0m\0a\0c\0zicsr\0zifencei\0svade\0zicntr");
  fdt_property_string(fdt, "mmu-type", MMU_TYPE);
  fdt_property_string(fdt, "status", "okay");

  fdt_begin_node(fdt, "interrupt-controller");
  fdt_property_string(fdt, "compatible", "riscv,cpu-intc");
  put_flag(fdt, "interrupt-controller");
  fdt_property_u32(fdt, "#interrupt-cells", 1);
  fdt_property_u32(fdt, "phandle", INTC_PHANDLE);
  fdt_end_node(fdt);

  fdt_end_node(fdt);
  fdt_end_node(fdt);
}

static void describe_clint(Fdt *fdt, const struct BoardDevices *devices) {
  fdt_begin_node(fdt, "clint@2000000");
  put_strings(fdt, "compatible", "sifive,clint0\0riscv,clint0");
  put_reg(fdt, devices->clint);
  put_hart_lines(fdt, INTERRUPT_M_SOFTWARE, INTERRUPT_M_TIMER);
  fdt_end_node(fdt);
}

static void describe_plic(Fdt *fdt, const struct BoardDevices *devices) {
  fdt_begin_node(fdt, "plic@c000000");
  put_strings(fdt, "compatible", "sifive,plic-1.0.0\0riscv,plic0");
  put_reg(fdt, devices->plic);
  put_flag(fdt, "interrupt-controller");
  fdt_property_u32(fdt, "#interrupt-cells", 1);
  fdt_property_u32(fdt, "#address-cells", 0);
  fdt_property_u32(fdt, "riscv,ndev", PLIC_SOURCES);
  put_hart_lines(fdt, INTERRUPT_M_EXTERNAL, INTERRUPT_S_EXTERNAL);
  fdt_property_u32(fdt, "phandle", PLIC_PHANDLE);
  fdt_end_node(fdt);
}

static void describe_uart(Fdt *fdt, const struct BoardDevices *devices) {
  fdt_begin_node(fdt, "serial@10000000");
  fdt_property_string(fdt, "compatible", "ns16550a");
  put_reg(fdt, devices->uart);
  fdt_property_u32(fdt, "clock-frequency", UART_CLOCK_FREQUENCY);
  fdt_property_u32(fdt, "interrupt-parent", PLIC_PHANDLE);
  fdt_property_u32(fdt, "interrupts", UART_IRQ);
  fdt_end_node(fdt);
}

static void describe_finisher(Fdt *fdt, const struct BoardDevices *devices) {
  fdt_begin_node(fdt, "test@100000");
  put_strings(fdt, "compatible", "sifive,test0\0syscon");
  put_reg(fdt, devices->finisher);
  fdt_property_u32(fdt, "phandle", FINISHER_PHANDLE);
  fdt_end_node(fdt);
}

static void describe_soc(Fdt *fdt, const struct BoardDevices *devices) {
  fdt_begin_node(fdt, "soc");
  fdt_property_string(fdt, "compatible", "simple-bus");
  fdt_property_u32(fdt, "#address-cells", 2);
  fdt_property_u32(fdt, "#size-cells", 2);
  put_flag(fdt, "ranges");
  describe_clint(fdt, devices);
  describe_plic(fdt, devices);
  describe_uart(fdt, devices);
  describe_finisher(fdt, devices);
  fdt_end_node(fdt);
}

static void describe_poweroff(Fdt *fdt) {
  fdt_begin_node(fdt, "poweroff");
  fdt_property_string(fdt, "compatible", "syscon-poweroff");
  fdt_property_u32(fdt, "regmap", FINISHER_PHANDLE);
  fdt_property_u32(fdt, "offset", 0);
  fdt_property_u32(fdt, "value", FINISHER_POWEROFF);
  fdt_end_node(fdt);
}

void device_tree_describe(Fdt *fdt, const struct BoardDevices *devices) {
  fdt_begin_node(fdt, "");
  fdt_property_u32(fdt, "#address-cells", 2);
  fdt_property_u32(fdt, "#size-cells", 2);

  fdt_begin_node(fdt, "chosen");
  fdt_property_string(fdt, "stdout-path", "/soc/serial@10000000");
  fdt_end_node(fdt);

  describe_memory(fdt, devices);
  describe_cpus(fdt);
  describe_soc(fdt, devices);
  describe_poweroff(fdt);
  fdt_end_node(fdt);
}
