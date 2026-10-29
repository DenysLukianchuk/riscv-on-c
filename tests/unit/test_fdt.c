#include "riscv/fdt.h"
#include "unity.h"
#include <string.h>

// Flattened device tree layout, from the Devicetree Specification v0.4,
// chapter 5. Every number in the blob is big-endian.
#define FDT_MAGIC 0xD00DFEED
#define FDT_VERSION 17
#define FDT_LAST_COMP_VERSION 16

// Header fields (byte offsets).
#define HDR_MAGIC 0
#define HDR_TOTALSIZE 4
#define HDR_OFF_DT_STRUCT 8
#define HDR_OFF_DT_STRINGS 12
#define HDR_OFF_MEM_RSVMAP 16
#define HDR_VERSION 20
#define HDR_LAST_COMP_VERSION 24
#define HDR_BOOT_CPUID_PHYS 28
#define HDR_SIZE_DT_STRINGS 32
#define HDR_SIZE_DT_STRUCT 36

// Structure block tokens.
#define FDT_BEGIN_NODE 0x1
#define FDT_END_NODE 0x2
#define FDT_PROP 0x3
#define FDT_END 0x9

static Fdt *fdt;
static const uint8_t *blob;
static uint32_t blob_size;

void setUp(void) {
  fdt = fdt_create();
  blob = NULL;
  blob_size = 0;
}

void tearDown(void) { fdt_destroy(fdt); }

static void finish(void) { blob = fdt_finish(fdt, &blob_size); }

static uint32_t be32(uint32_t offset) {
  const uint8_t *p = blob + offset;
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 |
         p[3];
}

static uint32_t header(uint32_t field) { return be32(field); }

// The n-th 32-bit word of the structure block.
static uint32_t struct_word(unsigned n) {
  return be32(header(HDR_OFF_DT_STRUCT) + 4 * n);
}

static const uint8_t *struct_bytes(unsigned word) {
  return blob + header(HDR_OFF_DT_STRUCT) + 4 * word;
}

static const char *string_at(uint32_t name_offset) {
  return (const char *)blob + header(HDR_OFF_DT_STRINGS) + name_offset;
}

static void empty_root(void) {
  fdt_begin_node(fdt, "");
  fdt_end_node(fdt);
  finish();
}

// The header.

static void test_header_identifies_the_blob(void) {
  empty_root();

  TEST_ASSERT_EQUAL_HEX32(FDT_MAGIC, header(HDR_MAGIC));
  TEST_ASSERT_EQUAL(FDT_VERSION, header(HDR_VERSION));
  TEST_ASSERT_EQUAL(FDT_LAST_COMP_VERSION, header(HDR_LAST_COMP_VERSION));
  TEST_ASSERT_EQUAL(0, header(HDR_BOOT_CPUID_PHYS));
  TEST_ASSERT_EQUAL(blob_size, header(HDR_TOTALSIZE));
}

static void test_blocks_are_aligned_and_inside_the_blob(void) {
  empty_root();

  TEST_ASSERT_EQUAL(0, header(HDR_OFF_MEM_RSVMAP) % 8);
  TEST_ASSERT_EQUAL(0, header(HDR_OFF_DT_STRUCT) % 4);
  TEST_ASSERT_LESS_OR_EQUAL(blob_size, header(HDR_OFF_DT_STRUCT) +
                                           header(HDR_SIZE_DT_STRUCT));
  TEST_ASSERT_LESS_OR_EQUAL(blob_size, header(HDR_OFF_DT_STRINGS) +
                                           header(HDR_SIZE_DT_STRINGS));
}

// No memory is reserved: the map holds only its terminating zero entry.
static void test_reservation_map_is_empty(void) {
  static const uint8_t zero_entry[16] = {0};
  empty_root();

  TEST_ASSERT_EQUAL_MEMORY(zero_entry, blob + header(HDR_OFF_MEM_RSVMAP), 16);
}

// Nodes.

// The root is named "", which pads to one word of zeros.
static void test_empty_root_node(void) {
  empty_root();

  TEST_ASSERT_EQUAL(16, header(HDR_SIZE_DT_STRUCT));
  TEST_ASSERT_EQUAL_HEX32(FDT_BEGIN_NODE, struct_word(0));
  TEST_ASSERT_EQUAL_HEX32(0, struct_word(1));
  TEST_ASSERT_EQUAL_HEX32(FDT_END_NODE, struct_word(2));
  TEST_ASSERT_EQUAL_HEX32(FDT_END, struct_word(3));
}

// A name keeps its terminating zero and is padded to a whole word.
static void test_child_name_is_padded_to_a_word(void) {
  fdt_begin_node(fdt, "");
  fdt_begin_node(fdt, "cpus");
  fdt_end_node(fdt);
  fdt_end_node(fdt);
  finish();

  TEST_ASSERT_EQUAL_HEX32(FDT_BEGIN_NODE, struct_word(2));
  TEST_ASSERT_EQUAL_MEMORY("cpus\0\0\0\0", struct_bytes(3), 8);
  TEST_ASSERT_EQUAL_HEX32(FDT_END_NODE, struct_word(5));
  TEST_ASSERT_EQUAL_HEX32(FDT_END_NODE, struct_word(6));
  TEST_ASSERT_EQUAL_HEX32(FDT_END, struct_word(7));
}

