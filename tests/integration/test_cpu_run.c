#include "cpu_fixture.h"

static void load_program(const uint32_t *program, size_t count) {
  for (size_t i = 0; i < count; i++)
    bus_write(bus, RAM_BASE + 4 * i, program[i], 4);
  cpu_set_pc(cpu, RAM_BASE);
}

static void run_steps(unsigned steps) {
  for (unsigned i = 0; i < steps; i++)
    cpu_step(cpu);
}

// x2 = 3 + 2 + 1:
//         addi x1, x0, 3
//         addi x2, x0, 0
// loop:   add  x2, x2, x1
//         addi x1, x1, -1
//         bne  x1, x0, loop
static const uint32_t SUM_DOWN_FROM_3[] = {
    0x00300093, 0x00000113, 0x00110133, 0xFFF08093, 0xFE009CE3,
};

#define SUM_DOWN_FROM_3_STEPS (2 + 3 * 3)

static void test_loop_sums_down_from_3(void) {
  load_program(SUM_DOWN_FROM_3, 5);

  run_steps(SUM_DOWN_FROM_3_STEPS);

  TEST_ASSERT_EQUAL_HEX64(6, get_reg(2));
  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 5 * 4, cpu_get_pc(cpu));
}

//          auipc x5, 0
//          addi  x5, x5, 0x20      # x5 = handler
//          csrrw x0, mtvec, x5
//          ecall
//          addi  x6, x0, 42        # runs after the handler returns
//          nop
//          nop
//          nop
// handler: csrrs x7, mepc, x0
//          addi  x7, x7, 4         # skip the ecall
//          csrrw x0, mepc, x7
//          mret
static const uint32_t ECALL_ROUND_TRIP[] = {
    0x00000297, 0x02028293, 0x30529073, 0x00000073, 0x02A00313, 0x00000013,
    0x00000013, 0x00000013, 0x341023F3, 0x00438393, 0x34139073, 0x30200073,
};

#define ECALL_ROUND_TRIP_STEPS (4 + 4 + 1)

static void test_ecall_handler_returns_past_the_ecall(void) {
  load_program(ECALL_ROUND_TRIP, 12);

  run_steps(ECALL_ROUND_TRIP_STEPS);

  TEST_ASSERT_EQUAL_HEX64(11, read_csr(CSR_REG_MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(42, get_reg(6));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 0x14, cpu_get_pc(cpu));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_loop_sums_down_from_3);
  RUN_TEST(test_ecall_handler_returns_past_the_ecall);
  return UNITY_END();
}
