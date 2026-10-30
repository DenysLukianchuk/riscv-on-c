#ifndef RISCV_HOST_CONSOLE_H
#define RISCV_HOST_CONSOLE_H

#include "riscv/system/machine.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum ConsoleAction { CONSOLE_SEND, CONSOLE_NOTHING, CONSOLE_QUIT };

struct ConsoleEscape {
  bool pending;
};

enum ConsoleAction console_key(struct ConsoleEscape *escape, uint8_t key,
                               uint8_t *out);

size_t console_feed(struct ConsoleEscape *escape, Machine *machine,
                    const uint8_t *bytes, size_t count, bool *quit);

#endif // RISCV_HOST_CONSOLE_H
