#include "riscv/csr.h"
#include "riscv/trap.h"
#include "unity.h"

static Csr *csrs;

void setUp(void) { csrs = csr_create(); }

void tearDown(void) { csr_destroy(csrs); }

#define FAULTING_PC 0x80000010

static void test_trap_saves_faulting_pc_to_mepc(void) {
  trap_raise(csrs, FAULTING_PC, TRAP_LOAD_ACCESS_FAULT, 0x0);

  uxlen_t mepc = 0;
  csr_read(csrs, CSR_REG_MEPC, &mepc);
  TEST_ASSERT_EQUAL_HEX64(FAULTING_PC, mepc);
}

static void test_load_access_fault_sets_mcause_to_5(void) {
  trap_raise(csrs, FAULTING_PC, TRAP_LOAD_ACCESS_FAULT, 0x0);

  uxlen_t mcause = 0;
  csr_read(csrs, CSR_REG_MCAUSE, &mcause);
  TEST_ASSERT_EQUAL_HEX64(5, mcause);
}

static void test_trap_saves_faulting_address_to_mtval(void) {
  trap_raise(csrs, FAULTING_PC, TRAP_LOAD_ACCESS_FAULT, 0x90000000);

  uxlen_t mtval = 0;
  csr_read(csrs, CSR_REG_MTVAL, &mtval);
  TEST_ASSERT_EQUAL_HEX64(0x90000000, mtval);
}

static void test_trap_jumps_to_mtvec_base_in_direct_mode(void) {
  csr_write(csrs, CSR_REG_MTVEC, 0x80000100);

  uxlen_t handler =
      trap_raise(csrs, FAULTING_PC, TRAP_LOAD_ACCESS_FAULT, 0x90000000);

  TEST_ASSERT_EQUAL_HEX64(0x80000100, handler);
}

static void test_exception_jumps_to_mtvec_base_in_vectored_mode(void) {
  csr_write(csrs, CSR_REG_MTVEC, 0x80000101);

  uxlen_t handler =
      trap_raise(csrs, FAULTING_PC, TRAP_LOAD_ACCESS_FAULT, 0x90000000);

  TEST_ASSERT_EQUAL_HEX64(0x80000100, handler);
}

static void test_trap_moves_mie_into_mpie_and_disables_interrupts(void) {
  csr_write(csrs, CSR_REG_MSTATUS, MSTATUS_MIE_BIT);
  trap_raise(csrs, FAULTING_PC, TRAP_LOAD_ACCESS_FAULT, 0x90000000);

  uxlen_t mstatus = 0;
  csr_read(csrs, CSR_REG_MSTATUS, &mstatus);
  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MPIE_BIT,
                          mstatus & (MSTATUS_MIE_BIT | MSTATUS_MPIE_BIT));
}

static void test_trap_clears_stale_mpie_when_interrupts_were_disabled(void) {
  csr_write(csrs, CSR_REG_MSTATUS, MSTATUS_MPIE_BIT);
  trap_raise(csrs, FAULTING_PC, TRAP_LOAD_ACCESS_FAULT, 0x90000000);

  uxlen_t mstatus = 0;
  csr_read(csrs, CSR_REG_MSTATUS, &mstatus);
  TEST_ASSERT_EQUAL_HEX64(0, mstatus & (MSTATUS_MIE_BIT | MSTATUS_MPIE_BIT));
}

static void test_trap_from_machine_mode_sets_mpp_to_machine(void) {
  trap_raise(csrs, FAULTING_PC, TRAP_LOAD_ACCESS_FAULT, 0x90000000);

  uxlen_t mstatus = 0;
  csr_read(csrs, CSR_REG_MSTATUS, &mstatus);
  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MPP_MACHINE, mstatus & MSTATUS_MPP_MASK);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_trap_saves_faulting_pc_to_mepc);
  RUN_TEST(test_load_access_fault_sets_mcause_to_5);
  RUN_TEST(test_trap_saves_faulting_address_to_mtval);
  RUN_TEST(test_trap_jumps_to_mtvec_base_in_direct_mode);
  RUN_TEST(test_exception_jumps_to_mtvec_base_in_vectored_mode);
  RUN_TEST(test_trap_moves_mie_into_mpie_and_disables_interrupts);
  RUN_TEST(test_trap_clears_stale_mpie_when_interrupts_were_disabled);
  RUN_TEST(test_trap_from_machine_mode_sets_mpp_to_machine);
  return UNITY_END();
}
