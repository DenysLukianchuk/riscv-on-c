#include "cpu_fixture.h"
#include "riscv/trap.h"

#define ADDI_X1_X0_5 0x00500093

static void test_execute_advances_pc_to_next_instruction(void) {
  cpu_set_pc(cpu, 0x80000000);

  execute_raw(ADDI_X1_X0_5);

  TEST_ASSERT_EQUAL_HEX64(0x80000004, cpu_get_pc(cpu));
}

#define ALL_ZEROS_INSTRUCTION 0x00000000

static void test_all_zeros_instruction_raises_illegal_instruction(void) {
  execute_raw(ALL_ZEROS_INSTRUCTION);

  TEST_ASSERT_EQUAL_HEX64(2, read_csr(CSR_REG_MCAUSE));
}

#define ALL_ONES_INSTRUCTION 0xFFFFFFFF

static void test_illegal_instruction_saves_instruction_bits_to_mtval(void) {
  execute_raw(ALL_ONES_INSTRUCTION);

  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFF, read_csr(CSR_REG_MTVAL));
}

static void test_step_fetches_and_executes_instruction_at_pc(void) {
  bus_write(bus, RAM_BASE, ADDI_X1_X0_5, 4);
  cpu_set_pc(cpu, RAM_BASE);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(5, get_reg(1));
}

#define UNMAPPED_ADDR 0x10

static void
test_fetch_from_unmapped_address_raises_instruction_access_fault(void) {
  write_csr(CSR_REG_MCAUSE, 0xFF);
  cpu_set_pc(cpu, UNMAPPED_ADDR);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(1, read_csr(CSR_REG_MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(UNMAPPED_ADDR, read_csr(CSR_REG_MTVAL));
  TEST_ASSERT_EQUAL_HEX64(UNMAPPED_ADDR, read_csr(CSR_REG_MEPC));
}

#define TRAP_HANDLER (RAM_BASE + 0x400)

static void test_instruction_access_fault_jumps_to_trap_handler(void) {
  write_csr(CSR_REG_MTVEC, TRAP_HANDLER);
  cpu_set_pc(cpu, UNMAPPED_ADDR);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
}

#define MTVEC_MODE_VECTORED 1

static void test_instruction_access_fault_ignores_mtvec_mode_bits(void) {
  write_csr(CSR_REG_MTVEC, TRAP_HANDLER | MTVEC_MODE_VECTORED);
  cpu_set_pc(cpu, UNMAPPED_ADDR);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
}

#define NOP 0x00000013

static void test_minstret_counts_retired_instructions(void) {
  for (int i = 0; i < 3; i++)
    bus_write(bus, RAM_BASE + 4 * i, NOP, 4);
  cpu_set_pc(cpu, RAM_BASE);
  write_csr(CSR_REG_MINSTRET, 0);

  for (int i = 0; i < 3; i++)
    cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(3, read_csr(CSR_REG_MINSTRET));
}

#define CSRR_X1_MINSTRET 0xB02020F3

static void test_minstret_is_readable_at_its_csr_address(void) {
  bus_write(bus, RAM_BASE, NOP, 4);
  bus_write(bus, RAM_BASE + 4, CSRR_X1_MINSTRET, 4);
  cpu_set_pc(cpu, RAM_BASE);
  write_csr(CSR_REG_MCAUSE, 0xFF);

  cpu_step(cpu);
  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(CSR_REG_MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
}

static void test_trapping_instruction_does_not_retire(void) {
  bus_write(bus, RAM_BASE, ALL_ZEROS_INSTRUCTION, 4);
  cpu_set_pc(cpu, RAM_BASE);
  write_csr(CSR_REG_MINSTRET, 0);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(CSR_REG_MINSTRET));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_execute_advances_pc_to_next_instruction);
  RUN_TEST(test_all_zeros_instruction_raises_illegal_instruction);
  RUN_TEST(test_illegal_instruction_saves_instruction_bits_to_mtval);
  RUN_TEST(test_step_fetches_and_executes_instruction_at_pc);
  RUN_TEST(test_fetch_from_unmapped_address_raises_instruction_access_fault);
  RUN_TEST(test_instruction_access_fault_jumps_to_trap_handler);
  RUN_TEST(test_instruction_access_fault_ignores_mtvec_mode_bits);
  RUN_TEST(test_minstret_counts_retired_instructions);
  RUN_TEST(test_minstret_is_readable_at_its_csr_address);
  RUN_TEST(test_trapping_instruction_does_not_retire);
  return UNITY_END();
}
