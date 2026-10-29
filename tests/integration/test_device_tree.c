#include "dt_reader.h"
#include "riscv/system/machine.h"
#include "unity.h"

// The board describes itself to the guest with a device tree. Node names and
// bindings follow the Linux and OpenSBI documentation for QEMU's virt board.
#define RAM_SIZE 0x10000
#if XLEN == 64
#define ISA "rv64ima_zicsr_zifencei"
#define MMU_TYPE "riscv,sv39"
#else
#define ISA "rv32ima_zicsr_zifencei"
#define MMU_TYPE "riscv,sv32"
#endif

static Machine *machine;
static const uint8_t *tree;

void setUp(void) {
  machine = machine_create(RAM_SIZE, NULL, NULL);
  uint32_t size = 0;
  tree = machine_device_tree(machine, &size);
}

void tearDown(void) { machine_destroy(machine); }

// Failures are reported at the caller's line.
#define property(path, name) property_at(__LINE__, path, name)
#define cell(path, name, n) dt_cell(property(path, name), n)
#define string(path, name) ((const char *)property(path, name).value)

static struct DtProperty property_at(int line, const char *path,
                                     const char *name) {
  struct DtProperty found = {0};
  if (!dt_lookup(tree, path, name, &found))
    UNITY_TEST_FAIL(line, "property missing");
  return found;
}

// Whether a string list such as compatible = "a", "b" holds the string.
#define has_string(path, name, wanted)                                         \
  has_string_in(property(path, name), wanted)

static bool has_string_in(struct DtProperty list, const char *wanted) {
  for (uint32_t at = 0; at < list.len;
       at += strlen((const char *)list.value + at) + 1)
    if (strcmp((const char *)list.value + at, wanted) == 0)
      return true;
  return false;
}

// reg = <base size>, two cells each.
#define ASSERT_REG(path, base, size)                                           \
  do {                                                                         \
    struct DtProperty reg = property(path, "reg");                             \
    TEST_ASSERT_EQUAL(16, reg.len);                                            \
    TEST_ASSERT_EQUAL_HEX32(0, dt_cell(reg, 0));                               \
    TEST_ASSERT_EQUAL_HEX32(base, dt_cell(reg, 1));                            \
    TEST_ASSERT_EQUAL_HEX32(0, dt_cell(reg, 2));                               \
    TEST_ASSERT_EQUAL_HEX32(size, dt_cell(reg, 3));                            \
  } while (0)

// The root: addresses and sizes are two cells each.

static void test_root_uses_two_cells_for_addresses_and_sizes(void) {
  TEST_ASSERT_EQUAL(2, cell("/", "#address-cells", 0));
  TEST_ASSERT_EQUAL(2, cell("/", "#size-cells", 0));
}

// Memory: reg is <address-high address-low size-high size-low>.

static void test_memory_node_covers_the_ram(void) {
  struct DtProperty reg = property("/memory@80000000", "reg");

  TEST_ASSERT_EQUAL_STRING("memory", string("/memory@80000000", "device_type"));
  TEST_ASSERT_EQUAL(16, reg.len);
  TEST_ASSERT_EQUAL_HEX32(0x0, dt_cell(reg, 0));
  TEST_ASSERT_EQUAL_HEX32(0x80000000, dt_cell(reg, 1));
  TEST_ASSERT_EQUAL_HEX32(0x0, dt_cell(reg, 2));
  TEST_ASSERT_EQUAL_HEX32(RAM_SIZE, dt_cell(reg, 3));
}

// The hart.

static void test_cpus_node_counts_harts_by_id(void) {
  TEST_ASSERT_EQUAL(1, cell("/cpus", "#address-cells", 0));
  TEST_ASSERT_EQUAL(0, cell("/cpus", "#size-cells", 0));
  TEST_ASSERT_TRUE(cell("/cpus", "timebase-frequency", 0) > 0);
}

