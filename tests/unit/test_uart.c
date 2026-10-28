#include "riscv/uart.h"
#include "unity.h"

// NS16550A registers (byte offsets) and bits, from the datasheet.
#define RBR 0 // read: receive buffer
#define THR 0 // write: transmit holding
#define DLL 0 // with LCR.DLAB: divisor latch, low byte
#define IER 1
#define DLM 1 // with LCR.DLAB: divisor latch, high byte
#define IIR 2 // read
#define FCR 2 // write
#define LCR 3
#define MCR 4
#define LSR 5
#define SCR 7

#define LCR_DLAB 0x80
#define LCR_8N1 0x03
#define LSR_DR 0x01   // data ready
#define LSR_THRE 0x20 // transmit holding register empty
#define LSR_TEMT 0x40 // transmitter empty
#define IIR_NO_INTERRUPT 0x01

static Uart *uart;
static char sent[64];
static unsigned sent_count;

static void capture(void *context, uint8_t byte) {
  (void)context;
  sent[sent_count++] = (char)byte;
  sent[sent_count] = '\0';
}

void setUp(void) {
  sent_count = 0;
  sent[0] = '\0';
  uart = uart_create(capture, NULL);
}

void tearDown(void) { uart_destroy(uart); }

// Failures are reported at the caller's line.
#define read_reg(offset) read_reg_at(__LINE__, offset)
#define write_reg(offset, value) write_reg_at(__LINE__, offset, value)

static uint8_t read_reg_at(int line, uxlen_t offset) {
  uint64_t value = 0;
  UNITY_TEST_ASSERT_EQUAL_INT(BUS_OK, uart_read(uart, offset, 1, &value), line,
                              "UART read rejected");
  return (uint8_t)value;
}

static void write_reg_at(int line, uxlen_t offset, uint8_t value) {
  UNITY_TEST_ASSERT_EQUAL_INT(BUS_OK, uart_write(uart, offset, 1, value), line,
                              "UART write rejected");
}

// Sending: a byte written to THR goes straight out.

static void test_writing_thr_sends_the_byte(void) {
  write_reg(THR, 'A');

  TEST_ASSERT_EQUAL_STRING("A", sent);
}

static void test_bytes_are_sent_in_order(void) {
  write_reg(THR, 'H');
  write_reg(THR, 'i');

  TEST_ASSERT_EQUAL_STRING("Hi", sent);
}

// Software polls LSR.THRE before each byte; the transmitter never gets busy.
static void test_transmitter_is_always_ready(void) {
  TEST_ASSERT_BITS(LSR_THRE | LSR_TEMT, LSR_THRE | LSR_TEMT, read_reg(LSR));

  write_reg(THR, 'A');

  TEST_ASSERT_BITS(LSR_THRE | LSR_TEMT, LSR_THRE | LSR_TEMT, read_reg(LSR));
}

// Receiving: bytes from the host wait in a FIFO until software reads RBR.

static void test_nothing_is_ready_after_reset(void) {
  TEST_ASSERT_BITS(LSR_DR, 0, read_reg(LSR));
}

static void test_received_byte_sets_data_ready(void) {
  uart_receive(uart, 'x');

  TEST_ASSERT_BITS(LSR_DR, LSR_DR, read_reg(LSR));
}

static void test_reading_rbr_takes_the_byte(void) {
  uart_receive(uart, 'x');

  TEST_ASSERT_EQUAL_HEX8('x', read_reg(RBR));
  TEST_ASSERT_BITS(LSR_DR, 0, read_reg(LSR));
}

static void test_received_bytes_are_read_in_order(void) {
  uart_receive(uart, 'a');
  uart_receive(uart, 'b');

  TEST_ASSERT_EQUAL_HEX8('a', read_reg(RBR));
  TEST_ASSERT_BITS(LSR_DR, LSR_DR, read_reg(LSR));
  TEST_ASSERT_EQUAL_HEX8('b', read_reg(RBR));
}

// RBR and THR share offset 0 but are separate registers.
static void test_rbr_does_not_echo_the_last_byte_sent(void) {
  write_reg(THR, 'A');

  TEST_ASSERT_EQUAL_HEX8(0, read_reg(RBR));
}

static void test_reading_rbr_with_nothing_received_gives_zero(void) {
  TEST_ASSERT_EQUAL_HEX8(0, read_reg(RBR));
  TEST_ASSERT_BITS(LSR_DR, 0, read_reg(LSR));
}

// LCR.DLAB turns offsets 0 and 1 into the baud-rate divisor, which firmware
// programs at start-up. It only matters to real wires, so it is just stored.

static void test_divisor_latch_is_written_without_sending(void) {
  write_reg(LCR, LCR_DLAB);
  write_reg(DLL, 0x03);
  write_reg(DLM, 0x01);

  TEST_ASSERT_EQUAL_STRING("", sent);
  TEST_ASSERT_EQUAL_HEX8(0x03, read_reg(DLL));
  TEST_ASSERT_EQUAL_HEX8(0x01, read_reg(DLM));
}

static void test_clearing_dlab_returns_to_thr_and_ier(void) {
  write_reg(LCR, LCR_DLAB);
  write_reg(DLM, 0x01);
  write_reg(LCR, LCR_8N1);

  write_reg(THR, 'A');

  TEST_ASSERT_EQUAL_STRING("A", sent);
  TEST_ASSERT_EQUAL_HEX8(0, read_reg(IER));
}

