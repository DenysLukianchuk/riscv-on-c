#include "fp_fixture.h"

// fmin/fmax and the comparisons feq/flt/fle. Neither rounds, so neither
// takes a rounding mode; the rules that matter are those for NaN and for the
// two zeros.

#define QUIET_NAN_F 0x7FC12345u

static uint64_t op_s(uint32_t raw, uint32_t a, uint32_t b) {
  return run_fp(raw, NAN_BOX(a), NAN_BOX(b));
}

// Runs a comparison of f1 and f2 and returns x5.
static uxlen_t compare(uint32_t raw, uint64_t a, uint64_t b) {
  run_fp(raw, a, b);
  return get_reg(5);
}

static uxlen_t compare_s(uint32_t raw, uint32_t a, uint32_t b) {
  return compare(raw, NAN_BOX(a), NAN_BOX(b));
}

// fmin / fmax

static void test_fmin_and_fmax_pick_the_smaller_and_larger(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(ONE_F), op_s(FMIN_S(3, 1, 2), TWO_F, ONE_F));
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(TWO_F), op_s(FMAX_S(3, 1, 2), ONE_F, TWO_F));
  TEST_ASSERT_EQUAL_HEX64(ONE_D, run_fp(FMIN_D(3, 1, 2), ONE_D, TWO_D));
  TEST_ASSERT_EQUAL_HEX64(TWO_D, run_fp(FMAX_D(3, 1, 2), TWO_D, ONE_D));
}

// -0 is taken as smaller than +0, whatever the order of the operands.
static void test_fmin_and_fmax_order_the_zeros(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(SIGN_F), op_s(FMIN_S(3, 1, 2), 0, SIGN_F));
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(SIGN_F), op_s(FMIN_S(3, 1, 2), SIGN_F, 0));
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0), op_s(FMAX_S(3, 1, 2), SIGN_F, 0));
  TEST_ASSERT_EQUAL_HEX64(SIGN_D, run_fp(FMIN_D(3, 1, 2), 0, SIGN_D));
}

// A NaN operand is ignored: the result is the other operand.
static void test_fmin_and_fmax_return_the_number_when_one_operand_is_nan(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(ONE_F),
                          op_s(FMIN_S(3, 1, 2), QUIET_NAN_F, ONE_F));
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(ONE_F),
                          op_s(FMAX_S(3, 1, 2), ONE_F, QUIET_NAN_F));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
  TEST_ASSERT_EQUAL_HEX64(ONE_D,
                          run_fp(FMAX_D(3, 1, 2), CANONICAL_NAN_D, ONE_D));
}

static void test_fmin_and_fmax_of_two_nans_give_the_canonical_nan(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(CANONICAL_NAN_F),
                          op_s(FMIN_S(3, 1, 2), QUIET_NAN_F, QUIET_NAN_F));
  TEST_ASSERT_EQUAL_HEX64(
      CANONICAL_NAN_D,
      run_fp(FMAX_D(3, 1, 2), 0x7FF8000000012345, 0xFFF8000000000001));
}

// A signaling NaN is still ignored, but it raises invalid.
static void test_fmin_with_a_signaling_nan_sets_invalid(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(ONE_F),
                          op_s(FMIN_S(3, 1, 2), SIGNALING_NAN_F, ONE_F));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

// An operand that is not NaN-boxed counts as the canonical NaN.
static void test_fmin_s_ignores_an_unboxed_operand(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(TWO_F),
                          run_fp(FMIN_S(3, 1, 2), ONE_F, NAN_BOX(TWO_F)));
}

static void test_fmin_with_reserved_funct3_is_illegal(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(RV_R(0x14, 2, 1, 2, 3, ASM_OP_FP));
}

// feq / flt / fle

