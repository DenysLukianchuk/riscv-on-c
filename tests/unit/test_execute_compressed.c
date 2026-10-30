#include "cpu_fixture.h"

// A 16-bit instruction runs as the 32-bit instruction it expands to, except
// that it is 2 bytes long: pc moves on by 2, and jumps link the address 2
// bytes on. Encodings from the GNU assembler, as in test_compressed.c.

static void test_compressed_instruction_advances_pc_by_2(void) {
  cpu_set_pc(cpu, 0x80000000);

  execute_raw(0x0001); // c.nop

  TEST_ASSERT_EQUAL_HEX64(0x80000002, cpu_get_pc(cpu));
}

static void test_compressed_instruction_does_its_work(void) {
  set_reg(5, 10);

  execute_raw(0x12f5); // c.addi t0, -3

  TEST_ASSERT_EQUAL_HEX64(7, get_reg(5));
}

static void test_not_taken_compressed_branch_advances_pc_by_2(void) {
  cpu_set_pc(cpu, 0x80000000);
  set_reg(10, 0);

  execute_raw(0xe931); // c.bnez a0, .+84

  TEST_ASSERT_EQUAL_HEX64(0x80000002, cpu_get_pc(cpu));
}

static void test_c_jalr_links_address_after_itself(void) {
  cpu_set_pc(cpu, 0x80000000);
  set_reg(15, 0x80001000);

  execute_raw(0x9782); // c.jalr a5

  TEST_ASSERT_EQUAL_HEX64(0x80001000, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL_HEX64(0x80000002, get_reg(1));
}

static void test_c_j_jumps_relative_to_its_own_address(void) {
  cpu_set_pc(cpu, 0x80000800);

  execute_raw(0xb001); // c.j .-2048

  TEST_ASSERT_EQUAL_HEX64(0x80000000, cpu_get_pc(cpu));
}

#if XLEN == 32
static void test_c_jal_links_address_after_itself(void) {
  cpu_set_pc(cpu, 0x80000800);

  execute_raw(0x3001); // c.jal .-2048

  TEST_ASSERT_EQUAL_HEX64(0x80000000, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL_HEX64(0x80000802, get_reg(1));
}
#endif

// mtval holds the 16 bits as fetched, upper half cleared.
static void test_illegal_compressed_instruction_reports_its_16_bits(void) {
  TEST_ASSERT_TRAP(0x8002, CAUSE_ILLEGAL_INSTRUCTION, 0x8002); // c.jr x0
  TEST_ASSERT_TRAP(0xffff8002, CAUSE_ILLEGAL_INSTRUCTION, 0x8002);
}

// A trap points mepc at the compressed instruction itself.
static void test_c_ebreak_traps_at_its_own_address(void) {
  cpu_set_pc(cpu, 0x80000002);

  execute_raw(0x9002); // c.ebreak

  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(0x80000002, read_csr(MEPC));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_compressed_instruction_advances_pc_by_2);
  RUN_TEST(test_compressed_instruction_does_its_work);
  RUN_TEST(test_not_taken_compressed_branch_advances_pc_by_2);
  RUN_TEST(test_c_jalr_links_address_after_itself);
  RUN_TEST(test_c_j_jumps_relative_to_its_own_address);
#if XLEN == 32
  RUN_TEST(test_c_jal_links_address_after_itself);
#endif
  RUN_TEST(test_illegal_compressed_instruction_reports_its_16_bits);
  RUN_TEST(test_c_ebreak_traps_at_its_own_address);
  return UNITY_END();
}
