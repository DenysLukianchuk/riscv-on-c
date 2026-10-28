#include "cpu_fixture.h"

// Each test runs one `li x1, MARKER` at RAM_BASE. An interrupt taken before
// it leaves x1 at zero.
#define MARKER 42
#define STVEC_BASE (RAM_BASE + 0x200)
#define INTERRUPT(code) (CAUSE_INTERRUPT | (code))
#define VECTORED 1u

// The CLINT and the PLIC drive MSIP, MTIP and MEIP; tests drive them directly.
static void raise(uxlen_t interrupts) {
  csr_set_interrupt_pending(cpu_get_csr(cpu), interrupts, true);
}

static void step_in(enum Privilege privilege) {
  static const uint32_t program[] = {LI(1, MARKER)};
  load_program(program, 1);
  set_reg(1, 0);
  write_csr(MTVEC, TRAP_HANDLER);
  write_csr(STVEC, STVEC_BASE);
  cpu_set_privilege(cpu, privilege);
  cpu_step(cpu);
}

#define TEST_ASSERT_NOT_INTERRUPTED()                                          \
  do {                                                                         \
    TEST_ASSERT_EQUAL_HEX64(MARKER, get_reg(1));                               \
    TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 4, cpu_get_pc(cpu));                    \
  } while (0)

#define TEST_ASSERT_INTERRUPTED_INTO_M(code)                                   \
  do {                                                                         \
    TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));                                    \
    TEST_ASSERT_EQUAL(PRIV_M, cpu_get_privilege(cpu));                         \
    TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));                    \
    TEST_ASSERT_EQUAL_HEX64(INTERRUPT(code), read_csr(MCAUSE));                \
  } while (0)

#define TEST_ASSERT_INTERRUPTED_INTO_S(code)                                   \
  do {                                                                         \
    TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));                                    \
    TEST_ASSERT_EQUAL(PRIV_S, cpu_get_privilege(cpu));                         \
    TEST_ASSERT_EQUAL_HEX64(STVEC_BASE, cpu_get_pc(cpu));                      \
    TEST_ASSERT_EQUAL_HEX64(INTERRUPT(code), read_csr(SCAUSE));                \
  } while (0)

// Machine-level interrupts.

static void test_pending_interrupt_that_is_not_enabled_is_ignored(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  raise(MTIP);

  step_in(PRIV_M);

  TEST_ASSERT_NOT_INTERRUPTED();
}

static void test_enabled_interrupt_that_is_not_pending_is_ignored(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  write_csr(MIE, MTIP);

  step_in(PRIV_M);

  TEST_ASSERT_NOT_INTERRUPTED();
}

static void test_interrupt_waits_for_mie_in_machine_mode(void) {
  write_csr(MIE, MTIP);
  raise(MTIP);

  step_in(PRIV_M);

  TEST_ASSERT_NOT_INTERRUPTED();
}

static void test_interrupt_is_taken_before_the_next_instruction(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  write_csr(MIE, MTIP);
  raise(MTIP);

  step_in(PRIV_M);

  TEST_ASSERT_INTERRUPTED_INTO_M(IRQ_M_TIMER);
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE, read_csr(MEPC));
  TEST_ASSERT_EQUAL_HEX64(0, read_csr(MTVAL));
}

static void test_interrupt_entry_saves_and_clears_mie(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  write_csr(MIE, MTIP);
  raise(MTIP);

  step_in(PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MPIE | MSTATUS_MPP_M,
                          read_csr(MSTATUS) &
                              (MSTATUS_MIE | MSTATUS_MPIE | MSTATUS_MPP));
}

// Lower modes cannot mask machine interrupts.
static void test_machine_interrupt_is_taken_in_user_mode_with_mie_clear(void) {
  write_csr(MIE, MTIP);
  raise(MTIP);

  step_in(PRIV_U);

  TEST_ASSERT_INTERRUPTED_INTO_M(IRQ_M_TIMER);
  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MPP_U, read_csr(MSTATUS) & MSTATUS_MPP);
}

static void
test_machine_interrupt_is_taken_in_supervisor_mode_with_mie_clear(void) {
  write_csr(MIE, MTIP);
  raise(MTIP);

  step_in(PRIV_S);

  TEST_ASSERT_INTERRUPTED_INTO_M(IRQ_M_TIMER);
}

static void test_software_interrupt_written_to_mip_is_taken(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  write_csr(MIE, SSIP);
  write_csr(MIP, SSIP);

  step_in(PRIV_M);

  TEST_ASSERT_INTERRUPTED_INTO_M(IRQ_S_SOFTWARE);
}

// Priority, highest first: MEI, MSI, MTI, SEI, SSI, STI.

static void take_one_of(uxlen_t interrupts) {
  write_csr(MSTATUS, MSTATUS_MIE);
  write_csr(MIE, interrupts);
  raise(interrupts);
  step_in(PRIV_M);
}

static void test_machine_external_interrupt_comes_first(void) {
  take_one_of(MEIP | MSIP | MTIP | SEIP | SSIP | STIP);

  TEST_ASSERT_INTERRUPTED_INTO_M(IRQ_M_EXTERNAL);
}

static void test_machine_software_interrupt_comes_before_the_timer(void) {
  take_one_of(MSIP | MTIP | SEIP | SSIP | STIP);

  TEST_ASSERT_INTERRUPTED_INTO_M(IRQ_M_SOFTWARE);
}

static void test_machine_timer_comes_before_supervisor_interrupts(void) {
  take_one_of(MTIP | SEIP | SSIP | STIP);

  TEST_ASSERT_INTERRUPTED_INTO_M(IRQ_M_TIMER);
}

