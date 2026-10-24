#include "cpu_fixture.h"

#define ECALL 0x00000073
#define TRAP_HANDLER (RAM_BASE + 0x400)

static void test_ecall_in_machine_mode_raises_environment_call_from_m(void) {
  write_csr(CSR_REG_MCAUSE, 0xFF);

  execute_raw(ECALL);

  TEST_ASSERT_EQUAL_HEX64(11, read_csr(CSR_REG_MCAUSE));
}

static void execute_ecall_at_ram_base(void) {
  write_csr(CSR_REG_MTVEC, TRAP_HANDLER);
  write_csr(CSR_REG_MTVAL, 0xFF);
  cpu_set_pc(cpu, RAM_BASE);

  execute_raw(ECALL);
}

static void test_ecall_saves_its_own_address_to_mepc(void) {
  execute_ecall_at_ram_base();

  TEST_ASSERT_EQUAL_HEX64(RAM_BASE, read_csr(CSR_REG_MEPC));
}

static void test_ecall_jumps_to_trap_handler(void) {
  execute_ecall_at_ram_base();

  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
}

static void test_ecall_sets_mtval_to_zero(void) {
  execute_ecall_at_ram_base();

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(CSR_REG_MTVAL));
}

#define EBREAK 0x00100073

static void execute_ebreak_at_ram_base(void) {
  write_csr(CSR_REG_MTVEC, TRAP_HANDLER);
  cpu_set_pc(cpu, RAM_BASE);

  execute_raw(EBREAK);
}

static void test_ebreak_raises_breakpoint(void) {
  execute_ebreak_at_ram_base();

  TEST_ASSERT_EQUAL_HEX64(3, read_csr(CSR_REG_MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE, read_csr(CSR_REG_MEPC));
  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
}

static void test_ebreak_saves_its_own_address_to_mtval(void) {
  execute_ebreak_at_ram_base();

  TEST_ASSERT_EQUAL_HEX64(RAM_BASE, read_csr(CSR_REG_MTVAL));
}

#define MRET 0x30200073

static void test_mret_jumps_to_mepc(void) {
  write_csr(CSR_REG_MEPC, RAM_BASE + 0x104);

  execute_raw(MRET);

  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 0x104, cpu_get_pc(cpu));
}

static void test_mret_restores_mie_from_mpie(void) {
  write_csr(CSR_REG_MSTATUS, MSTATUS_MPIE_BIT);

  execute_raw(MRET);

  TEST_ASSERT_BITS_HIGH(MSTATUS_MIE_BIT, read_csr(CSR_REG_MSTATUS));
}

static void test_mret_sets_mpie(void) {
  write_csr(CSR_REG_MSTATUS, MSTATUS_MIE_BIT);

  execute_raw(MRET);

  uxlen_t mstatus = read_csr(CSR_REG_MSTATUS);
  TEST_ASSERT_BITS_HIGH(MSTATUS_MPIE_BIT, mstatus);
  TEST_ASSERT_BITS_LOW(MSTATUS_MIE_BIT, mstatus);
}

#define FENCE_RW_RW 0x0330000F

static void test_fence_executes_as_no_op(void) {
  write_csr(CSR_REG_MCAUSE, 0xFF);
  cpu_set_pc(cpu, RAM_BASE);

  execute_raw(FENCE_RW_RW);

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(CSR_REG_MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 4, cpu_get_pc(cpu));
}

#define FENCE_I 0x0000100F

static void test_fence_i_executes_as_no_op(void) {
  write_csr(CSR_REG_MCAUSE, 0xFF);
  cpu_set_pc(cpu, RAM_BASE);

  execute_raw(FENCE_I);

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(CSR_REG_MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 4, cpu_get_pc(cpu));
}

#define MISC_MEM_FUNCT3_2 0x0330200F

static void test_misc_mem_with_reserved_funct3_raises_illegal(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(MISC_MEM_FUNCT3_2);
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
  return UNITY_END();
}
