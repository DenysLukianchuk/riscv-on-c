#include "cpu_fixture.h"

#define DELEGATE(cause) ((uxlen_t)1 << (cause))

#define FAULTING_PC 0x80000100
#define STVEC_BASE 0x80000200
#define MTVEC_BASE 0x80000300

static void prepare_trap(uxlen_t medeleg, enum Privilege privilege) {
  write_csr(MEDELEG, medeleg);
  write_csr(STVEC, STVEC_BASE);
  write_csr(MTVEC, MTVEC_BASE);
  cpu_set_pc(cpu, FAULTING_PC);
  cpu_set_privilege(cpu, privilege);
}

static void test_medeleg_written_value_can_be_read_back(void) {
  uxlen_t delegated =
      DELEGATE(CAUSE_ILLEGAL_INSTRUCTION) | DELEGATE(CAUSE_ECALL_FROM_U);
  write_csr(MEDELEG, delegated);

  TEST_ASSERT_EQUAL_HEX64(delegated, read_csr(MEDELEG));
}

static void test_medeleg_bit_for_ecall_from_m_is_read_only_zero(void) {
  write_csr(MEDELEG, DELEGATE(CAUSE_ECALL_FROM_M));

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(MEDELEG));
}

static void test_medeleg_is_not_accessible_from_supervisor_mode(void) {
  cpu_set_privilege(cpu, PRIV_S);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, MEDELEG));
}

static void test_delegated_exception_from_user_enters_supervisor_mode(void) {
  prepare_trap(DELEGATE(CAUSE_ILLEGAL_INSTRUCTION), PRIV_U);

  execute_raw(ILLEGAL_INSTRUCTION);

  TEST_ASSERT_EQUAL(PRIV_S, cpu_get_privilege(cpu));
  TEST_ASSERT_EQUAL_HEX64(STVEC_BASE, cpu_get_pc(cpu));
}

static void test_delegated_exception_writes_supervisor_trap_csrs(void) {
  prepare_trap(DELEGATE(CAUSE_ILLEGAL_INSTRUCTION), PRIV_U);

  execute_raw(ILLEGAL_INSTRUCTION);

  TEST_ASSERT_EQUAL_HEX64(FAULTING_PC, read_csr(SEPC));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_ILLEGAL_INSTRUCTION, read_csr(SCAUSE));
  TEST_ASSERT_EQUAL_HEX64(ILLEGAL_INSTRUCTION, read_csr(STVAL));
}

static void test_delegated_exception_leaves_machine_trap_csrs(void) {
  prepare_trap(DELEGATE(CAUSE_ILLEGAL_INSTRUCTION), PRIV_U);
  write_csr(MEPC, 0x80000AA0);
  write_csr(MCAUSE, CAUSE_LOAD_ACCESS);
  write_csr(MTVAL, 0x1234);
  write_csr(MSTATUS, MSTATUS_MIE | MSTATUS_MPP_M);

  execute_raw(ILLEGAL_INSTRUCTION);

  TEST_ASSERT_EQUAL_HEX64(0x80000AA0, read_csr(MEPC));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_LOAD_ACCESS, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(0x1234, read_csr(MTVAL));
  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MIE | MSTATUS_MPP_M,
                          read_csr(MSTATUS) &
                              (MSTATUS_MIE | MSTATUS_MPIE | MSTATUS_MPP));
}

static void test_delegated_exception_from_user_clears_spp(void) {
  prepare_trap(DELEGATE(CAUSE_ILLEGAL_INSTRUCTION), PRIV_U);
  write_csr(SSTATUS, SSTATUS_SPP);

  execute_raw(ILLEGAL_INSTRUCTION);

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(SSTATUS) & SSTATUS_SPP);
}

static void test_delegated_exception_from_supervisor_sets_spp(void) {
  prepare_trap(DELEGATE(CAUSE_ILLEGAL_INSTRUCTION), PRIV_S);

  execute_raw(ILLEGAL_INSTRUCTION);

  TEST_ASSERT_EQUAL(PRIV_S, cpu_get_privilege(cpu));
  TEST_ASSERT_EQUAL_HEX64(SSTATUS_SPP, read_csr(SSTATUS) & SSTATUS_SPP);
}

