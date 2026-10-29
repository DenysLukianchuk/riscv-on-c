#include "riscv/devices/uart.h"
#include "riscv/util/fifo.h"
#include <stdbool.h>
#include <stdlib.h>

enum UartRegister {
  RBR = 0,
  THR = 0,
  DLL = 0,
  IER = 1,
  DLM = 1,
  IIR = 2,
  FCR = 2,
  LCR = 3,
  MCR = 4,
  LSR = 5,
  MSR = 6,
  SCR = 7,
};

#define LSR_DR 0x01
#define LSR_THRE 0x20
#define LSR_TEMT 0x40
#define LCR_DLAB 0x80
#define IER_RX_DATA 0x01
#define IER_THR_EMPTY 0x02
#define IIR_NO_INTERRUPT 0x01
#define IIR_THR_EMPTY 0x02
#define IIR_RX_DATA 0x04

#define IER_WRITABLE 0x0F
#define MCR_WRITABLE 0x1F

#define RX_FIFO_DEPTH 16

#define UART_SIZE 0x100

struct Uart {
  uint8_t ier, lcr, mcr, scr, dll, dlm;
  UartOutputFn output;
  void *context;
  uint8_t rx_storage[RX_FIFO_DEPTH];
  struct Fifo rx;
  bool thr_empty_event;
};

Uart *uart_create(UartOutputFn output, void *context) {
  Uart *uart = (Uart *)malloc(sizeof(Uart));
  *uart = (Uart){.output = output, .context = context};
  fifo_init(&uart->rx, uart->rx_storage, RX_FIFO_DEPTH);
  return uart;
}

void uart_destroy(Uart *uart) { free(uart); }

static bool dlab(const Uart *uart) { return (uart->lcr & LCR_DLAB) != 0; }

static uint8_t line_status(const Uart *uart) {
  uint8_t status = LSR_THRE | LSR_TEMT;
  if (!fifo_empty(&uart->rx))
    status |= LSR_DR;
  return status;
}

static uint8_t interrupt_id(const Uart *uart) {
  if ((uart->ier & IER_RX_DATA) && !fifo_empty(&uart->rx))
    return IIR_RX_DATA;
  if ((uart->ier & IER_THR_EMPTY) && uart->thr_empty_event)
    return IIR_THR_EMPTY;
  return IIR_NO_INTERRUPT;
}

enum BusStatus uart_read(Uart *uart, uxlen_t offset, unsigned size,
                         uint64_t *value) {
  if (size != 1)
    return BUS_ERROR;
  if (offset > SCR)
    return BUS_ERROR;
  switch (offset) {
  case LSR:
    *value = line_status(uart);
    break;
  case RBR:
    *value = dlab(uart) ? uart->dll : fifo_pop(&uart->rx);
    break;
  case IER:
    *value = dlab(uart) ? uart->dlm : uart->ier;
    break;
  case IIR:
    *value = interrupt_id(uart);
    if (*value == IIR_THR_EMPTY)
      uart->thr_empty_event = false;
    break;
  case LCR:
    *value = uart->lcr;
    break;
  case MCR:
    *value = uart->mcr;
    break;
  case SCR:
    *value = uart->scr;
    break;
  case MSR:
    *value = 0;
    break;
  }
  return BUS_OK;
}

enum BusStatus uart_write(Uart *uart, uxlen_t offset, unsigned size,
                          uint64_t value) {
  if (size != 1)
    return BUS_ERROR;
  if (offset > SCR)
    return BUS_ERROR;
  switch (offset) {
  case THR:
    if (dlab(uart)) {
      uart->dll = value;
      break;
    }
    if (uart->output)
      uart->output(uart->context, value);
    uart->thr_empty_event = true;
    break;
  case LCR:
    uart->lcr = value;
    break;
  case IER:
    if (dlab(uart)) {
      uart->dlm = value;
      break;
    }
    uart->ier = value & IER_WRITABLE;
    uart->thr_empty_event = true;
    break;
  case MCR:
    uart->mcr = value & MCR_WRITABLE;
    break;
  case SCR:
    uart->scr = value;
    break;
  }
  return BUS_OK;
}

bool uart_interrupt_pending(const Uart *uart) {
  return interrupt_id(uart) != IIR_NO_INTERRUPT;
}

// A byte that arrives while the receive fifo is full is lost, as on the chip.
void uart_receive(Uart *uart, uint8_t byte) { fifo_push(&uart->rx, byte); }

static enum BusStatus bus_read(void *uart, uxlen_t offset, unsigned size,
                               uint64_t *value) {
  return uart_read(uart, offset, size, value);
}

static enum BusStatus bus_write(void *uart, uxlen_t offset, unsigned size,
                                uint64_t value) {
  return uart_write(uart, offset, size, value);
}

struct BusDevice uart_bus_device(Uart *uart, uxlen_t base) {
  return (struct BusDevice){.base = base,
                            .size = UART_SIZE,
                            .device = uart,
                            .read = bus_read,
                            .write = bus_write};
}
