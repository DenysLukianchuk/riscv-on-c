#include "riscv/devices/memory.h"
#include "riscv/system/board.h"
#include "riscv/system/bus.h"
#include "unity.h"

#define RAM_SIZE 1024

static Memory *ram;
static Bus *bus;

void setUp(void) {
  ram = memory_create(RAM_SIZE);
  bus = bus_create();
  bus_attach(bus, memory_bus_device(ram, RAM_BASE));
}

void tearDown(void) {
  memory_destroy(ram);
  bus_destroy(bus);
}

static void test_ram_is_mapped_at_ram_base(void) {
  memory_write(ram, 0, 4, 0xDEADBEEF);

  uint64_t value = 0;
  TEST_ASSERT_EQUAL(BUS_OK, bus_read(bus, RAM_BASE, 4, &value));
  TEST_ASSERT_EQUAL_HEX64(0xDEADBEEF, value);
}

static void test_read_from_unmapped_address_is_a_bus_error(void) {
  uint64_t value = 0;

  TEST_ASSERT_EQUAL(BUS_ERROR, bus_read(bus, 0x0, 4, &value));
}

static void test_read_straddling_start_of_ram_is_a_bus_error(void) {
  uint64_t value = 0;

  TEST_ASSERT_EQUAL(BUS_ERROR, bus_read(bus, RAM_BASE - 2, 4, &value));
}

static void test_write_to_ram_base_lands_at_ram_offset_zero(void) {
  TEST_ASSERT_EQUAL(BUS_OK, bus_write(bus, RAM_BASE, 4, 0xCAFEBABE));

  TEST_ASSERT_EQUAL_HEX64(0xCAFEBABE, memory_read(ram, 0, 4));
}

static void test_write_to_unmapped_address_is_a_bus_error(void) {
  TEST_ASSERT_EQUAL(BUS_ERROR, bus_write(bus, 0x0, 4, 0xCAFEBABE));
}

static void test_access_straddling_end_of_ram_is_a_bus_error(void) {
  uint64_t value = 0;

  TEST_ASSERT_EQUAL(BUS_ERROR,
                    bus_read(bus, RAM_BASE + RAM_SIZE - 2, 4, &value));
  TEST_ASSERT_EQUAL(BUS_ERROR, bus_write(bus, RAM_BASE + RAM_SIZE - 2, 4, 0));
}

// Devices: the bus hands each access inside a device's window to it, with
// the offset from the window's base.

#define DEVICE_BASE 0x10000000
#define DEVICE_SIZE 0x100
#define OTHER_BASE 0x10001000

struct FakeDevice {
  uint64_t value;
  enum BusStatus status;
  unsigned calls;
  uxlen_t offset;
  unsigned size;
};

static struct FakeDevice device, other;

static enum BusStatus fake_read(void *context, uxlen_t offset, unsigned size,
                                uint64_t *value) {
  struct FakeDevice *fake = context;
  fake->calls++;
  fake->offset = offset;
  fake->size = size;
  *value = fake->value;
  return fake->status;
}

static enum BusStatus fake_write(void *context, uxlen_t offset, unsigned size,
                                 uint64_t value) {
  struct FakeDevice *fake = context;
  fake->calls++;
  fake->offset = offset;
  fake->size = size;
  fake->value = value;
  return fake->status;
}

static struct BusDevice window(struct FakeDevice *fake, uxlen_t base,
                               uxlen_t size) {
  *fake = (struct FakeDevice){.status = BUS_OK};
  return (struct BusDevice){.base = base,
                            .size = size,
                            .device = fake,
                            .read = fake_read,
                            .write = fake_write};
}

static void attach_device(void) {
  TEST_ASSERT_TRUE(bus_attach(bus, window(&device, DEVICE_BASE, DEVICE_SIZE)));
}

static void test_read_inside_a_device_reaches_it_with_the_offset(void) {
  attach_device();
  device.value = 0x1234;

  uint64_t value = 0;
  TEST_ASSERT_EQUAL(BUS_OK, bus_read(bus, DEVICE_BASE + 0x10, 4, &value));

  TEST_ASSERT_EQUAL_HEX64(0x1234, value);
  TEST_ASSERT_EQUAL_HEX64(0x10, device.offset);
  TEST_ASSERT_EQUAL(4, device.size);
}

static void test_write_inside_a_device_reaches_it_with_the_offset(void) {
  attach_device();

  TEST_ASSERT_EQUAL(BUS_OK, bus_write(bus, DEVICE_BASE + 0x8, 8, 0xABCD));

  TEST_ASSERT_EQUAL_HEX64(0xABCD, device.value);
  TEST_ASSERT_EQUAL_HEX64(0x8, device.offset);
  TEST_ASSERT_EQUAL(8, device.size);
}