// Properties: a token, the value's length, where the name sits in the strings
// block, then the value padded to a whole word.

static void test_u32_property(void) {
  fdt_begin_node(fdt, "");
  fdt_property_u32(fdt, "reg", 0x12345678);
  fdt_end_node(fdt);
  finish();

  TEST_ASSERT_EQUAL_HEX32(FDT_PROP, struct_word(2));
  TEST_ASSERT_EQUAL(4, struct_word(3));
  TEST_ASSERT_EQUAL_STRING("reg", string_at(struct_word(4)));
  TEST_ASSERT_EQUAL_HEX32(0x12345678, struct_word(5));
  TEST_ASSERT_EQUAL_HEX32(FDT_END_NODE, struct_word(6));
}

static void test_u64_property_is_two_cells_high_first(void) {
  fdt_begin_node(fdt, "");
  fdt_property_u64(fdt, "reg", 0x0000000180000000);
  fdt_end_node(fdt);
  finish();

  TEST_ASSERT_EQUAL(8, struct_word(3));
  TEST_ASSERT_EQUAL_HEX32(0x00000001, struct_word(5));
  TEST_ASSERT_EQUAL_HEX32(0x80000000, struct_word(6));
}

static void test_string_property_keeps_its_terminator(void) {
  fdt_begin_node(fdt, "");
  fdt_property_string(fdt, "compatible", "ns16550a");
  fdt_end_node(fdt);
  finish();

  TEST_ASSERT_EQUAL(9, struct_word(3));
  TEST_ASSERT_EQUAL_STRING("compatible", string_at(struct_word(4)));
  TEST_ASSERT_EQUAL_MEMORY("ns16550a\0\0\0\0", struct_bytes(5), 12);
  TEST_ASSERT_EQUAL_HEX32(FDT_END_NODE, struct_word(8));
}

// A list of cells, each big-endian, as in reg = <base size>.
static void test_cells_property(void) {
  static const uint32_t reg[] = {0x0, 0x80000000, 0x0, 0x08000000};
  fdt_begin_node(fdt, "");
  fdt_property_cells(fdt, "reg", reg, 4);
  fdt_end_node(fdt);
  finish();

  TEST_ASSERT_EQUAL(16, struct_word(3));
  TEST_ASSERT_EQUAL_HEX32(0x0, struct_word(5));
  TEST_ASSERT_EQUAL_HEX32(0x80000000, struct_word(6));
  TEST_ASSERT_EQUAL_HEX32(0x0, struct_word(7));
  TEST_ASSERT_EQUAL_HEX32(0x08000000, struct_word(8));
  TEST_ASSERT_EQUAL_HEX32(FDT_END_NODE, struct_word(9));
}

// A property with no value, such as interrupt-controller, is just a flag.
static void test_empty_property(void) {
  fdt_begin_node(fdt, "");
  fdt_property(fdt, "interrupt-controller", NULL, 0);
  fdt_end_node(fdt);
  finish();

  TEST_ASSERT_EQUAL(0, struct_word(3));
  TEST_ASSERT_EQUAL_STRING("interrupt-controller", string_at(struct_word(4)));
  TEST_ASSERT_EQUAL_HEX32(FDT_END_NODE, struct_word(5));
}

static void test_each_property_names_its_own_string(void) {
  fdt_begin_node(fdt, "");
  fdt_property_u32(fdt, "#address-cells", 2);
  fdt_property_u32(fdt, "#size-cells", 2);
  fdt_end_node(fdt);
  finish();

  TEST_ASSERT_EQUAL_STRING("#address-cells", string_at(struct_word(4)));
  TEST_ASSERT_EQUAL_STRING("#size-cells", string_at(struct_word(8)));
}

// The strings block is just the names, one after another, each ending in \0.
static void test_strings_block_holds_only_the_names(void) {
  static const char names[] = "#address-cells\0#size-cells";
  fdt_begin_node(fdt, "");
  fdt_property_u32(fdt, "#address-cells", 2);
  fdt_property_u32(fdt, "#size-cells", 2);
  fdt_end_node(fdt);
  finish();

  TEST_ASSERT_EQUAL(sizeof names, header(HDR_SIZE_DT_STRINGS));
  TEST_ASSERT_EQUAL_MEMORY(names, string_at(0), sizeof names);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_header_identifies_the_blob);
  RUN_TEST(test_blocks_are_aligned_and_inside_the_blob);
  RUN_TEST(test_reservation_map_is_empty);
  RUN_TEST(test_empty_root_node);
  RUN_TEST(test_child_name_is_padded_to_a_word);
  RUN_TEST(test_u32_property);
  RUN_TEST(test_u64_property_is_two_cells_high_first);
  RUN_TEST(test_string_property_keeps_its_terminator);
  RUN_TEST(test_cells_property);
  RUN_TEST(test_empty_property);
  RUN_TEST(test_each_property_names_its_own_string);
  RUN_TEST(test_strings_block_holds_only_the_names);
  return UNITY_END();
}
