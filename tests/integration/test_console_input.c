#include "riscv/host/console.h"
#include "riscv/system/machine.h"
#include "unity.h"

// Bytes typed at the host terminal go to the board's UART while its receive
// fifo has room; the rest wait in the caller's buffer for the next call.
// Ctrl-A x along the way asks to quit.
#define UART_ADDR 0x10000000
#define UART_RBR 0
#define UART_LSR 5
#define LSR_DR 0x01
#define RX_FIFO_BYTES 16
#define CTRL_A 0x01

static Machine *machine;
static struct ConsoleEscape escape;
static bool quit;

void setUp(void) {
  machine = machine_create(4096, NULL, NULL);
  escape = (struct ConsoleEscape){0};
  quit = false;
}

void tearDown(void) { machine_destroy(machine); }

static uint64_t uart_reg(unsigned reg) {
  uint64_t value = 0;
  bus_read(machine_get_bus(machine), UART_ADDR + reg, 1, &value);
  return value;
}

static size_t feed(const char *bytes, size_t count) {
  return console_feed(&escape, machine, (const uint8_t *)bytes, count, &quit);
}

static void test_typed_bytes_reach_the_uart_in_order(void) {
  TEST_ASSERT_EQUAL(3, feed("ls\r", 3));

  TEST_ASSERT_FALSE(quit);
  TEST_ASSERT_EQUAL_HEX8('l', uart_reg(UART_RBR));
  TEST_ASSERT_EQUAL_HEX8('s', uart_reg(UART_RBR));
  TEST_ASSERT_EQUAL_HEX8('\r', uart_reg(UART_RBR));
  TEST_ASSERT_BITS(LSR_DR, 0, uart_reg(UART_LSR));
}

// A paste longer than the fifo: only what fits is taken, nothing is lost.
static void test_feeding_stops_when_the_uart_is_full(void) {
  const char text[] = "abcdefghijklmnopqrst"; // 20 bytes

  TEST_ASSERT_EQUAL(RX_FIFO_BYTES, feed(text, 20));

  uart_reg(UART_RBR);
  TEST_ASSERT_EQUAL(1, feed(text + RX_FIFO_BYTES, 20 - RX_FIFO_BYTES));
}

static void test_ctrl_a_x_quits_and_is_not_sent(void) {
  const char keys[] = {'a', CTRL_A, 'x'};

  TEST_ASSERT_EQUAL(3, feed(keys, 3));

  TEST_ASSERT_TRUE(quit);
  TEST_ASSERT_EQUAL_HEX8('a', uart_reg(UART_RBR));
  TEST_ASSERT_BITS(LSR_DR, 0, uart_reg(UART_LSR));
}

// Nothing after the quit command is taken.
static void test_bytes_after_quit_are_left_alone(void) {
  const char keys[] = {CTRL_A, 'x', 'b', 'c'};

  TEST_ASSERT_EQUAL(2, feed(keys, 4));

  TEST_ASSERT_TRUE(quit);
  TEST_ASSERT_BITS(LSR_DR, 0, uart_reg(UART_LSR));
}

// The escape can arrive split across two reads from the terminal.
static void test_escape_spans_two_calls(void) {
  const char first[] = {CTRL_A};

  TEST_ASSERT_EQUAL(1, feed(first, 1));
  TEST_ASSERT_FALSE(quit);

  TEST_ASSERT_EQUAL(1, feed("x", 1));
  TEST_ASSERT_TRUE(quit);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_typed_bytes_reach_the_uart_in_order);
  RUN_TEST(test_feeding_stops_when_the_uart_is_full);
  RUN_TEST(test_ctrl_a_x_quits_and_is_not_sent);
  RUN_TEST(test_bytes_after_quit_are_left_alone);
  RUN_TEST(test_escape_spans_two_calls);
  return UNITY_END();
}
