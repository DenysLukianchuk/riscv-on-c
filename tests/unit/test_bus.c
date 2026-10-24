#include "riscv/bus.h"
#include "riscv/memory.h"
#include "unity.h"

#define RAM_SIZE 1024

static Memory *ram;
static Bus *bus;

void setUp(void) {
  ram = memory_create(RAM_SIZE);
  bus = bus_create(ram);
}

void tearDown(void) {
  memory_destroy(ram);
  bus_destroy(bus);
}

static void test_ram_is_mapped_at_ram_base(void) {
  memory_write(ram, 0, 0xDEADBEEF, 4);

  uint64_t value = 0;
  TEST_ASSERT_EQUAL(BUS_OK, bus_read(bus, RAM_BASE, 4, &value));
  TEST_ASSERT_EQUAL_HEX64(0xDEADBEEF, value);
}

static void test_read_from_unmapped_address_is_access_fault(void) {
  uint64_t value = 0;

  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT, bus_read(bus, 0x0, 4, &value));
}

static void test_read_straddling_start_of_ram_is_access_fault(void) {
  uint64_t value = 0;

  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT, bus_read(bus, RAM_BASE - 2, 4, &value));
}

static void test_write_to_ram_base_lands_at_ram_offset_zero(void) {
  TEST_ASSERT_EQUAL(BUS_OK, bus_write(bus, RAM_BASE, 0xCAFEBABE, 4));

  TEST_ASSERT_EQUAL_HEX64(0xCAFEBABE, memory_read(ram, 0, 4));
}

static void test_write_to_unmapped_address_is_access_fault(void) {
  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT, bus_write(bus, 0x0, 0xCAFEBABE, 4));
}

static void test_access_straddling_end_of_ram_is_access_fault(void) {
  uint64_t value = 0;

  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT,
                    bus_read(bus, RAM_BASE + RAM_SIZE - 2, 4, &value));
  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT,
                    bus_write(bus, RAM_BASE + RAM_SIZE - 2, 0, 4));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_ram_is_mapped_at_ram_base);
  RUN_TEST(test_read_from_unmapped_address_is_access_fault);
  RUN_TEST(test_read_straddling_start_of_ram_is_access_fault);
  RUN_TEST(test_write_to_ram_base_lands_at_ram_offset_zero);
  RUN_TEST(test_write_to_unmapped_address_is_access_fault);
  RUN_TEST(test_access_straddling_end_of_ram_is_access_fault);
  return UNITY_END();
}
