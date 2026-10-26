#include "riscv/csr.h"
#include "riscv_spec.h"
#include "unity.h"

#define SSTATUS_FIELDS (SSTATUS_SIE | SSTATUS_SPIE | SSTATUS_SPP)
#define MSTATUS_FIELDS (MSTATUS_MIE | MSTATUS_MPIE | MSTATUS_MPP)

static Csr *csrs;

void setUp(void) { csrs = csr_create(); }

void tearDown(void) { csr_destroy(csrs); }

// Failures are reported at the caller's line.
#define write_as(privilege, addr, value)                                       \
  write_as_at(__LINE__, privilege, addr, value)
#define read_as(privilege, addr) read_as_at(__LINE__, privilege, addr)

static void write_as_at(int line, enum Privilege privilege, uint16_t addr,
                        uxlen_t value) {
  UNITY_TEST_ASSERT_EQUAL_INT(CSR_OK, csr_write(csrs, addr, privilege, value),
                              line, "CSR write rejected");
}

static uxlen_t read_as_at(int line, enum Privilege privilege, uint16_t addr) {
  uxlen_t value = 0;
  UNITY_TEST_ASSERT_EQUAL_INT(CSR_OK, csr_read(csrs, addr, privilege, &value),
                              line, "CSR read rejected");
  return value;
}

static void test_sscratch_written_value_can_be_read_back(void) {
  write_as(PRIV_S, SSCRATCH, 0xDEADBEEF);

  TEST_ASSERT_EQUAL_HEX64(0xDEADBEEF, read_as(PRIV_S, SSCRATCH));
}

static void test_sepc_bit_zero_is_always_zero(void) {
  write_as(PRIV_S, SEPC, 0x80001235);

  TEST_ASSERT_EQUAL_HEX64(0x80001234, read_as(PRIV_S, SEPC));
}

static void test_scause_written_value_can_be_read_back(void) {
  write_as(PRIV_S, SCAUSE, 5);

  TEST_ASSERT_EQUAL_HEX64(5, read_as(PRIV_S, SCAUSE));
}

static void test_stval_written_value_can_be_read_back(void) {
  write_as(PRIV_S, STVAL, 0xDEADBEEF);

  TEST_ASSERT_EQUAL_HEX64(0xDEADBEEF, read_as(PRIV_S, STVAL));
}

static void test_stvec_written_value_can_be_read_back(void) {
  write_as(PRIV_S, STVEC, 0x80000100);

  TEST_ASSERT_EQUAL_HEX64(0x80000100, read_as(PRIV_S, STVEC));
}

static void test_stvec_reserved_mode_reads_back_as_legal(void) {
  write_as(PRIV_S, STVEC, 0x80000102);

  TEST_ASSERT_EQUAL_HEX64(0x80000100, read_as(PRIV_S, STVEC));
}

static void test_stvec_vectored_mode_is_preserved(void) {
  write_as(PRIV_S, STVEC, 0x80000101);

  TEST_ASSERT_EQUAL_HEX64(0x80000101, read_as(PRIV_S, STVEC));
}

static void test_sstatus_fields_are_stored_in_mstatus(void) {
  write_as(PRIV_S, SSTATUS, SSTATUS_SIE | SSTATUS_SPIE | SSTATUS_SPP);

  TEST_ASSERT_EQUAL_HEX64(SSTATUS_FIELDS,
                          read_as(PRIV_M, MSTATUS) &
                              (SSTATUS_FIELDS | MSTATUS_FIELDS));
}

static void test_sstatus_hides_machine_fields_of_mstatus(void) {
  write_as(PRIV_M, MSTATUS,
           MSTATUS_MIE | MSTATUS_MPIE | MSTATUS_MPP | SSTATUS_SIE);

  uxlen_t sstatus = read_as(PRIV_S, SSTATUS);
  TEST_ASSERT_EQUAL_HEX64(0, sstatus & MSTATUS_FIELDS);
  TEST_ASSERT_EQUAL_HEX64(SSTATUS_SIE, sstatus & SSTATUS_FIELDS);
}

