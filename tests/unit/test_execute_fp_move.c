#include "fp_fixture.h"

// Instructions that only move or inspect bits: fmv between x and f registers,
// sign injection (fsgnj, which also gives fmv.s, fneg and fabs) and fclass.
// None of them does arithmetic, so none raises a floating-point exception.

// f1 holds a value that is not NaN-boxed: its upper half is not all ones.
// A single-precision operation must read it as the canonical NaN.
#define UNBOXED_MINUS_ONE_F ((uint64_t)MINUS_ONE_F)

// fmv.x.w / fmv.w.x

static void test_fmv_x_w_copies_low_word_sign_extended(void) {
  enable_fp();
  set_freg(1, NAN_BOX(MINUS_ONE_F));

  execute_raw(FMV_X_W(5, 1));

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)(int32_t)MINUS_ONE_F, get_reg(5));
}

// fmv.x.w moves bits as they are: the register need not be NaN-boxed.
static void test_fmv_x_w_ignores_nan_boxing(void) {
  enable_fp();
  set_freg(1, PI_D);

  execute_raw(FMV_X_W(5, 1));

  TEST_ASSERT_EQUAL_HEX64(PI_D & 0xFFFFFFFF, get_reg(5));
}

// rd is an x register and rs1 an f register.
static void test_fmv_x_w_reads_f_and_writes_x(void) {
  enable_fp();
  set_freg(1, NAN_BOX(ONE_F));
  set_freg(5, PI_D);

  execute_raw(FMV_X_W(5, 1));

  TEST_ASSERT_EQUAL_HEX64(ONE_F, get_reg(5));
  TEST_ASSERT_EQUAL_HEX64(PI_D, get_freg(5));
}

static void test_fmv_w_x_nan_boxes_low_word(void) {
  enable_fp();
  set_reg(1, ONE_F);

  execute_raw(FMV_W_X(3, 1));

  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(ONE_F), get_freg(3));
}

#if XLEN == 64
static void test_fmv_w_x_drops_upper_half_of_x(void) {
  enable_fp();
  set_reg(1, 0x123456783F800000);

  execute_raw(FMV_W_X(3, 1));

  TEST_ASSERT_EQUAL_HEX64(NAN_BOX(ONE_F), get_freg(3));
}

// fmv.x.d / fmv.d.x: RV64 only, an x register cannot hold a double on RV32.

static void test_fmv_x_d_copies_doubleword(void) {
  enable_fp();
  set_freg(1, PI_D);

  execute_raw(FMV_X_D(5, 1));

  TEST_ASSERT_EQUAL_HEX64(PI_D, get_reg(5));
}

static void test_fmv_d_x_copies_doubleword(void) {
  enable_fp();
  set_reg(1, PI_D);

  execute_raw(FMV_D_X(3, 1));

  TEST_ASSERT_EQUAL_HEX64(PI_D, get_freg(3));
}
#else
static void test_fmv_x_d_and_fmv_d_x_are_illegal_on_rv32(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(FMV_X_D(5, 1));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FMV_D_X(3, 1));
}
#endif

// Sign injection: the result has the magnitude of rs1 and a sign taken from
// rs2 (fsgnj), from its inverse (fsgnjn) or from the xor of both signs
// (fsgnjx).

static void test_fsgnj_s_takes_sign_of_rs2(void) {
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(MINUS_ONE_F),
      run_fp(FSGNJ_S(3, 1, 2), NAN_BOX(ONE_F), NAN_BOX(MINUS_ONE_F)));
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(ONE_F),
      run_fp(FSGNJ_S(3, 1, 2), NAN_BOX(MINUS_ONE_F), NAN_BOX(ONE_F)));
}

static void test_fsgnjn_s_takes_inverted_sign_of_rs2(void) {
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(MINUS_ONE_F),
      run_fp(FSGNJN_S(3, 1, 2), NAN_BOX(ONE_F), NAN_BOX(ONE_F)));
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(ONE_F),
      run_fp(FSGNJN_S(3, 1, 2), NAN_BOX(ONE_F), NAN_BOX(MINUS_ONE_F)));
}

static void test_fsgnjx_s_xors_the_signs(void) {
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(ONE_F),
      run_fp(FSGNJX_S(3, 1, 2), NAN_BOX(MINUS_ONE_F), NAN_BOX(MINUS_ONE_F)));
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(MINUS_ONE_F),
      run_fp(FSGNJX_S(3, 1, 2), NAN_BOX(MINUS_ONE_F), NAN_BOX(ONE_F)));
}

