#include "cpu_fixture.h"

static void test_ecall_in_machine_mode_raises_environment_call_from_m(void) {
  write_csr(MCAUSE, 0xFF);

  execute_raw(ECALL);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_ECALL_FROM_M, read_csr(MCAUSE));
}

static void execute_ecall_at_ram_base(void) {
  write_csr(MTVEC, TRAP_HANDLER);
  write_csr(MTVAL, 0xFF);
  cpu_set_pc(cpu, RAM_BASE);

  execute_raw(ECALL);
}

static void test_ecall_saves_its_own_address_to_mepc(void) {
  execute_ecall_at_ram_base();

  TEST_ASSERT_EQUAL_HEX64(RAM_BASE, read_csr(MEPC));
}

static void test_ecall_jumps_to_trap_handler(void) {
  execute_ecall_at_ram_base();

  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
}

static void test_ecall_sets_mtval_to_zero(void) {
  execute_ecall_at_ram_base();

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(MTVAL));
}

static void execute_ebreak_at_ram_base(void) {
  write_csr(MTVEC, TRAP_HANDLER);
  cpu_set_pc(cpu, RAM_BASE);

  execute_raw(EBREAK);
}

static void test_ebreak_raises_breakpoint(void) {
  execute_ebreak_at_ram_base();

  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE, read_csr(MEPC));
  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
}

static void test_ebreak_saves_its_own_address_to_mtval(void) {
  execute_ebreak_at_ram_base();

  TEST_ASSERT_EQUAL_HEX64(RAM_BASE, read_csr(MTVAL));
}

static void test_mret_jumps_to_mepc(void) {
  write_csr(MEPC, RAM_BASE + 0x104);

  execute_raw(MRET);

  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 0x104, cpu_get_pc(cpu));
}

static void test_mret_restores_mie_from_mpie(void) {
  write_csr(MSTATUS, MSTATUS_MPIE);

  execute_raw(MRET);

  TEST_ASSERT_BITS_HIGH(MSTATUS_MIE, read_csr(MSTATUS));
}

static void test_mret_sets_mpie(void) {
  write_csr(MSTATUS, MSTATUS_MIE);

  execute_raw(MRET);

  uxlen_t mstatus = read_csr(MSTATUS);
  TEST_ASSERT_BITS_HIGH(MSTATUS_MPIE, mstatus);
  TEST_ASSERT_BITS_LOW(MSTATUS_MIE, mstatus);
}

static void test_fence_executes_as_no_op(void) {
  write_csr(MCAUSE, 0xFF);
  cpu_set_pc(cpu, RAM_BASE);

  execute_raw(FENCE(FENCE_R | FENCE_W, FENCE_R | FENCE_W));

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 4, cpu_get_pc(cpu));
}

static void test_fence_i_executes_as_no_op(void) {
  write_csr(MCAUSE, 0xFF);
  cpu_set_pc(cpu, RAM_BASE);

  execute_raw(FENCE_I);

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 4, cpu_get_pc(cpu));
}

#define MISC_MEM_FUNCT3_2 RV_I(0x033, 0, 2, 0, ASM_MISC_MEM)

static void test_misc_mem_with_reserved_funct3_raises_illegal(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(MISC_MEM_FUNCT3_2);
}

// Without interrupts there is nothing to wait for, so wfi may simply retire.
static void assert_wfi_retires(void) {
  write_csr(MCAUSE, 0xFF);
  cpu_set_pc(cpu, RAM_BASE);

  execute_raw(WFI);

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 4, cpu_get_pc(cpu));
}

static void test_wfi_in_machine_mode_retires(void) { assert_wfi_retires(); }

static void test_wfi_in_machine_mode_ignores_tw(void) {
  write_csr(MSTATUS, MSTATUS_TW);

  assert_wfi_retires();
}

static void test_wfi_in_supervisor_mode_retires(void) {
  cpu_set_privilege(cpu, PRIV_S);

  assert_wfi_retires();
}

static void test_wfi_in_supervisor_mode_with_tw_is_illegal(void) {
  write_csr(MSTATUS, MSTATUS_TW);
  cpu_set_privilege(cpu, PRIV_S);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(WFI);
}

static void test_wfi_in_user_mode_is_illegal(void) {
  cpu_set_privilege(cpu, PRIV_U);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(WFI);
}

// sfence.vma orders page table writes before later translations. Without a
// TLB there is nothing to flush, so it retires; but it is still illegal in
// U-mode, and in S-mode when mstatus.TVM (trap virtual memory) is set.
static void assert_sfence_vma_retires(void) {
  write_csr(MCAUSE, 0xFF);
  cpu_set_pc(cpu, RAM_BASE);

  execute_raw(SFENCE_VMA(1, 2));

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 4, cpu_get_pc(cpu));
}

static void test_sfence_vma_in_supervisor_mode_retires(void) {
  cpu_set_privilege(cpu, PRIV_S);

  assert_sfence_vma_retires();
}

static void test_sfence_vma_in_machine_mode_ignores_tvm(void) {
  write_csr(MSTATUS, MSTATUS_TVM);

  assert_sfence_vma_retires();
}

static void test_sfence_vma_in_supervisor_mode_with_tvm_is_illegal(void) {
  write_csr(MSTATUS, MSTATUS_TVM);
  cpu_set_privilege(cpu, PRIV_S);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(SFENCE_VMA(1, 2));
}

static void test_sfence_vma_in_user_mode_is_illegal(void) {
  cpu_set_privilege(cpu, PRIV_U);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(SFENCE_VMA(1, 2));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_ecall_in_machine_mode_raises_environment_call_from_m);
  RUN_TEST(test_ecall_saves_its_own_address_to_mepc);
  RUN_TEST(test_ecall_jumps_to_trap_handler);
  RUN_TEST(test_ecall_sets_mtval_to_zero);
  RUN_TEST(test_ebreak_raises_breakpoint);
  RUN_TEST(test_ebreak_saves_its_own_address_to_mtval);
  RUN_TEST(test_mret_jumps_to_mepc);
  RUN_TEST(test_mret_restores_mie_from_mpie);
  RUN_TEST(test_mret_sets_mpie);
  RUN_TEST(test_fence_executes_as_no_op);
  RUN_TEST(test_fence_i_executes_as_no_op);
  RUN_TEST(test_misc_mem_with_reserved_funct3_raises_illegal);
  RUN_TEST(test_wfi_in_machine_mode_retires);
  RUN_TEST(test_wfi_in_machine_mode_ignores_tw);
  RUN_TEST(test_wfi_in_supervisor_mode_retires);
  RUN_TEST(test_wfi_in_supervisor_mode_with_tw_is_illegal);
  RUN_TEST(test_wfi_in_user_mode_is_illegal);
  RUN_TEST(test_sfence_vma_in_supervisor_mode_retires);
  RUN_TEST(test_sfence_vma_in_machine_mode_ignores_tvm);
  RUN_TEST(test_sfence_vma_in_supervisor_mode_with_tvm_is_illegal);
  RUN_TEST(test_sfence_vma_in_user_mode_is_illegal);
  return UNITY_END();
}
