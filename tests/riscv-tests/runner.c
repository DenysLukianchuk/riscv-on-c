#include "riscv/cpu/cpu.h"
#include "riscv/cpu/hart.h"
#include "riscv/devices/memory.h"
#include "riscv/system/board.h"
#include "riscv/system/bus.h"
#include "riscv/system/loader.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAM_SIZE (1 << 20)
#define MAX_STEPS 1000000

#if XLEN == 64
#define E_SHOFF 40
#define E_SHENTSIZE 58
#define E_SHNUM 60
#define SH_TYPE 4
#define SH_OFFSET 24
#define SH_SIZE 32
#define SH_LINK 40
#define SYM_SIZE 24
#define ST_VALUE 8
#else
#define E_SHOFF 32
#define E_SHENTSIZE 46
#define E_SHNUM 48
#define SH_TYPE 4
#define SH_OFFSET 16
#define SH_SIZE 20
#define SH_LINK 24
#define SYM_SIZE 16
#define ST_VALUE 4
#endif
#define SHT_SYMTAB 2

static uint64_t read_le(const uint8_t *at, unsigned size) {
  uint64_t value = 0;
  for (unsigned i = 0; i < size; i++)
    value |= (uint64_t)at[i] << (8 * i);
  return value;
}

static bool in_image(uint64_t offset, uint64_t length, size_t size) {
  return offset <= size && length <= size - offset;
}

static bool find_symbol(const uint8_t *image, size_t size, const char *name,
                        uint64_t *value) {
  uint64_t shoff = read_le(image + E_SHOFF, XLEN / 8);
  uint64_t shentsize = read_le(image + E_SHENTSIZE, 2);
  uint64_t shnum = read_le(image + E_SHNUM, 2);
  if (!in_image(shoff, shnum * shentsize, size))
    return false;

  for (uint64_t i = 0; i < shnum; i++) {
    const uint8_t *sh = image + shoff + i * shentsize;
    if (read_le(sh + SH_TYPE, 4) != SHT_SYMTAB)
      continue;
    uint64_t symoff = read_le(sh + SH_OFFSET, XLEN / 8);
    uint64_t symsize = read_le(sh + SH_SIZE, XLEN / 8);
    uint64_t link = read_le(sh + SH_LINK, 4);
    if (link >= shnum || !in_image(symoff, symsize, size))
      return false;
    const uint8_t *strsh = image + shoff + link * shentsize;
    uint64_t stroff = read_le(strsh + SH_OFFSET, XLEN / 8);
    uint64_t strsize = read_le(strsh + SH_SIZE, XLEN / 8);
    if (!in_image(stroff, strsize, size))
      return false;

    for (uint64_t at = 0; at + SYM_SIZE <= symsize; at += SYM_SIZE) {
      const uint8_t *sym = image + symoff + at;
      uint64_t name_at = read_le(sym, 4);
      if (name_at >= strsize)
        continue;
      const char *sym_name = (const char *)image + stroff + name_at;
      if (!memchr(sym_name, '\0', strsize - name_at))
        continue;
      if (strcmp(sym_name, name) == 0) {
        *value = read_le(sym + ST_VALUE, XLEN / 8);
        return true;
      }
    }
  }
  return false;
}

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

static int run(Bus *bus, Cpu *cpu, uint64_t tohost_addr, const char *name) {
  for (long step = 0; step < MAX_STEPS; step++) {
    cpu_step(cpu);
    uint64_t tohost = 0;
    bus_read(bus, tohost_addr, 8, &tohost);
    if (tohost == 1)
      return EXIT_SUCCESS;
    if (tohost != 0) {
      fprintf(stderr, "%s: test case %llu failed (tohost = 0x%llx)\n", name,
              (unsigned long long)(tohost >> 1), (unsigned long long)tohost);
      return EXIT_FAILURE;
    }
  }
  fprintf(stderr, "%s: no result after %d steps, pc=0x%llx\n", name, MAX_STEPS,
          (unsigned long long)cpu_get_pc(cpu));
  return EXIT_FAILURE;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <riscv-test.elf>\n", argv[0]);
    return EXIT_FAILURE;
  }

  size_t size = 0;
  uint8_t *image = read_file(argv[1], &size);
  if (!image) {
    fprintf(stderr, "%s: cannot read file\n", argv[1]);
    return EXIT_FAILURE;
  }

  Memory *mem = memory_create(RAM_SIZE);
  Bus *bus = bus_create();
  bus_attach(bus, memory_bus_device(mem, RAM_BASE));
  Cpu *cpu = cpu_create(bus);

  int status = EXIT_FAILURE;
  uxlen_t entry = 0;
  uint64_t tohost_addr = 0;
  if (loader_load_elf(bus, image, size, &entry) != LOADER_OK) {
    fprintf(stderr, "%s: not a loadable ELF\n", argv[1]);
  } else if (!find_symbol(image, size, "tohost", &tohost_addr)) {
    fprintf(stderr, "%s: no tohost symbol\n", argv[1]);
  } else {
    cpu_set_pc(cpu, entry);
    status = run(bus, cpu, tohost_addr, argv[1]);
  }

  cpu_destroy(cpu);
  bus_destroy(bus);
  memory_destroy(mem);
  free(image);
  return status;
}
