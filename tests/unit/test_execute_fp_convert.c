#include "fp_fixture.h"

// fcvt between integers and floating point, and between single and double.
// A conversion to an integer rounds by rm like arithmetic does. A value that
// does not fit, NaN included, gives the nearest end of the integer range and
// raises invalid instead of inexact.

#define TWO_AND_A_HALF_F 0x40200000u
#define MINUS_TWO_AND_A_HALF_F 0xC0200000u
#define TWO_POW_31_F 0x4F000000u
#define MINUS_TWO_POW_31_F 0xCF000000u
#define THREE_BILLION_F 0x4F32D05Eu // 3e9 = 0xB2D05E00
#define MINUS_HALF_F 0xBF000000u
#define TWO_POW_24_F 0x4B800000u // 16777216
#define THIRD_D 0x3FD5555555555555

#define INT32_MAX_X ((uxlen_t)0x7FFFFFFF)
#define INT32_MIN_X ((uxlen_t)(int32_t)0x80000000)
// 32-bit results are sign-extended on RV64, unsigned ones too.
#define UINT32_MAX_X ((uxlen_t)(int32_t)0xFFFFFFFF)

// Converts f1 and returns x5.
static uxlen_t to_int(uint32_t raw, uint64_t value) {
  enable_fp();
  set_freg(1, value);

  execute_raw(raw);

  return get_reg(5);
}

static uxlen_t to_int_s(uint32_t raw, uint32_t value) {
  return to_int(raw, NAN_BOX(value));
}

// Converts x1 and returns f3.
static uint64_t from_int(uint32_t raw, uxlen_t value) {
  enable_fp();
  set_reg(1, value);

  execute_raw(raw);

  return get_freg(3);
}

struct RoundCase {
  const char *name;
  uint8_t rm;
  uxlen_t expected;
};

// Float to integer

static void test_fcvt_w_s_converts_exactly(void) {
  TEST_ASSERT_EQUAL_HEX64(3, to_int_s(FCVT_W_S(5, 1, RM_RNE), THREE_F));
  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-1,
                          to_int_s(FCVT_W_S(5, 1, RM_RNE), MINUS_ONE_F));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

static void test_fcvt_w_s_rounds_by_rm(void) {
  const struct RoundCase up[] = {
      {"RNE", RM_RNE, 2}, {"RTZ", RM_RTZ, 2}, {"RDN", RM_RDN, 2},
      {"RUP", RM_RUP, 3}, {"RMM", RM_RMM, 3},
  };
  for (unsigned i = 0; i < sizeof up / sizeof up[0]; i++)
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(
        up[i].expected, to_int_s(FCVT_W_S(5, 1, up[i].rm), TWO_AND_A_HALF_F),
        up[i].name);

  const struct RoundCase down[] = {
      {"RNE", RM_RNE, (uxlen_t)-2}, {"RTZ", RM_RTZ, (uxlen_t)-2},
      {"RDN", RM_RDN, (uxlen_t)-3}, {"RUP", RM_RUP, (uxlen_t)-2},
      {"RMM", RM_RMM, (uxlen_t)-3},
  };
  for (unsigned i = 0; i < sizeof down / sizeof down[0]; i++)
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(
        down[i].expected,
        to_int_s(FCVT_W_S(5, 1, down[i].rm), MINUS_TWO_AND_A_HALF_F),
        down[i].name);
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NX, fflags());
}

static void test_fcvt_w_s_uses_frm_for_dynamic_rm(void) {
  set_frm(RM_RUP);

  TEST_ASSERT_EQUAL_HEX64(3,
                          to_int_s(FCVT_W_S(5, 1, RM_DYN), TWO_AND_A_HALF_F));
}

