#include "cpu_fixture.h"

#define COUNTEREN_ALL (COUNTEREN_CY | COUNTEREN_IR)

static void load_and_step(const uint32_t *program, int count) {
  load_program(program, count);
  run_steps(count);
}

static uxlen_t read_counter(uint16_t addr) {
  uxlen_t value = 0;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(cpu_get_csr(cpu), addr, PRIV_M, &value));
  return value;
}

static void test_mcycle_counts_every_step_including_traps(void) {
  write_csr(MCYCLE, 0);
  write_csr(MINSTRET, 0);
  const uint32_t program[] = {NOP, ILLEGAL_INSTRUCTION};

  load_and_step(program, 2);

  TEST_ASSERT_EQUAL_HEX64(2, read_counter(MCYCLE));
  TEST_ASSERT_EQUAL_HEX64(1, read_counter(MINSTRET));
}

static void test_cycle_and_instret_mirror_the_machine_counters(void) {
  write_csr(MCYCLE, 0);
  write_csr(MINSTRET, 0);
  const uint32_t program[] = {NOP, NOP, CSRR(1, CYCLE), CSRR(1, INSTRET)};

  load_program(program, 4);

  run_steps(3);
  TEST_ASSERT_EQUAL_HEX64(2, get_reg(1));

  run_steps(1);
  TEST_ASSERT_EQUAL_HEX64(3, get_reg(1));
}

static void test_cycle_is_read_only(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRW(CYCLE, 0));
}

static void test_write_to_minstret_suppresses_its_own_increment(void) {
  const uint32_t program[] = {CSRWI(MINSTRET, 5), CSRR(1, MINSTRET)};

  load_and_step(program, 2);

  TEST_ASSERT_EQUAL_HEX64(5, get_reg(1));
}

static void test_mcounteren_holds_the_cy_and_ir_bits(void) {
  write_csr(MCOUNTEREN, COUNTEREN_CY | COUNTEREN_IR);

  TEST_ASSERT_EQUAL_HEX64(COUNTEREN_CY | COUNTEREN_IR, read_csr(MCOUNTEREN));
}

static void test_mcounteren_is_not_accessible_from_user_mode(void) {
  cpu_set_privilege(cpu, PRIV_U);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, MCOUNTEREN));
}

static void test_user_mode_cannot_read_cycle_without_mcounteren_cy(void) {
  write_csr(MCOUNTEREN, COUNTEREN_IR);
  write_csr(SCOUNTEREN, COUNTEREN_ALL);
  cpu_set_privilege(cpu, PRIV_U);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, CYCLE));
}

static void test_user_mode_reads_cycle_with_both_counteren_cy(void) {
  write_csr(MCYCLE, 42);
  write_csr(MCOUNTEREN, COUNTEREN_CY);
  write_csr(SCOUNTEREN, COUNTEREN_CY);
  cpu_set_privilege(cpu, PRIV_U);

  execute_raw(CSRR(1, CYCLE));

  TEST_ASSERT_EQUAL_HEX64(42, get_reg(1));
}

static void test_user_mode_cannot_read_instret_without_mcounteren_ir(void) {
  write_csr(MCOUNTEREN, COUNTEREN_CY);
  write_csr(SCOUNTEREN, COUNTEREN_ALL);
  cpu_set_privilege(cpu, PRIV_U);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, INSTRET));
}

static void test_user_mode_reads_instret_with_both_counteren_ir(void) {
  write_csr(MINSTRET, 7);
  write_csr(MCOUNTEREN, COUNTEREN_IR);
  write_csr(SCOUNTEREN, COUNTEREN_IR);
  cpu_set_privilege(cpu, PRIV_U);

  execute_raw(CSRR(1, INSTRET));

  TEST_ASSERT_EQUAL_HEX64(7, get_reg(1));
}

static void test_machine_mode_reads_cycle_regardless_of_mcounteren(void) {
  write_csr(MCYCLE, 42);
  write_csr(MCOUNTEREN, 0);

  execute_raw(CSRR(1, CYCLE));

  TEST_ASSERT_EQUAL_HEX64(42, get_reg(1));
}

// time is a read-only view of the board's mtime, which the CLINT keeps; the
// board hands its current value to the CSRs before each step. Access below
// M-mode is gated by the TM bit of mcounteren and scounteren, like cycle.
#define MTIME 0x123456789ABCDEF0ull

