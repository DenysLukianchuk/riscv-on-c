#include "riscv/fifo.h"

void fifo_init(struct Fifo *fifo, uint8_t *storage, unsigned capacity) {
  *fifo = (struct Fifo){.data = storage, .capacity = capacity};
}

bool fifo_empty(const struct Fifo *fifo) { return fifo->count == 0; }

bool fifo_push(struct Fifo *fifo, uint8_t byte) {
  if (fifo->count == fifo->capacity)
    return false;
  fifo->data[(fifo->head + fifo->count) % fifo->capacity] = byte;
  fifo->count++;
  return true;
}

uint8_t fifo_pop(struct Fifo *fifo) {
  if (fifo_empty(fifo))
    return 0;
  uint8_t byte = fifo->data[fifo->head];
  fifo->head = (fifo->head + 1) % fifo->capacity;
  fifo->count--;
  return byte;
}
