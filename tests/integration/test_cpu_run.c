#include "cpu_fixture.h"

// x2 = 3 + 2 + 1
static const uint32_t SUM_DOWN_FROM_3[] = {
    LI(1, 3),       LI(2, 0),      ADD(2, 2, 1), // loop:
    ADDI(1, 1, -1), BNE(1, 0, -8),               // back to loop
};

#define SUM_DOWN_FROM_3_STEPS (2 + 3 * 3)

static void test_loop_sums_down_from_3(void) {
  load_program(SUM_DOWN_FROM_3, 5);

  run_steps(SUM_DOWN_FROM_3_STEPS);

  TEST_ASSERT_EQUAL_HEX64(6, get_reg(2));
  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 5 * 4, cpu_get_pc(cpu));
}

static const uint32_t ECALL_ROUND_TRIP[] = {
    AUIPC(5, 0),
    ADDI(5, 5, 0x20), // x5 = handler
    CSRW(MTVEC, 5),
    ECALL,
    LI(6, 42), // runs after the handler returns
    NOP,
    NOP,
    NOP,
    CSRR(7, MEPC), // handler:
    ADDI(7, 7, 4), // skip the ecall
    CSRW(MEPC, 7),
    MRET,
};

#define ECALL_ROUND_TRIP_STEPS (4 + 4 + 1)

static void test_ecall_handler_returns_past_the_ecall(void) {
  load_program(ECALL_ROUND_TRIP, 12);

  run_steps(ECALL_ROUND_TRIP_STEPS);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_ECALL_FROM_M, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(42, get_reg(6));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 0x14, cpu_get_pc(cpu));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_loop_sums_down_from_3);
  RUN_TEST(test_ecall_handler_returns_past_the_ecall);
  return UNITY_END();
}
