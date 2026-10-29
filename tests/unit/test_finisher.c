#include "riscv/devices/finisher.h"
#include "unity.h"

// QEMU's sifive_test device: one write-only word that powers the board off.
#define FINISHER_PASS 0x5555
#define FINISHER_FAIL 0x3333
#define FAIL_WITH(code) (((uint32_t)(code) << 16) | FINISHER_FAIL)

static Finisher *finisher;

void setUp(void) { finisher = finisher_create(); }

void tearDown(void) { finisher_destroy(finisher); }

static void write_word(uxlen_t offset, uint32_t value) {
  TEST_ASSERT_EQUAL(BUS_OK, finisher_write(finisher, offset, 4, value));
}

static void test_board_runs_after_reset(void) {
  TEST_ASSERT_FALSE(finisher_finished(finisher));
}

static void test_pass_powers_off_with_code_zero(void) {
  write_word(0, FINISHER_PASS);

  TEST_ASSERT_TRUE(finisher_finished(finisher));
  TEST_ASSERT_EQUAL(0, finisher_exit_code(finisher));
}

static void test_fail_powers_off_with_the_code_in_the_upper_half(void) {
  write_word(0, FAIL_WITH(3));

  TEST_ASSERT_TRUE(finisher_finished(finisher));
  TEST_ASSERT_EQUAL(3, finisher_exit_code(finisher));
}

static void test_pass_ignores_the_upper_half(void) {
  write_word(0, (5u << 16) | FINISHER_PASS);

  TEST_ASSERT_EQUAL(0, finisher_exit_code(finisher));
}

static void test_other_values_are_ignored(void) {
  write_word(0, 0x1234);

  TEST_ASSERT_FALSE(finisher_finished(finisher));
}

static void test_reads_give_zero(void) {
  uint64_t value = 0xFF;

  TEST_ASSERT_EQUAL(BUS_OK, finisher_read(finisher, 0, 4, &value));
  TEST_ASSERT_EQUAL_HEX64(0, value);
}

static void test_writes_past_the_register_are_ignored(void) {
  write_word(4, FINISHER_PASS);

  TEST_ASSERT_FALSE(finisher_finished(finisher));
}

static void test_only_word_accesses_reach_the_register(void) {
  uint64_t value = 0;

  TEST_ASSERT_EQUAL(BUS_ERROR, finisher_write(finisher, 0, 2, FINISHER_PASS));
  TEST_ASSERT_EQUAL(BUS_ERROR, finisher_write(finisher, 0, 8, FINISHER_PASS));
  TEST_ASSERT_EQUAL(BUS_ERROR, finisher_read(finisher, 0, 1, &value));
  TEST_ASSERT_FALSE(finisher_finished(finisher));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_board_runs_after_reset);
  RUN_TEST(test_pass_powers_off_with_code_zero);
  RUN_TEST(test_fail_powers_off_with_the_code_in_the_upper_half);
  RUN_TEST(test_pass_ignores_the_upper_half);
  RUN_TEST(test_other_values_are_ignored);
  RUN_TEST(test_reads_give_zero);
  RUN_TEST(test_writes_past_the_register_are_ignored);
  RUN_TEST(test_only_word_accesses_reach_the_register);
  return UNITY_END();
}
