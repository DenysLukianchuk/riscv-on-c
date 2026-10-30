#include "cpu_fixture.h"
#include "riscv/cpu/trap.h"
#include "riscv/system/board.h"

static void test_execute_advances_pc_to_next_instruction(void) {
  cpu_set_pc(cpu, 0x80000000);

  execute_raw(ADDI(1, 0, 5));

  TEST_ASSERT_EQUAL_HEX64(0x80000004, cpu_get_pc(cpu));
}

#define ALL_ZEROS_INSTRUCTION 0x00000000

static void test_all_zeros_instruction_raises_illegal_instruction(void) {
  execute_raw(ALL_ZEROS_INSTRUCTION);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_ILLEGAL_INSTRUCTION, read_csr(MCAUSE));
}

#define ALL_ONES_INSTRUCTION 0xFFFFFFFF

static void test_illegal_instruction_saves_instruction_bits_to_mtval(void) {
  execute_raw(ALL_ONES_INSTRUCTION);

  TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFF, read_csr(MTVAL));
}

static void test_step_fetches_and_executes_instruction_at_pc(void) {
  bus_write(bus, RAM_BASE, 4, ADDI(1, 0, 5));
  cpu_set_pc(cpu, RAM_BASE);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(5, get_reg(1));
}

static void
test_fetch_from_unmapped_address_raises_instruction_access_fault(void) {
  write_csr(MCAUSE, 0xFF);
  cpu_set_pc(cpu, UNMAPPED_ADDR);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_FETCH_ACCESS, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(UNMAPPED_ADDR, read_csr(MTVAL));
  TEST_ASSERT_EQUAL_HEX64(UNMAPPED_ADDR, read_csr(MEPC));
}

static void test_instruction_access_fault_jumps_to_trap_handler(void) {
  write_csr(MTVEC, TRAP_HANDLER);
  cpu_set_pc(cpu, UNMAPPED_ADDR);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
}

#define MTVEC_MODE_VECTORED 1

static void test_instruction_access_fault_ignores_mtvec_mode_bits(void) {
  write_csr(MTVEC, TRAP_HANDLER | MTVEC_MODE_VECTORED);
  cpu_set_pc(cpu, UNMAPPED_ADDR);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
}

static void test_minstret_counts_retired_instructions(void) {
  for (int i = 0; i < 3; i++)
    bus_write(bus, RAM_BASE + 4 * i, 4, NOP);
  cpu_set_pc(cpu, RAM_BASE);
  write_csr(MINSTRET, 0);

  for (int i = 0; i < 3; i++)
    cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(3, read_csr(MINSTRET));
}

static void test_minstret_is_readable_at_its_csr_address(void) {
  bus_write(bus, RAM_BASE, 4, NOP);
  bus_write(bus, RAM_BASE + 4, 4, CSRR(1, MINSTRET));
  cpu_set_pc(cpu, RAM_BASE);
  write_csr(MCAUSE, 0xFF);

  cpu_step(cpu);
  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
}

static void test_trapping_instruction_does_not_retire(void) {
  bus_write(bus, RAM_BASE, 4, ALL_ZEROS_INSTRUCTION);
  cpu_set_pc(cpu, RAM_BASE);
  write_csr(MINSTRET, 0);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(MINSTRET));
}

// Fetching with the C extension: the hart reads the first 2 bytes, and only
// when bits 1:0 are 11 the 2 bytes after them. A 2-byte instruction therefore
// never touches what follows it.
#define RAM_END (RAM_BASE + TEST_RAM_SIZE)
#define C_LI_S1_17 0x44c5 // c.li s1, 17

static void test_compressed_instruction_at_end_of_ram_is_fetched(void) {
  write_csr(MCAUSE, 0xFF);
  bus_write(bus, RAM_END - 2, 2, C_LI_S1_17);
  cpu_set_pc(cpu, RAM_END - 2);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(17, get_reg(9));
  TEST_ASSERT_EQUAL_HEX64(RAM_END, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL_HEX64(0xFF, read_csr(MCAUSE));
}

// mepc points at the instruction, mtval at the part that could not be read.
static void test_instruction_running_past_end_of_ram_faults_there(void) {
  bus_write(bus, RAM_END - 2, 2, ADDI(5, 0, 42) & 0xFFFF);
  cpu_set_pc(cpu, RAM_END - 2);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_FETCH_ACCESS, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_END, read_csr(MTVAL));
  TEST_ASSERT_EQUAL_HEX64(RAM_END - 2, read_csr(MEPC));
}

// After a 2-byte instruction the next 4-byte one starts at an address that
// is a multiple of 2 but not of 4.
static void test_mixed_lengths_run_one_after_another(void) {
  bus_write(bus, RAM_BASE, 2, C_LI_S1_17);
  bus_write(bus, RAM_BASE + 2, 2, ADDI(5, 9, 25) & 0xFFFF);
  bus_write(bus, RAM_BASE + 4, 2, ADDI(5, 9, 25) >> 16);
  bus_write(bus, RAM_BASE + 6, 2, 0x0001); // c.nop
  cpu_set_pc(cpu, RAM_BASE);

  run_steps(3);

  TEST_ASSERT_EQUAL_HEX64(42, get_reg(5));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 8, cpu_get_pc(cpu));
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
  RUN_TEST(test_compressed_instruction_at_end_of_ram_is_fetched);
  RUN_TEST(test_instruction_running_past_end_of_ram_faults_there);
  RUN_TEST(test_mixed_lengths_run_one_after_another);
  return UNITY_END();
}
