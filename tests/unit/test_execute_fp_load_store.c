#include "fp_fixture.h"

// flw/fsw move single-precision values and fld/fsd double-precision values
// between memory and the f registers. They copy bits; no arithmetic happens,
// so they raise no floating-point exceptions.

#define DATA_ADDR (RAM_BASE + 0x100)
#define BACKGROUND 0x1122334455667788

// Stores f3 at DATA_ADDR over a known doubleword and returns the doubleword.
static uint64_t store_over_background(uint32_t raw, uint64_t value) {
  enable_fp();
  bus_write(bus, DATA_ADDR, 8, BACKGROUND);
  set_reg(2, DATA_ADDR);
  set_freg(3, value);

  execute_raw(raw);

  uint64_t doubleword = 0;
  bus_read(bus, DATA_ADDR, 8, &doubleword);
  return doubleword;
}

// flw

static void test_flw_loads_word_nan_boxed(void) {
  enable_fp();
  bus_write(bus, DATA_ADDR, 4, ONE_F);
  set_reg(2, DATA_ADDR);

  execute_raw(FLW(1, 0, 2));

  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(ONE_F), get_freg(1));
}

// rd names an f register: x1 is not touched.
static void test_flw_leaves_the_x_register_with_the_same_number(void) {
  enable_fp();
  bus_write(bus, DATA_ADDR, 4, ONE_F);
  set_reg(1, 0xDEAD);
  set_reg(2, DATA_ADDR);

  execute_raw(FLW(1, 0, 2));

  TEST_ASSERT_EQUAL_HEX64(0xDEAD, get_reg(1));
}

static void test_flw_adds_negative_offset_to_rs1(void) {
  enable_fp();
  bus_write(bus, DATA_ADDR - 4, 4, ONE_F);
  set_reg(2, DATA_ADDR);

  execute_raw(FLW(1, -4, 2));

  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(ONE_F), get_freg(1));
}

// fsw

static void test_fsw_stores_low_word_of_rs2(void) {
  uint64_t stored = store_over_background(FSW(3, 0, 2), NAN_BOX(ONE_F));

  TEST_ASSERT_EQUAL_HEX64(0x11223344, stored >> 32);
  TEST_ASSERT_EQUAL_HEX64(ONE_F, stored & 0xFFFFFFFF);
}

// fsw stores the low 32 bits as they are, even if rs2 is not NaN-boxed.
static void test_fsw_stores_low_word_of_an_unboxed_register(void) {
  uint64_t stored = store_over_background(FSW(3, 0, 2), PI_D);

  TEST_ASSERT_EQUAL_HEX64(PI_D & 0xFFFFFFFF, stored & 0xFFFFFFFF);
}

// rs2 names an f register: the value of x3 is not stored.
static void test_fsw_stores_the_f_register(void) {
  set_reg(3, 0xDEAD);

  uint64_t stored = store_over_background(FSW(3, 0, 2), NAN_BOX(ONE_F));

  TEST_ASSERT_EQUAL_HEX64(ONE_F, stored & 0xFFFFFFFF);
}

static void test_fsw_adds_offset_to_rs1(void) {
  enable_fp();
  set_reg(2, DATA_ADDR - 8);
  set_freg(3, NAN_BOX(ONE_F));

  execute_raw(FSW(3, 8, 2));

  uint64_t word = 0;
  bus_read(bus, DATA_ADDR, 4, &word);
  TEST_ASSERT_EQUAL_HEX64(ONE_F, word);
}

// fld / fsd: 64 bits on RV32 as well as on RV64.

static void test_fld_loads_doubleword(void) {
  enable_fp();
  bus_write(bus, DATA_ADDR, 8, PI_D);
  set_reg(2, DATA_ADDR);

  execute_raw(FLD(1, 0, 2));

  TEST_ASSERT_EQUAL_HEX64(PI_D, get_freg(1));
}

static void test_fsd_stores_doubleword(void) {
  TEST_ASSERT_EQUAL_HEX64(PI_D, store_over_background(FSD(3, 0, 2), PI_D));
}

// mstatus.FS

static void test_fp_loads_and_stores_are_illegal_while_fs_is_off(void) {
  set_fs(MSTATUS_FS_OFF);
  set_reg(2, DATA_ADDR);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(FLW(1, 0, 2));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FLD(1, 0, 2));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FSW(3, 0, 2));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FSD(3, 0, 2));
}

static void test_illegal_flw_leaves_rd_unchanged(void) {
  set_fs(MSTATUS_FS_OFF);
  bus_write(bus, DATA_ADDR, 4, ONE_F);
  set_reg(2, DATA_ADDR);
  set_freg(1, PI_D);

  execute_raw(FLW(1, 0, 2));

  TEST_ASSERT_EQUAL_HEX64(PI_D, get_freg(1));
}

// A load changes an f register, so the state has to be saved again.
static void test_fp_loads_make_fs_dirty(void) {
  const uint32_t loads[] = {FLW(1, 0, 2), FLD(1, 0, 2)};
  for (unsigned i = 0; i < sizeof loads / sizeof loads[0]; i++) {
    set_fs(MSTATUS_FS_CLEAN);
    set_reg(2, DATA_ADDR);

    execute_raw(loads[i]);

    TEST_ASSERT_EQUAL_HEX64(MSTATUS_FS_DIRTY, fs());
  }
}

// Faults: the same as for integer loads and stores.

static void test_flw_from_unmapped_address_raises_load_access_fault(void) {
  enable_fp();
  set_reg(2, UNMAPPED_ADDR);
  set_freg(1, PI_D);

  TEST_ASSERT_TRAP(FLW(1, 0, 2), CAUSE_LOAD_ACCESS, UNMAPPED_ADDR);
  TEST_ASSERT_EQUAL_HEX64(PI_D, get_freg(1));
}

static void test_fsd_to_unmapped_address_raises_store_access_fault(void) {
  enable_fp();
  set_reg(2, UNMAPPED_ADDR);

  TEST_ASSERT_TRAP(FSD(3, 0, 2), CAUSE_STORE_ACCESS, UNMAPPED_ADDR);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_flw_loads_word_nan_boxed);
  RUN_TEST(test_flw_leaves_the_x_register_with_the_same_number);
  RUN_TEST(test_flw_adds_negative_offset_to_rs1);
  RUN_TEST(test_fsw_stores_low_word_of_rs2);
  RUN_TEST(test_fsw_stores_low_word_of_an_unboxed_register);
  RUN_TEST(test_fsw_stores_the_f_register);
  RUN_TEST(test_fsw_adds_offset_to_rs1);
  RUN_TEST(test_fld_loads_doubleword);
  RUN_TEST(test_fsd_stores_doubleword);
  RUN_TEST(test_fp_loads_and_stores_are_illegal_while_fs_is_off);
  RUN_TEST(test_illegal_flw_leaves_rd_unchanged);
  RUN_TEST(test_fp_loads_make_fs_dirty);
  RUN_TEST(test_flw_from_unmapped_address_raises_load_access_fault);
  RUN_TEST(test_fsd_to_unmapped_address_raises_store_access_fault);
  return UNITY_END();
}
