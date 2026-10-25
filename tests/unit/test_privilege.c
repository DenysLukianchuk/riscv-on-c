#include "cpu_fixture.h"

static void test_hart_starts_in_machine_mode(void) {
  TEST_ASSERT_EQUAL(PRIV_M, cpu_get_privilege(cpu));
}

#define MRET 0x30200073
#define MSTATUS_MPP_USER 0

static void test_mret_switches_to_privilege_in_mpp(void) {
  write_csr(CSR_REG_MSTATUS, MSTATUS_MPP_USER);

  execute_raw(MRET);

  TEST_ASSERT_EQUAL(PRIV_U, cpu_get_privilege(cpu));
}

static void test_mret_with_mpp_machine_stays_in_machine_mode(void) {
  write_csr(CSR_REG_MSTATUS, MSTATUS_MPP_MACHINE);

  execute_raw(MRET);

  TEST_ASSERT_EQUAL(PRIV_M, cpu_get_privilege(cpu));
}

#define ILLEGAL_INSTRUCTION 0x00000000
#define ECALL 0x00000073

static void test_trap_from_user_mode_switches_to_machine_mode(void) {
  cpu_set_privilege(cpu, PRIV_U);

  execute_raw(ILLEGAL_INSTRUCTION);

  TEST_ASSERT_EQUAL(PRIV_M, cpu_get_privilege(cpu));
}

static void test_trap_from_user_mode_saves_user_in_mpp(void) {
  write_csr(CSR_REG_MSTATUS, MSTATUS_MPP_MACHINE);
  cpu_set_privilege(cpu, PRIV_U);

  execute_raw(ILLEGAL_INSTRUCTION);

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MPP_USER,
                          read_csr(CSR_REG_MSTATUS) & MSTATUS_MPP_MASK);
}

static void test_ecall_from_user_mode_raises_environment_call_from_u(void) {
  cpu_set_privilege(cpu, PRIV_U);

  execute_raw(ECALL);

  TEST_ASSERT_EQUAL_HEX64(8, read_csr(CSR_REG_MCAUSE));
}

#define CSRR_X1_MSCRATCH 0x340020F3

static void test_machine_csr_is_not_accessible_from_user_mode(void) {
  cpu_set_privilege(cpu, PRIV_U);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR_X1_MSCRATCH);
}

static void test_mret_is_illegal_in_user_mode(void) {
  cpu_set_privilege(cpu, PRIV_U);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(MRET);
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
  return UNITY_END();
}
