#include "riscv/host/console.h"

#define ESCAPE 0x01 // Ctrl-A
#define QUIT_COMMAND 'x'

static enum ConsoleAction escape_command(uint8_t key, uint8_t *out) {
  if (key == QUIT_COMMAND)
    return CONSOLE_QUIT;
  if (key == ESCAPE) {
    *out = key;
    return CONSOLE_SEND;
  }
  return CONSOLE_NOTHING;
}

enum ConsoleAction console_key(struct ConsoleEscape *escape, uint8_t key,
                               uint8_t *out) {
  if (escape->pending) {
    escape->pending = false;
    return escape_command(key, out);
  }
  if (key == ESCAPE) {
    escape->pending = true;
    return CONSOLE_NOTHING;
  }
  *out = key;
  return CONSOLE_SEND;
}

size_t console_feed(struct ConsoleEscape *escape, Machine *machine,
                    const uint8_t *bytes, size_t count, bool *quit) {
  size_t used = 0;
  while (used < count && !*quit) {
    if (!machine_console_ready(machine))
      break;
    uint8_t out;
    enum ConsoleAction action = console_key(escape, bytes[used], &out);
    used++;
    if (action == CONSOLE_SEND)
      machine_console_input(machine, out);
    else if (action == CONSOLE_QUIT)
      *quit = true;
  }
  return used;
}