static void test_sstatus_write_leaves_machine_fields_of_mstatus(void) {
  write_as(PRIV_M, MSTATUS, MSTATUS_MIE | MSTATUS_MPP);

  write_as(PRIV_S, SSTATUS, 0);

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MIE | MSTATUS_MPP,
                          read_as(PRIV_M, MSTATUS) & MSTATUS_FIELDS);
}

static void test_sstatus_write_clears_supervisor_fields(void) {
  write_as(PRIV_M, MSTATUS, SSTATUS_SIE | SSTATUS_SPIE | SSTATUS_SPP);

  write_as(PRIV_S, SSTATUS, 0);

  TEST_ASSERT_EQUAL_HEX64(0, read_as(PRIV_S, SSTATUS) & SSTATUS_FIELDS);
}

static void test_supervisor_csr_is_not_accessible_from_user_mode(void) {
  uxlen_t value = 0;

  TEST_ASSERT_EQUAL(CSR_ILLEGAL, csr_read(csrs, SSCRATCH, PRIV_U, &value));
  TEST_ASSERT_EQUAL(CSR_ILLEGAL, csr_write(csrs, SSCRATCH, PRIV_U, 1));
}

static void test_machine_csr_is_not_accessible_from_supervisor_mode(void) {
  uxlen_t value = 0;

  TEST_ASSERT_EQUAL(CSR_ILLEGAL, csr_read(csrs, MSCRATCH, PRIV_S, &value));
  TEST_ASSERT_EQUAL(CSR_ILLEGAL, csr_write(csrs, MSCRATCH, PRIV_S, 1));
}

#if XLEN == 64

static void test_mstatus_reports_64_bit_user_and_supervisor_modes(void) {
  uxlen_t mstatus = read_as(PRIV_M, MSTATUS);

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_UXL_64 | MSTATUS_SXL_64,
                          mstatus & (MSTATUS_UXL | MSTATUS_SXL));
}

static void test_mstatus_xl_fields_are_read_only(void) {
  write_as(PRIV_M, MSTATUS, 0);

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_UXL_64 | MSTATUS_SXL_64,
                          read_as(PRIV_M, MSTATUS) &
                              (MSTATUS_UXL | MSTATUS_SXL));
}

static void test_mstatus_xl_fields_ignore_written_values(void) {
  uxlen_t xl_32 = ((uxlen_t)1 << 32) | ((uxlen_t)1 << 34);
  write_as(PRIV_M, MSTATUS, xl_32);

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_UXL_64 | MSTATUS_SXL_64,
                          read_as(PRIV_M, MSTATUS) &
                              (MSTATUS_UXL | MSTATUS_SXL));
}

static void test_sstatus_reports_64_bit_user_mode(void) {
  write_as(PRIV_S, SSTATUS, 0);

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_UXL_64, read_as(PRIV_S, SSTATUS) &
                                              (MSTATUS_UXL | MSTATUS_SXL));
}
#endif

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_sscratch_written_value_can_be_read_back);
  RUN_TEST(test_sepc_bit_zero_is_always_zero);
  RUN_TEST(test_scause_written_value_can_be_read_back);
  RUN_TEST(test_stval_written_value_can_be_read_back);
  RUN_TEST(test_stvec_written_value_can_be_read_back);
  RUN_TEST(test_stvec_reserved_mode_reads_back_as_legal);
  RUN_TEST(test_stvec_vectored_mode_is_preserved);
  RUN_TEST(test_sstatus_fields_are_stored_in_mstatus);
  RUN_TEST(test_sstatus_hides_machine_fields_of_mstatus);
  RUN_TEST(test_sstatus_write_leaves_machine_fields_of_mstatus);
  RUN_TEST(test_sstatus_write_clears_supervisor_fields);
  RUN_TEST(test_supervisor_csr_is_not_accessible_from_user_mode);
  RUN_TEST(test_machine_csr_is_not_accessible_from_supervisor_mode);
#if XLEN == 64
  RUN_TEST(test_mstatus_reports_64_bit_user_and_supervisor_modes);
  RUN_TEST(test_mstatus_xl_fields_are_read_only);
  RUN_TEST(test_mstatus_xl_fields_ignore_written_values);
  RUN_TEST(test_sstatus_reports_64_bit_user_mode);
#endif
  return UNITY_END();
}
