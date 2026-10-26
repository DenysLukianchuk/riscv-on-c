#include "riscv/csr.h"
#include "riscv_spec.h"
#include "unity.h"

static Csr *csrs;

void setUp(void) { csrs = csr_create(); }

void tearDown(void) { csr_destroy(csrs); }

static void test_mscratch_written_value_can_be_read_back(void) {
  TEST_ASSERT_EQUAL(CSR_OK, csr_write(csrs, MSCRATCH, PRIV_M, 0xDEADBEEF));

  uxlen_t value = 0;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MSCRATCH, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(0xDEADBEEF, value);
}

static void test_read_of_unimplemented_csr_is_illegal(void) {
  uxlen_t value = 0;

  TEST_ASSERT_EQUAL(CSR_ILLEGAL, csr_read(csrs, 0x7FF, PRIV_M, &value));
}

static void test_mhartid_reads_zero_on_single_hart(void) {
  uxlen_t value = 0xFFFFFFFF;

  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MHARTID, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(0, value);
}

static void test_write_to_read_only_mhartid_is_illegal(void) {
  TEST_ASSERT_EQUAL(CSR_ILLEGAL, csr_write(csrs, MHARTID, PRIV_M, 0x42));

  uxlen_t value = 0xFFFFFFFF;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MHARTID, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(0, value);
}

static void test_mepc_written_value_can_be_read_back(void) {
  TEST_ASSERT_EQUAL(CSR_OK, csr_write(csrs, MEPC, PRIV_M, 0x80001234));

  uxlen_t value = 0;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MEPC, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(0x80001234, value);
}

static void test_mepc_bit_zero_is_always_zero(void) {
  TEST_ASSERT_EQUAL(CSR_OK, csr_write(csrs, MEPC, PRIV_M, 0x80001235));

  uxlen_t value = 0;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MEPC, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(0x80001234, value);
}

static void test_mcause_written_value_can_be_read_back(void) {
  TEST_ASSERT_EQUAL(CSR_OK, csr_write(csrs, MCAUSE, PRIV_M, 5));

  uxlen_t value = 0;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MCAUSE, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(5, value);
}

static void test_mtval_written_value_can_be_read_back(void) {
  TEST_ASSERT_EQUAL(CSR_OK, csr_write(csrs, MTVAL, PRIV_M, 0xDEADBEEF));

  uxlen_t value = 0;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MTVAL, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(0xDEADBEEF, value);
}

static void test_mtvec_written_value_can_be_read_back(void) {
  TEST_ASSERT_EQUAL(CSR_OK, csr_write(csrs, MTVEC, PRIV_M, 0x80000100));

  uxlen_t value = 0;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MTVEC, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(0x80000100, value);
}

static void test_mtvec_reserved_mode_reads_back_as_legal(void) {
  TEST_ASSERT_EQUAL(CSR_OK, csr_write(csrs, MTVEC, PRIV_M, 0x80000102));

  uxlen_t value = 0;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MTVEC, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(0x80000100, value);
}

static void test_mtvec_vectored_mode_is_preserved(void) {
  TEST_ASSERT_EQUAL(CSR_OK, csr_write(csrs, MTVEC, PRIV_M, 0x80000101));

  uxlen_t value = 0;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MTVEC, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(0x80000101, value);
}

static void test_mstatus_mie_bit_can_be_set(void) {
  TEST_ASSERT_EQUAL(CSR_OK, csr_write(csrs, MSTATUS, PRIV_M, MSTATUS_MIE));

  uxlen_t value = 0;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MSTATUS, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MIE,
                          value & (MSTATUS_MIE | MSTATUS_MPIE | MSTATUS_MPP));
}

static void test_mie_machine_interrupt_enables_can_be_written(void) {
  uxlen_t enables = MSIP | MTIP | MEIP;
  TEST_ASSERT_EQUAL(CSR_OK, csr_write(csrs, MIE, PRIV_M, enables));

  uxlen_t value = 0;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MIE, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(enables, value);
}

static void test_misa_reports_xlen_and_implemented_extensions(void) {
  uxlen_t value = 0;

  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MISA, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(MISA_MXL_FIELD | MISA_EXT('I') | MISA_EXT('M') |
                              MISA_EXT('S') | MISA_EXT('U'),
                          value);
}

static void test_machine_id_csrs_read_zero(void) {
  static const uint16_t ids[] = {MVENDORID, MARCHID, MIMPID};
  for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
    uxlen_t value = 0xFF;
    TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, ids[i], PRIV_M, &value));
    TEST_ASSERT_EQUAL_HEX64(0, value);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_mscratch_written_value_can_be_read_back);
  RUN_TEST(test_read_of_unimplemented_csr_is_illegal);
  RUN_TEST(test_mhartid_reads_zero_on_single_hart);
  RUN_TEST(test_write_to_read_only_mhartid_is_illegal);
  RUN_TEST(test_mepc_written_value_can_be_read_back);
  RUN_TEST(test_mepc_bit_zero_is_always_zero);
  RUN_TEST(test_mcause_written_value_can_be_read_back);
  RUN_TEST(test_mtval_written_value_can_be_read_back);
  RUN_TEST(test_mtvec_written_value_can_be_read_back);
  RUN_TEST(test_mtvec_reserved_mode_reads_back_as_legal);
  RUN_TEST(test_mtvec_vectored_mode_is_preserved);
  RUN_TEST(test_mstatus_mie_bit_can_be_set);
  RUN_TEST(test_mie_machine_interrupt_enables_can_be_written);
  RUN_TEST(test_misa_reports_xlen_and_implemented_extensions);
  RUN_TEST(test_machine_id_csrs_read_zero);
  return UNITY_END();
}
