#include "riscv/devices/plic.h"
#include "riscv_spec.h"
#include "unity.h"

// Contexts of hart 0 on QEMU's virt board.
#define M_CONTEXT 0
#define S_CONTEXT 1

#define UART_SOURCE 10
#define OTHER_SOURCE 3
#define SOURCE_BIT(source) (1u << (source))
#define MAX_PRIORITY 7

// The board wires up sources 1..31 and two contexts.
#define MISSING_SOURCE 32
#define MISSING_CONTEXT 2

static Plic *plic;

void setUp(void) { plic = plic_create(); }

void tearDown(void) { plic_destroy(plic); }

// Failures are reported at the caller's line.
#define read_reg(offset) read_reg_at(__LINE__, offset)
#define write_reg(offset, value) write_reg_at(__LINE__, offset, value)

static uint32_t read_reg_at(int line, uxlen_t offset) {
  uint64_t value = 0;
  UNITY_TEST_ASSERT_EQUAL_INT(BUS_OK, plic_read(plic, offset, 4, &value), line,
                              "PLIC read rejected");
  return (uint32_t)value;
}

static void write_reg_at(int line, uxlen_t offset, uint32_t value) {
  UNITY_TEST_ASSERT_EQUAL_INT(BUS_OK, plic_write(plic, offset, 4, value), line,
                              "PLIC write rejected");
}

// A source that is raised, enabled for the context and above its threshold.
static void route(unsigned source, uint32_t priority, unsigned context) {
  write_reg(PLIC_PRIORITY(source), priority);
  write_reg(PLIC_ENABLE(context),
            read_reg(PLIC_ENABLE(context)) | SOURCE_BIT(source));
  plic_set_source(plic, source, true);
}

// Registers.

static void test_priorities_start_at_zero(void) {
  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_PRIORITY(UART_SOURCE)));
}

static void test_priority_keeps_three_bits(void) {
  write_reg(PLIC_PRIORITY(UART_SOURCE), 0xFF);

  TEST_ASSERT_EQUAL_HEX32(MAX_PRIORITY, read_reg(PLIC_PRIORITY(UART_SOURCE)));
}

// Source 0 means "no interrupt" and has no priority of its own.
static void test_source_zero_has_no_priority(void) {
  write_reg(PLIC_PRIORITY(0), MAX_PRIORITY);

  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_PRIORITY(0)));
}

static void test_enable_bits_read_back_except_source_zero(void) {
  write_reg(PLIC_ENABLE(M_CONTEXT), 0xFFFFFFFF);

  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFE, read_reg(PLIC_ENABLE(M_CONTEXT)));
}

static void test_threshold_keeps_three_bits(void) {
  write_reg(PLIC_THRESHOLD(M_CONTEXT), 0xFF);

  TEST_ASSERT_EQUAL_HEX32(MAX_PRIORITY, read_reg(PLIC_THRESHOLD(M_CONTEXT)));
}

static void test_each_context_has_its_own_enables_and_threshold(void) {
  write_reg(PLIC_ENABLE(S_CONTEXT), SOURCE_BIT(UART_SOURCE));
  write_reg(PLIC_THRESHOLD(S_CONTEXT), 2);

  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_ENABLE(M_CONTEXT)));
  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_THRESHOLD(M_CONTEXT)));
  TEST_ASSERT_EQUAL_HEX32(SOURCE_BIT(UART_SOURCE),
                          read_reg(PLIC_ENABLE(S_CONTEXT)));
  TEST_ASSERT_EQUAL_HEX32(2, read_reg(PLIC_THRESHOLD(S_CONTEXT)));
}

// Pending: a source's bit follows its line until the interrupt is claimed.

static void test_nothing_is_pending_after_reset(void) {
  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_PENDING));
}

static void test_raised_source_is_pending(void) {
  plic_set_source(plic, UART_SOURCE, true);

  TEST_ASSERT_EQUAL_HEX32(SOURCE_BIT(UART_SOURCE), read_reg(PLIC_PENDING));
}

static void test_lowered_source_is_no_longer_pending(void) {
  plic_set_source(plic, UART_SOURCE, true);
  plic_set_source(plic, UART_SOURCE, false);

  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_PENDING));
}

static void test_pending_bits_ignore_writes(void) {
  write_reg(PLIC_PENDING, 0xFFFFFFFF);

  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_PENDING));
}

// The line to a context: some pending source is enabled for it and has a
// priority above its threshold.

static void test_no_interrupt_after_reset(void) {
  TEST_ASSERT_FALSE(plic_interrupt_pending(plic, M_CONTEXT));
  TEST_ASSERT_FALSE(plic_interrupt_pending(plic, S_CONTEXT));
}

