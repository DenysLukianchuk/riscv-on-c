#include "riscv/util/fifo.h"
#include "unity.h"

// Small on purpose: the size is the owner's choice, not the fifo's.
#define CAPACITY 4

static uint8_t storage[CAPACITY];
static struct Fifo fifo;

void setUp(void) { fifo_init(&fifo, storage, CAPACITY); }

void tearDown(void) {}

static void test_a_new_fifo_is_empty(void) {
  TEST_ASSERT_TRUE(fifo_empty(&fifo));
}

static void test_a_pushed_byte_makes_it_non_empty(void) {
  fifo_push(&fifo, 'a');

  TEST_ASSERT_FALSE(fifo_empty(&fifo));
}

static void test_bytes_come_out_in_the_order_they_went_in(void) {
  fifo_push(&fifo, 'a');
  fifo_push(&fifo, 'b');

  TEST_ASSERT_EQUAL_HEX8('a', fifo_pop(&fifo));
  TEST_ASSERT_EQUAL_HEX8('b', fifo_pop(&fifo));
  TEST_ASSERT_TRUE(fifo_empty(&fifo));
}

static void test_popping_an_empty_fifo_gives_zero(void) {
  TEST_ASSERT_EQUAL_HEX8(0, fifo_pop(&fifo));
  TEST_ASSERT_TRUE(fifo_empty(&fifo));
}

// A full fifo keeps what it has and refuses the new byte.
static void test_a_full_fifo_refuses_new_bytes(void) {
  for (unsigned i = 0; i < CAPACITY; i++)
    TEST_ASSERT_TRUE(fifo_push(&fifo, (uint8_t)i));

  TEST_ASSERT_FALSE(fifo_push(&fifo, 0xFF));
  TEST_ASSERT_EQUAL_HEX8(0, fifo_pop(&fifo));
}

// Pushing and popping past the end of the storage wraps around.
static void test_the_fifo_is_full_at_capacity(void) {
  for (int i = 0; i < CAPACITY - 1; i++)
    fifo_push(&fifo, i);
  TEST_ASSERT_FALSE(fifo_full(&fifo));

  fifo_push(&fifo, 0);
  TEST_ASSERT_TRUE(fifo_full(&fifo));

  fifo_pop(&fifo);
  TEST_ASSERT_FALSE(fifo_full(&fifo));
}

static void test_the_fifo_wraps_around(void) {
  for (unsigned round = 0; round < 3; round++) {
    for (unsigned i = 0; i < CAPACITY; i++)
      fifo_push(&fifo, (uint8_t)(round * CAPACITY + i));
    for (unsigned i = 0; i < CAPACITY; i++)
      TEST_ASSERT_EQUAL_HEX8(round * CAPACITY + i, fifo_pop(&fifo));
  }
  TEST_ASSERT_TRUE(fifo_empty(&fifo));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_a_new_fifo_is_empty);
  RUN_TEST(test_a_pushed_byte_makes_it_non_empty);
  RUN_TEST(test_bytes_come_out_in_the_order_they_went_in);
  RUN_TEST(test_popping_an_empty_fifo_gives_zero);
  RUN_TEST(test_a_full_fifo_refuses_new_bytes);
  RUN_TEST(test_the_fifo_is_full_at_capacity);
  RUN_TEST(test_the_fifo_wraps_around);
  return UNITY_END();
}
