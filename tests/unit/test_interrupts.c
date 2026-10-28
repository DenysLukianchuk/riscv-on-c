#include "riscv/csr.h"
#include "riscv_spec.h"
#include "unity.h"

#define SUPERVISOR_INTERRUPTS (SSIP | STIP | SEIP)
#define MACHINE_INTERRUPTS (MSIP | MTIP | MEIP)
#define ALL_INTERRUPTS (SUPERVISOR_INTERRUPTS | MACHINE_INTERRUPTS)

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

static void test_no_interrupt_is_pending_after_reset(void) {
  TEST_ASSERT_EQUAL_HEX64(0, read_as(PRIV_M, MIP));
}

// MSIP, MTIP and MEIP come from the CLINT and the PLIC; software can only
// raise the supervisor ones directly.
static void test_mip_lets_software_set_only_supervisor_pending_bits(void) {
  write_as(PRIV_M, MIP, ~(uxlen_t)0);

  TEST_ASSERT_EQUAL_HEX64(SUPERVISOR_INTERRUPTS, read_as(PRIV_M, MIP));
}

static void test_mip_supervisor_pending_bits_can_be_cleared(void) {
  write_as(PRIV_M, MIP, SUPERVISOR_INTERRUPTS);
  write_as(PRIV_M, MIP, 0);

  TEST_ASSERT_EQUAL_HEX64(0, read_as(PRIV_M, MIP));
}

static void test_mip_is_machine_only(void) {
  uxlen_t value = 0;

  TEST_ASSERT_EQUAL(CSR_ILLEGAL, csr_read(csrs, MIP, PRIV_S, &value));
}

// sie and sip show only the interrupts mideleg hands to S-mode.

static void test_sie_shows_only_delegated_enables(void) {
  write_as(PRIV_M, MIE, ALL_INTERRUPTS);
  write_as(PRIV_M, MIDELEG, SSIP | STIP);

  TEST_ASSERT_EQUAL_HEX64(SSIP | STIP, read_as(PRIV_S, SIE));
}

static void test_sie_write_changes_only_delegated_enables(void) {
  write_as(PRIV_M, MIE, MTIP | SEIP);
  write_as(PRIV_M, MIDELEG, STIP);

  write_as(PRIV_S, SIE, ~(uxlen_t)0 & ~SEIP);

  TEST_ASSERT_EQUAL_HEX64(MTIP | STIP | SEIP, read_as(PRIV_M, MIE));
}

static void test_sie_reads_zero_without_delegation(void) {
  write_as(PRIV_M, MIE, ALL_INTERRUPTS);

  TEST_ASSERT_EQUAL_HEX64(0, read_as(PRIV_S, SIE));
}

static void test_sip_shows_only_delegated_pending_bits(void) {
  write_as(PRIV_M, MIP, SUPERVISOR_INTERRUPTS);
  write_as(PRIV_M, MIDELEG, SSIP | STIP);

  TEST_ASSERT_EQUAL_HEX64(SSIP | STIP, read_as(PRIV_S, SIP));
}

// STIP and SEIP are raised by M-mode software or devices; S-mode can only
// raise and clear its own software interrupt.
static void test_sip_lets_supervisor_write_only_ssip(void) {
  write_as(PRIV_M, MIDELEG, SUPERVISOR_INTERRUPTS);

  write_as(PRIV_S, SIP, SUPERVISOR_INTERRUPTS);

  TEST_ASSERT_EQUAL_HEX64(SSIP, read_as(PRIV_M, MIP));
}

static void test_sip_cannot_touch_an_undelegated_ssip(void) {
  write_as(PRIV_M, MIP, SSIP);

  write_as(PRIV_S, SIP, 0);

  TEST_ASSERT_EQUAL_HEX64(SSIP, read_as(PRIV_M, MIP));
}

static void test_sie_and_sip_are_not_accessible_from_user_mode(void) {
  uxlen_t value = 0;

  TEST_ASSERT_EQUAL(CSR_ILLEGAL, csr_read(csrs, SIE, PRIV_U, &value));
  TEST_ASSERT_EQUAL(CSR_ILLEGAL, csr_read(csrs, SIP, PRIV_U, &value));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_no_interrupt_is_pending_after_reset);
  RUN_TEST(test_mip_lets_software_set_only_supervisor_pending_bits);
  RUN_TEST(test_mip_supervisor_pending_bits_can_be_cleared);
  RUN_TEST(test_mip_is_machine_only);
  RUN_TEST(test_sie_shows_only_delegated_enables);
  RUN_TEST(test_sie_write_changes_only_delegated_enables);
  RUN_TEST(test_sie_reads_zero_without_delegation);
  RUN_TEST(test_sip_shows_only_delegated_pending_bits);
  RUN_TEST(test_sip_lets_supervisor_write_only_ssip);
  RUN_TEST(test_sip_cannot_touch_an_undelegated_ssip);
  RUN_TEST(test_sie_and_sip_are_not_accessible_from_user_mode);
  return UNITY_END();
}
