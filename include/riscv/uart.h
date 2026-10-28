#ifndef RISCV_UART_H
#define RISCV_UART_H

#include "riscv/device.h"
#include "riscv/xlen.h"
#include <stdbool.h>

typedef struct Uart Uart;
typedef void (*UartOutputFn)(void *context, uint8_t byte);

Uart *uart_create(UartOutputFn output, void *context);
void uart_destroy(Uart *uart);
enum BusStatus uart_read(Uart *uart, uxlen_t offset, unsigned size,
                         uint64_t *value);
enum BusStatus uart_write(Uart *uart, uxlen_t offset, unsigned size,
                          uint64_t value);
void uart_receive(Uart *uart, uint8_t byte);
bool uart_interrupt_pending(const Uart *uart);
struct BusDevice uart_bus_device(Uart *uart, uxlen_t base);

#endif // RISCV_UART_H