static void test_time_reads_the_board_time(void) {
  csr_set_time(cpu_get_csr(cpu), MTIME);

  execute_raw(CSRR(1, TIME));

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)MTIME, get_reg(1));
}

static void test_time_is_read_only(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRW(TIME, 0));
}

static void test_mcounteren_holds_the_tm_bit(void) {
  write_csr(MCOUNTEREN, COUNTEREN_TM);

  TEST_ASSERT_EQUAL_HEX64(COUNTEREN_TM, read_csr(MCOUNTEREN));
}

static void test_supervisor_mode_cannot_read_time_without_mcounteren_tm(void) {
  write_csr(MCOUNTEREN, COUNTEREN_CY | COUNTEREN_IR);
  cpu_set_privilege(cpu, PRIV_S);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, TIME));
}

static void test_supervisor_mode_reads_time_with_mcounteren_tm(void) {
  csr_set_time(cpu_get_csr(cpu), MTIME);
  write_csr(MCOUNTEREN, COUNTEREN_TM);
  cpu_set_privilege(cpu, PRIV_S);

  execute_raw(CSRR(1, TIME));

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)MTIME, get_reg(1));
}

static void test_user_mode_needs_both_counteren_tm(void) {
  csr_set_time(cpu_get_csr(cpu), MTIME);
  write_csr(MCOUNTEREN, COUNTEREN_TM);
  cpu_set_privilege(cpu, PRIV_U);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, TIME));

  write_csr(SCOUNTEREN, COUNTEREN_TM);
  cpu_set_privilege(cpu, PRIV_U); // the trap above left the hart in M-mode
  execute_raw(CSRR(1, TIME));

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)MTIME, get_reg(1));
}

static void test_scounteren_holds_the_cy_and_ir_bits(void) {
  cpu_set_privilege(cpu, PRIV_S);
  set_reg(2, COUNTEREN_ALL);

  execute_raw(CSRW(SCOUNTEREN, 2));
  execute_raw(CSRR(1, SCOUNTEREN));

  TEST_ASSERT_EQUAL_HEX64(COUNTEREN_ALL, get_reg(1));
}

static void test_scounteren_is_not_accessible_from_user_mode(void) {
  cpu_set_privilege(cpu, PRIV_U);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, SCOUNTEREN));
}

static void test_user_mode_cannot_read_cycle_without_scounteren_cy(void) {
  write_csr(MCOUNTEREN, COUNTEREN_ALL);
  write_csr(SCOUNTEREN, COUNTEREN_IR);
  cpu_set_privilege(cpu, PRIV_U);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, CYCLE));
}

static void test_user_mode_cannot_read_instret_without_scounteren_ir(void) {
  write_csr(MCOUNTEREN, COUNTEREN_ALL);
  write_csr(SCOUNTEREN, COUNTEREN_CY);
  cpu_set_privilege(cpu, PRIV_U);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, INSTRET));
}

static void test_supervisor_mode_reads_cycle_without_scounteren(void) {
  write_csr(MCYCLE, 42);
  write_csr(MCOUNTEREN, COUNTEREN_CY);
  write_csr(SCOUNTEREN, 0);
  cpu_set_privilege(cpu, PRIV_S);

  execute_raw(CSRR(1, CYCLE));

  TEST_ASSERT_EQUAL_HEX64(42, get_reg(1));
}

static void test_supervisor_mode_cannot_read_cycle_without_mcounteren_cy(void) {
  write_csr(MCOUNTEREN, COUNTEREN_IR);
  write_csr(SCOUNTEREN, COUNTEREN_ALL);
  cpu_set_privilege(cpu, PRIV_S);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, CYCLE));
}

#if XLEN == 64
static void test_mcounteren_is_32_bits_wide_on_rv64(void) {
  write_csr(MCOUNTEREN, (uxlen_t)-1);

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(MCOUNTEREN) >> 32);
}

static void test_scounteren_is_32_bits_wide_on_rv64(void) {
  write_csr(SCOUNTEREN, (uxlen_t)-1);

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(SCOUNTEREN) >> 32);
}
#endif

#if XLEN == 32

static void test_write_to_minstreth_also_suppresses_the_increment(void) {
  const uint32_t program[] = {LI(5, -1), CSRW(MINSTRET, 5), CSRW(MINSTRETH, 5),
                              NOP,       CSRR(1, MINSTRET), CSRR(2, MINSTRETH)};

  load_and_step(program, 6);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(0, get_reg(2));
}