// The canonical NaN is positive, so an unboxed rs2 gives a positive result.
static void test_fsgnj_s_reads_unboxed_rs2_as_canonical_nan(void) {
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(ONE_F),
      run_fp(FSGNJ_S(3, 1, 2), NAN_BOX(ONE_F), UNBOXED_MINUS_ONE_F));
}

static void test_fsgnj_s_reads_unboxed_rs1_as_canonical_nan(void) {
  TEST_ASSERT_EQUAL_HEX64(
      NAN_BOX(CANONICAL_NAN_F | 0x80000000),
      run_fp(FSGNJ_S(3, 1, 2), UNBOXED_MINUS_ONE_F, NAN_BOX(MINUS_ONE_F)));
}

static void test_fsgnj_d_takes_sign_of_rs2(void) {
  TEST_ASSERT_EQUAL_HEX64(PI_D | 0x8000000000000000,
                          run_fp(FSGNJ_D(3, 1, 2), PI_D, MINUS_ONE_D));
}

static void test_fsgnjn_d_takes_inverted_sign_of_rs2(void) {
  TEST_ASSERT_EQUAL_HEX64(PI_D, run_fp(FSGNJN_D(3, 1, 2), PI_D, MINUS_ONE_D));
}

static void test_fsgnjx_d_xors_the_signs(void) {
  TEST_ASSERT_EQUAL_HEX64(
      PI_D, run_fp(FSGNJX_D(3, 1, 2), PI_D | 0x8000000000000000, MINUS_ONE_D));
}

// fsgnj on a signaling NaN only copies bits: no invalid-operation flag.
static void test_fsgnj_raises_no_flags(void) {
  run_fp(FSGNJ_S(3, 1, 2), NAN_BOX(SIGNALING_NAN_F), NAN_BOX(ONE_F));

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(FFLAGS));
}

static void test_fsgnj_with_reserved_funct3_is_illegal(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(RV_R(0x10, 2, 1, 3, 3, ASM_OP_FP));
}

// fclass: one bit in rd tells which kind of number rs1 holds.

struct ClassCase {
  const char *name;
  uint64_t value;
  uxlen_t class;
};

static void check_classes(uint32_t raw, const struct ClassCase *cases,
                          unsigned count) {
  enable_fp();
  for (unsigned i = 0; i < count; i++) {
    set_freg(1, cases[i].value);

    execute_raw(raw);

    TEST_ASSERT_EQUAL_HEX64_MESSAGE(cases[i].class, get_reg(5), cases[i].name);
  }
}

static void test_fclass_s_classifies_each_kind(void) {
  const struct ClassCase cases[] = {
      {"-inf", NAN_BOX(0xFF800000), FCLASS_NEG_INF},
      {"-1", NAN_BOX(MINUS_ONE_F), FCLASS_NEG_NORMAL},
      {"-subnormal", NAN_BOX(0x80000001), FCLASS_NEG_SUBNORMAL},
      {"-0", NAN_BOX(0x80000000), FCLASS_NEG_ZERO},
      {"+0", NAN_BOX(0x00000000), FCLASS_POS_ZERO},
      {"+subnormal", NAN_BOX(0x00000001), FCLASS_POS_SUBNORMAL},
      {"+1", NAN_BOX(ONE_F), FCLASS_POS_NORMAL},
      {"+inf", NAN_BOX(0x7F800000), FCLASS_POS_INF},
      {"signaling NaN", NAN_BOX(SIGNALING_NAN_F), FCLASS_SIGNALING_NAN},
      {"quiet NaN", NAN_BOX(CANONICAL_NAN_F), FCLASS_QUIET_NAN},
      {"unboxed", UNBOXED_MINUS_ONE_F, FCLASS_QUIET_NAN},
  };
  check_classes(FCLASS_S(5, 1), cases, sizeof cases / sizeof cases[0]);
}