static void test_enabled_pending_source_interrupts_its_context(void) {
  route(UART_SOURCE, 1, M_CONTEXT);

  TEST_ASSERT_TRUE(plic_interrupt_pending(plic, M_CONTEXT));
  TEST_ASSERT_FALSE(plic_interrupt_pending(plic, S_CONTEXT));
}

static void test_disabled_source_does_not_interrupt(void) {
  write_reg(PLIC_PRIORITY(UART_SOURCE), 1);
  plic_set_source(plic, UART_SOURCE, true);

  TEST_ASSERT_FALSE(plic_interrupt_pending(plic, M_CONTEXT));
}

// Priority 0 means "never interrupt".
static void test_source_with_priority_zero_does_not_interrupt(void) {
  route(UART_SOURCE, 0, M_CONTEXT);

  TEST_ASSERT_FALSE(plic_interrupt_pending(plic, M_CONTEXT));
}

static void test_priority_must_be_above_the_threshold(void) {
  route(UART_SOURCE, 3, S_CONTEXT);

  write_reg(PLIC_THRESHOLD(S_CONTEXT), 3);
  TEST_ASSERT_FALSE(plic_interrupt_pending(plic, S_CONTEXT));

  write_reg(PLIC_THRESHOLD(S_CONTEXT), 2);
  TEST_ASSERT_TRUE(plic_interrupt_pending(plic, S_CONTEXT));
}

// Claim and complete: a handler reads the claim register to learn which
// source to serve, and writes the same ID back once it is done.

static void test_claim_returns_the_source(void) {
  route(UART_SOURCE, 1, M_CONTEXT);

  TEST_ASSERT_EQUAL_HEX32(UART_SOURCE, read_reg(PLIC_CLAIM(M_CONTEXT)));
}

static void test_claim_with_nothing_pending_returns_zero(void) {
  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_CLAIM(M_CONTEXT)));
}

static void test_claimed_source_is_quiet_until_completed(void) {
  route(UART_SOURCE, 1, M_CONTEXT);

  read_reg(PLIC_CLAIM(M_CONTEXT));

  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_PENDING));
  TEST_ASSERT_FALSE(plic_interrupt_pending(plic, M_CONTEXT));
  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_CLAIM(M_CONTEXT)));
}

// The UART still has data, so its line is still up after the handler is done.
static void test_source_still_raised_after_complete_is_pending_again(void) {
  route(UART_SOURCE, 1, M_CONTEXT);
  read_reg(PLIC_CLAIM(M_CONTEXT));

  write_reg(PLIC_CLAIM(M_CONTEXT), UART_SOURCE);

  TEST_ASSERT_TRUE(plic_interrupt_pending(plic, M_CONTEXT));
  TEST_ASSERT_EQUAL_HEX32(UART_SOURCE, read_reg(PLIC_CLAIM(M_CONTEXT)));
}

static void test_completing_a_source_that_was_not_claimed_is_ignored(void) {
  route(UART_SOURCE, 1, M_CONTEXT);
  route(OTHER_SOURCE, 1, M_CONTEXT);
  read_reg(PLIC_CLAIM(M_CONTEXT)); // takes OTHER_SOURCE, the lower ID

  write_reg(PLIC_CLAIM(M_CONTEXT), UART_SOURCE);

  TEST_ASSERT_EQUAL_HEX32(SOURCE_BIT(UART_SOURCE), read_reg(PLIC_PENDING));
}

static void test_completing_from_a_context_without_the_source_is_ignored(void) {
  route(UART_SOURCE, 1, M_CONTEXT);
  read_reg(PLIC_CLAIM(M_CONTEXT));

  write_reg(PLIC_CLAIM(S_CONTEXT), UART_SOURCE);

  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_PENDING));
}

static void test_claim_takes_the_highest_priority_first(void) {
  route(OTHER_SOURCE, 1, M_CONTEXT);
  route(UART_SOURCE, 5, M_CONTEXT);

  TEST_ASSERT_EQUAL_HEX32(UART_SOURCE, read_reg(PLIC_CLAIM(M_CONTEXT)));
  TEST_ASSERT_EQUAL_HEX32(OTHER_SOURCE, read_reg(PLIC_CLAIM(M_CONTEXT)));
}

static void test_equal_priorities_go_to_the_lowest_id(void) {
  route(UART_SOURCE, 2, M_CONTEXT);
  route(OTHER_SOURCE, 2, M_CONTEXT);

  TEST_ASSERT_EQUAL_HEX32(OTHER_SOURCE, read_reg(PLIC_CLAIM(M_CONTEXT)));
}

static void test_claim_skips_sources_at_or_below_the_threshold(void) {
  route(OTHER_SOURCE, 1, M_CONTEXT);
  write_reg(PLIC_THRESHOLD(M_CONTEXT), 1);

  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_CLAIM(M_CONTEXT)));
}

