#include "cpu_fixture.h"

static void test_hart_starts_in_machine_mode(void) {
  TEST_ASSERT_EQUAL(PRIV_M, cpu_get_privilege(cpu));
}

static void test_mret_switches_to_privilege_in_mpp(void) {
  write_csr(MSTATUS, MSTATUS_MPP_U);

  execute_raw(MRET);

  TEST_ASSERT_EQUAL(PRIV_U, cpu_get_privilege(cpu));
}

static void test_mret_with_mpp_machine_stays_in_machine_mode(void) {
  write_csr(MSTATUS, MSTATUS_MPP_M);

  execute_raw(MRET);

  TEST_ASSERT_EQUAL(PRIV_M, cpu_get_privilege(cpu));
}

static void test_trap_from_user_mode_switches_to_machine_mode(void) {
  cpu_set_privilege(cpu, PRIV_U);

  execute_raw(ILLEGAL_INSTRUCTION);

  TEST_ASSERT_EQUAL(PRIV_M, cpu_get_privilege(cpu));
}

static void test_trap_from_user_mode_saves_user_in_mpp(void) {
  write_csr(MSTATUS, MSTATUS_MPP_M);
  cpu_set_privilege(cpu, PRIV_U);

  execute_raw(ILLEGAL_INSTRUCTION);

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MPP_U, read_csr(MSTATUS) & MSTATUS_MPP);
}

static void test_ecall_from_user_mode_raises_environment_call_from_u(void) {
  cpu_set_privilege(cpu, PRIV_U);

  execute_raw(ECALL);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_ECALL_FROM_U, read_csr(MCAUSE));
}

static void test_machine_csr_is_not_accessible_from_user_mode(void) {
  cpu_set_privilege(cpu, PRIV_U);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, MSCRATCH));
}

static void test_mret_is_illegal_in_user_mode(void) {
  cpu_set_privilege(cpu, PRIV_U);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(MRET);
}

static void test_mret_with_mpp_supervisor_enters_supervisor_mode(void) {
  write_csr(MSTATUS, MSTATUS_MPP_S);

  execute_raw(MRET);

  TEST_ASSERT_EQUAL(PRIV_S, cpu_get_privilege(cpu));
}

static void test_trap_from_supervisor_mode_saves_supervisor_in_mpp(void) {
  write_csr(MSTATUS, MSTATUS_MPP_M);
  cpu_set_privilege(cpu, PRIV_S);

  execute_raw(ILLEGAL_INSTRUCTION);

  TEST_ASSERT_EQUAL(PRIV_M, cpu_get_privilege(cpu));
  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MPP_S, read_csr(MSTATUS) & MSTATUS_MPP);
}

static void
test_ecall_from_supervisor_mode_raises_environment_call_from_s(void) {
  cpu_set_privilege(cpu, PRIV_S);

  execute_raw(ECALL);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_ECALL_FROM_S, read_csr(MCAUSE));
}

static void test_supervisor_mode_reads_supervisor_csr(void) {
  write_csr(SSCRATCH, 0x1234);
  cpu_set_privilege(cpu, PRIV_S);

  execute_raw(CSRR(1, SSCRATCH));

  TEST_ASSERT_EQUAL_HEX64(0x1234, get_reg(1));
}

static void test_mret_is_illegal_in_supervisor_mode(void) {
  cpu_set_privilege(cpu, PRIV_S);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(MRET);
}

#define SSTATUS_FIELDS (SSTATUS_SIE | SSTATUS_SPIE | SSTATUS_SPP)
#define SEPC_TARGET 0x80000400

static void enter_supervisor_with_sstatus(uxlen_t sstatus) {
  write_csr(SSTATUS, sstatus);
  write_csr(SEPC, SEPC_TARGET);
  cpu_set_privilege(cpu, PRIV_S);
}

static void test_sret_jumps_to_sepc(void) {
  enter_supervisor_with_sstatus(SSTATUS_SPP_U);

  execute_raw(SRET);

  TEST_ASSERT_EQUAL_HEX64(SEPC_TARGET, cpu_get_pc(cpu));
}

static void test_sret_with_spp_user_returns_to_user_mode(void) {
  enter_supervisor_with_sstatus(SSTATUS_SPP_U);

  execute_raw(SRET);

  TEST_ASSERT_EQUAL(PRIV_U, cpu_get_privilege(cpu));
}

static void test_sret_with_spp_supervisor_stays_in_supervisor_mode(void) {
  enter_supervisor_with_sstatus(SSTATUS_SPP);

  execute_raw(SRET);

  TEST_ASSERT_EQUAL(PRIV_S, cpu_get_privilege(cpu));
}

