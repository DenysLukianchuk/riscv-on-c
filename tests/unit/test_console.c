#include "riscv/host/console.h"
#include "unity.h"

// Keys typed at the host terminal go to the guest, except after the escape
// key Ctrl-A, which starts a command for the emulator itself (as in QEMU):
// Ctrl-A x quits, Ctrl-A Ctrl-A sends one Ctrl-A to the guest.
#define CTRL_A 0x01

static struct ConsoleEscape escape;
static uint8_t out;

void setUp(void) {
  escape = (struct ConsoleEscape){0};
  out = 0;
}

void tearDown(void) {}

static void test_a_plain_key_goes_to_the_guest(void) {
  TEST_ASSERT_EQUAL(CONSOLE_SEND, console_key(&escape, 'a', &out));
  TEST_ASSERT_EQUAL_HEX8('a', out);
}

// Ctrl-C must reach the guest: there it stops a program, not the emulator.
static void test_ctrl_c_goes_to_the_guest(void) {
  TEST_ASSERT_EQUAL(CONSOLE_SEND, console_key(&escape, 0x03, &out));
  TEST_ASSERT_EQUAL_HEX8(0x03, out);
}

static void test_ctrl_a_alone_sends_nothing(void) {
  TEST_ASSERT_EQUAL(CONSOLE_NOTHING, console_key(&escape, CTRL_A, &out));
}

static void test_ctrl_a_then_x_quits(void) {
  console_key(&escape, CTRL_A, &out);

  TEST_ASSERT_EQUAL(CONSOLE_QUIT, console_key(&escape, 'x', &out));
}

static void test_ctrl_a_twice_sends_one_ctrl_a(void) {
  console_key(&escape, CTRL_A, &out);

  TEST_ASSERT_EQUAL(CONSOLE_SEND, console_key(&escape, CTRL_A, &out));
  TEST_ASSERT_EQUAL_HEX8(CTRL_A, out);
}

// An unknown command is dropped, and the key after it is an ordinary key.
static void test_ctrl_a_then_an_unknown_key_drops_it(void) {
  console_key(&escape, CTRL_A, &out);

  TEST_ASSERT_EQUAL(CONSOLE_NOTHING, console_key(&escape, 'q', &out));
  TEST_ASSERT_EQUAL(CONSOLE_SEND, console_key(&escape, 'x', &out));
  TEST_ASSERT_EQUAL_HEX8('x', out);
}

// x on its own is just a letter.
static void test_x_without_ctrl_a_goes_to_the_guest(void) {
  TEST_ASSERT_EQUAL(CONSOLE_SEND, console_key(&escape, 'x', &out));
  TEST_ASSERT_EQUAL_HEX8('x', out);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_a_plain_key_goes_to_the_guest);
  RUN_TEST(test_ctrl_c_goes_to_the_guest);
  RUN_TEST(test_ctrl_a_alone_sends_nothing);
  RUN_TEST(test_ctrl_a_then_x_quits);
  RUN_TEST(test_ctrl_a_twice_sends_one_ctrl_a);
  RUN_TEST(test_ctrl_a_then_an_unknown_key_drops_it);
  RUN_TEST(test_x_without_ctrl_a_goes_to_the_guest);
  return UNITY_END();
}
