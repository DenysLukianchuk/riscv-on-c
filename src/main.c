#include "riscv/host/console.h"
#include "riscv/host/terminal.h"
#include "riscv/system/loader.h"
#include "riscv/system/machine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAM_SIZE (128u << 20)
// Steps between looks at the keyboard: a system call per instruction would
// slow the emulator down several times over.
#define INPUT_INTERVAL 4096

static uint8_t *read_file(const char *path, size_t *size) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return NULL;
  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);
  uint8_t *data = length > 0 ? malloc((size_t)length) : NULL;
  if (data && fread(data, 1, (size_t)length, file) != (size_t)length) {
    free(data);
    data = NULL;
  }
  fclose(file);
  *size = (size_t)length;
  return data;
}

static void print_to_terminal(void *context, uint8_t byte) {
  (void)context;
  putchar(byte);
  fflush(stdout);
}

// Runs the board until it powers off or Ctrl-A x asks to quit. Keys wait in
// `keys` until the UART has room for them; new ones are read only once all
// of the old ones are in.
static bool run(Machine *machine) {
  struct Terminal terminal;
  terminal_open(&terminal);
  struct ConsoleEscape escape = {0};
  bool quit = false;
  uint8_t keys[64];
  size_t pending = 0;

  for (unsigned long step = 0; !machine_finished(machine) && !quit; step++) {
    if (step % INPUT_INTERVAL == 0) {
      if (pending == 0)
        pending = terminal_read(&terminal, keys, sizeof keys);
      size_t used = console_feed(&escape, machine, keys, pending, &quit);
      memmove(keys, keys + used, pending - used);
      pending -= used;
    }
    machine_step(machine);
  }

  terminal_close(&terminal);
  return quit;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: riscv <program.elf>\n");
    return EXIT_FAILURE;
  }

  size_t program_size;
  uint8_t *image = read_file(argv[1], &program_size);
  if (!image) {
    fprintf(stderr, "failed to read program file: %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  Machine *machine = machine_create(RAM_SIZE, print_to_terminal, NULL);

  uxlen_t entry = 0;
  if (loader_load_elf(machine_get_bus(machine), image, program_size, &entry) !=
      LOADER_OK) {
    fprintf(stderr, "%s: not a loadable ELF\n", argv[1]);
    machine_destroy(machine);
    free(image);
    return EXIT_FAILURE;
  }

  machine_boot(machine, entry);
  bool quit = run(machine);

  int exit_code = quit ? EXIT_SUCCESS : machine_exit_code(machine);
  machine_destroy(machine);
  free(image);
  return exit_code;
}