static void test_fcvt_w_s_saturates_out_of_range_values(void) {
  TEST_ASSERT_EQUAL_HEX64(INT32_MAX_X,
                          to_int_s(FCVT_W_S(5, 1, RM_RNE), TWO_POW_31_F));
  TEST_ASSERT_EQUAL_HEX64(INT32_MAX_X, to_int_s(FCVT_W_S(5, 1, RM_RNE), INF_F));
  TEST_ASSERT_EQUAL_HEX64(INT32_MIN_X,
                          to_int_s(FCVT_W_S(5, 1, RM_RNE), MINUS_INF_F));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

// -2^31 itself fits.
static void test_fcvt_w_s_converts_the_most_negative_int(void) {
  TEST_ASSERT_EQUAL_HEX64(INT32_MIN_X,
                          to_int_s(FCVT_W_S(5, 1, RM_RNE), MINUS_TWO_POW_31_F));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

// NaN converts to the largest integer, whatever its sign.
static void test_fcvt_of_nan_gives_the_largest_integer(void) {
  TEST_ASSERT_EQUAL_HEX64(INT32_MAX_X,
                          to_int_s(FCVT_W_S(5, 1, RM_RNE), CANONICAL_NAN_F));
  TEST_ASSERT_EQUAL_HEX64(
      INT32_MAX_X, to_int_s(FCVT_W_S(5, 1, RM_RNE), CANONICAL_NAN_F | SIGN_F));
  TEST_ASSERT_EQUAL_HEX64(UINT32_MAX_X,
                          to_int_s(FCVT_WU_S(5, 1, RM_RNE), CANONICAL_NAN_F));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

static void test_fcvt_wu_s_converts_values_above_int32(void) {
  TEST_ASSERT_EQUAL_HEX64((uxlen_t)(int32_t)0xB2D05E00,
                          to_int_s(FCVT_WU_S(5, 1, RM_RNE), THREE_BILLION_F));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

static void test_fcvt_wu_s_of_a_negative_value_is_invalid(void) {
  TEST_ASSERT_EQUAL_HEX64(0, to_int_s(FCVT_WU_S(5, 1, RM_RNE), MINUS_ONE_F));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

// -0.5 rounds to 0, which fits: only inexact.
static void test_fcvt_wu_s_of_a_value_rounding_to_zero_is_inexact(void) {
  TEST_ASSERT_EQUAL_HEX64(0, to_int_s(FCVT_WU_S(5, 1, RM_RTZ), MINUS_HALF_F));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NX, fflags());
}

// The operand of a single-precision conversion must be NaN-boxed.
static void test_fcvt_w_s_reads_an_unboxed_operand_as_nan(void) {
  TEST_ASSERT_EQUAL_HEX64(INT32_MAX_X, to_int(FCVT_W_S(5, 1, RM_RNE), ONE_F));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

// Double to integer

static void test_fcvt_w_d_rounds_and_saturates(void) {
  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-2,
                          to_int(FCVT_W_D(5, 1, RM_RNE), 0xBFF8000000000000));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NX, fflags());
  TEST_ASSERT_EQUAL_HEX64(INT32_MAX_X,
                          to_int(FCVT_W_D(5, 1, RM_RNE), 0x4202A05F20000000));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NX | FFLAG_NV, fflags());
}

static void test_fcvt_wu_d_converts_the_largest_uint32(void) {
  TEST_ASSERT_EQUAL_HEX64(UINT32_MAX_X,
                          to_int(FCVT_WU_D(5, 1, RM_RNE), 0x41EFFFFFFFE00000));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

#if XLEN == 64
static void test_fcvt_l_d_converts_64_bit_values(void) {
  TEST_ASSERT_EQUAL_HEX64(0x4000000000000000,
                          to_int(FCVT_L_D(5, 1, RM_RNE), 0x43D0000000000000));
  TEST_ASSERT_EQUAL_HEX64(0x8000000000000000,
                          to_int(FCVT_L_D(5, 1, RM_RNE), 0xC3E0000000000000));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

static void test_fcvt_l_d_saturates_at_two_pow_63(void) {
  TEST_ASSERT_EQUAL_HEX64(0x7FFFFFFFFFFFFFFF,
                          to_int(FCVT_L_D(5, 1, RM_RNE), 0x43E0000000000000));
  TEST_ASSERT_EQUAL_HEX64(0x7FFFFFFFFFFFFFFF,
                          to_int(FCVT_L_D(5, 1, RM_RNE), CANONICAL_NAN_D));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

static void test_fcvt_lu_d_converts_and_saturates(void) {
  TEST_ASSERT_EQUAL_HEX64(0x8000000000000000,
                          to_int(FCVT_LU_D(5, 1, RM_RNE), 0x43E0000000000000));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
  TEST_ASSERT_EQUAL_HEX64(0, to_int(FCVT_LU_D(5, 1, RM_RNE), MINUS_ONE_D));
  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFFFFFFFFF,
                          to_int(FCVT_LU_D(5, 1, RM_RNE), INF_D));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

static void test_fcvt_l_s_converts_to_64_bits(void) {
  TEST_ASSERT_EQUAL_HEX64(0x80000000,
                          to_int_s(FCVT_L_S(5, 1, RM_RNE), TWO_POW_31_F));
  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFFFFFFFFF,
                          to_int_s(FCVT_LU_S(5, 1, RM_RNE), INF_F));
}
#else
static void test_64_bit_integer_conversions_are_illegal_on_rv32(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(FCVT_L_S(5, 1, RM_RNE));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FCVT_LU_D(5, 1, RM_RNE));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FCVT_S_L(3, 1, RM_RNE));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FCVT_D_LU(3, 1, RM_RNE));
}
#endif

// rs2 = 4 names no integer type.
static void test_conversion_to_an_unknown_integer_type_is_illegal(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(RV_R(0x60, 4, 1, RM_RNE, 5, ASM_OP_FP));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(RV_R(0x68, 4, 1, RM_RNE, 3, ASM_OP_FP));
}

// Integer to float

static void test_fcvt_s_w_converts_exactly(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0xC0E00000),
                          from_int(FCVT_S_W(3, 1, RM_RNE), (uxlen_t)-7));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

