#ifndef RISCV_UTIL_FIFO_H
#define RISCV_UTIL_FIFO_H

#include <stdbool.h>
#include <stdint.h>

struct Fifo {
  uint8_t *data;
  unsigned capacity;
  unsigned head;
  unsigned count;
};

void fifo_init(struct Fifo *fifo, uint8_t *storage, unsigned capacity);
bool fifo_empty(const struct Fifo *fifo);
bool fifo_push(struct Fifo *fifo, uint8_t byte);
uint8_t fifo_pop(struct Fifo *fifo);

#endif // RISCV_UTIL_FIFO_H
