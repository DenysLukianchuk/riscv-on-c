#include "fp_fixture.h"

// Arithmetic: fadd, fsub, fmul, fdiv, fsqrt, and with them what every
// arithmetic instruction shares. The rm field (funct3) picks the rounding mode,
// or rm = 7 takes it from frm. Exceptions are not trapped: they set bits in
// fflags, which stay set until software clears them. A NaN result is always the
// canonical NaN.

// The float right above 1.0 is 1 + 2^-23. 2^-24 is half that step, so
// 1 + 2^-24 lies exactly between two floats and the rounding mode decides.
#define ULP_ABOVE_ONE_F 0x3F800001u
#define HALF_ULP_OF_ONE_F 0x33800000u // 2^-24
#define ULP_ABOVE_ONE_D 0x3FF0000000000001
#define HALF_ULP_OF_ONE_D 0x3CA0000000000000 // 2^-53

static uint64_t add_s(uint32_t a, uint32_t b, uint8_t rm) {
  return run_fp(FADD_S(3, 1, 2, rm), NAN_BOX(a), NAN_BOX(b));
}

// Results

static void test_fadd_s_adds(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(THREE_F), add_s(ONE_F, TWO_F, RM_RNE));
}

static void test_fsub_s_subtracts(void) {
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(MINUS_ONE_F),
      run_fp(FSUB_S(3, 1, 2, RM_RNE), NAN_BOX(ONE_F), NAN_BOX(TWO_F)));
}

static void test_fadd_d_adds(void) {
  TEST_ASSERT_EQUAL_HEX64(THREE_D,
                          run_fp(FADD_D(3, 1, 2, RM_RNE), ONE_D, TWO_D));
}

static void test_fsub_d_subtracts(void) {
  TEST_ASSERT_EQUAL_HEX64(MINUS_ONE_D,
                          run_fp(FSUB_D(3, 1, 2, RM_RNE), ONE_D, TWO_D));
}

// An exact result raises no flag.
static void test_exact_result_sets_no_flags(void) {
  add_s(ONE_F, TWO_F, RM_RNE);

  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

// Rounding modes. The exact sum 1 + 2^-24 is a tie between 1.0 and the next
// float up, so each mode shows its rule: RNE picks the even one (1.0), RMM
// the one away from zero.

struct RoundCase {
  const char *name;
  uint8_t rm;
  uint32_t expected;
};

static void test_fadd_s_rounds_a_positive_tie_by_rm(void) {
  const struct RoundCase cases[] = {
      {"RNE", RM_RNE, ONE_F},           {"RTZ", RM_RTZ, ONE_F},
      {"RDN", RM_RDN, ONE_F},           {"RUP", RM_RUP, ULP_ABOVE_ONE_F},
      {"RMM", RM_RMM, ULP_ABOVE_ONE_F},
  };
  for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(
        NAN_BOX(cases[i].expected),
        add_s(ONE_F, HALF_ULP_OF_ONE_F, cases[i].rm), cases[i].name);
}

// Below zero, RDN and RUP swap: rounding down means away from zero.
static void test_fadd_s_rounds_a_negative_tie_by_rm(void) {
  const struct RoundCase cases[] = {
      {"RNE", RM_RNE, MINUS_ONE_F},
      {"RTZ", RM_RTZ, MINUS_ONE_F},
      {"RDN", RM_RDN, ULP_ABOVE_ONE_F | 0x80000000},
      {"RUP", RM_RUP, MINUS_ONE_F},
      {"RMM", RM_RMM, ULP_ABOVE_ONE_F | 0x80000000},
  };
  for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(
        NAN_BOX(cases[i].expected),
        add_s(MINUS_ONE_F, HALF_ULP_OF_ONE_F | 0x80000000, cases[i].rm),
        cases[i].name);
}

static void test_fadd_d_rounds_by_rm(void) {
  TEST_ASSERT_EQUAL_HEX64(
      ONE_D, run_fp(FADD_D(3, 1, 2, RM_RNE), ONE_D, HALF_ULP_OF_ONE_D));
  TEST_ASSERT_EQUAL_HEX64(ULP_ABOVE_ONE_D, run_fp(FADD_D(3, 1, 2, RM_RUP),
                                                  ONE_D, HALF_ULP_OF_ONE_D));
}

// x - x is +0 in every mode but RDN, where it is -0.
static void
test_fsub_of_equal_values_gives_minus_zero_only_when_rounding_down(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0), run_fp(FSUB_S(3, 1, 2, RM_RNE),
                                             NAN_BOX(ONE_F), NAN_BOX(ONE_F)));
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(0x80000000),
      run_fp(FSUB_S(3, 1, 2, RM_RDN), NAN_BOX(ONE_F), NAN_BOX(ONE_F)));
}

