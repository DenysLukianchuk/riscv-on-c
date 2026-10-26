#include "cpu_fixture.h"

#define ALL_ONES ((uxlen_t) - 1)
#define MOST_NEGATIVE ((uxlen_t)1 << (XLEN - 1))

static uxlen_t execute_with(uint32_t raw, uxlen_t x2, uxlen_t x3) {
  set_reg(2, x2);
  set_reg(3, x3);
  execute_raw(raw);
  return get_reg(1);
}

static void test_mul_multiplies(void) {
  TEST_ASSERT_EQUAL_HEX64(42, execute_with(MUL(1, 2, 3), 7, 6));
}

static void test_mul_keeps_low_xlen_bits_of_product(void) {
  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-2, execute_with(MUL(1, 2, 3), ALL_ONES, 2));
}

static void test_mulh_returns_high_bits_of_signed_product(void) {
  TEST_ASSERT_EQUAL_HEX64(ALL_ONES,
                          execute_with(MULH(1, 2, 3), MOST_NEGATIVE, 2));
}

static void test_mulhu_returns_high_bits_of_unsigned_product(void) {
  TEST_ASSERT_EQUAL_HEX64(1, execute_with(MULHU(1, 2, 3), MOST_NEGATIVE, 2));
}

static void test_mulhsu_treats_rs1_as_signed_and_rs2_as_unsigned(void) {
  TEST_ASSERT_EQUAL_HEX64(ALL_ONES,
                          execute_with(MULHSU(1, 2, 3), ALL_ONES, ALL_ONES));
}

static void test_div_rounds_towards_zero(void) {
  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-3,
                          execute_with(DIV(1, 2, 3), (uxlen_t)-20, 6));
}

static void test_divu_divides_as_unsigned(void) {
  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-20 / 6,
                          execute_with(DIVU(1, 2, 3), (uxlen_t)-20, 6));
}

static void test_rem_takes_sign_of_dividend(void) {
  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-2,
                          execute_with(REM(1, 2, 3), (uxlen_t)-20, 6));
}

static void test_remu_returns_unsigned_remainder(void) {
  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-20 % 6,
                          execute_with(REMU(1, 2, 3), (uxlen_t)-20, 6));
}

static void test_division_by_zero_does_not_trap(void) {
  write_csr(MCAUSE, 0xFF);

  TEST_ASSERT_EQUAL_HEX64(ALL_ONES, execute_with(DIV(1, 2, 3), 20, 0));
  TEST_ASSERT_EQUAL_HEX64(ALL_ONES, execute_with(DIVU(1, 2, 3), 20, 0));
  TEST_ASSERT_EQUAL_HEX64(20, execute_with(REM(1, 2, 3), 20, 0));
  TEST_ASSERT_EQUAL_HEX64(20, execute_with(REMU(1, 2, 3), 20, 0));
  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(MCAUSE));
}

static void test_signed_division_overflow_does_not_trap(void) {
  TEST_ASSERT_EQUAL_HEX64(MOST_NEGATIVE,
                          execute_with(DIV(1, 2, 3), MOST_NEGATIVE, ALL_ONES));
  TEST_ASSERT_EQUAL_HEX64(0,
                          execute_with(REM(1, 2, 3), MOST_NEGATIVE, ALL_ONES));
}

#if XLEN == 64

#define MINUS_20_IN_LOW_32 0xABCD0000FFFFFFEC

static void test_mulw_sign_extends_low_32_bits_of_product(void) {
  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFFFFFFFFE,
                          execute_with(MULW(1, 2, 3), 0x123456787FFFFFFF, 2));
}

static void test_divw_divides_low_32_bits_as_signed(void) {
  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-3,
                          execute_with(DIVW(1, 2, 3), MINUS_20_IN_LOW_32, 6));
}

static void test_divuw_divides_low_32_bits_as_unsigned(void) {
  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFECu / 6,
                          execute_with(DIVUW(1, 2, 3), MINUS_20_IN_LOW_32, 6));
}

static void test_divuw_sign_extends_32_bit_quotient(void) {
  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFF80000000,
                          execute_with(DIVUW(1, 2, 3), 0x80000000, 1));
}

static void test_remw_takes_sign_of_32_bit_dividend(void) {
  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-2,
                          execute_with(REMW(1, 2, 3), MINUS_20_IN_LOW_32, 6));
}

static void test_remuw_returns_32_bit_unsigned_remainder(void) {
  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFECu % 6,
                          execute_with(REMUW(1, 2, 3), MINUS_20_IN_LOW_32, 6));
}

static void test_w_division_by_zero_and_overflow(void) {
  TEST_ASSERT_EQUAL_HEX64(ALL_ONES, execute_with(DIVW(1, 2, 3), 20, 0));
  TEST_ASSERT_EQUAL_HEX64(ALL_ONES, execute_with(DIVUW(1, 2, 3), 20, 0));
  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-20,
                          execute_with(REMW(1, 2, 3), MINUS_20_IN_LOW_32, 0));
  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFF80000000,
                          execute_with(DIVW(1, 2, 3), 0x80000000, ALL_ONES));
  TEST_ASSERT_EQUAL_HEX64(0, execute_with(REMW(1, 2, 3), 0x80000000, ALL_ONES));
}

#define MULHW_X1_X2_X3 RV_R(0x01, 3, 2, 1, 1, ASM_OP_32)
#define MULHSUW_X1_X2_X3 RV_R(0x01, 3, 2, 2, 1, ASM_OP_32)
#define MULHUW_X1_X2_X3 RV_R(0x01, 3, 2, 3, 1, ASM_OP_32)

static void test_op_32_has_no_high_multiply(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(MULHW_X1_X2_X3);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(MULHSUW_X1_X2_X3);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(MULHUW_X1_X2_X3);
}
#endif

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_mul_multiplies);
  RUN_TEST(test_mul_keeps_low_xlen_bits_of_product);
  RUN_TEST(test_mulh_returns_high_bits_of_signed_product);
  RUN_TEST(test_mulhu_returns_high_bits_of_unsigned_product);
  RUN_TEST(test_mulhsu_treats_rs1_as_signed_and_rs2_as_unsigned);
  RUN_TEST(test_div_rounds_towards_zero);
  RUN_TEST(test_divu_divides_as_unsigned);
  RUN_TEST(test_rem_takes_sign_of_dividend);
  RUN_TEST(test_remu_returns_unsigned_remainder);
  RUN_TEST(test_division_by_zero_does_not_trap);
  RUN_TEST(test_signed_division_overflow_does_not_trap);
#if XLEN == 64
  RUN_TEST(test_mulw_sign_extends_low_32_bits_of_product);
  RUN_TEST(test_divw_divides_low_32_bits_as_signed);
  RUN_TEST(test_divuw_divides_low_32_bits_as_unsigned);
  RUN_TEST(test_divuw_sign_extends_32_bit_quotient);
  RUN_TEST(test_remw_takes_sign_of_32_bit_dividend);
  RUN_TEST(test_remuw_returns_32_bit_unsigned_remainder);
  RUN_TEST(test_w_division_by_zero_and_overflow);
  RUN_TEST(test_op_32_has_no_high_multiply);
#endif
  return UNITY_END();
}
