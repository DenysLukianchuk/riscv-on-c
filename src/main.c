#include "riscv/host/clock.h"
#include "riscv/host/console.h"
#include "riscv/host/ram_size.h"
#include "riscv/host/terminal.h"
#include "riscv/system/board.h"
#include "riscv/system/loader.h"
#include "riscv/system/machine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define NS_PER_TICK (1000000000u / TIMEBASE_FREQUENCY)
#define TICKS_PER_MS (TIMEBASE_FREQUENCY / 1000)
// Long enough to cost nothing, short enough to notice the board powering off.
#define MAX_SLEEP_MS 1000

struct Host {
  struct Terminal terminal;
  struct ConsoleEscape escape;
  uint64_t start_ns;
  uint8_t keys[64];
  size_t pending;
  bool quit;
};

static uint64_t host_ticks(const struct Host *host) {
  return (host_clock_ns() - host->start_ns) / NS_PER_TICK;
}

// Brings the board's clock up to date and hands it the keys typed since the
// last look. Keys wait in `keys` until the UART has room for them; new ones
// are read only once all of the old ones are in.
static void sync(struct Host *host, Machine *machine) {
  machine_set_time(machine, host_ticks(host));
  if (host->pending == 0)
    host->pending =
        terminal_read(&host->terminal, host->keys, sizeof host->keys);
  size_t used = console_feed(&host->escape, machine, host->keys, host->pending,
                             &host->quit);
  memmove(host->keys, host->keys + used, host->pending - used);
  host->pending -= used;
}

// The hart waits for an interrupt: sleep until the timer is due or a key
// arrives.
static void sleep_until_event(struct Host *host, Machine *machine) {
  uint64_t now = host_ticks(host);
  uint64_t due = machine_next_timer(machine);
  if (host->pending > 0 || due <= now)
    return;
  uint64_t ms = (due - now + TICKS_PER_MS - 1) / TICKS_PER_MS;
  terminal_wait(&host->terminal, ms < MAX_SLEEP_MS ? (int)ms : MAX_SLEEP_MS);
}

// Runs the board until it powers off or Ctrl-A x asks to quit.
static bool run(Machine *machine) {
  struct Host host = {.start_ns = host_clock_ns()};
  terminal_open(&host.terminal);

  for (unsigned long step = 0; !machine_finished(machine) && !host.quit;
       step++) {
    if (step % INPUT_INTERVAL == 0)
      sync(&host, machine);
    machine_step(machine);
    if (machine_idle(machine)) {
      sleep_until_event(&host, machine);
      sync(&host, machine);
    }
  }

  terminal_close(&host.terminal);
  return host.quit;
}

static void usage(void) {
  fprintf(stderr, "usage: riscv [-m size] <program.elf>\n"
                  "  -m size  RAM, in megabytes or with a K, M or G suffix "
                  "(default 128M)\n");
}

int main(int argc, char *argv[]) {
  uint64_t ram_size = RAM_SIZE_DEFAULT;
  int arg = 1;
  if (arg < argc && strcmp(argv[arg], "-m") == 0) {
    if (arg + 1 >= argc || !ram_size_parse(argv[arg + 1], &ram_size)) {
      fprintf(stderr, "riscv: bad RAM size: %s\n",
              arg + 1 < argc ? argv[arg + 1] : "(none)");
      return EXIT_FAILURE;
    }
    arg += 2;
  }
  if (arg + 1 != argc) {
    usage();
    return EXIT_FAILURE;
  }
  const char *program = argv[arg];

  size_t program_size;
  uint8_t *image = read_file(program, &program_size);
  if (!image) {
    fprintf(stderr, "failed to read program file: %s\n", program);
    return EXIT_FAILURE;
  }

  Machine *machine = machine_create(ram_size, print_to_terminal, NULL);
  if (!machine) {
    fprintf(stderr, "riscv: cannot allocate %llu bytes of RAM\n",
            (unsigned long long)ram_size);
    free(image);
    return EXIT_FAILURE;
  }

  uxlen_t entry = 0;
  if (loader_load_elf(machine_get_bus(machine), image, program_size, &entry) !=
      LOADER_OK) {
    fprintf(stderr, "%s: not a loadable ELF\n", program);
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
