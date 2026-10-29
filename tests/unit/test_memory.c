#include "riscv/devices/memory.h"
#include "unity.h"

#define MEM_SIZE 1024

static Memory *mem;

void setUp(void) { mem = memory_create(MEM_SIZE); }

void tearDown(void) { memory_destroy(mem); }

static void test_written_byte_can_be_read_back(void) {
  memory_write(mem, 0x10, 1, 0xAB);

  TEST_ASSERT_EQUAL_HEX8(0xAB, memory_read(mem, 0x10, 1));
}

static void test_halfword_is_read_in_little_endian_order(void) {
  memory_write(mem, 0x60, 1, 0x34);
  memory_write(mem, 0x61, 1, 0x12);

  TEST_ASSERT_EQUAL_HEX64(0x1234, memory_read(mem, 0x60, 2));
}

static void test_halfword_is_written_in_little_endian_order(void) {
  memory_write(mem, 0x70, 2, 0xBEEF);

  TEST_ASSERT_EQUAL_HEX8(0xEF, memory_read(mem, 0x70, 1));
  TEST_ASSERT_EQUAL_HEX8(0xBE, memory_read(mem, 0x71, 1));
}

static void test_word_is_read_in_little_endian_order(void) {
  memory_write(mem, 0x20, 1, 0x78);
  memory_write(mem, 0x21, 1, 0x56);
  memory_write(mem, 0x22, 1, 0x34);
  memory_write(mem, 0x23, 1, 0x12);

  TEST_ASSERT_EQUAL_HEX64(0x12345678, memory_read(mem, 0x20, 4));
}

static void test_word_is_written_in_little_endian_order(void) {
  memory_write(mem, 0x40, 4, 0xDEADBEEF);

  TEST_ASSERT_EQUAL_HEX8(0xEF, memory_read(mem, 0x40, 1));
  TEST_ASSERT_EQUAL_HEX8(0xBE, memory_read(mem, 0x41, 1));
  TEST_ASSERT_EQUAL_HEX8(0xAD, memory_read(mem, 0x42, 1));
  TEST_ASSERT_EQUAL_HEX8(0xDE, memory_read(mem, 0x43, 1));
}

static void test_word_with_high_bit_set_round_trips(void) {
  memory_write(mem, 0x80, 4, 0xDEADBEEF);

  TEST_ASSERT_EQUAL_HEX64(0xDEADBEEF, memory_read(mem, 0x80, 4));
}

static void test_doubleword_with_high_bit_set_round_trips(void) {
  memory_write(mem, 0x90, 8, 0x8877665544332211);

  TEST_ASSERT_EQUAL_HEX64(0x8877665544332211, memory_read(mem, 0x90, 8));
}

static void test_narrow_write_does_not_touch_neighbour_bytes(void) {
  memory_write(mem, 0xA0, 4, 0xFFFFFFFF);

  memory_write(mem, 0xA1, 1, 0x00);

  TEST_ASSERT_EQUAL_HEX64(0xFFFF00FF, memory_read(mem, 0xA0, 4));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_written_byte_can_be_read_back);
  RUN_TEST(test_halfword_is_read_in_little_endian_order);
  RUN_TEST(test_halfword_is_written_in_little_endian_order);
  RUN_TEST(test_word_is_read_in_little_endian_order);
  RUN_TEST(test_word_is_written_in_little_endian_order);
  RUN_TEST(test_word_with_high_bit_set_round_trips);
  RUN_TEST(test_doubleword_with_high_bit_set_round_trips);
  RUN_TEST(test_narrow_write_does_not_touch_neighbour_bytes);
  return UNITY_END();
}