// 2^24 + 1 needs 25 bits: halfway between 2^24 (even) and 2^24 + 2.
static void test_fcvt_s_w_rounds_by_rm(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(TWO_POW_24_F),
                          from_int(FCVT_S_W(3, 1, RM_RNE), 0x1000001));
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(TWO_POW_24_F + 1),
                          from_int(FCVT_S_W(3, 1, RM_RUP), 0x1000001));
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(TWO_POW_24_F + 1),
                          from_int(FCVT_S_W(3, 1, RM_RMM), 0x1000001));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NX, fflags());
}

static void test_fcvt_s_wu_reads_rs1_as_unsigned(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0x4F800000),
                          from_int(FCVT_S_WU(3, 1, RM_RNE), 0xFFFFFFFF));
}

static void test_fcvt_d_w_converts_exactly(void) {
  TEST_ASSERT_EQUAL_HEX64(0xC1E0000000000000,
                          from_int(FCVT_D_W(3, 1, RM_RNE), INT32_MIN_X));
  TEST_ASSERT_EQUAL_HEX64(0x41EFFFFFFFE00000,
                          from_int(FCVT_D_WU(3, 1, RM_RNE), 0xFFFFFFFF));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

#if XLEN == 64
// The 32-bit conversions use only the low word of rs1.
static void test_fcvt_s_w_ignores_the_upper_half_of_rs1(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0xC0E00000),
                          from_int(FCVT_S_W(3, 1, RM_RNE), 0x00000001FFFFFFF9));
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0x4F800000), from_int(FCVT_S_WU(3, 1, RM_RNE),
                                                        0xFFFFFFFFFFFFFFFF));
}

static void test_fcvt_from_64_bit_integers(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0xDF000000),
                          from_int(FCVT_S_L(3, 1, RM_RNE), 0x8000000000000000));
  TEST_ASSERT_EQUAL_HEX64(0x43F0000000000000, from_int(FCVT_D_LU(3, 1, RM_RNE),
                                                       0xFFFFFFFFFFFFFFFF));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NX, fflags());
}

// 2^53 + 1 is a tie for a double.
static void test_fcvt_d_l_rounds_by_rm(void) {
  TEST_ASSERT_EQUAL_HEX64(0x4340000000000000,
                          from_int(FCVT_D_L(3, 1, RM_RNE), 0x20000000000001));
  TEST_ASSERT_EQUAL_HEX64(0x4340000000000001,
                          from_int(FCVT_D_L(3, 1, RM_RMM), 0x20000000000001));
}
#endif

// Single <-> double

static void test_fcvt_d_s_widens_exactly(void) {
  TEST_ASSERT_EQUAL_HEX64(ONE_D,
                          run_fp(FCVT_D_S(3, 1, RM_RNE), NAN_BOX(ONE_F), 0));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

static void test_fcvt_d_s_gives_the_canonical_nan(void) {
  TEST_ASSERT_EQUAL_HEX64(
      CANONICAL_NAN_D, run_fp(FCVT_D_S(3, 1, RM_RNE), NAN_BOX(0x7FC12345), 0));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
  TEST_ASSERT_EQUAL_HEX64(CANONICAL_NAN_D,
                          run_fp(FCVT_D_S(3, 1, RM_RNE), ONE_F, 0));
  TEST_ASSERT_EQUAL_HEX64(CANONICAL_NAN_D, run_fp(FCVT_D_S(3, 1, RM_RNE),
                                                  NAN_BOX(SIGNALING_NAN_F), 0));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

static void test_fcvt_s_d_rounds_by_rm(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0x3EAAAAAB),
                          run_fp(FCVT_S_D(3, 1, RM_RNE), THIRD_D, 0));
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0x3EAAAAAA),
                          run_fp(FCVT_S_D(3, 1, RM_RTZ), THIRD_D, 0));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NX, fflags());
}

