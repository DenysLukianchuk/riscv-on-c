#include "fp_fixture.h"

// Fused multiply-add: rs1 * rs2 + rs3 rounded once, as if the product were
// exact. The four instructions differ in signs:
//   fmadd  rs1 * rs2 + rs3     fmsub   rs1 * rs2 - rs3
//   fnmsub -(rs1 * rs2) + rs3  fnmadd  -(rs1 * rs2) - rs3

#define SEVEN_F 0x40E00000u
#define FIVE_F 0x40A00000u
#define ONE_PLUS_2_POW_12_F 0x3F800800u       // 1 + 2^-12
#define MINUS_ONE_PLUS_2_POW_11_F 0xBF801000u // -(1 + 2^-11)
#define MINUS_2_POW_11_F 0xBA000000u          // -2^-11
#define TWO_POW_24_INV_F 0x33800000u          // 2^-24

// Runs an instruction with f1, f2 and f3 as operands and returns f4.
static uint64_t run_fma(uint32_t raw, uint64_t a, uint64_t b, uint64_t c) {
  enable_fp();
  set_freg(1, a);
  set_freg(2, b);
  set_freg(3, c);

  execute_raw(raw);

  return get_freg(4);
}

static uint64_t fma_s(uint32_t raw, uint32_t a, uint32_t b, uint32_t c) {
  return run_fma(raw, NAN_BOX(a), NAN_BOX(b), NAN_BOX(c));
}

static void test_the_four_sign_variants(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(SEVEN_F), fma_s(FMADD_S(4, 1, 2, 3, RM_RNE),
                                                  TWO_F, THREE_F, ONE_F));
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(FIVE_F), fma_s(FMSUB_S(4, 1, 2, 3, RM_RNE),
                                                 TWO_F, THREE_F, ONE_F));
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(FIVE_F | SIGN_F),
      fma_s(FNMSUB_S(4, 1, 2, 3, RM_RNE), TWO_F, THREE_F, ONE_F));
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(SEVEN_F | SIGN_F),
      fma_s(FNMADD_S(4, 1, 2, 3, RM_RNE), TWO_F, THREE_F, ONE_F));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

static void test_fmadd_d(void) {
  TEST_ASSERT_EQUAL_HEX64(
      0x401C000000000000,
      run_fma(FMADD_D(4, 1, 2, 3, RM_RNE), TWO_D, THREE_D, ONE_D));
}

// (1 + 2^-12)^2 = 1 + 2^-11 + 2^-24 does not fit a float. Rounded on its own
// it would be 1 + 2^-11, and subtracting that would give 0. Fused, the
// 2^-24 survives and the result is exact.
static void test_fmadd_s_rounds_once(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(TWO_POW_24_INV_F),
                          fma_s(FMADD_S(4, 1, 2, 3, RM_RNE),
                                ONE_PLUS_2_POW_12_F, ONE_PLUS_2_POW_12_F,
                                MINUS_ONE_PLUS_2_POW_11_F));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

// The same with (1 + 2^-27)^2 = 1 + 2^-26 + 2^-54.
static void test_fmadd_d_rounds_once(void) {
  TEST_ASSERT_EQUAL_HEX64(0x3C90000000000000,
                          run_fma(FMADD_D(4, 1, 2, 3, RM_RNE),
                                  0x3FF0000002000000, 0x3FF0000002000000,
                                  0xBFF0000004000000));
}

// (1 + 2^-12)^2 - 2^-11 = 1 + 2^-24, a tie between 1.0 and the float above.
static void test_fmadd_rounds_a_tie_by_rm(void) {
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(ONE_F), fma_s(FMADD_S(4, 1, 2, 3, RM_RNE), ONE_PLUS_2_POW_12_F,
                            ONE_PLUS_2_POW_12_F, MINUS_2_POW_11_F));
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0x3F800001),
                          fma_s(FMADD_S(4, 1, 2, 3, RM_RMM),
                                ONE_PLUS_2_POW_12_F, ONE_PLUS_2_POW_12_F,
                                MINUS_2_POW_11_F));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NX, fflags());
}

static void test_fmadd_uses_frm_for_dynamic_rm(void) {
  set_frm(RM_RUP);

  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0x3F800001),
                          fma_s(FMADD_S(4, 1, 2, 3, RM_DYN),
                                ONE_PLUS_2_POW_12_F, ONE_PLUS_2_POW_12_F,
                                MINUS_2_POW_11_F));
}

// Infinity times zero is invalid even when the addend is a quiet NaN, which
// would otherwise make the result NaN without a flag.
static void test_infinity_times_zero_is_invalid_even_with_a_nan_addend(void) {
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(CANONICAL_NAN_F),
      fma_s(FMADD_S(4, 1, 2, 3, RM_RNE), INF_F, 0, CANONICAL_NAN_F));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

static void test_fmadd_with_a_quiet_nan_is_quiet(void) {
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(CANONICAL_NAN_F),
      fma_s(FMADD_S(4, 1, 2, 3, RM_RNE), ONE_F, ONE_F, 0x7FC12345));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

static void test_fmadd_s_reads_an_unboxed_operand_as_nan(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(CANONICAL_NAN_F),
                          run_fma(FMADD_S(4, 1, 2, 3, RM_RNE), NAN_BOX(ONE_F),
                                  NAN_BOX(ONE_F), ONE_F));
}

static void test_fmadd_makes_fs_dirty(void) {
  set_fs(MSTATUS_FS_CLEAN);

  execute_raw(FMADD_S(4, 1, 2, 3, RM_RNE));

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_FS_DIRTY, fs());
}

static void test_fmadd_is_illegal_while_fs_is_off(void) {
  set_fs(MSTATUS_FS_OFF);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(FMADD_S(4, 1, 2, 3, RM_RNE));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FNMADD_S(4, 1, 2, 3, RM_RNE));
}

static void test_fmadd_with_reserved_rm_is_illegal(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(FMADD_S(4, 1, 2, 3, 5));
}

// fmt 2 (half) and 3 (quad) are not implemented.
static void test_fmadd_of_an_unimplemented_format_is_illegal(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(RV_R4(3, 2, 2, 1, RM_RNE, 4, ASM_MADD));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(RV_R4(3, 3, 2, 1, RM_RNE, 4, ASM_NMSUB));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_the_four_sign_variants);
  RUN_TEST(test_fmadd_d);
  RUN_TEST(test_fmadd_s_rounds_once);
  RUN_TEST(test_fmadd_d_rounds_once);
  RUN_TEST(test_fmadd_rounds_a_tie_by_rm);
  RUN_TEST(test_fmadd_uses_frm_for_dynamic_rm);
  RUN_TEST(test_infinity_times_zero_is_invalid_even_with_a_nan_addend);
  RUN_TEST(test_fmadd_with_a_quiet_nan_is_quiet);
  RUN_TEST(test_fmadd_s_reads_an_unboxed_operand_as_nan);
  RUN_TEST(test_fmadd_makes_fs_dirty);
  RUN_TEST(test_fmadd_is_illegal_while_fs_is_off);
  RUN_TEST(test_fmadd_with_reserved_rm_is_illegal);
  RUN_TEST(test_fmadd_of_an_unimplemented_format_is_illegal);
  return UNITY_END();
}
