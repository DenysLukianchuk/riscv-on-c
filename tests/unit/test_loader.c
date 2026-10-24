#include "riscv/bus.h"
#include "riscv/loader.h"
#include "riscv/memory.h"
#include "unity.h"

#include <string.h>

#define TEST_RAM_SIZE 0x10000

static Memory *mem;
static Bus *bus;

void setUp(void) {
  mem = memory_create(TEST_RAM_SIZE);
  bus = bus_create(mem);
}

void tearDown(void) {
  bus_destroy(bus);
  memory_destroy(mem);
}

static void put_le(uint8_t *at, uint64_t value, unsigned size) {
  for (unsigned i = 0; i < size; i++)
    at[i] = (uint8_t)(value >> (8 * i));
}

// A minimal executable ELF of the build's XLEN with one PT_LOAD segment whose
// file data is `payload`, placed right after the program header.
struct Segment {
  uint64_t paddr;
  const uint8_t *payload;
  uint64_t filesz;
  uint64_t memsz;
};

#if XLEN == 64
#define EHDR_SIZE 64
#define PHDR_SIZE 56
#else
#define EHDR_SIZE 52
#define PHDR_SIZE 32
#endif

#define ELF_MACHINE_RISCV 0xF3
#define PT_LOAD_TYPE 1

static size_t build_elf(uint8_t *image, uint64_t entry,
                        const struct Segment *segment) {
  size_t data_offset = EHDR_SIZE + PHDR_SIZE;
  memset(image, 0, data_offset + segment->filesz);

  static const uint8_t magic[] = {0x7F, 'E', 'L', 'F'};
  memcpy(image, magic, sizeof magic);
  image[4] = XLEN == 64 ? 2 : 1; // EI_CLASS
  image[5] = 1;                  // EI_DATA: little-endian
  image[6] = 1;                  // EI_VERSION
  put_le(image + 16, 2, 2);      // e_type: ET_EXEC
  put_le(image + 18, ELF_MACHINE_RISCV, 2);
  put_le(image + 20, 1, 4); // e_version

  uint8_t *ph = image + EHDR_SIZE;
#if XLEN == 64
  put_le(image + 24, entry, 8);     // e_entry
  put_le(image + 32, EHDR_SIZE, 8); // e_phoff
  put_le(image + 52, EHDR_SIZE, 2); // e_ehsize
  put_le(image + 54, PHDR_SIZE, 2); // e_phentsize
  put_le(image + 56, 1, 2);         // e_phnum

  put_le(ph + 0, PT_LOAD_TYPE, 4);
  put_le(ph + 4, 5, 4); // p_flags: R+X
  put_le(ph + 8, data_offset, 8);
  put_le(ph + 16, segment->paddr, 8); // p_vaddr
  put_le(ph + 24, segment->paddr, 8); // p_paddr
  put_le(ph + 32, segment->filesz, 8);
  put_le(ph + 40, segment->memsz, 8);
#else
  put_le(image + 24, entry, 4);     // e_entry
  put_le(image + 28, EHDR_SIZE, 4); // e_phoff
  put_le(image + 40, EHDR_SIZE, 2); // e_ehsize
  put_le(image + 42, PHDR_SIZE, 2); // e_phentsize
  put_le(image + 44, 1, 2);         // e_phnum

  put_le(ph + 0, PT_LOAD_TYPE, 4);
  put_le(ph + 4, data_offset, 4);
  put_le(ph + 8, segment->paddr, 4);  // p_vaddr
  put_le(ph + 12, segment->paddr, 4); // p_paddr
  put_le(ph + 16, segment->filesz, 4);
  put_le(ph + 20, segment->memsz, 4);
  put_le(ph + 24, 5, 4); // p_flags: R+X
#endif

  memcpy(image + data_offset, segment->payload, segment->filesz);
  return data_offset + segment->filesz;
}

static const uint8_t ADDI_X1_X0_5[] = {0x93, 0x00, 0x50, 0x00};

static void test_load_copies_segment_to_its_physical_address(void) {
  uint8_t image[256];
  struct Segment segment = {RAM_BASE + 0x100, ADDI_X1_X0_5, 4, 4};
  size_t size = build_elf(image, RAM_BASE + 0x100, &segment);
  uxlen_t entry = 0;

  TEST_ASSERT_EQUAL(LOADER_OK, loader_load_elf(bus, image, size, &entry));

  uint64_t word = 0;
  bus_read(bus, RAM_BASE + 0x100, 4, &word);
  TEST_ASSERT_EQUAL_HEX64(0x00500093, word);
}

static void test_load_returns_entry_point(void) {
  uint8_t image[256];
  struct Segment segment = {RAM_BASE + 0x100, ADDI_X1_X0_5, 4, 4};
  size_t size = build_elf(image, RAM_BASE + 0x104, &segment);
  uxlen_t entry = 0;

  loader_load_elf(bus, image, size, &entry);

  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 0x104, entry);
}

#define BACKGROUND 0xAAAAAAAAAAAAAAAA