static void test_delegated_exception_moves_sie_into_spie(void) {
  prepare_trap(DELEGATE(CAUSE_ILLEGAL_INSTRUCTION), PRIV_U);
  write_csr(SSTATUS, SSTATUS_SIE);

  execute_raw(ILLEGAL_INSTRUCTION);

  TEST_ASSERT_EQUAL_HEX64(SSTATUS_SPIE,
                          read_csr(SSTATUS) & (SSTATUS_SIE | SSTATUS_SPIE));
}

static void test_undelegated_exception_from_user_goes_to_machine_mode(void) {
  prepare_trap(DELEGATE(CAUSE_ECALL_FROM_U), PRIV_U);

  execute_raw(ILLEGAL_INSTRUCTION);

  TEST_ASSERT_EQUAL(PRIV_M, cpu_get_privilege(cpu));
  TEST_ASSERT_EQUAL_HEX64(MTVEC_BASE, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_ILLEGAL_INSTRUCTION, read_csr(MCAUSE));
}

static void test_exception_in_machine_mode_is_never_delegated(void) {
  prepare_trap(DELEGATE(CAUSE_ILLEGAL_INSTRUCTION), PRIV_M);

  execute_raw(ILLEGAL_INSTRUCTION);

  TEST_ASSERT_EQUAL(PRIV_M, cpu_get_privilege(cpu));
  TEST_ASSERT_EQUAL_HEX64(MTVEC_BASE, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_ILLEGAL_INSTRUCTION, read_csr(MCAUSE));
}

static void test_delegated_ecall_from_user_reaches_supervisor(void) {
  prepare_trap(DELEGATE(CAUSE_ECALL_FROM_U), PRIV_U);

  execute_raw(ECALL);

  TEST_ASSERT_EQUAL(PRIV_S, cpu_get_privilege(cpu));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_ECALL_FROM_U, read_csr(SCAUSE));
}

static void
test_ecall_from_supervisor_goes_to_machine_when_not_delegated(void) {
  prepare_trap(DELEGATE(CAUSE_ECALL_FROM_U), PRIV_S);

  execute_raw(ECALL);

  TEST_ASSERT_EQUAL(PRIV_M, cpu_get_privilege(cpu));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_ECALL_FROM_S, read_csr(MCAUSE));
}

static void test_mideleg_holds_supervisor_interrupt_bits(void) {
  write_csr(MIDELEG, SSIP | STIP | SEIP);

  TEST_ASSERT_EQUAL_HEX64(SSIP | STIP | SEIP, read_csr(MIDELEG));
}

static void test_mideleg_machine_interrupt_bits_are_read_only_zero(void) {
  write_csr(MIDELEG, MSIP | MTIP | MEIP);

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(MIDELEG));
}

static void test_mideleg_is_not_accessible_from_supervisor_mode(void) {
  cpu_set_privilege(cpu, PRIV_S);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, MIDELEG));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_medeleg_written_value_can_be_read_back);
  RUN_TEST(test_medeleg_bit_for_ecall_from_m_is_read_only_zero);
  RUN_TEST(test_medeleg_is_not_accessible_from_supervisor_mode);
  RUN_TEST(test_delegated_exception_from_user_enters_supervisor_mode);
  RUN_TEST(test_delegated_exception_writes_supervisor_trap_csrs);
  RUN_TEST(test_delegated_exception_leaves_machine_trap_csrs);
  RUN_TEST(test_delegated_exception_from_user_clears_spp);
  RUN_TEST(test_delegated_exception_from_supervisor_sets_spp);
  RUN_TEST(test_delegated_exception_moves_sie_into_spie);
  RUN_TEST(test_undelegated_exception_from_user_goes_to_machine_mode);
  RUN_TEST(test_exception_in_machine_mode_is_never_delegated);
  RUN_TEST(test_delegated_ecall_from_user_reaches_supervisor);
  RUN_TEST(test_ecall_from_supervisor_goes_to_machine_when_not_delegated);
  RUN_TEST(test_mideleg_holds_supervisor_interrupt_bits);
  RUN_TEST(test_mideleg_machine_interrupt_bits_are_read_only_zero);
  RUN_TEST(test_mideleg_is_not_accessible_from_supervisor_mode);
  return UNITY_END();
}