static void test_claim_only_sees_sources_enabled_for_its_context(void) {
  route(UART_SOURCE, 1, S_CONTEXT);

  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_CLAIM(M_CONTEXT)));
  TEST_ASSERT_EQUAL_HEX32(UART_SOURCE, read_reg(PLIC_CLAIM(S_CONTEXT)));
}

// Words the PLIC does not implement: reads give 0, writes are ignored.

static void test_second_enable_word_is_separate_from_the_first(void) {
  write_reg(PLIC_ENABLE(M_CONTEXT) + 4, 0xFFFFFFFF);

  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_ENABLE(M_CONTEXT)));
  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_ENABLE(M_CONTEXT) + 4));
}

static void test_second_pending_word_is_empty(void) {
  route(UART_SOURCE, 1, M_CONTEXT);

  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_PENDING + 4));
}

static void test_word_after_claim_does_not_claim(void) {
  route(UART_SOURCE, 1, M_CONTEXT);

  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_CLAIM(M_CONTEXT) + 4));
  TEST_ASSERT_EQUAL_HEX32(SOURCE_BIT(UART_SOURCE), read_reg(PLIC_PENDING));
}

static void test_missing_source_priority_reads_zero(void) {
  write_reg(PLIC_PRIORITY(MISSING_SOURCE), MAX_PRIORITY);

  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_PRIORITY(MISSING_SOURCE)));
}

static void test_missing_context_registers_read_zero(void) {
  route(UART_SOURCE, 1, M_CONTEXT);

  write_reg(PLIC_ENABLE(MISSING_CONTEXT), 0xFFFFFFFF);
  write_reg(PLIC_THRESHOLD(MISSING_CONTEXT), MAX_PRIORITY);
  write_reg(PLIC_CLAIM(MISSING_CONTEXT), UART_SOURCE);

  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_ENABLE(MISSING_CONTEXT)));
  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_THRESHOLD(MISSING_CONTEXT)));
  TEST_ASSERT_EQUAL_HEX32(0, read_reg(PLIC_CLAIM(MISSING_CONTEXT)));
}

// Access: the registers are 32 bits wide.

static void test_only_aligned_word_accesses_reach_the_registers(void) {
  uint64_t value = 0;

  TEST_ASSERT_EQUAL(BUS_ERROR,
                    plic_read(plic, PLIC_PRIORITY(UART_SOURCE), 1, &value));
  TEST_ASSERT_EQUAL(BUS_ERROR,
                    plic_read(plic, PLIC_PRIORITY(UART_SOURCE), 8, &value));
  TEST_ASSERT_EQUAL(BUS_ERROR,
                    plic_write(plic, PLIC_PRIORITY(UART_SOURCE) + 2, 4, 1));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_priorities_start_at_zero);
  RUN_TEST(test_priority_keeps_three_bits);
  RUN_TEST(test_source_zero_has_no_priority);
  RUN_TEST(test_enable_bits_read_back_except_source_zero);
  RUN_TEST(test_threshold_keeps_three_bits);
  RUN_TEST(test_each_context_has_its_own_enables_and_threshold);
  RUN_TEST(test_nothing_is_pending_after_reset);
  RUN_TEST(test_raised_source_is_pending);
  RUN_TEST(test_lowered_source_is_no_longer_pending);
  RUN_TEST(test_pending_bits_ignore_writes);
  RUN_TEST(test_no_interrupt_after_reset);
  RUN_TEST(test_enabled_pending_source_interrupts_its_context);
  RUN_TEST(test_disabled_source_does_not_interrupt);
  RUN_TEST(test_source_with_priority_zero_does_not_interrupt);
  RUN_TEST(test_priority_must_be_above_the_threshold);
  RUN_TEST(test_claim_returns_the_source);
  RUN_TEST(test_claim_with_nothing_pending_returns_zero);
  RUN_TEST(test_claimed_source_is_quiet_until_completed);
  RUN_TEST(test_source_still_raised_after_complete_is_pending_again);
  RUN_TEST(test_completing_a_source_that_was_not_claimed_is_ignored);
  RUN_TEST(test_completing_from_a_context_without_the_source_is_ignored);
  RUN_TEST(test_claim_takes_the_highest_priority_first);
  RUN_TEST(test_equal_priorities_go_to_the_lowest_id);
  RUN_TEST(test_claim_skips_sources_at_or_below_the_threshold);
  RUN_TEST(test_claim_only_sees_sources_enabled_for_its_context);
  RUN_TEST(test_second_enable_word_is_separate_from_the_first);
  RUN_TEST(test_second_pending_word_is_empty);
  RUN_TEST(test_word_after_claim_does_not_claim);
  RUN_TEST(test_missing_source_priority_reads_zero);
  RUN_TEST(test_missing_context_registers_read_zero);
  RUN_TEST(test_only_aligned_word_accesses_reach_the_registers);
  return UNITY_END();
}