static uint64_t load_over_background(uint64_t filesz, uint64_t memsz) {
  bus_write(bus, RAM_BASE + 0x100, BACKGROUND, 8);
  bus_write(bus, RAM_BASE + 0x108, BACKGROUND, 8);
  uint8_t image[256];
  memset(image, 0x55, sizeof image);
  struct Segment segment = {RAM_BASE + 0x100, ADDI_X1_X0_5, filesz, memsz};
  size_t size = build_elf(image, RAM_BASE + 0x100, &segment);
  uxlen_t entry = 0;

  loader_load_elf(bus, image, size, &entry);

  uint64_t doubleword = 0;
  bus_read(bus, RAM_BASE + 0x100, 8, &doubleword);
  return doubleword;
}

static void test_load_does_not_write_past_segment_end(void) {
  TEST_ASSERT_EQUAL_HEX64(0xAAAAAAAA00500093, load_over_background(4, 4));
}

static void test_load_zero_fills_memsz_beyond_filesz(void) {
  TEST_ASSERT_EQUAL_HEX64(0x0000000000500093, load_over_background(4, 8));
}

static uint8_t valid_image[256];
static size_t valid_size;

static void build_valid_elf(uint64_t paddr, uint64_t filesz, uint64_t memsz) {
  struct Segment segment = {paddr, ADDI_X1_X0_5, filesz, memsz};
  valid_size = build_elf(valid_image, paddr, &segment);
}

static int load_valid_image(void) {
  uxlen_t entry = 0;
  return loader_load_elf(bus, valid_image, valid_size, &entry);
}

static void test_load_rejects_wrong_magic(void) {
  build_valid_elf(RAM_BASE, 4, 4);
  valid_image[1] = 'X';

  TEST_ASSERT_EQUAL(LOADER_ERR, load_valid_image());
}

static void test_load_rejects_elf_of_other_xlen(void) {
  build_valid_elf(RAM_BASE, 4, 4);
  valid_image[4] = XLEN == 64 ? 1 : 2;

  TEST_ASSERT_EQUAL(LOADER_ERR, load_valid_image());
}

static void test_load_rejects_big_endian_elf(void) {
  build_valid_elf(RAM_BASE, 4, 4);
  valid_image[5] = 2;

  TEST_ASSERT_EQUAL(LOADER_ERR, load_valid_image());
}

#define ELF_MACHINE_X86_64 0x3E

static void test_load_rejects_elf_for_other_machine(void) {
  build_valid_elf(RAM_BASE, 4, 4);
  put_le(valid_image + 18, ELF_MACHINE_X86_64, 2);

  TEST_ASSERT_EQUAL(LOADER_ERR, load_valid_image());
}

static void test_load_rejects_truncated_image(void) {
  build_valid_elf(RAM_BASE, 4, 4);
  valid_size = EHDR_SIZE - 1;

  TEST_ASSERT_EQUAL(LOADER_ERR, load_valid_image());
}

#if XLEN == 64
#define E_PHOFF_AT 32
#define P_OFFSET_AT (EHDR_SIZE + 8)
#else
#define E_PHOFF_AT 28
#define P_OFFSET_AT (EHDR_SIZE + 4)
#endif

static void test_load_rejects_program_headers_outside_image(void) {
  build_valid_elf(RAM_BASE, 4, 4);
  put_le(valid_image + E_PHOFF_AT, (uxlen_t)-8, XLEN / 8);

  TEST_ASSERT_EQUAL(LOADER_ERR, load_valid_image());
}

static void test_load_rejects_segment_data_outside_image(void) {
  build_valid_elf(RAM_BASE, 4, 4);
  put_le(valid_image + P_OFFSET_AT, (uxlen_t)-2, XLEN / 8);

  TEST_ASSERT_EQUAL(LOADER_ERR, load_valid_image());
}

static void test_load_rejects_filesz_larger_than_memsz(void) {
  build_valid_elf(RAM_BASE, 4, 2);

  TEST_ASSERT_EQUAL(LOADER_ERR, load_valid_image());
}

#define UNMAPPED_ADDR 0x10

static void test_load_rejects_segment_outside_ram(void) {
  build_valid_elf(UNMAPPED_ADDR, 4, 4);

  TEST_ASSERT_EQUAL(LOADER_ERR, load_valid_image());
}

#define PT_RISCV_ATTRIBUTES 0x70000003

static void test_load_skips_non_load_segments_whatever_their_sizes(void) {
  build_valid_elf(RAM_BASE, 4, 2);
  put_le(valid_image + EHDR_SIZE, PT_RISCV_ATTRIBUTES, 4);

  TEST_ASSERT_EQUAL(LOADER_OK, load_valid_image());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_load_copies_segment_to_its_physical_address);
  RUN_TEST(test_load_returns_entry_point);
  RUN_TEST(test_load_does_not_write_past_segment_end);
  RUN_TEST(test_load_zero_fills_memsz_beyond_filesz);
  RUN_TEST(test_load_rejects_wrong_magic);
  RUN_TEST(test_load_rejects_elf_of_other_xlen);
  RUN_TEST(test_load_rejects_big_endian_elf);
  RUN_TEST(test_load_rejects_elf_for_other_machine);
  RUN_TEST(test_load_rejects_truncated_image);
  RUN_TEST(test_load_rejects_program_headers_outside_image);
  RUN_TEST(test_load_rejects_segment_data_outside_image);
  RUN_TEST(test_load_rejects_filesz_larger_than_memsz);
  RUN_TEST(test_load_rejects_segment_outside_ram);
  RUN_TEST(test_load_skips_non_load_segments_whatever_their_sizes);
  return UNITY_END();
}