// rm = 7 (dynamic): the rounding mode comes from frm.
static void test_dynamic_rm_uses_frm(void) {
  set_frm(RM_RUP);

  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(ULP_ABOVE_ONE_F),
                          add_s(ONE_F, HALF_ULP_OF_ONE_F, RM_DYN));
}

// A static rm wins over frm.
static void test_static_rm_ignores_frm(void) {
  set_frm(RM_RUP);

  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(ONE_F),
                          add_s(ONE_F, HALF_ULP_OF_ONE_F, RM_RTZ));
}

// rm values 5 and 6 are reserved.
static void test_reserved_rm_is_illegal(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(FADD_S(3, 1, 2, 5));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FADD_S(3, 1, 2, 6));
}

// frm can hold any 3-bit value, but with rm = 7 only a valid one can be used.
static void test_dynamic_rm_with_invalid_frm_is_illegal(void) {
  const uxlen_t invalid[] = {5, 6, 7};
  for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
    set_frm(invalid[i]);

    TEST_ASSERT_ILLEGAL_INSTRUCTION(FADD_S(3, 1, 2, RM_DYN));
  }
}

static void test_illegal_fadd_leaves_rd_unchanged(void) {
  enable_fp();
  set_freg(3, PI_D);

  execute_raw(FADD_S(3, 1, 2, 5));

  TEST_ASSERT_EQUAL_HEX64(PI_D, get_freg(3));
}

// Exception flags

static void test_rounded_result_sets_inexact(void) {
  add_s(ONE_F, HALF_ULP_OF_ONE_F, RM_RNE);

  TEST_ASSERT_EQUAL_HEX64(FFLAG_NX, fflags());
}

static void test_overflow_gives_infinity_and_sets_overflow_and_inexact(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(INF_F), add_s(MAX_F, MAX_F, RM_RNE));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_OF | FFLAG_NX, fflags());
}

// Rounding toward zero never reaches infinity: it stops at the largest float.
static void test_overflow_toward_zero_gives_largest_float(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(MAX_F), add_s(MAX_F, MAX_F, RM_RTZ));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_OF | FFLAG_NX, fflags());
}

// Flags accumulate: an instruction ORs its flags into fflags.
static void test_flags_accumulate(void) {
  enable_fp();
  write_csr(FFLAGS, FFLAG_DZ);

  add_s(ONE_F, HALF_ULP_OF_ONE_F, RM_RNE);

  TEST_ASSERT_EQUAL_HEX64(FFLAG_DZ | FFLAG_NX, fflags());
}

static void test_arithmetic_makes_fs_dirty(void) {
  set_fs(MSTATUS_FS_CLEAN);

  execute_raw(FADD_S(3, 1, 2, RM_RNE));

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_FS_DIRTY, fs());
}

static void test_arithmetic_is_illegal_while_fs_is_off(void) {
  set_fs(MSTATUS_FS_OFF);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(FADD_S(3, 1, 2, RM_RNE));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FADD_D(3, 1, 2, RM_RNE));
}

// NaNs. RISC-V does not pass NaN payloads on: every NaN result is the
// canonical NaN, whatever NaN went in.

static void test_infinity_minus_infinity_is_invalid(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(CANONICAL_NAN_F),
                          add_s(INF_F, MINUS_INF_F, RM_RNE));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

static void test_quiet_nan_input_gives_canonical_nan_without_flags(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(CANONICAL_NAN_F),
                          add_s(0xFFC12345, ONE_F, RM_RNE));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

// A signaling NaN is an invalid operand.
static void test_signaling_nan_input_gives_canonical_nan_and_invalid(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(CANONICAL_NAN_F),
                          add_s(ONE_F, SIGNALING_NAN_F, RM_RNE));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

static void test_fadd_d_nan_result_is_canonical(void) {
  TEST_ASSERT_EQUAL_HEX64(CANONICAL_NAN_D, run_fp(FADD_D(3, 1, 2, RM_RNE),
                                                  0xFFF8000000012345, ONE_D));
}

// An operand that is not NaN-boxed is read as the canonical NaN.
static void test_fadd_s_reads_unboxed_operand_as_canonical_nan(void) {
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(CANONICAL_NAN_F),
      run_fp(FADD_S(3, 1, 2, RM_RNE), ONE_F, NAN_BOX(ONE_F)));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

