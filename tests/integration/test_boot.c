#include "riscv/system/machine.h"
#include "riscv_asm.h"
#include "unity.h"

// The boot protocol OpenSBI and Linux expect on RISC-V: the hart starts in
// M-mode at the entry point with its hart ID in a0 and the address of the
// device tree in a1. The tree must be 8-byte aligned.
#define RAM_ADDR 0x80000000
#define RAM_SIZE 0x10000
#define RAM_END (RAM_ADDR + RAM_SIZE)
#define ENTRY (RAM_ADDR + 0x100)
#define A0 10
#define A1 11
#define FDT_MAGIC_IN_A_LOAD 0xEDFE0DD0 // big-endian 0xD00DFEED read with lw

static Machine *machine;
static Cpu *cpu;
static Bus *bus;

void setUp(void) {
  machine = machine_create(RAM_SIZE, NULL, NULL);
  cpu = machine_get_cpu(machine);
  bus = machine_get_bus(machine);
}

void tearDown(void) { machine_destroy(machine); }

static uxlen_t reg(uint8_t index) {
  return registers_read(cpu_get_registers(cpu), index);
}

static void test_boot_starts_in_machine_mode_at_the_entry(void) {
  machine_boot(machine, ENTRY);

  TEST_ASSERT_EQUAL_HEX64(ENTRY, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL(PRIV_M, cpu_get_privilege(cpu));
}

static void test_boot_passes_hart_zero_in_a0(void) {
  registers_write(cpu_get_registers(cpu), A0, 0xDEAD);

  machine_boot(machine, ENTRY);

  TEST_ASSERT_EQUAL_HEX64(0, reg(A0));
}

static void test_boot_puts_the_device_tree_at_a1(void) {
  uint32_t size = 0;
  const uint8_t *tree = machine_device_tree(machine, &size);

  machine_boot(machine, ENTRY);

  uxlen_t address = reg(A1);
  TEST_ASSERT_EQUAL(0, address % 8);
  for (uint32_t i = 0; i < size; i++) {
    uint64_t byte = 0;
    TEST_ASSERT_EQUAL(BUS_OK, bus_read(bus, address + i, 1, &byte));
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(tree[i], byte, "device tree byte differs");
  }
}

// At the top of RAM the tree stays out of the way of a program loaded low.
static void test_device_tree_sits_at_the_top_of_ram(void) {
  uint32_t size = 0;
  machine_device_tree(machine, &size);

  machine_boot(machine, ENTRY);

  uxlen_t end = reg(A1) + size;
  TEST_ASSERT_TRUE(end <= RAM_END);
  TEST_ASSERT_TRUE(RAM_END - end < 8);
}

static void test_booted_program_reads_the_device_tree(void) {
  bus_write(bus, ENTRY, 4, LW(5, 0, A1));

  machine_boot(machine, ENTRY);
  machine_step(machine);

  TEST_ASSERT_EQUAL_HEX32(FDT_MAGIC_IN_A_LOAD, (uint32_t)reg(5));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_boot_starts_in_machine_mode_at_the_entry);
  RUN_TEST(test_boot_passes_hart_zero_in_a0);
  RUN_TEST(test_boot_puts_the_device_tree_at_a1);
  RUN_TEST(test_device_tree_sits_at_the_top_of_ram);
  RUN_TEST(test_booted_program_reads_the_device_tree);
  return UNITY_END();
}
