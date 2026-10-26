#include "cpu_fixture.h"

#define DATA_ADDR (RAM_BASE + 0x100)

static void test_amoadd_w_adds_to_memory_and_returns_old_value(void) {
  bus_write(bus, DATA_ADDR, 4, 10);
  set_reg(2, DATA_ADDR);
  set_reg(3, 5);

  execute_raw(AMO_W(AMO_ADD, 1, 3, 2));

  uint64_t word = 0;
  bus_read(bus, DATA_ADDR, 4, &word);
  TEST_ASSERT_EQUAL_HEX64(10, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(15, word);
}

static uint64_t memory_at(uxlen_t addr, unsigned size) {
  uint64_t value = 0;
  bus_read(bus, addr, size, &value);
  return value;
}

// x1 = amo<funct5>.w x3, (x2) with x2 pointing at DATA_ADDR.
static void amo_w(uint8_t funct5, uint32_t memory, uxlen_t x3) {
  bus_write(bus, DATA_ADDR, 4, memory);
  set_reg(2, DATA_ADDR);
  set_reg(3, x3);
  execute_raw(AMO_W(funct5, 1, 3, 2));
}

static void test_amoadd_w_uses_the_whole_32_bit_word(void) {
  amo_w(AMO_ADD, 0x12345678, 0x11111111);

  TEST_ASSERT_EQUAL_HEX64(0x12345678, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(0x23456789, memory_at(DATA_ADDR, 4));
}

static void test_amo_w_sign_extends_old_value_into_rd(void) {
  amo_w(AMO_ADD, 0x80000000, 0);

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-1 << 31, get_reg(1));
}

static void test_amo_w_leaves_the_next_word_untouched(void) {
  bus_write(bus, DATA_ADDR + 4, 4, 0xAAAAAAAA);

  amo_w(AMO_ADD, 1, 1);

  TEST_ASSERT_EQUAL_HEX64(0xAAAAAAAA, memory_at(DATA_ADDR + 4, 4));
}

static void test_amoswap_w_stores_rs2_and_returns_old_value(void) {
  amo_w(AMO_SWAP, 1, 7);

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(7, memory_at(DATA_ADDR, 4));
}

static void test_amoswap_w_with_rd_equal_rs2_stores_old_rs2(void) {
  bus_write(bus, DATA_ADDR, 4, 1);
  set_reg(2, DATA_ADDR);
  set_reg(3, 7);

  execute_raw(AMO_W(AMO_SWAP, 3, 3, 2));

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(3));
  TEST_ASSERT_EQUAL_HEX64(7, memory_at(DATA_ADDR, 4));
}

static void test_amo_bitwise_operations(void) {
  amo_w(AMO_XOR, 0xF0, 0x3C);
  TEST_ASSERT_EQUAL_HEX64(0xCC, memory_at(DATA_ADDR, 4));

  amo_w(AMO_AND, 0xF0, 0x3C);
  TEST_ASSERT_EQUAL_HEX64(0x30, memory_at(DATA_ADDR, 4));

  amo_w(AMO_OR, 0xF0, 0x3C);
  TEST_ASSERT_EQUAL_HEX64(0xFC, memory_at(DATA_ADDR, 4));
}

#define MINUS_5_W 0xFFFFFFFB

static void test_amomin_and_amomax_compare_as_signed(void) {
  amo_w(AMO_MIN, MINUS_5_W, 3);
  TEST_ASSERT_EQUAL_HEX64(MINUS_5_W, memory_at(DATA_ADDR, 4));

  amo_w(AMO_MAX, MINUS_5_W, 3);
  TEST_ASSERT_EQUAL_HEX64(3, memory_at(DATA_ADDR, 4));
}

static void test_amominu_and_amomaxu_compare_as_unsigned(void) {
  amo_w(AMO_MINU, MINUS_5_W, 3);
  TEST_ASSERT_EQUAL_HEX64(3, memory_at(DATA_ADDR, 4));

  amo_w(AMO_MAXU, MINUS_5_W, 3);
  TEST_ASSERT_EQUAL_HEX64(MINUS_5_W, memory_at(DATA_ADDR, 4));
}

static void test_amo_w_compares_only_the_low_32_bits_of_rs2(void) {
  amo_w(AMO_MIN, 3, (uxlen_t)0xFFFFFFFF00000005);

  TEST_ASSERT_EQUAL_HEX64(3, memory_at(DATA_ADDR, 4));
}