static void test_ier_survives_a_divisor_change(void) {
  write_reg(IER, 0x03);
  write_reg(LCR, LCR_DLAB);
  write_reg(DLL, 0x03);
  write_reg(DLM, 0x00);
  write_reg(LCR, LCR_8N1);

  TEST_ASSERT_EQUAL_HEX8(0x03, read_reg(IER));
}

// Drivers usually set DLAB together with the line format.
static void test_dlab_works_alongside_other_lcr_bits(void) {
  write_reg(LCR, LCR_DLAB | LCR_8N1);
  write_reg(DLL, 0x03);

  TEST_ASSERT_EQUAL_STRING("", sent);
  TEST_ASSERT_EQUAL_HEX8(0x03, read_reg(DLL));
}

static void test_reading_the_divisor_leaves_received_bytes_alone(void) {
  uart_receive(uart, 'x');
  write_reg(LCR, LCR_DLAB);
  read_reg(DLL);
  write_reg(LCR, LCR_8N1);

  TEST_ASSERT_EQUAL_HEX8('x', read_reg(RBR));
}

static void test_lcr_starts_at_zero(void) {
  TEST_ASSERT_EQUAL_HEX8(0, read_reg(LCR));
}

static void test_lcr_reads_back(void) {
  write_reg(LCR, LCR_8N1);

  TEST_ASSERT_EQUAL_HEX8(LCR_8N1, read_reg(LCR));
}

// The other registers.

static void test_ier_keeps_its_four_enable_bits(void) {
  write_reg(IER, 0xFF);

  TEST_ASSERT_EQUAL_HEX8(0x0F, read_reg(IER));
}

static void test_mcr_keeps_its_five_control_bits(void) {
  write_reg(MCR, 0xFF);

  TEST_ASSERT_EQUAL_HEX8(0x1F, read_reg(MCR));
}

static void test_scratch_register_holds_a_byte(void) {
  write_reg(SCR, 0x5A);

  TEST_ASSERT_EQUAL_HEX8(0x5A, read_reg(SCR));
}

// No interrupts until the UART is wired to the PLIC.
static void test_iir_reports_no_interrupt(void) {
  write_reg(FCR, 0x07);

  TEST_ASSERT_BITS(IIR_NO_INTERRUPT, IIR_NO_INTERRUPT, read_reg(IIR));
}

// IIR and FCR share offset 2 too.
static void test_writing_fcr_does_not_change_iir(void) {
  write_reg(FCR, 0x06);

  TEST_ASSERT_BITS(IIR_NO_INTERRUPT, IIR_NO_INTERRUPT, read_reg(IIR));
}

static void test_lsr_ignores_writes(void) {
  write_reg(LSR, 0x00);

  TEST_ASSERT_BITS(LSR_THRE | LSR_TEMT, LSR_THRE | LSR_TEMT, read_reg(LSR));
}

// The registers are bytes, and only eight of them exist.

static void test_only_byte_accesses_reach_the_registers(void) {
  uint64_t value = 0;

  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT, uart_read(uart, LSR, 4, &value));
  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT, uart_write(uart, THR, 2, 'A'));
  TEST_ASSERT_EQUAL_STRING("", sent);
}

static void test_offsets_past_the_registers_are_access_faults(void) {
  uint64_t value = 0;

  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT, uart_read(uart, 8, 1, &value));
  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT, uart_write(uart, 0xFF, 1, 0));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_writing_thr_sends_the_byte);
  RUN_TEST(test_bytes_are_sent_in_order);
  RUN_TEST(test_transmitter_is_always_ready);
  RUN_TEST(test_nothing_is_ready_after_reset);
  RUN_TEST(test_received_byte_sets_data_ready);
  RUN_TEST(test_reading_rbr_takes_the_byte);
  RUN_TEST(test_received_bytes_are_read_in_order);
  RUN_TEST(test_rbr_does_not_echo_the_last_byte_sent);
  RUN_TEST(test_reading_rbr_with_nothing_received_gives_zero);
  RUN_TEST(test_divisor_latch_is_written_without_sending);
  RUN_TEST(test_clearing_dlab_returns_to_thr_and_ier);
  RUN_TEST(test_ier_survives_a_divisor_change);
  RUN_TEST(test_dlab_works_alongside_other_lcr_bits);
  RUN_TEST(test_reading_the_divisor_leaves_received_bytes_alone);
  RUN_TEST(test_lcr_starts_at_zero);
  RUN_TEST(test_lcr_reads_back);
  RUN_TEST(test_ier_keeps_its_four_enable_bits);
  RUN_TEST(test_mcr_keeps_its_five_control_bits);
  RUN_TEST(test_scratch_register_holds_a_byte);
  RUN_TEST(test_iir_reports_no_interrupt);
  RUN_TEST(test_writing_fcr_does_not_change_iir);
  RUN_TEST(test_lsr_ignores_writes);
  RUN_TEST(test_only_byte_accesses_reach_the_registers);
  RUN_TEST(test_offsets_past_the_registers_are_access_faults);
  return UNITY_END();
}
