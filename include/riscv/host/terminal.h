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
// Sleeps until a key arrives or timeout_ms passes.
void terminal_wait(struct Terminal *terminal, int timeout_ms);

#endif // RISCV_HOST_TERMINAL_H