// 1 + 2^-24 is a tie for a float.
static void test_fcvt_s_d_rounds_a_tie_away_from_zero_in_rmm(void) {
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(ONE_F), run_fp(FCVT_S_D(3, 1, RM_RNE), 0x3FF0000010000000, 0));
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0x3F800001), run_fp(FCVT_S_D(3, 1, RM_RMM),
                                                      0x3FF0000010000000, 0));
}

static void test_fcvt_s_d_overflows_and_underflows(void) {
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(INF_F), run_fp(FCVT_S_D(3, 1, RM_RNE), 0x7E37E43C8800759C, 0));
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(MAX_F), run_fp(FCVT_S_D(3, 1, RM_RTZ), 0x7E37E43C8800759C, 0));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_OF | FFLAG_NX, fflags());

  enable_fp();
  write_csr(FFLAGS, 0);
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(0), run_fp(FCVT_S_D(3, 1, RM_RNE), 0x358DEE7A4AD4B81F, 0));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_UF | FFLAG_NX, fflags());
}

static void test_fcvt_s_d_gives_the_canonical_nan(void) {
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(CANONICAL_NAN_F),
      run_fp(FCVT_S_D(3, 1, RM_RNE), 0xFFF8000000012345, 0));
}

// rs2 names the source format, which must be the other one.
static void test_fp_conversion_with_wrong_source_format_is_illegal(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(RV_R(0x20, 0, 1, RM_RNE, 3, ASM_OP_FP));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(RV_R(0x21, 1, 1, RM_RNE, 3, ASM_OP_FP));
}

static void test_conversion_with_reserved_rm_is_illegal(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(FCVT_W_S(5, 1, 5));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FCVT_S_D(3, 1, 6));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_fcvt_w_s_converts_exactly);
  RUN_TEST(test_fcvt_w_s_rounds_by_rm);
  RUN_TEST(test_fcvt_w_s_uses_frm_for_dynamic_rm);
  RUN_TEST(test_fcvt_w_s_saturates_out_of_range_values);
  RUN_TEST(test_fcvt_w_s_converts_the_most_negative_int);
  RUN_TEST(test_fcvt_of_nan_gives_the_largest_integer);
  RUN_TEST(test_fcvt_wu_s_converts_values_above_int32);
  RUN_TEST(test_fcvt_wu_s_of_a_negative_value_is_invalid);
  RUN_TEST(test_fcvt_wu_s_of_a_value_rounding_to_zero_is_inexact);
  RUN_TEST(test_fcvt_w_s_reads_an_unboxed_operand_as_nan);
  RUN_TEST(test_fcvt_w_d_rounds_and_saturates);
  RUN_TEST(test_fcvt_wu_d_converts_the_largest_uint32);
#if XLEN == 64
  RUN_TEST(test_fcvt_l_d_converts_64_bit_values);
  RUN_TEST(test_fcvt_l_d_saturates_at_two_pow_63);
  RUN_TEST(test_fcvt_lu_d_converts_and_saturates);
  RUN_TEST(test_fcvt_l_s_converts_to_64_bits);
#else
  RUN_TEST(test_64_bit_integer_conversions_are_illegal_on_rv32);
#endif
  RUN_TEST(test_conversion_to_an_unknown_integer_type_is_illegal);
  RUN_TEST(test_fcvt_s_w_converts_exactly);
  RUN_TEST(test_fcvt_s_w_rounds_by_rm);
  RUN_TEST(test_fcvt_s_wu_reads_rs1_as_unsigned);
  RUN_TEST(test_fcvt_d_w_converts_exactly);
#if XLEN == 64
  RUN_TEST(test_fcvt_s_w_ignores_the_upper_half_of_rs1);
  RUN_TEST(test_fcvt_from_64_bit_integers);
  RUN_TEST(test_fcvt_d_l_rounds_by_rm);
#endif
  RUN_TEST(test_fcvt_d_s_widens_exactly);
  RUN_TEST(test_fcvt_d_s_gives_the_canonical_nan);
  RUN_TEST(test_fcvt_s_d_rounds_by_rm);
  RUN_TEST(test_fcvt_s_d_rounds_a_tie_away_from_zero_in_rmm);
  RUN_TEST(test_fcvt_s_d_overflows_and_underflows);
  RUN_TEST(test_fcvt_s_d_gives_the_canonical_nan);
  RUN_TEST(test_fp_conversion_with_wrong_source_format_is_illegal);
  RUN_TEST(test_conversion_with_reserved_rm_is_illegal);
  return UNITY_END();
}
