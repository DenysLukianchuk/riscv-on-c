#include "riscv/loader.h"
#include "riscv/machine.h"
#include <stdio.h>
#include <stdlib.h>

#define RAM_SIZE (128u << 20)

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
  while (!machine_finished(machine))
    machine_step(machine);

  int exit_code = machine_exit_code(machine);
  machine_destroy(machine);
  free(image);
  return exit_code;
}