// fmul, fdiv, fsqrt

#define SIX_F 0x40C00000u
#define FOUR_F 0x40800000u
#define HALF_F 0x3F000000u
#define THIRD_F_RNE 0x3EAAAAABu
#define THIRD_F_RTZ 0x3EAAAAAAu
#define SQRT_TWO_F 0x3FB504F3u
#define THIRD_D 0x3FD5555555555555
#define SQRT_TWO_D 0x3FF6A09E667F3BCD

static uint64_t op_s(uint32_t raw, uint32_t a, uint32_t b) {
  return run_fp(raw, NAN_BOX(a), NAN_BOX(b));
}

static void test_fmul_multiplies(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(SIX_F),
                          op_s(FMUL_S(3, 1, 2, RM_RNE), TWO_F, THREE_F));
  TEST_ASSERT_EQUAL_HEX64(0x4018000000000000,
                          run_fp(FMUL_D(3, 1, 2, RM_RNE), TWO_D, THREE_D));
}

// (1 + 2^-12)^2 = 1 + 2^-11 + 2^-24: halfway between 1 + 2^-11 (even) and the
// float above it.
static void test_fmul_s_rounds_a_tie_by_rm(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0x3F801000), op_s(FMUL_S(3, 1, 2, RM_RNE),
                                                    0x3F800800, 0x3F800800));
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0x3F801001), op_s(FMUL_S(3, 1, 2, RM_RMM),
                                                    0x3F800800, 0x3F800800));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NX, fflags());
}

// (1 + 2^-27) * (1 + 2^-26) = 1 + 2^-26 + 2^-27 + 2^-53: a tie again.
static void test_fmul_d_rounds_a_tie_by_rm(void) {
  TEST_ASSERT_EQUAL_HEX64(
      0x3FF0000006000000,
      run_fp(FMUL_D(3, 1, 2, RM_RNE), 0x3FF0000002000000, 0x3FF0000004000000));
  TEST_ASSERT_EQUAL_HEX64(
      0x3FF0000006000001,
      run_fp(FMUL_D(3, 1, 2, RM_RMM), 0x3FF0000002000000, 0x3FF0000004000000));
}

// A result too small for a normal float that also had to be rounded is an
// underflow. (1 + 2^-23) * 2^-126 * 0.5 loses its last bit.
static void test_tiny_inexact_result_sets_underflow(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0x00400000),
                          op_s(FMUL_S(3, 1, 2, RM_RNE), 0x00800001, HALF_F));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_UF | FFLAG_NX, fflags());
}

// A tiny result that is exact is not an underflow.
static void test_tiny_exact_result_sets_no_flags(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(0x00400000),
                          op_s(FMUL_S(3, 1, 2, RM_RNE), 0x00800000, HALF_F));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

static void test_fmul_of_infinity_and_zero_is_invalid(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(CANONICAL_NAN_F),
                          op_s(FMUL_S(3, 1, 2, RM_RNE), INF_F, 0));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

static void test_fdiv_divides_and_rounds_by_rm(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(THREE_F),
                          op_s(FDIV_S(3, 1, 2, RM_RNE), SIX_F, TWO_F));
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(THIRD_F_RNE),
                          op_s(FDIV_S(3, 1, 2, RM_RNE), ONE_F, THREE_F));
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(THIRD_F_RTZ),
                          op_s(FDIV_S(3, 1, 2, RM_RTZ), ONE_F, THREE_F));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NX, fflags());
  TEST_ASSERT_EQUAL_HEX64(THIRD_D,
                          run_fp(FDIV_D(3, 1, 2, RM_RNE), ONE_D, THREE_D));
}

static void test_division_by_zero_gives_signed_infinity(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(INF_F),
                          op_s(FDIV_S(3, 1, 2, RM_RNE), ONE_F, 0));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_DZ, fflags());
  TEST_ASSERT_EQUAL_HEX64(MINUS_INF_F | 0xFFFFFFFF00000000,
                          op_s(FDIV_S(3, 1, 2, RM_RNE), MINUS_ONE_F, 0));
  TEST_ASSERT_EQUAL_HEX64(INF_D | SIGN_D,
                          run_fp(FDIV_D(3, 1, 2, RM_RNE), MINUS_ONE_D, 0));
}

static void test_zero_divided_by_zero_is_invalid(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(CANONICAL_NAN_F),
                          op_s(FDIV_S(3, 1, 2, RM_RNE), 0, 0));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

