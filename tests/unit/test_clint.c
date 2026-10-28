#include "riscv/clint.h"
#include "riscv_spec.h"
#include "unity.h"

static Clint *clint;

void setUp(void) { clint = clint_create(); }

void tearDown(void) { clint_destroy(clint); }

// Failures are reported at the caller's line.
#define read_reg(offset, size) read_reg_at(__LINE__, offset, size)
#define write_reg(offset, size, value)                                         \
  write_reg_at(__LINE__, offset, size, value)

static uint64_t read_reg_at(int line, uxlen_t offset, unsigned size) {
  uint64_t value = 0;
  UNITY_TEST_ASSERT_EQUAL_INT(BUS_OK, clint_read(clint, offset, size, &value),
                              line, "CLINT read rejected");
  return value;
}

static void write_reg_at(int line, uxlen_t offset, unsigned size,
                         uint64_t value) {
  UNITY_TEST_ASSERT_EQUAL_INT(BUS_OK, clint_write(clint, offset, size, value),
                              line, "CLINT write rejected");
}

// mtime: a 64-bit counter that only moves forward with time.

static void test_mtime_starts_at_zero(void) {
  TEST_ASSERT_EQUAL_HEX64(0, read_reg(CLINT_MTIME, 8));
}

static void test_mtime_advances_with_ticks(void) {
  clint_tick(clint, 5);
  clint_tick(clint, 3);

  TEST_ASSERT_EQUAL_HEX64(8, read_reg(CLINT_MTIME, 8));
}

static void test_mtime_can_be_set_by_software(void) {
  write_reg(CLINT_MTIME, 8, 0x123456789A);
  clint_tick(clint, 1);

  TEST_ASSERT_EQUAL_HEX64(0x123456789B, read_reg(CLINT_MTIME, 8));
}

// RV32 software reaches the 64-bit registers as two 32-bit halves.
static void test_mtime_halves_can_be_read_separately(void) {
  write_reg(CLINT_MTIME, 8, 0x1122334455667788);

  TEST_ASSERT_EQUAL_HEX64(0x55667788, read_reg(CLINT_MTIME, 4));
  TEST_ASSERT_EQUAL_HEX64(0x11223344, read_reg(CLINT_MTIME + 4, 4));
}

static void test_mtimecmp_halves_can_be_written_separately(void) {
  write_reg(CLINT_MTIMECMP, 4, 0x55667788);
  write_reg(CLINT_MTIMECMP + 4, 4, 0x11223344);

  TEST_ASSERT_EQUAL_HEX64(0x1122334455667788, read_reg(CLINT_MTIMECMP, 8));
}

// The timer interrupt (MTIP) is pending while mtime >= mtimecmp.

static void test_timer_is_not_pending_after_reset(void) {
  clint_tick(clint, 1000);

  TEST_ASSERT_FALSE(clint_timer_pending(clint));
}

static void test_timer_is_not_pending_before_mtimecmp(void) {
  write_reg(CLINT_MTIMECMP, 8, 10);
  clint_tick(clint, 9);

  TEST_ASSERT_FALSE(clint_timer_pending(clint));
}

static void test_timer_becomes_pending_at_mtimecmp(void) {
  write_reg(CLINT_MTIMECMP, 8, 10);
  clint_tick(clint, 10);

  TEST_ASSERT_TRUE(clint_timer_pending(clint));
}

static void test_timer_stays_pending_after_mtimecmp(void) {
  write_reg(CLINT_MTIMECMP, 8, 10);
  clint_tick(clint, 25);

  TEST_ASSERT_TRUE(clint_timer_pending(clint));
}

// The usual way to acknowledge a timer interrupt.
static void test_moving_mtimecmp_ahead_clears_the_timer(void) {
  write_reg(CLINT_MTIMECMP, 8, 10);
  clint_tick(clint, 10);

  write_reg(CLINT_MTIMECMP, 8, 20);

  TEST_ASSERT_FALSE(clint_timer_pending(clint));
}

// msip: the machine software interrupt (MSIP) of hart 0; only bit 0 exists.

static void test_software_interrupt_is_not_pending_after_reset(void) {
  TEST_ASSERT_FALSE(clint_software_pending(clint));
  TEST_ASSERT_EQUAL_HEX64(0, read_reg(CLINT_MSIP, 4));
}

static void test_writing_one_to_msip_raises_the_software_interrupt(void) {
  write_reg(CLINT_MSIP, 4, 1);

  TEST_ASSERT_TRUE(clint_software_pending(clint));
}

static void test_writing_zero_to_msip_clears_it(void) {
  write_reg(CLINT_MSIP, 4, 1);
  write_reg(CLINT_MSIP, 4, 0);

  TEST_ASSERT_FALSE(clint_software_pending(clint));
}

static void test_msip_keeps_only_bit_zero(void) {
  write_reg(CLINT_MSIP, 4, 0xFFFFFFFF);

  TEST_ASSERT_EQUAL_HEX64(1, read_reg(CLINT_MSIP, 4));
}

// Anything else in the CLINT window is an access fault: other harts'
// registers, odd sizes and misaligned accesses.

static void test_registers_of_other_harts_do_not_exist(void) {
  uint64_t value = 0;

  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT,
                    clint_read(clint, CLINT_MSIP + 4, 4, &value));
  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT,
                    clint_write(clint, CLINT_MTIMECMP + 8, 8, 1));
}

static void test_msip_is_only_reached_with_word_accesses(void) {
  uint64_t value = 0;

  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT, clint_read(clint, CLINT_MSIP, 1, &value));
  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT, clint_read(clint, CLINT_MSIP, 8, &value));
  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT, clint_write(clint, CLINT_MSIP, 1, 1));
  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT, clint_write(clint, CLINT_MSIP, 8, 1));
}

static void test_timer_registers_need_aligned_word_or_doubleword(void) {
  uint64_t value = 0;

  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT,
                    clint_read(clint, CLINT_MTIME, 2, &value));
  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT,
                    clint_read(clint, CLINT_MTIME + 4, 8, &value));
  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT,
                    clint_write(clint, CLINT_MTIMECMP + 2, 4, 0));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_mtime_starts_at_zero);
  RUN_TEST(test_mtime_advances_with_ticks);
  RUN_TEST(test_mtime_can_be_set_by_software);
  RUN_TEST(test_mtime_halves_can_be_read_separately);
  RUN_TEST(test_mtimecmp_halves_can_be_written_separately);
  RUN_TEST(test_timer_is_not_pending_after_reset);
  RUN_TEST(test_timer_is_not_pending_before_mtimecmp);
  RUN_TEST(test_timer_becomes_pending_at_mtimecmp);
  RUN_TEST(test_timer_stays_pending_after_mtimecmp);
  RUN_TEST(test_moving_mtimecmp_ahead_clears_the_timer);
  RUN_TEST(test_software_interrupt_is_not_pending_after_reset);
  RUN_TEST(test_writing_one_to_msip_raises_the_software_interrupt);
  RUN_TEST(test_writing_zero_to_msip_clears_it);
  RUN_TEST(test_msip_keeps_only_bit_zero);
  RUN_TEST(test_registers_of_other_harts_do_not_exist);
  RUN_TEST(test_msip_is_only_reached_with_word_accesses);
  RUN_TEST(test_timer_registers_need_aligned_word_or_doubleword);
  return UNITY_END();
}