static void test_misaligned_amo_raises_store_address_misaligned(void) {
  set_reg(2, DATA_ADDR + 2);

  TEST_ASSERT_TRAP(AMO_W(AMO_ADD, 1, 3, 2), CAUSE_MISALIGNED_STORE,
                   DATA_ADDR + 2);
}

static void test_amo_outside_ram_raises_store_access_fault(void) {
  set_reg(2, UNMAPPED_ADDR);

  TEST_ASSERT_TRAP(AMO_W(AMO_ADD, 1, 3, 2), CAUSE_STORE_ACCESS, UNMAPPED_ADDR);
}

#define AMO_RESERVED_FUNCT5 0x05
#define AMO_WIDTH_B 0

static void test_reserved_amo_encodings_raise_illegal_instruction(void) {
  set_reg(2, DATA_ADDR);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(AMO_W(AMO_RESERVED_FUNCT5, 1, 3, 2));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(RV_AMO(AMO_ADD, AMO_WIDTH_B, 1, 3, 2));
}

#if XLEN == 64
static void test_amoadd_d_works_on_64_bits(void) {
  bus_write(bus, DATA_ADDR, 8, 0xFFFFFFFF);
  set_reg(2, DATA_ADDR);
  set_reg(3, 1);

  execute_raw(AMO_D(AMO_ADD, 1, 3, 2));

  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFF, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(0x100000000, memory_at(DATA_ADDR, 8));
}

static void test_amomin_d_compares_64_bit_signed_values(void) {
  bus_write(bus, DATA_ADDR, 8, (uint64_t)-1);
  set_reg(2, DATA_ADDR);
  set_reg(3, 1);

  execute_raw(AMO_D(AMO_MIN, 1, 3, 2));

  TEST_ASSERT_EQUAL_HEX64((uint64_t)-1, memory_at(DATA_ADDR, 8));
}
#else
static void test_amo_d_is_illegal_on_rv32(void) {
  set_reg(2, DATA_ADDR);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(AMO_D(AMO_ADD, 1, 3, 2));
}
#endif

static void test_reserved_funct5_is_illegal_before_any_memory_access(void) {
  set_reg(2, UNMAPPED_ADDR);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(AMO_W(AMO_RESERVED_FUNCT5, 1, 3, 2));
}

#define OTHER_ADDR (DATA_ADDR + 0x40)

static void setup_lr_sc(uint32_t memory, uxlen_t x3) {
  bus_write(bus, DATA_ADDR, 4, memory);
  bus_write(bus, OTHER_ADDR, 4, memory);
  set_reg(2, DATA_ADDR);
  set_reg(3, x3);
}

static void test_lr_w_loads_sign_extended_word_without_writing(void) {
  setup_lr_sc(0x80000000, 0);

  execute_raw(LR_W(1, 2));

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-1 << 31, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(0x80000000, memory_at(DATA_ADDR, 4));
}

static void test_sc_w_after_lr_w_stores_and_reports_success(void) {
  setup_lr_sc(1, 7);

  execute_raw(LR_W(1, 2));
  execute_raw(SC_W(4, 3, 2));

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(4));
  TEST_ASSERT_EQUAL_HEX64(7, memory_at(DATA_ADDR, 4));
}

static void test_sc_w_without_reservation_fails_and_does_not_store(void) {
  setup_lr_sc(1, 7);
  set_reg(4, 0xDEAD);

  execute_raw(SC_W(4, 3, 2));

  TEST_ASSERT_NOT_EQUAL(0, get_reg(4));
  TEST_ASSERT_EQUAL_HEX64(1, memory_at(DATA_ADDR, 4));
}

static void test_sc_w_consumes_the_reservation(void) {
  setup_lr_sc(1, 7);
  execute_raw(LR_W(1, 2));
  execute_raw(SC_W(4, 3, 2));
  set_reg(3, 9);

  execute_raw(SC_W(4, 3, 2));

  TEST_ASSERT_NOT_EQUAL(0, get_reg(4));
  TEST_ASSERT_EQUAL_HEX64(7, memory_at(DATA_ADDR, 4));
}

static void test_sc_w_to_other_address_than_reserved_fails(void) {
  setup_lr_sc(1, 7);
  execute_raw(LR_W(1, 2));
  set_reg(2, OTHER_ADDR);

  execute_raw(SC_W(4, 3, 2));

  TEST_ASSERT_NOT_EQUAL(0, get_reg(4));
  TEST_ASSERT_EQUAL_HEX64(1, memory_at(OTHER_ADDR, 4));
}