static void test_cpu_node_describes_hart_zero(void) {
  TEST_ASSERT_EQUAL_STRING("cpu", string("/cpus/cpu@0", "device_type"));
  TEST_ASSERT_EQUAL(0, cell("/cpus/cpu@0", "reg", 0));
  TEST_ASSERT_EQUAL_STRING("riscv", string("/cpus/cpu@0", "compatible"));
  TEST_ASSERT_EQUAL_STRING(ISA, string("/cpus/cpu@0", "riscv,isa"));
  TEST_ASSERT_EQUAL_STRING("okay", string("/cpus/cpu@0", "status"));
}

// The paging scheme. OpenSBI disables a hart whose node has no mmu-type, as
// one Linux cannot run on.
static void test_cpu_node_names_its_mmu(void) {
  TEST_ASSERT_EQUAL_STRING(MMU_TYPE, string("/cpus/cpu@0", "mmu-type"));
}

// The console: /chosen points at the UART.

static void test_chosen_points_stdout_at_the_uart(void) {
  TEST_ASSERT_EQUAL_STRING("/soc/serial@10000000",
                           string("/chosen", "stdout-path"));
  TEST_ASSERT_TRUE(dt_has_node(tree, "/soc/serial@10000000"));
}

// Interrupts. A controller is a node with interrupt-controller and a phandle,
// a number other nodes use to point at it.

#define INTC "/cpus/cpu@0/interrupt-controller"
#define CLINT "/soc/clint@2000000"
#define PLIC "/soc/plic@c000000"
#define UART "/soc/serial@10000000"
#define FINISHER "/soc/test@100000"

// Lines into the hart, as numbered in mip.
#define IRQ_S_EXTERNAL 9
#define IRQ_M_SOFTWARE 3
#define IRQ_M_TIMER 7
#define IRQ_M_EXTERNAL 11
#define UART_IRQ 10

static void test_hart_has_its_interrupt_controller(void) {
  TEST_ASSERT_EQUAL_STRING("riscv,cpu-intc", string(INTC, "compatible"));
  TEST_ASSERT_EQUAL(0, property(INTC, "interrupt-controller").len);
  TEST_ASSERT_EQUAL(1, cell(INTC, "#interrupt-cells", 0));
  TEST_ASSERT_NOT_EQUAL(0, cell(INTC, "phandle", 0));
}

// The devices sit on a bus with the root's address layout.
static void test_soc_is_a_simple_bus(void) {
  TEST_ASSERT_EQUAL_STRING("simple-bus", string("/soc", "compatible"));
  TEST_ASSERT_EQUAL(2, cell("/soc", "#address-cells", 0));
  TEST_ASSERT_EQUAL(2, cell("/soc", "#size-cells", 0));
  TEST_ASSERT_EQUAL(0, property("/soc", "ranges").len);
}

// interrupts-extended lists <controller line> pairs.
static void test_clint_drives_the_software_and_timer_lines(void) {
  uint32_t intc = cell(INTC, "phandle", 0);
  struct DtProperty lines = property(CLINT, "interrupts-extended");

  TEST_ASSERT_TRUE(has_string(CLINT, "compatible", "riscv,clint0"));
  ASSERT_REG(CLINT, 0x02000000, 0x10000);
  TEST_ASSERT_EQUAL(16, lines.len);
  TEST_ASSERT_EQUAL(intc, dt_cell(lines, 0));
  TEST_ASSERT_EQUAL(IRQ_M_SOFTWARE, dt_cell(lines, 1));
  TEST_ASSERT_EQUAL(intc, dt_cell(lines, 2));
  TEST_ASSERT_EQUAL(IRQ_M_TIMER, dt_cell(lines, 3));
}