static void test_minstret_carries_into_minstreth_on_rv32(void) {
  write_csr(MINSTRET, 0xFFFFFFFF);
  write_csr(MINSTRETH, 0);
  const uint32_t program[] = {NOP};

  load_and_step(program, 1);

  TEST_ASSERT_EQUAL_HEX64(0, read_counter(MINSTRET));
  TEST_ASSERT_EQUAL_HEX64(1, read_counter(MINSTRETH));
}

static void test_mcycle_carries_into_mcycleh_on_rv32(void) {
  write_csr(MCYCLE, 0xFFFFFFFF);
  write_csr(MCYCLEH, 0);
  const uint32_t program[] = {NOP};

  load_and_step(program, 1);

  TEST_ASSERT_EQUAL_HEX64(0, read_counter(MCYCLE));
  TEST_ASSERT_EQUAL_HEX64(1, read_counter(MCYCLEH));
}

// On RV32 the upper half of time is timeh.
static void test_timeh_reads_the_upper_half_on_rv32(void) {
  csr_set_time(cpu_get_csr(cpu), MTIME);

  execute_raw(CSRR(1, TIMEH));

  TEST_ASSERT_EQUAL_HEX32((uint32_t)(MTIME >> 32), get_reg(1));
}

static void test_mcounteren_tm_also_controls_timeh(void) {
  write_csr(MCOUNTEREN, COUNTEREN_CY | COUNTEREN_IR);
  cpu_set_privilege(cpu, PRIV_S);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, TIMEH));
}

static void test_mcounteren_cy_also_controls_cycleh(void) {
  write_csr(MCOUNTEREN, COUNTEREN_IR);
  cpu_set_privilege(cpu, PRIV_U);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, CYCLEH));
}
#endif

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_mcycle_counts_every_step_including_traps);
  RUN_TEST(test_cycle_and_instret_mirror_the_machine_counters);
  RUN_TEST(test_cycle_is_read_only);
  RUN_TEST(test_write_to_minstret_suppresses_its_own_increment);
  RUN_TEST(test_mcounteren_holds_the_cy_and_ir_bits);
  RUN_TEST(test_mcounteren_is_not_accessible_from_user_mode);
  RUN_TEST(test_user_mode_cannot_read_cycle_without_mcounteren_cy);
  RUN_TEST(test_user_mode_reads_cycle_with_both_counteren_cy);
  RUN_TEST(test_user_mode_cannot_read_instret_without_mcounteren_ir);
  RUN_TEST(test_user_mode_reads_instret_with_both_counteren_ir);
  RUN_TEST(test_machine_mode_reads_cycle_regardless_of_mcounteren);
  RUN_TEST(test_time_reads_the_board_time);
  RUN_TEST(test_time_is_read_only);
  RUN_TEST(test_mcounteren_holds_the_tm_bit);
  RUN_TEST(test_supervisor_mode_cannot_read_time_without_mcounteren_tm);
  RUN_TEST(test_supervisor_mode_reads_time_with_mcounteren_tm);
  RUN_TEST(test_user_mode_needs_both_counteren_tm);
  RUN_TEST(test_scounteren_holds_the_cy_and_ir_bits);
  RUN_TEST(test_scounteren_is_not_accessible_from_user_mode);
  RUN_TEST(test_user_mode_cannot_read_cycle_without_scounteren_cy);
  RUN_TEST(test_user_mode_cannot_read_instret_without_scounteren_ir);
  RUN_TEST(test_supervisor_mode_reads_cycle_without_scounteren);
  RUN_TEST(test_supervisor_mode_cannot_read_cycle_without_mcounteren_cy);
#if XLEN == 64
  RUN_TEST(test_mcounteren_is_32_bits_wide_on_rv64);
  RUN_TEST(test_scounteren_is_32_bits_wide_on_rv64);
#endif
#if XLEN == 32
  RUN_TEST(test_mcounteren_cy_also_controls_cycleh);
  RUN_TEST(test_timeh_reads_the_upper_half_on_rv32);
  RUN_TEST(test_mcounteren_tm_also_controls_timeh);
  RUN_TEST(test_write_to_minstreth_also_suppresses_the_increment);
  RUN_TEST(test_minstret_carries_into_minstreth_on_rv32);
  RUN_TEST(test_mcycle_carries_into_mcycleh_on_rv32);
#endif
  return UNITY_END();
}