static void test_fsqrt_takes_the_square_root(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(TWO_F),
                          op_s(FSQRT_S(3, 1, RM_RNE), FOUR_F, 0));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(SQRT_TWO_F),
                          op_s(FSQRT_S(3, 1, RM_RNE), TWO_F, 0));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NX, fflags());
  TEST_ASSERT_EQUAL_HEX64(SQRT_TWO_D, run_fp(FSQRT_D(3, 1, RM_RNE), TWO_D, 0));
}

static void test_fsqrt_of_a_negative_number_is_invalid(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(CANONICAL_NAN_F),
                          op_s(FSQRT_S(3, 1, RM_RNE), MINUS_ONE_F, 0));
  TEST_ASSERT_EQUAL_HEX64(FFLAG_NV, fflags());
}

// The square root of -0 is -0, not an invalid operation.
static void test_fsqrt_of_minus_zero_is_minus_zero(void) {
  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(SIGN_F),
                          op_s(FSQRT_S(3, 1, RM_RNE), SIGN_F, 0));
  TEST_ASSERT_EQUAL_HEX64(0, fflags());
}

// fsqrt has one operand: the rs2 field must be 0.
static void test_fsqrt_with_nonzero_rs2_is_illegal(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(RV_R(0x2C, 1, 1, RM_RNE, 3, ASM_OP_FP));
}

// The formats beyond single and double (fmt 2 is half, 3 is quad) are not
// implemented.
static void test_unimplemented_format_is_illegal(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(RV_R(0x02, 2, 1, RM_RNE, 3, ASM_OP_FP));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(RV_R(0x03, 2, 1, RM_RNE, 3, ASM_OP_FP));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_fadd_s_adds);
  RUN_TEST(test_fsub_s_subtracts);
  RUN_TEST(test_fadd_d_adds);
  RUN_TEST(test_fsub_d_subtracts);
  RUN_TEST(test_exact_result_sets_no_flags);
  RUN_TEST(test_fadd_s_rounds_a_positive_tie_by_rm);
  RUN_TEST(test_fadd_s_rounds_a_negative_tie_by_rm);
  RUN_TEST(test_fadd_d_rounds_by_rm);
  RUN_TEST(test_fsub_of_equal_values_gives_minus_zero_only_when_rounding_down);
  RUN_TEST(test_dynamic_rm_uses_frm);
  RUN_TEST(test_static_rm_ignores_frm);
  RUN_TEST(test_reserved_rm_is_illegal);
  RUN_TEST(test_dynamic_rm_with_invalid_frm_is_illegal);
  RUN_TEST(test_illegal_fadd_leaves_rd_unchanged);
  RUN_TEST(test_rounded_result_sets_inexact);
  RUN_TEST(test_overflow_gives_infinity_and_sets_overflow_and_inexact);
  RUN_TEST(test_overflow_toward_zero_gives_largest_float);
  RUN_TEST(test_flags_accumulate);
  RUN_TEST(test_arithmetic_makes_fs_dirty);
  RUN_TEST(test_arithmetic_is_illegal_while_fs_is_off);
  RUN_TEST(test_infinity_minus_infinity_is_invalid);
  RUN_TEST(test_quiet_nan_input_gives_canonical_nan_without_flags);
  RUN_TEST(test_signaling_nan_input_gives_canonical_nan_and_invalid);
  RUN_TEST(test_fadd_d_nan_result_is_canonical);
  RUN_TEST(test_fadd_s_reads_unboxed_operand_as_canonical_nan);
  RUN_TEST(test_fmul_multiplies);
  RUN_TEST(test_fmul_s_rounds_a_tie_by_rm);
  RUN_TEST(test_fmul_d_rounds_a_tie_by_rm);
  RUN_TEST(test_tiny_inexact_result_sets_underflow);
  RUN_TEST(test_tiny_exact_result_sets_no_flags);
  RUN_TEST(test_fmul_of_infinity_and_zero_is_invalid);
  RUN_TEST(test_fdiv_divides_and_rounds_by_rm);
  RUN_TEST(test_division_by_zero_gives_signed_infinity);
  RUN_TEST(test_zero_divided_by_zero_is_invalid);
  RUN_TEST(test_fsqrt_takes_the_square_root);
  RUN_TEST(test_fsqrt_of_a_negative_number_is_invalid);
  RUN_TEST(test_fsqrt_of_minus_zero_is_minus_zero);
  RUN_TEST(test_fsqrt_with_nonzero_rs2_is_illegal);
  RUN_TEST(test_unimplemented_format_is_illegal);
  return UNITY_END();
}