static void test_failed_sc_w_also_clears_the_reservation(void) {
  setup_lr_sc(1, 7);
  execute_raw(LR_W(1, 2));
  set_reg(2, OTHER_ADDR);
  execute_raw(SC_W(4, 3, 2));
  set_reg(2, DATA_ADDR);
  set_reg(4, 0);

  execute_raw(SC_W(4, 3, 2));

  TEST_ASSERT_NOT_EQUAL(0, get_reg(4));
  TEST_ASSERT_EQUAL_HEX64(1, memory_at(DATA_ADDR, 4));
}

static void test_misaligned_lr_raises_load_address_misaligned(void) {
  set_reg(2, DATA_ADDR + 2);

  TEST_ASSERT_TRAP(LR_W(1, 2), CAUSE_MISALIGNED_LOAD, DATA_ADDR + 2);
}

static void test_lr_outside_ram_raises_load_access_fault(void) {
  set_reg(2, UNMAPPED_ADDR);

  TEST_ASSERT_TRAP(LR_W(1, 2), CAUSE_LOAD_ACCESS, UNMAPPED_ADDR);
}

static void test_misaligned_sc_raises_store_address_misaligned(void) {
  set_reg(2, DATA_ADDR + 2);

  TEST_ASSERT_TRAP(SC_W(4, 3, 2), CAUSE_MISALIGNED_STORE, DATA_ADDR + 2);
}

#if XLEN == 64

static void test_sc_d_after_lr_d_stores_doubleword(void) {
  bus_write(bus, DATA_ADDR, 8, 1);
  set_reg(2, DATA_ADDR);
  set_reg(3, 0x123456789ABCDEF0);

  execute_raw(LR_D(1, 2));
  execute_raw(SC_D(4, 3, 2));

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(4));
  TEST_ASSERT_EQUAL_HEX64(0x123456789ABCDEF0, memory_at(DATA_ADDR, 8));
}
#endif

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_amoadd_w_adds_to_memory_and_returns_old_value);
  RUN_TEST(test_amoadd_w_uses_the_whole_32_bit_word);
  RUN_TEST(test_amo_w_sign_extends_old_value_into_rd);
  RUN_TEST(test_amo_w_leaves_the_next_word_untouched);
  RUN_TEST(test_amoswap_w_stores_rs2_and_returns_old_value);
  RUN_TEST(test_amoswap_w_with_rd_equal_rs2_stores_old_rs2);
  RUN_TEST(test_amo_bitwise_operations);
  RUN_TEST(test_amomin_and_amomax_compare_as_signed);
  RUN_TEST(test_amominu_and_amomaxu_compare_as_unsigned);
  RUN_TEST(test_amo_w_compares_only_the_low_32_bits_of_rs2);
  RUN_TEST(test_misaligned_amo_raises_store_address_misaligned);
  RUN_TEST(test_amo_outside_ram_raises_store_access_fault);
  RUN_TEST(test_reserved_amo_encodings_raise_illegal_instruction);
#if XLEN == 64
  RUN_TEST(test_amoadd_d_works_on_64_bits);
  RUN_TEST(test_amomin_d_compares_64_bit_signed_values);
#else
  RUN_TEST(test_amo_d_is_illegal_on_rv32);
#endif
  RUN_TEST(test_reserved_funct5_is_illegal_before_any_memory_access);
  RUN_TEST(test_lr_w_loads_sign_extended_word_without_writing);
  RUN_TEST(test_sc_w_after_lr_w_stores_and_reports_success);
  RUN_TEST(test_sc_w_without_reservation_fails_and_does_not_store);
  RUN_TEST(test_sc_w_consumes_the_reservation);
  RUN_TEST(test_sc_w_to_other_address_than_reserved_fails);
  RUN_TEST(test_failed_sc_w_also_clears_the_reservation);
  RUN_TEST(test_misaligned_lr_raises_load_address_misaligned);
  RUN_TEST(test_lr_outside_ram_raises_load_access_fault);
  RUN_TEST(test_misaligned_sc_raises_store_address_misaligned);
#if XLEN == 64
  RUN_TEST(test_sc_d_after_lr_d_stores_doubleword);
#endif
  return UNITY_END();
}
