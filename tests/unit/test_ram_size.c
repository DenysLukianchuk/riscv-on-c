#include "riscv/host/ram_size.h"
#include "unity.h"

// The -m option: a RAM size like QEMU's, a number with an optional K, M or G
// suffix. A bare number counts megabytes.

#define KIB 1024ull
#define MIB (1024 * KIB)
#define GIB (1024 * MIB)

void setUp(void) {}
void tearDown(void) {}

static uint64_t parsed(const char *text) {
  uint64_t bytes = 0;
  TEST_ASSERT_TRUE_MESSAGE(ram_size_parse(text, &bytes), text);
  return bytes;
}

static void assert_rejected(const char *text) {
  uint64_t bytes = 0;
  TEST_ASSERT_FALSE_MESSAGE(ram_size_parse(text, &bytes), text);
}

static void test_a_bare_number_is_megabytes(void) {
  TEST_ASSERT_EQUAL_UINT64(256 * MIB, parsed("256"));
}

static void test_suffixes(void) {
  TEST_ASSERT_EQUAL_UINT64(512 * KIB, parsed("512K"));
  TEST_ASSERT_EQUAL_UINT64(64 * MIB, parsed("64M"));
  TEST_ASSERT_EQUAL_UINT64(2 * GIB, parsed("2G"));
}

static void test_suffixes_are_case_insensitive(void) {
  TEST_ASSERT_EQUAL_UINT64(64 * MIB, parsed("64m"));
  TEST_ASSERT_EQUAL_UINT64(1 * GIB, parsed("1g"));
}

static void test_malformed_sizes_are_rejected(void) {
  const char *malformed[] = {"",     "M",  "abc",  "12X",
                             "12MB", "-5", "1.5G", " 64"};
  for (unsigned i = 0; i < sizeof malformed / sizeof malformed[0]; i++)
    assert_rejected(malformed[i]);
}

static void test_zero_is_rejected(void) { assert_rejected("0"); }

// RAM starts at 0x80000000. On RV32 it must end below 4 GiB, so 2 GiB is the
// most there is room for.
static void test_ram_must_fit_the_address_space(void) {
#if XLEN == 32
  TEST_ASSERT_EQUAL_UINT64(2 * GIB, parsed("2G"));
  assert_rejected("2049M");
#else
  TEST_ASSERT_EQUAL_UINT64(64 * GIB, parsed("64G"));
#endif
}

static void test_sizes_that_overflow_are_rejected(void) {
  assert_rejected("99999999999999999999");
  assert_rejected("17179869184G");
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_a_bare_number_is_megabytes);
  RUN_TEST(test_suffixes);
  RUN_TEST(test_suffixes_are_case_insensitive);
  RUN_TEST(test_malformed_sizes_are_rejected);
  RUN_TEST(test_zero_is_rejected);
  RUN_TEST(test_ram_must_fit_the_address_space);
  RUN_TEST(test_sizes_that_overflow_are_rejected);
  return UNITY_END();
}