static void test_comparisons_write_one_or_zero(void) {
  TEST_ASSERT_EQUAL_HEX64(1, compare_s(FEQ_S(5, 1, 2), ONE_F, ONE_F));
  TEST_ASSERT_EQUAL_HEX64(0, compare_s(FEQ_S(5, 1, 2), ONE_F, TWO_F));
  TEST_ASSERT_EQUAL_HEX64(1, compare_s(FLT_S(5, 1, 2), ONE_F, TWO_F));
  TEST_ASSERT_EQUAL_HEX64(0, compare_s(FLT_S(5, 1, 2), TWO_F, ONE_F));
  TEST_ASSERT_EQUAL_HEX64(0, compare_s(FLT_S(5, 1, 2), ONE_F, ONE_F));
  TEST_ASSERT_EQUAL_HEX64(1, compare_s(FLE_S(5, 1, 2), ONE_F, ONE_F));
  TEST_ASSERT_EQUAL_HEX64(0, compare_s(FLE_S(5, 1, 2), TWO_F, ONE_F));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

static void test_double_comparisons(void) {
  TEST_ASSERT_EQUAL_HEX64(1, compare(FEQ_D(5, 1, 2), PI_D, PI_D));
  TEST_ASSERT_EQUAL_HEX64(1, compare(FLT_D(5, 1, 2), MINUS_ONE_D, ONE_D));
  TEST_ASSERT_EQUAL_HEX64(0, compare(FLE_D(5, 1, 2), TWO_D, ONE_D));
}

// The comparisons read f registers and write an x register.
static void test_comparison_writes_the_x_register(void) {
  enable_fp();
  set_freg(5, PI_D);

  compare_s(FEQ_S(5, 1, 2), ONE_F, ONE_F);

  TEST_ASSERT_EQUAL_HEX64(PI_D, get_freg(5));
}

// Unlike fmin/fmax, the comparisons treat the zeros as equal.
static void test_the_two_zeros_compare_equal(void) {
  TEST_ASSERT_EQUAL_HEX64(1, compare_s(FEQ_S(5, 1, 2), SIGN_F, 0));
  TEST_ASSERT_EQUAL_HEX64(0, compare_s(FLT_S(5, 1, 2), SIGN_F, 0));
  TEST_ASSERT_EQUAL_HEX64(1, compare_s(FLE_S(5, 1, 2), 0, SIGN_F));
}

// Any comparison with a NaN is false. feq is a quiet comparison: only a
// signaling NaN raises invalid.
static void test_feq_with_a_nan_is_false_and_quiet(void) {
  TEST_ASSERT_EQUAL_HEX64(0, compare_s(FEQ_S(5, 1, 2), QUIET_NAN_F, ONE_F));
  TEST_ASSERT_EQUAL_HEX64(0,
                          compare_s(FEQ_S(5, 1, 2), QUIET_NAN_F, QUIET_NAN_F));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());

  TEST_ASSERT_EQUAL_HEX64(0, compare_s(FEQ_S(5, 1, 2), ONE_F, SIGNALING_NAN_F));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

// flt and fle are signaling comparisons: any NaN raises invalid.
static void test_flt_and_fle_with_a_quiet_nan_set_invalid(void) {
  TEST_ASSERT_EQUAL_HEX64(0, compare_s(FLT_S(5, 1, 2), QUIET_NAN_F, ONE_F));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());

  enable_fp();
  write_csr(FFLAGS, 0);
  TEST_ASSERT_EQUAL_HEX64(0, compare(FLE_D(5, 1, 2), ONE_D, CANONICAL_NAN_D));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

static void test_comparison_with_reserved_funct3_is_illegal(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(RV_R(0x50, 2, 1, 3, 5, ASM_OP_FP));
}

// Setting a flag changes fcsr, which is floating-point state too.
static void test_raising_a_flag_makes_fs_dirty(void) {
  enable_fp();
  set_freg(1, NAN_BOX(SIGNALING_NAN_F));
  set_fs(MSTATUS_FS_CLEAN);

  execute_raw(FEQ_S(5, 1, 2));

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_FS_DIRTY, fs());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_fmin_and_fmax_pick_the_smaller_and_larger);
  RUN_TEST(test_fmin_and_fmax_order_the_zeros);
  RUN_TEST(test_fmin_and_fmax_return_the_number_when_one_operand_is_nan);
  RUN_TEST(test_fmin_and_fmax_of_two_nans_give_the_canonical_nan);
  RUN_TEST(test_fmin_with_a_signaling_nan_sets_invalid);
  RUN_TEST(test_fmin_s_ignores_an_unboxed_operand);
  RUN_TEST(test_fmin_with_reserved_funct3_is_illegal);
  RUN_TEST(test_comparisons_write_one_or_zero);
  RUN_TEST(test_double_comparisons);
  RUN_TEST(test_comparison_writes_the_x_register);
  RUN_TEST(test_the_two_zeros_compare_equal);
  RUN_TEST(test_feq_with_a_nan_is_false_and_quiet);
  RUN_TEST(test_flt_and_fle_with_a_quiet_nan_set_invalid);
  RUN_TEST(test_comparison_with_reserved_funct3_is_illegal);
  RUN_TEST(test_raising_a_flag_makes_fs_dirty);
  return UNITY_END();
}
