#include "riscv/loader.h"
#include <stdbool.h>

#define ELF_MACHINE_RISCV 0xF3
#define ELF_DATA_LITTLE_ENDIAN 1
#define ELF_PT_LOAD 1

#define EI_CLASS 4
#define EI_DATA 5

#if XLEN == 64
#define ELF_CLASS 2
#define EHDR_SIZE 64
#define PHDR_SIZE 56
#define E_ENTRY 24
#define E_PHOFF 32
#define E_PHENTSIZE 54
#define E_PHNUM 56
#define P_TYPE 0
#define P_OFFSET 8
#define P_PADDR 24
#define P_FILESZ 32
#define P_MEMSZ 40
#else
#define ELF_CLASS 1
#define EHDR_SIZE 52
#define PHDR_SIZE 32
#define E_ENTRY 24
#define E_PHOFF 28
#define E_PHENTSIZE 42
#define E_PHNUM 44
#define P_TYPE 0
#define P_OFFSET 4
#define P_PADDR 12
#define P_FILESZ 16
#define P_MEMSZ 20
#endif
#define E_MACHINE 18

struct ElfHeader {
  uxlen_t entry;
  uint8_t class;
  uint8_t data;
  uxlen_t phoff;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t machine;
};

struct ElfProgramHeader {
  uint32_t type;
  uxlen_t offset;
  uxlen_t paddr;
  uxlen_t filesz;
  uxlen_t memsz;
};

static uint64_t read_le(const uint8_t *at, unsigned size) {
  uint64_t value = 0;
  for (unsigned i = 0; i < size; i++)
    value |= (uint64_t)at[i] << (8 * i);
  return value;
}

static uxlen_t read_address(const uint8_t *at) { return read_le(at, XLEN / 8); }

static bool parse_header(const uint8_t *image, size_t size,
                         struct ElfHeader *header) {
  if (size < EHDR_SIZE)
    return false;
  if (image[0] != 0x7F || image[1] != 'E' || image[2] != 'L' || image[3] != 'F')
    return false;

  header->class = image[EI_CLASS];
  if (header->class != ELF_CLASS)
    return false;
  header->data = image[EI_DATA];
  if (header->data != ELF_DATA_LITTLE_ENDIAN)
    return false;
  header->entry = read_address(image + E_ENTRY);
  header->phoff = read_address(image + E_PHOFF);
  header->phentsize = read_le(image + E_PHENTSIZE, 2);
  header->phnum = read_le(image + E_PHNUM, 2);
  header->machine = read_le(image + E_MACHINE, 2);
  if (header->machine != ELF_MACHINE_RISCV)
    return false;

  if (header->phentsize < PHDR_SIZE)
    return false;
  if (header->phoff > size ||
      (size_t)header->phnum * header->phentsize > size - header->phoff)
    return false;
  return true;
}

static int parse_program_header(const uint8_t *at,
                                struct ElfProgramHeader *ph) {
  ph->type = read_le(at + P_TYPE, 4);
  ph->offset = read_address(at + P_OFFSET);
  ph->paddr = read_address(at + P_PADDR);
  ph->filesz = read_address(at + P_FILESZ);
  ph->memsz = read_address(at + P_MEMSZ);
  if (ph->type == ELF_PT_LOAD && ph->filesz > ph->memsz)
    return -1;
  return 0;
}

static bool load_segment(Bus *bus, const uint8_t *image,
                         const struct ElfProgramHeader *ph) {
  for (uxlen_t i = 0; i < ph->memsz; i++) {
    uint8_t byte = i < ph->filesz ? image[ph->offset + i] : 0;
    if (bus_write(bus, ph->paddr + i, byte, 1) != BUS_OK)
      return false;
  }
  return true;
}

int loader_load_elf(Bus *bus, const uint8_t *image, size_t size,
                    uxlen_t *entry) {
  struct ElfHeader header;
  if (!parse_header(image, size, &header))
    return LOADER_ERR;
  *entry = header.entry;

  for (uint16_t i = 0; i < header.phnum; i++) {
    struct ElfProgramHeader ph;
    if (parse_program_header(
            image + header.phoff + (size_t)i * header.phentsize, &ph) < 0)
      return LOADER_ERR;
    if (ph.type != ELF_PT_LOAD)
      continue;
    if (ph.offset > size || ph.filesz > size - ph.offset)
      return LOADER_ERR;
    if (!load_segment(bus, image, &ph))
      return LOADER_ERR;
  }

  return LOADER_OK;
}
