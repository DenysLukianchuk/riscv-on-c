// isatty, poll and read are POSIX, which strict C11 leaves out of the headers.
#define _POSIX_C_SOURCE 200809L

#include "riscv/host/terminal.h"
#include <poll.h>
#include <unistd.h>

void terminal_open(struct Terminal *terminal) {
  terminal->open = true;
  terminal->raw = false;

  if (!isatty(STDIN_FILENO))
    return;

  tcgetattr(STDIN_FILENO, &terminal->saved);
  struct termios raw = terminal->saved;
  raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
  raw.c_iflag &= ~(IXON | ICRNL);
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  terminal->raw = true;
}

void terminal_close(struct Terminal *terminal) {
  if (terminal->raw)
    tcsetattr(STDIN_FILENO, TCSANOW, &terminal->saved);
}

size_t terminal_read(struct Terminal *terminal, uint8_t *buffer, size_t size) {
  if (!terminal->open)
    return 0;

  struct pollfd p = {.fd = STDIN_FILENO, .events = POLLIN};
  if (poll(&p, 1, 0) <= 0)
    return 0;
  ssize_t n = read(STDIN_FILENO, buffer, size);
  if (n <= 0) {
    terminal->open = false;
    return 0;
  }
  return (size_t)n;
}
