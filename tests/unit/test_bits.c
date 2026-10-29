#include "riscv/util/bits.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

static void test_bits_extracts_field_from_the_middle(void) {
  TEST_ASSERT_EQUAL_HEX64(0x1F, bits(0x00000F80, 11, 7));
}

static void test_bits_extracts_single_bit(void) {
  TEST_ASSERT_EQUAL_HEX64(1, bits(0x80000000, 31, 31));
  TEST_ASSERT_EQUAL_HEX64(0, bits(0x7FFFFFFF, 31, 31));
}

static void test_bits_extracts_lowest_bits(void) {
  TEST_ASSERT_EQUAL_HEX64(0x13, bits(0x00500093, 6, 0));
}

static void test_bits_extracts_full_64_bit_width(void) {
  TEST_ASSERT_EQUAL_HEX64(0x8877665544332211, bits(0x8877665544332211, 63, 0));
}

static void test_bits_extracts_top_of_64_bit_value(void) {
  TEST_ASSERT_EQUAL_HEX64(0x88, bits(0x8877665544332211, 63, 56));
}

static void test_clear_bits_clears_field_and_keeps_the_rest(void) {
  TEST_ASSERT_EQUAL_HEX64(0x80000100, clear_bits(0x80000103, 1, 0));
}

static void test_clear_bits_clears_single_bit(void) {
  TEST_ASSERT_EQUAL_HEX64(0x80001234, clear_bits(0x80001235, 0, 0));
}

static void test_clear_bits_clears_top_of_64_bit_value(void) {
  TEST_ASSERT_EQUAL_HEX64(0x0077665544332211,
                          clear_bits(0x8877665544332211, 63, 56));
}

static void test_clear_bits_clears_full_64_bit_width(void) {
  TEST_ASSERT_EQUAL_HEX64(0, clear_bits(0x8877665544332211, 63, 0));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_bits_extracts_field_from_the_middle);
  RUN_TEST(test_bits_extracts_single_bit);
  RUN_TEST(test_bits_extracts_lowest_bits);
  RUN_TEST(test_bits_extracts_full_64_bit_width);
  RUN_TEST(test_bits_extracts_top_of_64_bit_value);
  RUN_TEST(test_clear_bits_clears_field_and_keeps_the_rest);
  RUN_TEST(test_clear_bits_clears_single_bit);
  RUN_TEST(test_clear_bits_clears_top_of_64_bit_value);
  RUN_TEST(test_clear_bits_clears_full_64_bit_width);
  return UNITY_END();
}
