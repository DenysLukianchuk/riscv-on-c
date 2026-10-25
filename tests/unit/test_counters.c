#include "cpu_fixture.h"

#define MCYCLE 0xB00
#define MINSTRET 0xB02
#define MCYCLEH 0xB80
#define MINSTRETH 0xB82

#define NOP 0x00000013
#define ILLEGAL_INSTRUCTION 0x00000000
#define CSRR_X1_CYCLE 0xC00020F3
#define CSRR_X1_INSTRET 0xC02020F3
#define CSRW_CYCLE_X0 0xC0001073

static void load_program(const uint32_t *program, int count) {
  for (int i = 0; i < count; i++)
    bus_write(bus, RAM_BASE + 4 * i, program[i], 4);
  cpu_set_pc(cpu, RAM_BASE);
}

static void run_steps(int steps) {
  for (int i = 0; i < steps; i++)
    cpu_step(cpu);
}

static void load_and_step(const uint32_t *program, int count) {
  load_program(program, count);
  run_steps(count);
}

static uxlen_t read_counter(uint16_t addr) {
  uxlen_t value = 0;
  TEST_ASSERT_EQUAL(CSR_OK, csr_read(cpu_get_csr(cpu), addr, &value));
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

static void test_cycle_and_instret_are_readable_from_any_program(void) {
  write_csr(MCYCLE, 0);
  write_csr(MINSTRET, 0);
  const uint32_t program[] = {NOP, NOP, CSRR_X1_CYCLE, CSRR_X1_INSTRET};

  load_program(program, 4);

  run_steps(3);
  TEST_ASSERT_EQUAL_HEX64(2, get_reg(1));

  run_steps(1);
  TEST_ASSERT_EQUAL_HEX64(3, get_reg(1));
}

static void test_cycle_is_read_only(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRW_CYCLE_X0);
}

#if XLEN == 32
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
#endif

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_mcycle_counts_every_step_including_traps);
  RUN_TEST(test_cycle_and_instret_are_readable_from_any_program);
  RUN_TEST(test_cycle_is_read_only);
#if XLEN == 32
  RUN_TEST(test_minstret_carries_into_minstreth_on_rv32);
  RUN_TEST(test_mcycle_carries_into_mcycleh_on_rv32);
#endif
  return UNITY_END();
}
