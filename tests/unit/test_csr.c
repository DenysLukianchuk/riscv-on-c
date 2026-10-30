#include "riscv/cpu/csr.h"
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

static uxlen_t mstatus_after_writing(uxlen_t value) {
  TEST_ASSERT_EQUAL(CSR_OK, csr_write(csrs, MSTATUS, PRIV_M, value));
  uxlen_t read = 0;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MSTATUS, PRIV_M, &read));
  return read;
}

static void test_mstatus_keeps_its_implemented_fields(void) {
  uxlen_t fields = MSTATUS_SIE | MSTATUS_MIE | MSTATUS_SPIE | MSTATUS_MPIE |
                   MSTATUS_SPP | MSTATUS_MPP_S | MSTATUS_TW;

  uxlen_t value = mstatus_after_writing(fields);

  TEST_ASSERT_EQUAL_HEX64(fields, value & (fields | MSTATUS_MPP));
}

// No F, V or custom extension state to track, so FS, VS, XS and SD stay 0.
static void test_mstatus_extension_state_reads_zero(void) {
  uxlen_t state = MSTATUS_FS | MSTATUS_VS | MSTATUS_XS | MSTATUS_SD;

  TEST_ASSERT_EQUAL_HEX64(0, mstatus_after_writing(state) & state);
}

static void test_mstatus_endianness_is_fixed_to_little(void) {
  uxlen_t endianness = MSTATUS_UBE;
#if XLEN == 64
  endianness |= MSTATUS_SBE | MSTATUS_MBE;
#endif

  TEST_ASSERT_EQUAL_HEX64(0, mstatus_after_writing(endianness) & endianness);
}

// MPP is WARL and holds only implemented modes; 2 (H-mode) is not one.
static void test_mstatus_mpp_never_holds_an_unimplemented_mode(void) {
  uxlen_t mpp = mstatus_after_writing(MSTATUS_MPP_H) & MSTATUS_MPP;

  TEST_ASSERT_NOT_EQUAL(MSTATUS_MPP_H, mpp);
}

static void test_mie_keeps_only_implemented_interrupt_enables(void) {
  TEST_ASSERT_EQUAL(CSR_OK, csr_write(csrs, MIE, PRIV_M, ~(uxlen_t)0));

  uxlen_t value = 0;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MIE, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(SSIP | MSIP | STIP | MTIP | SEIP | MEIP, value);
}

#define IMPLEMENTED_MISA                                                       \
  (MISA_MXL_FIELD | MISA_EXT('A') | MISA_EXT('C') | MISA_EXT('I') |            \
   MISA_EXT('M') | MISA_EXT('S') | MISA_EXT('U'))

static void test_misa_reports_xlen_and_implemented_extensions(void) {
  uxlen_t value = 0;

  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MISA, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(IMPLEMENTED_MISA, value);
}

static void test_misa_write_is_ignored_without_trapping(void) {
  TEST_ASSERT_EQUAL(CSR_OK, csr_write(csrs, MISA, PRIV_M, 0));

  uxlen_t value = 0;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(csrs, MISA, PRIV_M, &value));
  TEST_ASSERT_EQUAL_HEX64(IMPLEMENTED_MISA, value);
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
  RUN_TEST(test_mstatus_keeps_its_implemented_fields);
  RUN_TEST(test_mstatus_extension_state_reads_zero);
  RUN_TEST(test_mstatus_endianness_is_fixed_to_little);
  RUN_TEST(test_mstatus_mpp_never_holds_an_unimplemented_mode);
  RUN_TEST(test_mie_keeps_only_implemented_interrupt_enables);
  RUN_TEST(test_mie_machine_interrupt_enables_can_be_written);
  RUN_TEST(test_misa_reports_xlen_and_implemented_extensions);
  RUN_TEST(test_misa_write_is_ignored_without_trapping);
  RUN_TEST(test_machine_id_csrs_read_zero);
  return UNITY_END();
}