// Context 0 drives the M-mode external line, context 1 the S-mode one.
static void test_plic_drives_the_external_lines(void) {
  uint32_t intc = cell(INTC, "phandle", 0);
  struct DtProperty contexts = property(PLIC, "interrupts-extended");

  TEST_ASSERT_TRUE(has_string(PLIC, "compatible", "riscv,plic0"));
  ASSERT_REG(PLIC, 0x0C000000, 0x600000);
  TEST_ASSERT_EQUAL(16, contexts.len);
  TEST_ASSERT_EQUAL(intc, dt_cell(contexts, 0));
  TEST_ASSERT_EQUAL(IRQ_M_EXTERNAL, dt_cell(contexts, 1));
  TEST_ASSERT_EQUAL(intc, dt_cell(contexts, 2));
  TEST_ASSERT_EQUAL(IRQ_S_EXTERNAL, dt_cell(contexts, 3));
}

static void test_plic_is_a_controller_for_31_sources(void) {
  TEST_ASSERT_EQUAL(0, property(PLIC, "interrupt-controller").len);
  TEST_ASSERT_EQUAL(1, cell(PLIC, "#interrupt-cells", 0));
  TEST_ASSERT_EQUAL(0, cell(PLIC, "#address-cells", 0));
  TEST_ASSERT_EQUAL(31, cell(PLIC, "riscv,ndev", 0));
  TEST_ASSERT_NOT_EQUAL(0, cell(PLIC, "phandle", 0));
  TEST_ASSERT_NOT_EQUAL(cell(INTC, "phandle", 0), cell(PLIC, "phandle", 0));
}

static void test_uart_interrupts_through_the_plic(void) {
  TEST_ASSERT_EQUAL_STRING("ns16550a", string(UART, "compatible"));
  ASSERT_REG(UART, 0x10000000, 0x100);
  TEST_ASSERT_TRUE(cell(UART, "clock-frequency", 0) > 0);
  TEST_ASSERT_EQUAL(cell(PLIC, "phandle", 0),
                    cell(UART, "interrupt-parent", 0));
  TEST_ASSERT_EQUAL(UART_IRQ, cell(UART, "interrupts", 0));
}

// Power: the finisher is a syscon, and /poweroff names the write that turns
// the board off.
static void test_finisher_is_a_syscon(void) {
  TEST_ASSERT_TRUE(has_string(FINISHER, "compatible", "sifive,test0"));
  TEST_ASSERT_TRUE(has_string(FINISHER, "compatible", "syscon"));
  ASSERT_REG(FINISHER, 0x00100000, 0x1000);
  TEST_ASSERT_NOT_EQUAL(0, cell(FINISHER, "phandle", 0));
}

static void test_poweroff_writes_pass_to_the_finisher(void) {
  TEST_ASSERT_EQUAL_STRING("syscon-poweroff",
                           string("/poweroff", "compatible"));
  TEST_ASSERT_EQUAL(cell(FINISHER, "phandle", 0),
                    cell("/poweroff", "regmap", 0));
  TEST_ASSERT_EQUAL(0, cell("/poweroff", "offset", 0));
  TEST_ASSERT_EQUAL_HEX32(0x5555, cell("/poweroff", "value", 0));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_root_uses_two_cells_for_addresses_and_sizes);
  RUN_TEST(test_memory_node_covers_the_ram);
  RUN_TEST(test_cpus_node_counts_harts_by_id);
  RUN_TEST(test_cpu_node_describes_hart_zero);
  RUN_TEST(test_cpu_node_names_its_mmu);
  RUN_TEST(test_chosen_points_stdout_at_the_uart);
  RUN_TEST(test_hart_has_its_interrupt_controller);
  RUN_TEST(test_soc_is_a_simple_bus);
  RUN_TEST(test_clint_drives_the_software_and_timer_lines);
  RUN_TEST(test_plic_drives_the_external_lines);
  RUN_TEST(test_plic_is_a_controller_for_31_sources);
  RUN_TEST(test_uart_interrupts_through_the_plic);
  RUN_TEST(test_finisher_is_a_syscon);
  RUN_TEST(test_poweroff_writes_pass_to_the_finisher);
  return UNITY_END();
}