static void test_sret_restores_sie_from_spie(void) {
  enter_supervisor_with_sstatus(SSTATUS_SPIE);

  execute_raw(SRET);

  TEST_ASSERT_EQUAL_HEX64(SSTATUS_SIE | SSTATUS_SPIE,
                          read_csr(SSTATUS) & SSTATUS_FIELDS);
}

static void test_sret_sets_spie_and_clears_spp(void) {
  enter_supervisor_with_sstatus(SSTATUS_SIE | SSTATUS_SPP);

  execute_raw(SRET);

  TEST_ASSERT_EQUAL_HEX64(SSTATUS_SPIE, read_csr(SSTATUS) & SSTATUS_FIELDS);
}

static void test_sret_leaves_machine_fields_of_mstatus(void) {
  write_csr(MSTATUS, MSTATUS_MIE | MSTATUS_MPIE | MSTATUS_MPP_M);
  write_csr(SEPC, SEPC_TARGET);
  cpu_set_privilege(cpu, PRIV_S);

  execute_raw(SRET);

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MIE | MSTATUS_MPIE | MSTATUS_MPP_M,
                          read_csr(MSTATUS) &
                              (MSTATUS_MIE | MSTATUS_MPIE | MSTATUS_MPP));
}

static void test_sret_is_allowed_in_machine_mode(void) {
  write_csr(SSTATUS, SSTATUS_SPP_U);
  write_csr(SEPC, SEPC_TARGET);

  execute_raw(SRET);

  TEST_ASSERT_EQUAL_HEX64(SEPC_TARGET, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL(PRIV_U, cpu_get_privilege(cpu));
}

static void test_sret_is_illegal_in_user_mode(void) {
  cpu_set_privilege(cpu, PRIV_U);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(SRET);
}

// Returning to a mode below M clears MPRV, so M-mode code that set it to
// reach S or U memory cannot leave it behind.
static void test_mret_to_a_lower_mode_clears_mprv(void) {
  write_csr(MSTATUS, MSTATUS_MPRV | MSTATUS_MPP_S);

  execute_raw(MRET);

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(MSTATUS) & MSTATUS_MPRV);
}

static void test_mret_to_machine_mode_keeps_mprv(void) {
  write_csr(MSTATUS, MSTATUS_MPRV | MSTATUS_MPP_M);

  execute_raw(MRET);

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MPRV, read_csr(MSTATUS) & MSTATUS_MPRV);
}

static void test_sret_clears_mprv(void) {
  write_csr(MSTATUS, MSTATUS_MPRV);
  write_csr(SEPC, SEPC_TARGET);
  cpu_set_privilege(cpu, PRIV_S);

  execute_raw(SRET);

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(MSTATUS) & MSTATUS_MPRV);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_hart_starts_in_machine_mode);
  RUN_TEST(test_mret_switches_to_privilege_in_mpp);
  RUN_TEST(test_mret_with_mpp_machine_stays_in_machine_mode);
  RUN_TEST(test_trap_from_user_mode_switches_to_machine_mode);
  RUN_TEST(test_trap_from_user_mode_saves_user_in_mpp);
  RUN_TEST(test_ecall_from_user_mode_raises_environment_call_from_u);
  RUN_TEST(test_machine_csr_is_not_accessible_from_user_mode);
  RUN_TEST(test_mret_is_illegal_in_user_mode);
  RUN_TEST(test_mret_with_mpp_supervisor_enters_supervisor_mode);
  RUN_TEST(test_trap_from_supervisor_mode_saves_supervisor_in_mpp);
  RUN_TEST(test_ecall_from_supervisor_mode_raises_environment_call_from_s);
  RUN_TEST(test_supervisor_mode_reads_supervisor_csr);
  RUN_TEST(test_mret_is_illegal_in_supervisor_mode);
  RUN_TEST(test_sret_jumps_to_sepc);
  RUN_TEST(test_sret_with_spp_user_returns_to_user_mode);
  RUN_TEST(test_sret_with_spp_supervisor_stays_in_supervisor_mode);
  RUN_TEST(test_sret_restores_sie_from_spie);
  RUN_TEST(test_sret_sets_spie_and_clears_spp);
  RUN_TEST(test_sret_leaves_machine_fields_of_mstatus);
  RUN_TEST(test_sret_is_allowed_in_machine_mode);
  RUN_TEST(test_sret_is_illegal_in_user_mode);
  RUN_TEST(test_mret_to_a_lower_mode_clears_mprv);
  RUN_TEST(test_mret_to_machine_mode_keeps_mprv);
  RUN_TEST(test_sret_clears_mprv);
  return UNITY_END();
}
