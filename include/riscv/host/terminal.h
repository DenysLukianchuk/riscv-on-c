#ifndef RISCV_HOST_TERMINAL_H
#define RISCV_HOST_TERMINAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <termios.h>

struct Terminal {
  struct termios saved;
  bool raw;
  bool open;
};

void terminal_open(struct Terminal *terminal);
void terminal_close(struct Terminal *terminal);
size_t terminal_read(struct Terminal *terminal, uint8_t *buffer, size_t size);

#endif // RISCV_HOST_TERMINAL_H