static void test_fclass_d_classifies_each_kind(void) {
  const struct ClassCase cases[] = {
      {"-inf", 0xFFF0000000000000, FCLASS_NEG_INF},
      {"-1", MINUS_ONE_D, FCLASS_NEG_NORMAL},
      {"-subnormal", 0x8000000000000001, FCLASS_NEG_SUBNORMAL},
      {"-0", 0x8000000000000000, FCLASS_NEG_ZERO},
      {"+0", 0x0000000000000000, FCLASS_POS_ZERO},
      {"+subnormal", 0x0000000000000001, FCLASS_POS_SUBNORMAL},
      {"+pi", PI_D, FCLASS_POS_NORMAL},
      {"+inf", 0x7FF0000000000000, FCLASS_POS_INF},
      {"signaling NaN", 0x7FF0000000000001, FCLASS_SIGNALING_NAN},
      {"quiet NaN", 0x7FF8000000000000, FCLASS_QUIET_NAN},
  };
  check_classes(FCLASS_D(5, 1), cases, sizeof cases / sizeof cases[0]);
}

// mstatus.FS

static void test_fp_moves_are_illegal_while_fs_is_off(void) {
  set_fs(MSTATUS_FS_OFF);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(FMV_X_W(5, 1));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FMV_W_X(3, 1));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FSGNJ_S(3, 1, 2));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FSGNJ_D(3, 1, 2));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FCLASS_S(5, 1));
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FCLASS_D(5, 1));
}

static void test_writing_an_f_register_makes_fs_dirty(void) {
  const uint32_t writes[] = {FMV_W_X(3, 1), FSGNJ_S(3, 1, 2), FSGNJ_D(3, 1, 2)};
  for (unsigned i = 0; i < sizeof writes / sizeof writes[0]; i++) {
    set_fs(MSTATUS_FS_CLEAN);

    execute_raw(writes[i]);

    TEST_ASSERT_EQUAL_HEX64(MSTATUS_FS_DIRTY, fs());
  }
}

// funct5 11111 is not assigned in OP-FP.
static void test_unknown_op_fp_instruction_is_illegal(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(RV_R(0x7C, 0, 1, 0, 5, ASM_OP_FP));
}

// fmv and fclass have one operand: rs2 must be 0. fmv.w.x has no variants:
// funct3 must be 0 too.
static void test_fmv_and_fclass_with_nonzero_rs2_are_illegal(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(FMV_X_W(5, 1) | 1u << 20);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FCLASS_D(5, 1) | 1u << 20);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(FMV_W_X(3, 1) | 1u << 20);
}

static void test_fmv_w_x_with_nonzero_funct3_is_illegal(void) {
  enable_fp();

  TEST_ASSERT_ILLEGAL_INSTRUCTION(FMV_W_X(3, 1) | 1u << 12);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_fmv_x_w_copies_low_word_sign_extended);
  RUN_TEST(test_fmv_x_w_ignores_nan_boxing);
  RUN_TEST(test_fmv_x_w_reads_f_and_writes_x);
  RUN_TEST(test_fmv_w_x_nan_boxes_low_word);
#if XLEN == 64
  RUN_TEST(test_fmv_w_x_drops_upper_half_of_x);
  RUN_TEST(test_fmv_x_d_copies_doubleword);
  RUN_TEST(test_fmv_d_x_copies_doubleword);
#else
  RUN_TEST(test_fmv_x_d_and_fmv_d_x_are_illegal_on_rv32);
#endif
  RUN_TEST(test_fsgnj_s_takes_sign_of_rs2);
  RUN_TEST(test_fsgnjn_s_takes_inverted_sign_of_rs2);
  RUN_TEST(test_fsgnjx_s_xors_the_signs);
  RUN_TEST(test_fsgnj_s_reads_unboxed_rs2_as_canonical_nan);
  RUN_TEST(test_fsgnj_s_reads_unboxed_rs1_as_canonical_nan);
  RUN_TEST(test_fsgnj_d_takes_sign_of_rs2);
  RUN_TEST(test_fsgnjn_d_takes_inverted_sign_of_rs2);
  RUN_TEST(test_fsgnjx_d_xors_the_signs);
  RUN_TEST(test_fsgnj_raises_no_flags);
  RUN_TEST(test_fsgnj_with_reserved_funct3_is_illegal);
  RUN_TEST(test_fclass_s_classifies_each_kind);
  RUN_TEST(test_fclass_d_classifies_each_kind);
  RUN_TEST(test_fp_moves_are_illegal_while_fs_is_off);
  RUN_TEST(test_writing_an_f_register_makes_fs_dirty);
  RUN_TEST(test_unknown_op_fp_instruction_is_illegal);
  RUN_TEST(test_fmv_and_fclass_with_nonzero_rs2_are_illegal);
  RUN_TEST(test_fmv_w_x_with_nonzero_funct3_is_illegal);
  return UNITY_END();
}