static void test_supervisor_external_interrupt_comes_next(void) {
  take_one_of(SEIP | SSIP | STIP);

  TEST_ASSERT_INTERRUPTED_INTO_M(IRQ_S_EXTERNAL);
}

static void test_supervisor_software_interrupt_comes_before_its_timer(void) {
  take_one_of(SSIP | STIP);

  TEST_ASSERT_INTERRUPTED_INTO_M(IRQ_S_SOFTWARE);
}

// Interrupts delegated with mideleg go to S-mode.

static void delegate(uxlen_t interrupts) {
  write_csr(MIDELEG, interrupts);
  write_csr(MIE, interrupts);
  write_csr(MIP, interrupts);
}

static void test_delegated_interrupt_is_taken_in_supervisor_mode(void) {
  delegate(STIP);

  step_in(PRIV_U);

  TEST_ASSERT_INTERRUPTED_INTO_S(IRQ_S_TIMER);
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE, read_csr(SEPC));
  TEST_ASSERT_EQUAL_HEX64(0, read_csr(STVAL));
  TEST_ASSERT_EQUAL_HEX64(SSTATUS_SPP_U, read_csr(SSTATUS) & SSTATUS_SPP);
}

static void test_delegated_interrupt_waits_for_sie_in_supervisor_mode(void) {
  delegate(STIP);

  step_in(PRIV_S);

  TEST_ASSERT_NOT_INTERRUPTED();
}

static void test_delegated_interrupt_is_taken_with_sie_set(void) {
  delegate(STIP);
  write_csr(SSTATUS, SSTATUS_SIE);

  step_in(PRIV_S);

  TEST_ASSERT_INTERRUPTED_INTO_S(IRQ_S_TIMER);
  TEST_ASSERT_EQUAL_HEX64(SSTATUS_SPIE | SSTATUS_SPP,
                          read_csr(SSTATUS) &
                              (SSTATUS_SIE | SSTATUS_SPIE | SSTATUS_SPP));
}

// M-mode is above S, so an S-level interrupt never stops it.
static void test_delegated_interrupt_is_never_taken_in_machine_mode(void) {
  delegate(STIP);
  write_csr(MSTATUS, MSTATUS_MIE | MSTATUS_SIE);

  step_in(PRIV_M);

  TEST_ASSERT_NOT_INTERRUPTED();
}

static void test_machine_interrupt_comes_before_a_delegated_one(void) {
  delegate(SEIP);
  write_csr(MIE, SEIP | MTIP);
  raise(MTIP);
  write_csr(SSTATUS, SSTATUS_SIE);

  step_in(PRIV_S);

  TEST_ASSERT_INTERRUPTED_INTO_M(IRQ_M_TIMER);
}

// Vectored mode (tvec mode 1): interrupts jump to base + 4 * code, exceptions
// still go to base.

static void test_vectored_mtvec_sends_an_interrupt_to_its_own_entry(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  write_csr(MIE, MTIP);
  raise(MTIP);
  static const uint32_t program[] = {LI(1, MARKER)};
  load_program(program, 1);
  write_csr(MTVEC, TRAP_HANDLER | VECTORED);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER + 4 * IRQ_M_TIMER, cpu_get_pc(cpu));
}

static void test_vectored_stvec_sends_an_interrupt_to_its_own_entry(void) {
  delegate(STIP);
  static const uint32_t program[] = {LI(1, MARKER)};
  load_program(program, 1);
  write_csr(STVEC, STVEC_BASE | VECTORED);
  cpu_set_privilege(cpu, PRIV_U);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(STVEC_BASE + 4 * IRQ_S_TIMER, cpu_get_pc(cpu));
}

static void test_vectored_mtvec_still_sends_exceptions_to_base(void) {
  static const uint32_t program[] = {0};
  load_program(program, 1);
  write_csr(MTVEC, TRAP_HANDLER | VECTORED);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_ILLEGAL_INSTRUCTION, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_pending_interrupt_that_is_not_enabled_is_ignored);
  RUN_TEST(test_enabled_interrupt_that_is_not_pending_is_ignored);
  RUN_TEST(test_interrupt_waits_for_mie_in_machine_mode);
  RUN_TEST(test_interrupt_is_taken_before_the_next_instruction);
  RUN_TEST(test_interrupt_entry_saves_and_clears_mie);
  RUN_TEST(test_machine_interrupt_is_taken_in_user_mode_with_mie_clear);
  RUN_TEST(test_machine_interrupt_is_taken_in_supervisor_mode_with_mie_clear);
  RUN_TEST(test_software_interrupt_written_to_mip_is_taken);
  RUN_TEST(test_machine_external_interrupt_comes_first);
  RUN_TEST(test_machine_software_interrupt_comes_before_the_timer);
  RUN_TEST(test_machine_timer_comes_before_supervisor_interrupts);
  RUN_TEST(test_supervisor_external_interrupt_comes_next);
  RUN_TEST(test_supervisor_software_interrupt_comes_before_its_timer);
  RUN_TEST(test_delegated_interrupt_is_taken_in_supervisor_mode);
  RUN_TEST(test_delegated_interrupt_waits_for_sie_in_supervisor_mode);
  RUN_TEST(test_delegated_interrupt_is_taken_with_sie_set);
  RUN_TEST(test_delegated_interrupt_is_never_taken_in_machine_mode);
  RUN_TEST(test_machine_interrupt_comes_before_a_delegated_one);
  RUN_TEST(test_vectored_mtvec_sends_an_interrupt_to_its_own_entry);
  RUN_TEST(test_vectored_stvec_sends_an_interrupt_to_its_own_entry);
  RUN_TEST(test_vectored_mtvec_still_sends_exceptions_to_base);
  return UNITY_END();
}
