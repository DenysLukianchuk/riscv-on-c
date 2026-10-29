#include "cpu_fixture.h"
#include "riscv/cpu/trap.h"

#define FAULTING_PC 0x80000010

static uxlen_t raise_load_access_fault(uxlen_t addr) {
  cpu_set_pc(cpu, FAULTING_PC);
  return cpu_raise_exception(cpu, CAUSE_LOAD_ACCESS, addr);
}

static void test_trap_saves_faulting_pc_to_mepc(void) {
  raise_load_access_fault(0x0);

  TEST_ASSERT_EQUAL_HEX64(FAULTING_PC, read_csr(MEPC));
}

static void test_load_access_fault_sets_mcause(void) {
  raise_load_access_fault(0x0);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_LOAD_ACCESS, read_csr(MCAUSE));
}

static void test_trap_saves_faulting_address_to_mtval(void) {
  raise_load_access_fault(0x90000000);

  TEST_ASSERT_EQUAL_HEX64(0x90000000, read_csr(MTVAL));
}

static void test_trap_jumps_to_mtvec_base_in_direct_mode(void) {
  write_csr(MTVEC, 0x80000100);

  uxlen_t handler = raise_load_access_fault(0x90000000);

  TEST_ASSERT_EQUAL_HEX64(0x80000100, handler);
}

static void test_exception_jumps_to_mtvec_base_in_vectored_mode(void) {
  write_csr(MTVEC, 0x80000101);

  uxlen_t handler = raise_load_access_fault(0x90000000);

  TEST_ASSERT_EQUAL_HEX64(0x80000100, handler);
}

static void test_trap_moves_mie_into_mpie_and_disables_interrupts(void) {
  write_csr(MSTATUS, MSTATUS_MIE);

  raise_load_access_fault(0x90000000);

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MPIE,
                          read_csr(MSTATUS) & (MSTATUS_MIE | MSTATUS_MPIE));
}

static void test_trap_clears_stale_mpie_when_interrupts_were_disabled(void) {
  write_csr(MSTATUS, MSTATUS_MPIE);

  raise_load_access_fault(0x90000000);

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(MSTATUS) & (MSTATUS_MIE | MSTATUS_MPIE));
}

static void test_trap_from_machine_mode_sets_mpp_to_machine(void) {
  raise_load_access_fault(0x90000000);

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MPP_M, read_csr(MSTATUS) & MSTATUS_MPP);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_trap_saves_faulting_pc_to_mepc);
  RUN_TEST(test_load_access_fault_sets_mcause);
  RUN_TEST(test_trap_saves_faulting_address_to_mtval);
  RUN_TEST(test_trap_jumps_to_mtvec_base_in_direct_mode);
  RUN_TEST(test_exception_jumps_to_mtvec_base_in_vectored_mode);
  RUN_TEST(test_trap_moves_mie_into_mpie_and_disables_interrupts);
  RUN_TEST(test_trap_clears_stale_mpie_when_interrupts_were_disabled);
  RUN_TEST(test_trap_from_machine_mode_sets_mpp_to_machine);
  return UNITY_END();
}