static void test_a_fault_from_the_device_is_passed_on(void) {
  attach_device();
  device.status = BUS_ERROR;

  uint64_t value = 0;
  TEST_ASSERT_EQUAL(BUS_ERROR, bus_read(bus, DEVICE_BASE, 4, &value));
  TEST_ASSERT_EQUAL(BUS_ERROR, bus_write(bus, DEVICE_BASE, 4, 0));
}

static void test_access_just_past_a_device_is_a_bus_error(void) {
  attach_device();

  uint64_t value = 0;
  TEST_ASSERT_EQUAL(BUS_ERROR,
                    bus_read(bus, DEVICE_BASE + DEVICE_SIZE, 4, &value));
  TEST_ASSERT_EQUAL(0, device.calls);
}

// The device is not asked about an access that only partly lies inside it.
static void test_access_straddling_the_end_of_a_device_is_a_bus_error(void) {
  attach_device();

  uint64_t value = 0;
  TEST_ASSERT_EQUAL(BUS_ERROR,
                    bus_read(bus, DEVICE_BASE + DEVICE_SIZE - 2, 4, &value));
  TEST_ASSERT_EQUAL(BUS_ERROR,
                    bus_write(bus, DEVICE_BASE + DEVICE_SIZE - 2, 4, 0));
  TEST_ASSERT_EQUAL(0, device.calls);
}

static void test_access_straddling_the_start_of_a_device_is_a_bus_error(void) {
  attach_device();

  uint64_t value = 0;
  TEST_ASSERT_EQUAL(BUS_ERROR, bus_read(bus, DEVICE_BASE - 2, 4, &value));
  TEST_ASSERT_EQUAL(BUS_ERROR, bus_write(bus, DEVICE_BASE - 2, 4, 0));
  TEST_ASSERT_EQUAL(0, device.calls);
}

static void test_ram_still_works_with_a_device_attached(void) {
  attach_device();

  TEST_ASSERT_EQUAL(BUS_OK, bus_write(bus, RAM_BASE, 4, 0xCAFEBABE));
  TEST_ASSERT_EQUAL_HEX64(0xCAFEBABE, memory_read(ram, 0, 4));
  TEST_ASSERT_EQUAL(0, device.calls);
}

static void test_each_access_goes_to_its_own_device(void) {
  attach_device();
  TEST_ASSERT_TRUE(bus_attach(bus, window(&other, OTHER_BASE, DEVICE_SIZE)));
  device.value = 1;
  other.value = 2;

  uint64_t value = 0;
  bus_read(bus, OTHER_BASE, 4, &value);
  TEST_ASSERT_EQUAL_HEX64(2, value);
  bus_read(bus, DEVICE_BASE, 4, &value);
  TEST_ASSERT_EQUAL_HEX64(1, value);
}

static void test_a_device_cannot_overlap_another(void) {
  attach_device();

  TEST_ASSERT_FALSE(bus_attach(
      bus, window(&other, DEVICE_BASE + DEVICE_SIZE / 2, DEVICE_SIZE)));
}

static void test_a_device_cannot_overlap_ram(void) {
  TEST_ASSERT_FALSE(
      bus_attach(bus, window(&device, RAM_BASE + RAM_SIZE - 4, 0x100)));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_ram_is_mapped_at_ram_base);
  RUN_TEST(test_read_from_unmapped_address_is_a_bus_error);
  RUN_TEST(test_read_straddling_start_of_ram_is_a_bus_error);
  RUN_TEST(test_write_to_ram_base_lands_at_ram_offset_zero);
  RUN_TEST(test_write_to_unmapped_address_is_a_bus_error);
  RUN_TEST(test_access_straddling_end_of_ram_is_a_bus_error);
  RUN_TEST(test_read_inside_a_device_reaches_it_with_the_offset);
  RUN_TEST(test_write_inside_a_device_reaches_it_with_the_offset);
  RUN_TEST(test_a_fault_from_the_device_is_passed_on);
  RUN_TEST(test_access_just_past_a_device_is_a_bus_error);
  RUN_TEST(test_access_straddling_the_end_of_a_device_is_a_bus_error);
  RUN_TEST(test_access_straddling_the_start_of_a_device_is_a_bus_error);
  RUN_TEST(test_ram_still_works_with_a_device_attached);
  RUN_TEST(test_each_access_goes_to_its_own_device);
  RUN_TEST(test_a_device_cannot_overlap_another);
  RUN_TEST(test_a_device_cannot_overlap_ram);
  return UNITY_END();
}
