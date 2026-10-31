#include "cpu_fixture.h"

#define TRIGGER_COUNT 4
#define DISABLED TDATA1_TYPE(TRIGGER_TYPE_DISABLED)
#define MCONTROL TDATA1_TYPE(TRIGGER_TYPE_MCONTROL)
#define EXECUTE_BREAKPOINT (MCONTROL | MCONTROL_M | MCONTROL_EXECUTE)

static void select_trigger(uxlen_t index) { write_csr(TSELECT, index); }

static void test_tselect_selects_each_trigger(void) {
  for (uxlen_t i = 0; i < TRIGGER_COUNT; i++) {
    select_trigger(i);

    TEST_ASSERT_EQUAL_HEX64(i, read_csr(TSELECT));
  }
}

static void test_tselect_ignores_an_index_past_the_last_trigger(void) {
  select_trigger(1);

  select_trigger(TRIGGER_COUNT);

  TEST_ASSERT_EQUAL_HEX64(1, read_csr(TSELECT));
}

#define SUPPORTED_TYPES                                                        \
  (TINFO_VERSION_1 | TINFO_TYPE(TRIGGER_TYPE_MCONTROL) |                       \
   TINFO_TYPE(TRIGGER_TYPE_ICOUNT) | TINFO_TYPE(TRIGGER_TYPE_ETRIGGER) |       \
   TINFO_TYPE(TRIGGER_TYPE_MCONTROL6) | TINFO_TYPE(TRIGGER_TYPE_DISABLED))

static void test_tinfo_reports_version_and_supported_types(void) {
  TEST_ASSERT_EQUAL_HEX64(SUPPORTED_TYPES, read_csr(TINFO));
}

static void test_tinfo_ignores_writes(void) {
  write_csr(TINFO, 0);

  TEST_ASSERT_EQUAL_HEX64(SUPPORTED_TYPES, read_csr(TINFO));
}

static void test_triggers_start_disabled(void) {
  for (uxlen_t i = 0; i < TRIGGER_COUNT; i++) {
    select_trigger(i);

    TEST_ASSERT_EQUAL_HEX64(DISABLED, read_csr(TDATA1));
  }
}

static void test_tdata1_holds_an_mcontrol_breakpoint(void) {
  uxlen_t trigger = MCONTROL | MCONTROL_M | MCONTROL_S | MCONTROL_U |
                    MCONTROL_EXECUTE | MCONTROL_LOAD | MCONTROL_STORE;
  write_csr(TDATA1, trigger);

  TEST_ASSERT_EQUAL_HEX64(trigger, read_csr(TDATA1));
}

static void test_writing_zero_to_tdata1_disables_the_trigger(void) {
  write_csr(TDATA1, EXECUTE_BREAKPOINT);

  write_csr(TDATA1, 0);

  TEST_ASSERT_EQUAL_HEX64(DISABLED, read_csr(TDATA1));
}

// While no trigger is set the hart skips all trigger checks: the module
// reports whether any trigger is set.

static bool armed(void) { return csr_triggers_armed(cpu_get_csr(cpu)); }

static void test_no_trigger_is_armed_after_reset(void) {
  TEST_ASSERT_FALSE(armed());
}

static void test_setting_any_trigger_arms_the_module(void) {
  select_trigger(2);

  write_csr(TDATA1, EXECUTE_BREAKPOINT);

  TEST_ASSERT_TRUE(armed());
}

static void test_module_stays_armed_until_the_last_trigger_is_disabled(void) {
  select_trigger(0);
  write_csr(TDATA1, EXECUTE_BREAKPOINT);
  select_trigger(1);
  write_csr(TDATA1, EXECUTE_BREAKPOINT);

  write_csr(TDATA1, 0);
  TEST_ASSERT_TRUE(armed());

  select_trigger(0);
  write_csr(TDATA1, 0);
  TEST_ASSERT_FALSE(armed());
}

static void test_tdata1_with_an_unsupported_type_is_disabled(void) {
  write_csr(TDATA1, TDATA1_TYPE(TRIGGER_TYPE_TMEXTTRIGGER) | 0x1FF);

  TEST_ASSERT_EQUAL_HEX64(DISABLED, read_csr(TDATA1));
}

// Without Debug Mode a trigger can only raise a breakpoint exception, and
// only "before" timing is implemented.
static void test_mcontrol_keeps_only_supported_settings(void) {
  write_csr(TDATA1, EXECUTE_BREAKPOINT | TDATA1_DMODE | MCONTROL_ACTION(1) |
                        MCONTROL_TIMING);

  TEST_ASSERT_EQUAL_HEX64(EXECUTE_BREAKPOINT, read_csr(TDATA1));
}

static void test_mcontrol_reports_no_napot_support(void) {
  write_csr(TDATA1, EXECUTE_BREAKPOINT | MCONTROL_MASKMAX);

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(TDATA1) & MCONTROL_MASKMAX);
}

static void test_tdata2_holds_any_address(void) {
  uxlen_t address = (uxlen_t)-16;
  write_csr(TDATA2, address);

  TEST_ASSERT_EQUAL_HEX64(address, read_csr(TDATA2));
}

static void test_each_trigger_keeps_its_own_tdata(void) {
  select_trigger(0);
  write_csr(TDATA1, EXECUTE_BREAKPOINT);
  write_csr(TDATA2, 0x80000100);
  select_trigger(1);
  write_csr(TDATA1, 0);
  write_csr(TDATA2, 0x80000200);

  select_trigger(0);

  TEST_ASSERT_EQUAL_HEX64(EXECUTE_BREAKPOINT, read_csr(TDATA1));
  TEST_ASSERT_EQUAL_HEX64(0x80000100, read_csr(TDATA2));
}

static void test_tdata3_reads_zero_without_context_matching(void) {
  write_csr(TDATA3, (uxlen_t)-1);

  TEST_ASSERT_EQUAL_HEX64(0, read_csr(TDATA3));
}

static void test_tcontrol_is_not_implemented(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, TCONTROL));
}

static void test_trigger_csrs_are_machine_only(void) {
  cpu_set_privilege(cpu, PRIV_S);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRR(1, TSELECT));

  cpu_set_privilege(cpu, PRIV_S);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(CSRW(TDATA1, 0));
}

static void test_hit_can_be_set_and_cleared_by_software(void) {
  write_csr(TDATA1, EXECUTE_BREAKPOINT | MCONTROL_HIT);
  TEST_ASSERT_EQUAL_HEX64(EXECUTE_BREAKPOINT | MCONTROL_HIT, read_csr(TDATA1));

  write_csr(TDATA1, EXECUTE_BREAKPOINT);
  TEST_ASSERT_EQUAL_HEX64(EXECUTE_BREAKPOINT, read_csr(TDATA1));
}

// Execute triggers. Each test runs one `li x1, MARKER` at BREAKPOINT_PC: x1
// stays zero when a trigger stops the instruction.

#define BREAKPOINT_PC RAM_BASE
#define MARKER 42
#define DELEGATE_BREAKPOINT ((uxlen_t)1 << CAUSE_BREAKPOINT)
#define STVEC_BASE (RAM_BASE + 0x200)

static void set_trigger(uxlen_t index, uxlen_t tdata1, uxlen_t tdata2) {
  select_trigger(index);
  write_csr(TDATA1, 0);
  write_csr(TDATA2, tdata2);
  write_csr(TDATA1, tdata1);
}

static uxlen_t trigger_tdata1(uxlen_t index) {
  select_trigger(index);
  return read_csr(TDATA1);
}

static void step_marker(enum Privilege privilege) {
  static const uint32_t program[] = {LI(1, MARKER)};
  load_program(program, 1);
  set_reg(1, 0);
  write_csr(MTVEC, TRAP_HANDLER);
  write_csr(STVEC, STVEC_BASE);
  cpu_set_privilege(cpu, privilege);
  cpu_step(cpu);
}

#define TEST_ASSERT_STOPPED_IN_MACHINE_MODE()                                  \
  do {                                                                         \
    TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));                                    \
    TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));                    \
    TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));               \
  } while (0)

#define TEST_ASSERT_NOT_STOPPED()                                              \
  do {                                                                         \
    TEST_ASSERT_EQUAL_HEX64(MARKER, get_reg(1));                               \
    TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC + 4, cpu_get_pc(cpu));               \
  } while (0)

// In M-mode, triggers that raise a breakpoint only fire while mstatus.MIE is
// set, so a breakpoint inside a trap handler cannot re-enter it.
static void test_execute_breakpoint_stops_before_the_instruction(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, EXECUTE_BREAKPOINT, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
}

static void test_execute_breakpoint_reports_the_instruction_address(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, EXECUTE_BREAKPOINT, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC, read_csr(MEPC));
  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC, read_csr(MTVAL));
}

static void test_execute_breakpoint_sets_hit(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, EXECUTE_BREAKPOINT, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(EXECUTE_BREAKPOINT | MCONTROL_HIT, trigger_tdata1(0));
}

static void test_stopped_instruction_does_not_retire(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, EXECUTE_BREAKPOINT, BREAKPOINT_PC);
  uxlen_t instret = read_csr(MINSTRET);

  step_marker(PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(instret, read_csr(MINSTRET));
}

static void test_execute_breakpoint_at_another_address_does_not_fire(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, EXECUTE_BREAKPOINT, BREAKPOINT_PC + 4);

  step_marker(PRIV_M);

  TEST_ASSERT_NOT_STOPPED();
  TEST_ASSERT_EQUAL_HEX64(EXECUTE_BREAKPOINT, trigger_tdata1(0));
}

static void test_disabled_trigger_does_not_fire(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, 0, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_NOT_STOPPED();
}

static void test_load_store_trigger_does_not_fire_on_execute(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, MCONTROL | MCONTROL_M | MCONTROL_LOAD | MCONTROL_STORE,
              BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_NOT_STOPPED();
}

static void test_every_trigger_is_checked_not_only_the_selected_one(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(TRIGGER_COUNT - 1, EXECUTE_BREAKPOINT, BREAKPOINT_PC);
  select_trigger(0);

  step_marker(PRIV_M);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
  TEST_ASSERT_EQUAL_HEX64(EXECUTE_BREAKPOINT | MCONTROL_HIT,
                          trigger_tdata1(TRIGGER_COUNT - 1));
}

static void test_every_matching_trigger_sets_hit(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, EXECUTE_BREAKPOINT, BREAKPOINT_PC);
  set_trigger(2, EXECUTE_BREAKPOINT, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(EXECUTE_BREAKPOINT | MCONTROL_HIT, trigger_tdata1(0));
  TEST_ASSERT_EQUAL_HEX64(EXECUTE_BREAKPOINT | MCONTROL_HIT, trigger_tdata1(2));
}

static void test_execute_breakpoint_ignores_modes_it_is_not_enabled_in(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, MCONTROL | MCONTROL_S | MCONTROL_U | MCONTROL_EXECUTE,
              BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_NOT_STOPPED();
}

static void test_execute_breakpoint_fires_in_user_mode(void) {
  set_trigger(0, MCONTROL | MCONTROL_U | MCONTROL_EXECUTE, BREAKPOINT_PC);

  step_marker(PRIV_U);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
  TEST_ASSERT_EQUAL(PRIV_M, cpu_get_privilege(cpu));
  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MPP_U, read_csr(MSTATUS) & MSTATUS_MPP);
}

static void test_execute_breakpoint_fires_in_supervisor_mode(void) {
  set_trigger(0, MCONTROL | MCONTROL_S | MCONTROL_EXECUTE, BREAKPOINT_PC);

  step_marker(PRIV_S);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MPP_S, read_csr(MSTATUS) & MSTATUS_MPP);
}

static void test_execute_breakpoint_has_priority_over_fetch_access_fault(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  write_csr(MTVEC, TRAP_HANDLER);
  set_trigger(0, EXECUTE_BREAKPOINT, UNMAPPED_ADDR);
  cpu_set_pc(cpu, UNMAPPED_ADDR);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(UNMAPPED_ADDR, read_csr(MTVAL));
}

// Re-entrancy protection (Sdtrig "Native M-mode triggers", option 1).

static void test_breakpoint_does_not_fire_in_machine_mode_with_mie_clear(void) {
  write_csr(MSTATUS, 0);
  set_trigger(0, EXECUTE_BREAKPOINT, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_NOT_STOPPED();
  TEST_ASSERT_EQUAL_HEX64(EXECUTE_BREAKPOINT, trigger_tdata1(0));
}

static void
test_undelegated_breakpoint_fires_in_supervisor_mode_with_sie_clear(void) {
  write_csr(MSTATUS, 0);
  set_trigger(0, MCONTROL | MCONTROL_S | MCONTROL_EXECUTE, BREAKPOINT_PC);

  step_marker(PRIV_S);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
}

static void
test_delegated_breakpoint_does_not_fire_in_supervisor_mode_with_sie_clear(
    void) {
  write_csr(MSTATUS, 0);
  write_csr(MEDELEG, DELEGATE_BREAKPOINT);
  set_trigger(0, MCONTROL | MCONTROL_S | MCONTROL_EXECUTE, BREAKPOINT_PC);

  step_marker(PRIV_S);

  TEST_ASSERT_NOT_STOPPED();
}

static void
test_delegated_breakpoint_fires_in_supervisor_mode_with_sie_set(void) {
  write_csr(MSTATUS, MSTATUS_SIE);
  write_csr(MEDELEG, DELEGATE_BREAKPOINT);
  set_trigger(0, MCONTROL | MCONTROL_S | MCONTROL_EXECUTE, BREAKPOINT_PC);

  step_marker(PRIV_S);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL(PRIV_S, cpu_get_privilege(cpu));
  TEST_ASSERT_EQUAL_HEX64(STVEC_BASE, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(SCAUSE));
  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC, read_csr(SEPC));
  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC, read_csr(STVAL));
}

static void
test_delegated_breakpoint_fires_in_user_mode_regardless_of_sie(void) {
  write_csr(MSTATUS, 0);
  write_csr(MEDELEG, DELEGATE_BREAKPOINT);
  set_trigger(0, MCONTROL | MCONTROL_U | MCONTROL_EXECUTE, BREAKPOINT_PC);

  step_marker(PRIV_U);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL(PRIV_S, cpu_get_privilege(cpu));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(SCAUSE));
}

// Load and store triggers. Each test runs one instruction at BREAKPOINT_PC
// that accesses the word at DATA_ADDR through x2; the word starts as
// OLD_VALUE, and a store writes NEW_VALUE from x3.

#define DATA_ADDR (RAM_BASE + 0x100)
#define OLD_VALUE 0x11111111
#define NEW_VALUE 0x55555555
#define LOAD_BREAKPOINT (MCONTROL | MCONTROL_M | MCONTROL_LOAD)
#define STORE_BREAKPOINT (MCONTROL | MCONTROL_M | MCONTROL_STORE)

static void step_access_over(uint32_t instruction, uxlen_t base,
                             enum Privilege privilege, uint32_t word) {
  load_program(&instruction, 1);
  bus_write(bus, DATA_ADDR, 4, word);
  set_reg(1, 0);
  set_reg(2, base);
  set_reg(3, NEW_VALUE);
  write_csr(MTVEC, TRAP_HANDLER);
  write_csr(STVEC, STVEC_BASE);
  cpu_set_privilege(cpu, privilege);
  cpu_step(cpu);
}

// Steps one access while DATA_ADDR holds OLD_VALUE.
static void step_access(uint32_t instruction, uxlen_t base,
                        enum Privilege privilege) {
  step_access_over(instruction, base, privilege, OLD_VALUE);
}

static uxlen_t data_word(void) {
  uint64_t value = 0;
  bus_read(bus, DATA_ADDR, 4, &value);
  return value;
}

static void test_load_breakpoint_stops_before_the_load(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

// 0 is an ordinary compare value: a data trigger looking for a load of 0.
static void test_trigger_comparing_against_zero_fires(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT | MCONTROL_SELECT, 0);

  step_access_over(LW(1, 0, 2), DATA_ADDR, PRIV_M, 0);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

// Writing another trigger's registers leaves this one armed.
static void test_writing_another_trigger_keeps_this_one(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR);
  select_trigger(1);
  write_csr(TDATA2, 0);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_load_breakpoint_reports_the_pc_and_the_data_address(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC, read_csr(MEPC));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, read_csr(MTVAL));
}

static void test_load_breakpoint_sets_hit(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(LOAD_BREAKPOINT | MCONTROL_HIT, trigger_tdata1(0));
}

static void test_stopped_load_does_not_retire(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR);
  uxlen_t instret = read_csr(MINSTRET);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(instret, read_csr(MINSTRET));
}

static void test_load_breakpoint_compares_the_computed_address(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR);

  step_access(LW(1, 8, 2), DATA_ADDR - 8, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, read_csr(MTVAL));
}

static void test_store_breakpoint_stops_before_memory_is_written(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, STORE_BREAKPOINT, DATA_ADDR);

  step_access(SW(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_store_breakpoint_reports_the_pc_and_the_data_address(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, STORE_BREAKPOINT, DATA_ADDR);

  step_access(SW(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC, read_csr(MEPC));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, read_csr(MTVAL));
  TEST_ASSERT_EQUAL_HEX64(STORE_BREAKPOINT | MCONTROL_HIT, trigger_tdata1(0));
}

static void test_load_breakpoint_at_another_address_does_not_fire(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR + 4);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(LOAD_BREAKPOINT, trigger_tdata1(0));
}

static void test_load_breakpoint_does_not_fire_on_a_store(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR);

  step_access(SW(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(NEW_VALUE, data_word());
}

static void test_store_breakpoint_does_not_fire_on_a_load(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, STORE_BREAKPOINT, DATA_ADDR);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
}

static void test_execute_breakpoint_does_not_fire_on_a_load(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, EXECUTE_BREAKPOINT, DATA_ADDR);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
}

static void test_load_breakpoint_does_not_fire_on_instruction_fetch(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, BREAKPOINT_PC);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
}

static void test_load_breakpoint_has_priority_over_load_access_fault(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, UNMAPPED_ADDR);

  step_access(LW(1, 0, 2), UNMAPPED_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(UNMAPPED_ADDR, read_csr(MTVAL));
}

static void test_store_breakpoint_has_priority_over_store_access_fault(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, STORE_BREAKPOINT, UNMAPPED_ADDR);

  step_access(SW(3, 0, 2), UNMAPPED_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(UNMAPPED_ADDR, read_csr(MTVAL));
}

static void test_load_breakpoint_ignores_modes_it_is_not_enabled_in(void) {
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_U);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
}

static void test_load_breakpoint_fires_in_user_mode(void) {
  set_trigger(0, MCONTROL | MCONTROL_U | MCONTROL_LOAD, DATA_ADDR);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_U);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL(PRIV_M, cpu_get_privilege(cpu));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void
test_load_breakpoint_does_not_fire_in_machine_mode_with_mie_clear(void) {
  write_csr(MSTATUS, 0);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(LOAD_BREAKPOINT, trigger_tdata1(0));
}

static void test_delegated_store_breakpoint_enters_supervisor_mode(void) {
  write_csr(MEDELEG, DELEGATE_BREAKPOINT);
  set_trigger(0, MCONTROL | MCONTROL_U | MCONTROL_STORE, DATA_ADDR);

  step_access(SW(3, 0, 2), DATA_ADDR, PRIV_U);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL(PRIV_S, cpu_get_privilege(cpu));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(SCAUSE));
  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC, read_csr(SEPC));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, read_csr(STVAL));
}

// Atomics (Sdtrig "Memory accesses"): lr is a load, a successful sc is a
// store, and an AMO is both. Whether a failing sc is a store is left to the
// implementation; here it is not, since it writes nothing.

static void test_lr_is_a_load(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR);

  step_access(LR_W(1, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, read_csr(MTVAL));
}

static void test_lr_is_not_a_store(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, STORE_BREAKPOINT, DATA_ADDR);

  step_access(LR_W(1, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
}

static void test_lr_breakpoint_has_priority_over_misaligned_address(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR + 2);

  step_access(LR_W(1, 2), DATA_ADDR + 2, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR + 2, read_csr(MTVAL));
}

static void test_successful_sc_is_a_store(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, STORE_BREAKPOINT, DATA_ADDR);
  cpu_set_amo_reservation(cpu, DATA_ADDR);

  step_access(SC_W(1, 3, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, read_csr(MTVAL));
}

static void test_failing_sc_is_not_a_store(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, STORE_BREAKPOINT, DATA_ADDR);

  step_access(SC_W(1, 3, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC + 4, cpu_get_pc(cpu));
  TEST_ASSERT_NOT_EQUAL(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(STORE_BREAKPOINT, trigger_tdata1(0));
}

static void test_sc_is_not_a_load(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR);
  cpu_set_amo_reservation(cpu, DATA_ADDR);

  step_access(SC_W(1, 3, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(NEW_VALUE, data_word());
}

static void test_amo_is_a_load(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR);

  step_access(AMO_W(AMO_ADD, 1, 3, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, read_csr(MTVAL));
}

static void test_amo_is_a_store(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, STORE_BREAKPOINT, DATA_ADDR);

  step_access(AMO_W(AMO_ADD, 1, 3, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_amo_breakpoint_has_priority_over_misaligned_address(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, STORE_BREAKPOINT, DATA_ADDR + 2);

  step_access(AMO_W(AMO_ADD, 1, 3, 2), DATA_ADDR + 2, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR + 2, read_csr(MTVAL));
}

// match: how the compare value (here the pc) is tested against tdata2.

static uxlen_t match_field_after_writing(unsigned match) {
  write_csr(TDATA1, EXECUTE_BREAKPOINT | MCONTROL_MATCH(match));
  return read_csr(TDATA1) & MCONTROL_MATCH_FIELD;
}

static void test_mcontrol_keeps_supported_match_modes(void) {
  static const unsigned supported[] = {MATCH_EQUAL,        MATCH_GE,
                                       MATCH_LT,           MATCH_MASK_LOW,
                                       MATCH_MASK_HIGH,    MATCH_NOT_EQUAL,
                                       MATCH_NOT_MASK_LOW, MATCH_NOT_MASK_HIGH};
  for (unsigned i = 0; i < sizeof supported / sizeof supported[0]; i++)
    TEST_ASSERT_EQUAL_HEX64(MCONTROL_MATCH(supported[i]),
                            match_field_after_writing(supported[i]));
}

// NAPOT needs maskmax > 0, and the remaining values are reserved.
static void test_unsupported_match_modes_read_back_as_equal(void) {
  static const unsigned unsupported[] = {MATCH_NAPOT, 6,  7,  MATCH_NOT_NAPOT,
                                         10,          11, 14, 15};
  for (unsigned i = 0; i < sizeof unsupported / sizeof unsupported[0]; i++)
    TEST_ASSERT_EQUAL_HEX64(MCONTROL_MATCH(MATCH_EQUAL),
                            match_field_after_writing(unsupported[i]));
}

static void set_match_trigger(unsigned match, uxlen_t tdata2) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, EXECUTE_BREAKPOINT | MCONTROL_MATCH(match), tdata2);
}

static void test_match_ge_fires_above_tdata2(void) {
  set_match_trigger(MATCH_GE, BREAKPOINT_PC - 4);

  step_marker(PRIV_M);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
}

static void test_match_ge_fires_on_tdata2_itself(void) {
  set_match_trigger(MATCH_GE, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
}

static void test_match_ge_ignores_addresses_below_tdata2(void) {
  set_match_trigger(MATCH_GE, BREAKPOINT_PC + 4);

  step_marker(PRIV_M);

  TEST_ASSERT_NOT_STOPPED();
}

static void test_match_lt_fires_below_tdata2(void) {
  set_match_trigger(MATCH_LT, BREAKPOINT_PC + 4);

  step_marker(PRIV_M);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
}

static void test_match_lt_ignores_tdata2_itself(void) {
  set_match_trigger(MATCH_LT, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_NOT_STOPPED();
}

// A signed comparison would see (uxlen_t)-4 as -4, below the pc on RV64.
static void test_match_lt_compares_unsigned(void) {
  set_match_trigger(MATCH_LT, (uxlen_t)-4);

  step_marker(PRIV_M);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
}

static void test_match_not_equal_fires_on_other_addresses(void) {
  set_match_trigger(MATCH_NOT_EQUAL, BREAKPOINT_PC + 4);

  step_marker(PRIV_M);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
}

static void test_match_not_equal_ignores_tdata2_itself(void) {
  set_match_trigger(MATCH_NOT_EQUAL, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_NOT_STOPPED();
}

// Mask modes split tdata2 in halves: the high half is a mask ANDed into one
// half of the compare value, the low half is what the result must equal.
#define HALF_BITS (XLEN / 2)
#define HALF_ONES (((uxlen_t)1 << HALF_BITS) - 1)
#define LOW_HALF(value) ((uxlen_t)(value) & HALF_ONES)
#define HIGH_HALF(value) ((uxlen_t)(value) >> HALF_BITS)
#define MASKED(mask, value) (((uxlen_t)(mask) << HALF_BITS) | (value))

// A load from DATA_ADDR (page offset 0x100) matches a trigger that masks off
// the page offset: it watches the whole 4 KiB page.
static void test_match_mask_low_ignores_bits_outside_the_mask(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT | MCONTROL_MATCH(MATCH_MASK_LOW),
              MASKED(HALF_ONES & ~0xFFFu, LOW_HALF(DATA_ADDR) & ~0xFFFu));

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_match_mask_low_ignores_a_different_low_half(void) {
  set_match_trigger(MATCH_MASK_LOW,
                    MASKED(HALF_ONES, LOW_HALF(BREAKPOINT_PC) | 0x10));

  step_marker(PRIV_M);

  TEST_ASSERT_NOT_STOPPED();
}

static void test_match_mask_high_compares_the_high_half(void) {
  set_match_trigger(MATCH_MASK_HIGH,
                    MASKED(HALF_ONES, HIGH_HALF(BREAKPOINT_PC)));

  step_marker(PRIV_M);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
}

static void test_match_mask_high_ignores_a_different_high_half(void) {
  set_match_trigger(MATCH_MASK_HIGH,
                    MASKED(HALF_ONES, HIGH_HALF(BREAKPOINT_PC) ^ 1));

  step_marker(PRIV_M);

  TEST_ASSERT_NOT_STOPPED();
}

static void test_match_not_mask_low_fires_when_mask_low_would_not(void) {
  set_match_trigger(MATCH_NOT_MASK_LOW,
                    MASKED(HALF_ONES, LOW_HALF(BREAKPOINT_PC) | 0x10));

  step_marker(PRIV_M);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
}

static void test_match_not_mask_high_fires_when_mask_high_would_not(void) {
  set_match_trigger(MATCH_NOT_MASK_HIGH,
                    MASKED(HALF_ONES, HIGH_HALF(BREAKPOINT_PC) ^ 1));

  step_marker(PRIV_M);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
}

static void test_match_modes_apply_to_loads(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT | MCONTROL_MATCH(MATCH_GE), DATA_ADDR - 0x10);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, read_csr(MTVAL));
}

// Every byte a load or store touches is a compare value, as the spec
// recommends; mtval reports the lowest byte that matched. An instruction
// fetch compares only the pc.

static void test_load_trigger_fires_on_any_byte_of_the_access(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR + 3);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR + 3, read_csr(MTVAL));
}

static void test_store_trigger_fires_on_any_byte_of_the_access(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, STORE_BREAKPOINT, DATA_ADDR + 1);

  step_access(SW(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR + 1, read_csr(MTVAL));
}

static void test_amo_trigger_fires_on_any_byte_of_the_access(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, STORE_BREAKPOINT, DATA_ADDR + 2);

  step_access(AMO_W(AMO_ADD, 1, 3, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR + 2, read_csr(MTVAL));
}

static void test_load_trigger_ignores_bytes_outside_the_access(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR + 1);

  step_access(LB(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE & 0xFF, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC + 4, cpu_get_pc(cpu));
}

static void test_range_trigger_reports_the_lowest_matching_byte(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT | MCONTROL_MATCH(MATCH_GE), DATA_ADDR + 2);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR + 2, read_csr(MTVAL));
}

// DATA_ADDR itself does not match, but the three bytes after it do.
static void test_not_equal_fires_on_the_other_bytes_of_the_access(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT | MCONTROL_MATCH(MATCH_NOT_EQUAL), DATA_ADDR);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR + 1, read_csr(MTVAL));
}

static void test_execute_trigger_compares_only_the_pc(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, EXECUTE_BREAKPOINT, BREAKPOINT_PC + 2);

  step_marker(PRIV_M);

  TEST_ASSERT_NOT_STOPPED();
}

// size (sizelo, plus sizehi on RV64): match only accesses or instructions of
// this width; 0 matches any.

static uxlen_t size_field_after_writing(unsigned size) {
  write_csr(TDATA1, EXECUTE_BREAKPOINT | MCONTROL_SIZE(size));
  return read_csr(TDATA1) & MCONTROL_SIZE_FIELD;
}

static void test_mcontrol_keeps_supported_sizes(void) {
  static const unsigned supported[] = {SIZE_ANY, SIZE_8BIT, SIZE_16BIT,
                                       SIZE_32BIT,
#if XLEN == 64
                                       SIZE_64BIT
#endif
  };
  for (unsigned i = 0; i < sizeof supported / sizeof supported[0]; i++)
    TEST_ASSERT_EQUAL_HEX64(MCONTROL_SIZE(supported[i]),
                            size_field_after_writing(supported[i]));
}

#if XLEN == 64
// 48-bit and wider instructions and 128-bit accesses do not exist here; the
// remaining values are reserved.
static void test_unsupported_sizes_read_back_as_any(void) {
  static const unsigned unsupported[] = {SIZE_48BIT, 6,  7,  8,  9, 10,
                                         11,         12, 13, 14, 15};
  for (unsigned i = 0; i < sizeof unsupported / sizeof unsupported[0]; i++)
    TEST_ASSERT_EQUAL_HEX64(MCONTROL_SIZE(SIZE_ANY),
                            size_field_after_writing(unsupported[i]));
}
#endif

static void set_sized_trigger(uxlen_t tdata1, unsigned size, uxlen_t tdata2) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, tdata1 | MCONTROL_SIZE(size), tdata2);
}

static void test_8bit_trigger_fires_on_a_byte_load(void) {
  set_sized_trigger(LOAD_BREAKPOINT, SIZE_8BIT, DATA_ADDR);

  step_access(LB(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_8bit_trigger_ignores_a_word_load(void) {
  set_sized_trigger(LOAD_BREAKPOINT, SIZE_8BIT, DATA_ADDR);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
}

static void test_16bit_trigger_fires_on_a_halfword_store(void) {
  set_sized_trigger(STORE_BREAKPOINT, SIZE_16BIT, DATA_ADDR);

  step_access(SH(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_32bit_trigger_ignores_a_halfword_store(void) {
  set_sized_trigger(STORE_BREAKPOINT, SIZE_32BIT, DATA_ADDR);

  step_access(SH(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64((OLD_VALUE & 0xFFFF0000) | (NEW_VALUE & 0xFFFF),
                          data_word());
}

static void test_32bit_trigger_fires_on_a_word_amo(void) {
  set_sized_trigger(STORE_BREAKPOINT, SIZE_32BIT, DATA_ADDR);

  step_access(AMO_W(AMO_ADD, 1, 3, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_32bit_trigger_fires_on_a_32bit_instruction(void) {
  set_sized_trigger(EXECUTE_BREAKPOINT, SIZE_32BIT, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
}

static void test_16bit_trigger_ignores_a_32bit_instruction(void) {
  set_sized_trigger(EXECUTE_BREAKPOINT, SIZE_16BIT, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_NOT_STOPPED();
}

#if XLEN == 64
static void test_64bit_trigger_fires_on_a_doubleword_load(void) {
  set_sized_trigger(LOAD_BREAKPOINT, SIZE_64BIT, DATA_ADDR);

  step_access(LD(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_64bit_trigger_ignores_a_word_load(void) {
  set_sized_trigger(LOAD_BREAKPOINT, SIZE_64BIT, DATA_ADDR);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
}
#endif

// chain: a trigger with chain set keeps the next one from matching unless it
// matches the same access too. Only the last trigger of a chain has chain
// clear, and the whole chain fires as one.

static void test_mcontrol_keeps_chain(void) {
  write_csr(TDATA1, LOAD_BREAKPOINT | MCONTROL_CHAIN);

  TEST_ASSERT_EQUAL_HEX64(LOAD_BREAKPOINT | MCONTROL_CHAIN, read_csr(TDATA1));
}

// There is no trigger after the last one for its chain to reach.
static void test_last_trigger_cannot_start_a_chain(void) {
  select_trigger(TRIGGER_COUNT - 1);
  write_csr(TDATA1, LOAD_BREAKPOINT | MCONTROL_CHAIN);

  TEST_ASSERT_EQUAL_HEX64(LOAD_BREAKPOINT, read_csr(TDATA1));
}

// Watches loads from [start, end) with triggers first and first + 1.
static void set_range(uxlen_t first, uxlen_t start, uxlen_t end) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(first,
              LOAD_BREAKPOINT | MCONTROL_MATCH(MATCH_GE) | MCONTROL_CHAIN,
              start);
  set_trigger(first + 1, LOAD_BREAKPOINT | MCONTROL_MATCH(MATCH_LT), end);
}

static void test_chained_range_fires_inside_the_range(void) {
  set_range(0, DATA_ADDR - 0x10, DATA_ADDR + 0x10);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, read_csr(MTVAL));
}

// The lt trigger alone would fire here.
static void test_chained_range_ignores_an_access_below_the_range(void) {
  set_range(0, DATA_ADDR + 0x8, DATA_ADDR + 0x10);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
}

// The ge trigger alone would fire here.
static void test_chained_range_ignores_an_access_above_the_range(void) {
  set_range(0, DATA_ADDR - 0x10, DATA_ADDR - 0x8);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
}

static void test_fired_chain_sets_hit_on_every_trigger_in_it(void) {
  set_range(0, DATA_ADDR - 0x10, DATA_ADDR + 0x10);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(MCONTROL_HIT, trigger_tdata1(0) & MCONTROL_HIT);
  TEST_ASSERT_EQUAL_HEX64(MCONTROL_HIT, trigger_tdata1(1) & MCONTROL_HIT);
}

static void test_trigger_held_back_by_its_chain_does_not_set_hit(void) {
  set_range(0, DATA_ADDR + 0x8, DATA_ADDR + 0x10);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, trigger_tdata1(1) & MCONTROL_HIT);
}

static void test_chain_can_start_at_any_trigger(void) {
  set_range(2, DATA_ADDR - 0x10, DATA_ADDR + 0x10);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_trigger_before_a_chain_is_not_part_of_it(void) {
  set_range(1, DATA_ADDR + 0x8, DATA_ADDR + 0x10);
  set_trigger(0, LOAD_BREAKPOINT, DATA_ADDR);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, read_csr(MTVAL));
  TEST_ASSERT_EQUAL_HEX64(0, trigger_tdata1(2) & MCONTROL_HIT);
}

static void test_chain_of_three_fires_when_every_trigger_matches(void) {
  set_range(0, DATA_ADDR - 0x10, DATA_ADDR + 0x10);
  set_trigger(1, LOAD_BREAKPOINT | MCONTROL_MATCH(MATCH_LT) | MCONTROL_CHAIN,
              DATA_ADDR + 0x10);
  set_trigger(2, LOAD_BREAKPOINT, DATA_ADDR + 2);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_chain_of_three_needs_every_trigger(void) {
  set_range(0, DATA_ADDR - 0x10, DATA_ADDR + 0x10);
  set_trigger(1, LOAD_BREAKPOINT | MCONTROL_MATCH(MATCH_LT) | MCONTROL_CHAIN,
              DATA_ADDR + 0x10);
  set_trigger(2, LOAD_BREAKPOINT, DATA_ADDR + 0x8);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
}

// The fetch and the load are separate accesses, so the two triggers never
// match at the same time.
static void test_chained_triggers_must_match_the_same_access(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, EXECUTE_BREAKPOINT | MCONTROL_CHAIN, BREAKPOINT_PC);
  set_trigger(1, LOAD_BREAKPOINT, DATA_ADDR);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC + 4, cpu_get_pc(cpu));
}

// select=1: tdata2 is compared with the data loaded or stored, or with the
// instruction executed, instead of the address. Bits past the access size are
// zero. mtval still reports the address. A load is performed before its data
// trigger fires, but its destination register is left unchanged.

#define LOAD_DATA_BREAKPOINT (LOAD_BREAKPOINT | MCONTROL_SELECT)
#define STORE_DATA_BREAKPOINT (STORE_BREAKPOINT | MCONTROL_SELECT)
#define EXECUTE_DATA_BREAKPOINT (EXECUTE_BREAKPOINT | MCONTROL_SELECT)

static void set_data_trigger(uxlen_t index, uxlen_t tdata1, uxlen_t value) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(index, tdata1, value);
}

static void test_mcontrol_keeps_select(void) {
  write_csr(TDATA1, LOAD_DATA_BREAKPOINT);

  TEST_ASSERT_EQUAL_HEX64(LOAD_DATA_BREAKPOINT, read_csr(TDATA1));
}

static void test_store_data_trigger_fires_on_the_value_written(void) {
  set_data_trigger(0, STORE_DATA_BREAKPOINT, NEW_VALUE);

  step_access(SW(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC, read_csr(MEPC));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, read_csr(MTVAL));
}

static void test_store_data_trigger_ignores_other_values(void) {
  set_data_trigger(0, STORE_DATA_BREAKPOINT, NEW_VALUE + 1);

  step_access(SW(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(NEW_VALUE, data_word());
}

static void test_store_data_is_cut_to_the_access_size(void) {
  set_data_trigger(0, STORE_DATA_BREAKPOINT, NEW_VALUE & 0xFF);

  step_access(SB(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_store_data_trigger_ignores_the_address(void) {
  set_data_trigger(0, STORE_DATA_BREAKPOINT, DATA_ADDR);

  step_access(SW(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(NEW_VALUE, data_word());
}

static void test_load_data_trigger_fires_on_the_value_read(void) {
  set_data_trigger(0, LOAD_DATA_BREAKPOINT, OLD_VALUE);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC, read_csr(MEPC));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, read_csr(MTVAL));
}

static void test_load_data_trigger_ignores_other_values(void) {
  set_data_trigger(0, LOAD_DATA_BREAKPOINT, OLD_VALUE + 1);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
}

// lb sign-extends 0x80 into rd, but the compare value is the byte itself.
static void test_load_data_is_compared_before_sign_extension(void) {
  set_data_trigger(0, LOAD_DATA_BREAKPOINT, 0x80);

  step_access_over(LB(1, 0, 2), DATA_ADDR, PRIV_M, 0x80);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

// Without data there is nothing to compare, so the fault is reported.
static void test_load_data_trigger_does_not_hide_a_load_access_fault(void) {
  set_data_trigger(0, LOAD_DATA_BREAKPOINT, 0);

  step_access(LW(1, 0, 2), UNMAPPED_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_LOAD_ACCESS, read_csr(MCAUSE));
}

// DATA_ADDR is above tdata2, so only the data can match here.
static void test_match_modes_apply_to_data(void) {
  set_data_trigger(0, LOAD_DATA_BREAKPOINT | MCONTROL_MATCH(MATCH_LT),
                   OLD_VALUE + 1);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_execute_data_trigger_fires_on_the_instruction(void) {
  set_data_trigger(0, EXECUTE_DATA_BREAKPOINT, LI(1, MARKER));

  step_marker(PRIV_M);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC, read_csr(MEPC));
  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC, read_csr(MTVAL));
}

// A compressed instruction is 16 bits: the halfword after it belongs to the
// next instruction and is not part of the data compared.
#define C_LI_RA_10 0x40a9 // c.li ra, 10
#define NEXT_HALFWORD 0xABCDu

static void test_execute_data_trigger_compares_16_bits_of_compressed(void) {
  static const uint32_t program[] = {NEXT_HALFWORD << 16 | C_LI_RA_10};
  load_program(program, 1);
  set_reg(1, 0);
  write_csr(MTVEC, TRAP_HANDLER);
  set_data_trigger(0, EXECUTE_DATA_BREAKPOINT, C_LI_RA_10);

  cpu_step(cpu);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
}

static void test_execute_data_trigger_ignores_other_instructions(void) {
  set_data_trigger(0, EXECUTE_DATA_BREAKPOINT, LI(1, MARKER + 1));

  step_marker(PRIV_M);

  TEST_ASSERT_NOT_STOPPED();
}

static void test_execute_data_trigger_ignores_the_pc(void) {
  set_data_trigger(0, EXECUTE_DATA_BREAKPOINT, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_NOT_STOPPED();
}

static void test_lr_data_trigger_fires_on_the_value_read(void) {
  set_data_trigger(0, LOAD_DATA_BREAKPOINT, OLD_VALUE);

  step_access(LR_W(1, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_sc_data_trigger_fires_on_the_value_written(void) {
  set_data_trigger(0, STORE_DATA_BREAKPOINT, NEW_VALUE);
  cpu_set_amo_reservation(cpu, DATA_ADDR);

  step_access(SC_W(1, 3, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_amo_load_data_is_the_old_value(void) {
  set_data_trigger(0, LOAD_DATA_BREAKPOINT, OLD_VALUE);

  step_access(AMO_W(AMO_ADD, 1, 3, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_amo_store_data_is_the_new_value(void) {
  set_data_trigger(0, STORE_DATA_BREAKPOINT, OLD_VALUE + NEW_VALUE);

  step_access(AMO_W(AMO_ADD, 1, 3, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

// The usual watchpoint: an address trigger chained to a data trigger stops
// when this value is written to this address.
static void set_value_at_address(uxlen_t tdata1, uxlen_t addr, uxlen_t value) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, tdata1 | MCONTROL_CHAIN, addr);
  set_trigger(1, tdata1 | MCONTROL_SELECT, value);
}

static void test_address_and_data_chain_fires_on_both(void) {
  set_value_at_address(STORE_BREAKPOINT, DATA_ADDR, NEW_VALUE);

  step_access(SW(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, read_csr(MTVAL));
}

static void test_address_and_data_chain_ignores_the_value_elsewhere(void) {
  set_value_at_address(STORE_BREAKPOINT, DATA_ADDR + 8, NEW_VALUE);

  step_access(SW(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(NEW_VALUE, data_word());
}

static void test_address_and_data_chain_ignores_another_value(void) {
  set_value_at_address(STORE_BREAKPOINT, DATA_ADDR, OLD_VALUE);

  step_access(SW(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(NEW_VALUE, data_word());
}

static void test_address_and_data_chain_works_on_loads(void) {
  set_value_at_address(LOAD_BREAKPOINT, DATA_ADDR, OLD_VALUE);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, read_csr(MTVAL));
}

// The compare value of a data trigger is cut to the access size, and only that
// many low bits of tdata2 take part in the compare.
static void test_data_compare_ignores_tdata2_bits_past_the_access_size(void) {
  set_data_trigger(0, STORE_DATA_BREAKPOINT, 0xAB00 | (NEW_VALUE & 0xFF));

  step_access(SB(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

// mcontrol6 (type 6) replaces mcontrol. It matches the same way; the fields
// moved, size grew to three bits, and hit became a two-bit field that says
// when the trigger fired (1: before the instruction retired).

#define MCONTROL6 TDATA1_TYPE(TRIGGER_TYPE_MCONTROL6)
#define MCONTROL6_HIT_FIELD (MCONTROL6_HIT1 | MCONTROL6_HIT0)
#define MCONTROL6_HIT_BEFORE MCONTROL6_HIT0
#define EXECUTE_BREAKPOINT6 (MCONTROL6 | MCONTROL_M | MCONTROL_EXECUTE)
#define LOAD_BREAKPOINT6 (MCONTROL6 | MCONTROL_M | MCONTROL_LOAD)
#define STORE_BREAKPOINT6 (MCONTROL6 | MCONTROL_M | MCONTROL_STORE)

static void test_tdata1_holds_an_mcontrol6_breakpoint(void) {
  write_csr(TDATA1, EXECUTE_BREAKPOINT6);

  TEST_ASSERT_EQUAL_HEX64(EXECUTE_BREAKPOINT6, read_csr(TDATA1));
}

static void test_mcontrol6_keeps_its_supported_fields(void) {
  uxlen_t tdata1 = MCONTROL6 | MCONTROL6_SELECT | MCONTROL6_SIZE(SIZE_32BIT) |
                   MCONTROL_CHAIN | MCONTROL_MATCH(MATCH_GE) | MCONTROL_M |
                   MCONTROL_S | MCONTROL_U | MCONTROL_EXECUTE | MCONTROL_STORE |
                   MCONTROL_LOAD;
  write_csr(TDATA1, tdata1);

  TEST_ASSERT_EQUAL_HEX64(tdata1, read_csr(TDATA1));
}

// No debug mode, no hypervisor, every access is fully observed, only the
// breakpoint action. Bits 20 and 19 are hit and select in mcontrol but
// unused in mcontrol6.
static void test_mcontrol6_drops_unsupported_fields(void) {
  write_csr(TDATA1, EXECUTE_BREAKPOINT6 | TDATA1_DMODE | MCONTROL6_UNCERTAIN |
                        MCONTROL6_VS | MCONTROL6_VU | MCONTROL6_UNCERTAINEN |
                        MCONTROL_ACTION(1) | MCONTROL_HIT | MCONTROL_SELECT);

  TEST_ASSERT_EQUAL_HEX64(EXECUTE_BREAKPOINT6, read_csr(TDATA1));
}

static uxlen_t size6_field_after_writing(unsigned size) {
  write_csr(TDATA1, EXECUTE_BREAKPOINT6 | MCONTROL6_SIZE(size));
  return read_csr(TDATA1) & MCONTROL6_SIZE_FIELD;
}

static void test_mcontrol6_keeps_supported_sizes(void) {
  static const unsigned supported[] = {SIZE_ANY, SIZE_8BIT, SIZE_16BIT,
                                       SIZE_32BIT,
#if XLEN == 64
                                       SIZE_64BIT
#endif
  };
  for (unsigned i = 0; i < sizeof supported / sizeof supported[0]; i++)
    TEST_ASSERT_EQUAL_HEX64(MCONTROL6_SIZE(supported[i]),
                            size6_field_after_writing(supported[i]));
}

static void test_mcontrol6_unsupported_sizes_read_back_as_any(void) {
  static const unsigned unsupported[] = {SIZE_48BIT,
#if XLEN == 32
                                         SIZE_64BIT,
#endif
                                         SIZE_128BIT, 7};
  for (unsigned i = 0; i < sizeof unsupported / sizeof unsupported[0]; i++)
    TEST_ASSERT_EQUAL_HEX64(MCONTROL6_SIZE(SIZE_ANY),
                            size6_field_after_writing(unsupported[i]));
}

static void test_mcontrol6_unsupported_match_modes_read_back_as_equal(void) {
  static const unsigned unsupported[] = {MATCH_NAPOT, 6,  7,  MATCH_NOT_NAPOT,
                                         10,          11, 14, 15};
  for (unsigned i = 0; i < sizeof unsupported / sizeof unsupported[0]; i++) {
    write_csr(TDATA1, EXECUTE_BREAKPOINT6 | MCONTROL_MATCH(unsupported[i]));
    TEST_ASSERT_EQUAL_HEX64(EXECUTE_BREAKPOINT6, read_csr(TDATA1));
  }
}

static void test_mcontrol6_hit_can_be_set_and_cleared_by_software(void) {
  write_csr(TDATA1, EXECUTE_BREAKPOINT6 | MCONTROL6_HIT_FIELD);
  TEST_ASSERT_EQUAL_HEX64(EXECUTE_BREAKPOINT6 | MCONTROL6_HIT_FIELD,
                          read_csr(TDATA1));

  write_csr(TDATA1, EXECUTE_BREAKPOINT6);
  TEST_ASSERT_EQUAL_HEX64(EXECUTE_BREAKPOINT6, read_csr(TDATA1));
}

static void test_last_mcontrol6_trigger_cannot_start_a_chain(void) {
  select_trigger(TRIGGER_COUNT - 1);
  write_csr(TDATA1, LOAD_BREAKPOINT6 | MCONTROL_CHAIN);

  TEST_ASSERT_EQUAL_HEX64(LOAD_BREAKPOINT6, read_csr(TDATA1));
}

static void
test_mcontrol6_execute_breakpoint_stops_before_the_instruction(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, EXECUTE_BREAKPOINT6, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_STOPPED_IN_MACHINE_MODE();
  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC, read_csr(MEPC));
  TEST_ASSERT_EQUAL_HEX64(BREAKPOINT_PC, read_csr(MTVAL));
}

static void test_mcontrol6_reports_firing_before_the_instruction(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, EXECUTE_BREAKPOINT6, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(MCONTROL6_HIT_BEFORE,
                          trigger_tdata1(0) & MCONTROL6_HIT_FIELD);
}

// Firing replaces whatever the hit field held, so hit1 goes back to 0.
static void test_mcontrol6_firing_overwrites_an_old_hit_value(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, EXECUTE_BREAKPOINT6 | MCONTROL6_HIT1, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(MCONTROL6_HIT_BEFORE,
                          trigger_tdata1(0) & MCONTROL6_HIT_FIELD);
}

static void test_mcontrol6_ignores_modes_it_is_not_enabled_in(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, MCONTROL6 | MCONTROL_U | MCONTROL_EXECUTE, BREAKPOINT_PC);

  step_marker(PRIV_M);

  TEST_ASSERT_NOT_STOPPED();
}

static void test_mcontrol6_load_trigger_fires_on_any_byte_of_the_access(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT6, DATA_ADDR + 3);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR + 3, read_csr(MTVAL));
}

static void test_mcontrol6_8bit_trigger_ignores_a_word_load(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT6 | MCONTROL6_SIZE(SIZE_8BIT), DATA_ADDR);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
}

static void test_mcontrol6_8bit_trigger_fires_on_a_byte_load(void) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT6 | MCONTROL6_SIZE(SIZE_8BIT), DATA_ADDR);

  step_access(LB(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void test_mcontrol6_store_data_trigger_fires_on_the_value_written(void) {
  set_data_trigger(0, STORE_BREAKPOINT6 | MCONTROL6_SELECT, NEW_VALUE);

  step_access(SW(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, read_csr(MTVAL));
}

static void test_mcontrol6_data_trigger_ignores_the_address(void) {
  set_data_trigger(0, STORE_BREAKPOINT6 | MCONTROL6_SELECT, DATA_ADDR);

  step_access(SW(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(NEW_VALUE, data_word());
}

static void
test_mcontrol6_data_compare_ignores_tdata2_bits_past_the_size(void) {
  set_data_trigger(0, STORE_BREAKPOINT6 | MCONTROL6_SELECT,
                   0xAB00 | (NEW_VALUE & 0xFF));

  step_access(SB(3, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, data_word());
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

static void set_range6(uxlen_t start, uxlen_t end) {
  write_csr(MSTATUS, MSTATUS_MIE);
  set_trigger(0, LOAD_BREAKPOINT6 | MCONTROL_MATCH(MATCH_GE) | MCONTROL_CHAIN,
              start);
  set_trigger(1, LOAD_BREAKPOINT6 | MCONTROL_MATCH(MATCH_LT), end);
}

static void test_mcontrol6_chained_range_fires_inside_the_range(void) {
  set_range6(DATA_ADDR - 0x10, DATA_ADDR + 0x10);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(MCONTROL6_HIT_BEFORE,
                          trigger_tdata1(1) & MCONTROL6_HIT_FIELD);
}

static void test_mcontrol6_chained_range_ignores_an_access_above_it(void) {
  set_range6(DATA_ADDR - 0x10, DATA_ADDR - 0x8);

  step_access(LW(1, 0, 2), DATA_ADDR, PRIV_M);

  TEST_ASSERT_EQUAL_HEX64(OLD_VALUE, get_reg(1));
}

// etrigger (type 5): fires when an exception whose code is set in tdata2 is
// taken from an enabled mode, right after the trap and before the first
// instruction of the handler. The breakpoint has mtval 0 and its epc is the
// handler address. The recursion guard applies in the handler's mode, so an
// M-mode handler (MIE just cleared) is never interrupted.

#define ETRIGGER TDATA1_TYPE(TRIGGER_TYPE_ETRIGGER)
#define EXCEPTION_BIT(cause) ((uxlen_t)1 << (cause))
#define ILLEGAL_WORD 0u
// Exception codes this hart can raise: 0-9 and 11.
#define RAISABLE_EXCEPTIONS ((uxlen_t)0xBFF)

static void test_tdata1_holds_an_etrigger(void) {
  write_csr(TDATA1, ETRIGGER | ETRIGGER_M | ETRIGGER_S | ETRIGGER_U);

  TEST_ASSERT_EQUAL_HEX64(ETRIGGER | ETRIGGER_M | ETRIGGER_S | ETRIGGER_U,
                          read_csr(TDATA1));
}

static void test_etrigger_drops_unsupported_fields(void) {
  write_csr(TDATA1, ETRIGGER | ETRIGGER_U | TDATA1_DMODE | ETRIGGER_VS |
                        ETRIGGER_VU | (1u << 10) | (1u << 8) |
                        ETRIGGER_ACTION(1));

  TEST_ASSERT_EQUAL_HEX64(ETRIGGER | ETRIGGER_U, read_csr(TDATA1));
}

static void test_etrigger_hit_can_be_set_and_cleared_by_software(void) {
  write_csr(TDATA1, ETRIGGER | ETRIGGER_U | ETRIGGER_HIT);
  TEST_ASSERT_EQUAL_HEX64(ETRIGGER | ETRIGGER_U | ETRIGGER_HIT,
                          read_csr(TDATA1));

  write_csr(TDATA1, ETRIGGER | ETRIGGER_U);
  TEST_ASSERT_EQUAL_HEX64(ETRIGGER | ETRIGGER_U, read_csr(TDATA1));
}

// A debugger reads tdata2 back to learn which codes are supported.
static void test_etrigger_keeps_only_exception_codes_this_hart_raises(void) {
  write_csr(TDATA1, ETRIGGER | ETRIGGER_U);
  write_csr(TDATA2, ~(uxlen_t)0);

  TEST_ASSERT_EQUAL_HEX64(RAISABLE_EXCEPTIONS, read_csr(TDATA2));
}

// Runs an illegal instruction; STVEC_BASE holds an instruction that would
// set x1 if the S-mode handler got to run.
static void step_illegal(enum Privilege privilege, uxlen_t medeleg) {
  static const uint32_t program[] = {ILLEGAL_WORD};
  load_program(program, 1);
  bus_write(bus, STVEC_BASE, 4, LI(1, MARKER));
  set_reg(1, 0);
  write_csr(MTVEC, TRAP_HANDLER);
  write_csr(STVEC, STVEC_BASE);
  write_csr(MEDELEG, medeleg);
  cpu_set_privilege(cpu, privilege);
  cpu_step(cpu);
}

#define DELEGATE_ILLEGAL EXCEPTION_BIT(CAUSE_ILLEGAL_INSTRUCTION)

static void set_etrigger(uxlen_t modes, uxlen_t causes) {
  set_trigger(0, ETRIGGER | modes, causes);
}

static void test_etrigger_fires_before_the_handler_runs(void) {
  set_etrigger(ETRIGGER_U, EXCEPTION_BIT(CAUSE_ILLEGAL_INSTRUCTION));

  step_illegal(PRIV_U, DELEGATE_ILLEGAL);

  TEST_ASSERT_EQUAL(PRIV_M, cpu_get_privilege(cpu));
  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(STVEC_BASE, read_csr(MEPC));
  TEST_ASSERT_EQUAL_HEX64(0, read_csr(MTVAL));
  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MPP_S, read_csr(MSTATUS) & MSTATUS_MPP);
}

static void test_etrigger_keeps_the_state_of_the_original_trap(void) {
  set_etrigger(ETRIGGER_U, EXCEPTION_BIT(CAUSE_ILLEGAL_INSTRUCTION));

  step_illegal(PRIV_U, DELEGATE_ILLEGAL);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_ILLEGAL_INSTRUCTION, read_csr(SCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE, read_csr(SEPC));
  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
}

static void test_etrigger_sets_hit(void) {
  set_etrigger(ETRIGGER_U, EXCEPTION_BIT(CAUSE_ILLEGAL_INSTRUCTION));

  step_illegal(PRIV_U, DELEGATE_ILLEGAL);

  TEST_ASSERT_EQUAL_HEX64(ETRIGGER_HIT, trigger_tdata1(0) & ETRIGGER_HIT);
}

#define TEST_ASSERT_IN_SUPERVISOR_HANDLER()                                    \
  do {                                                                         \
    TEST_ASSERT_EQUAL(PRIV_S, cpu_get_privilege(cpu));                         \
    TEST_ASSERT_EQUAL_HEX64(STVEC_BASE, cpu_get_pc(cpu));                      \
    TEST_ASSERT_EQUAL_HEX64(CAUSE_ILLEGAL_INSTRUCTION, read_csr(SCAUSE));      \
  } while (0)

static void test_etrigger_ignores_other_exceptions(void) {
  set_etrigger(ETRIGGER_U, EXCEPTION_BIT(CAUSE_ECALL_FROM_U));

  step_illegal(PRIV_U, DELEGATE_ILLEGAL);

  TEST_ASSERT_IN_SUPERVISOR_HANDLER();
  TEST_ASSERT_EQUAL_HEX64(0, trigger_tdata1(0) & ETRIGGER_HIT);
}

// The mode bits name the mode the exception was taken from, not the one it
// goes to.
static void test_etrigger_ignores_modes_it_is_not_enabled_in(void) {
  set_etrigger(ETRIGGER_S, EXCEPTION_BIT(CAUSE_ILLEGAL_INSTRUCTION));

  step_illegal(PRIV_U, DELEGATE_ILLEGAL);

  TEST_ASSERT_IN_SUPERVISOR_HANDLER();
}

static void test_etrigger_fires_on_an_exception_from_supervisor_mode(void) {
  set_etrigger(ETRIGGER_S, EXCEPTION_BIT(CAUSE_ILLEGAL_INSTRUCTION));

  step_illegal(PRIV_S, DELEGATE_ILLEGAL);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(STVEC_BASE, read_csr(MEPC));
}

static void test_etrigger_does_not_fire_in_a_machine_mode_handler(void) {
  set_etrigger(ETRIGGER_U, EXCEPTION_BIT(CAUSE_ILLEGAL_INSTRUCTION));

  step_illegal(PRIV_U, 0);

  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_ILLEGAL_INSTRUCTION, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE, read_csr(MEPC));
}

// The breakpoint would be taken in S-mode, whose handler has SIE clear.
static void
test_etrigger_does_not_fire_when_breakpoints_are_delegated_too(void) {
  set_etrigger(ETRIGGER_U, EXCEPTION_BIT(CAUSE_ILLEGAL_INSTRUCTION));

  step_illegal(PRIV_U, DELEGATE_ILLEGAL | DELEGATE_BREAKPOINT);

  TEST_ASSERT_IN_SUPERVISOR_HANDLER();
}

// icount (type 3): counts instructions retired, and traps taken, in enabled
// modes. When count goes from 1 to 0, pending is set; the trigger then fires
// just before the next instruction in an enabled mode, with mtval 0.

#define ICOUNT TDATA1_TYPE(TRIGGER_TYPE_ICOUNT)

static void test_tdata1_holds_an_icount(void) {
  uxlen_t tdata1 = ICOUNT | ICOUNT_COUNT(5) | ICOUNT_M | ICOUNT_S | ICOUNT_U;
  write_csr(TDATA1, tdata1);

  TEST_ASSERT_EQUAL_HEX64(tdata1, read_csr(TDATA1));
}

static void test_icount_drops_unsupported_fields(void) {
  write_csr(TDATA1, ICOUNT | ICOUNT_COUNT(1) | ICOUNT_M | TDATA1_DMODE |
                        ICOUNT_VS | ICOUNT_VU | ICOUNT_ACTION(1));

  TEST_ASSERT_EQUAL_HEX64(ICOUNT | ICOUNT_COUNT(1) | ICOUNT_M,
                          read_csr(TDATA1));
}

static void test_icount_count_holds_fourteen_bits(void) {
  write_csr(TDATA1, ICOUNT | ICOUNT_COUNT_FIELD | ICOUNT_M);

  TEST_ASSERT_EQUAL_HEX64(ICOUNT | ICOUNT_COUNT_FIELD | ICOUNT_M,
                          read_csr(TDATA1));
}

static void test_icount_pending_and_hit_can_be_set_and_cleared(void) {
  write_csr(TDATA1, ICOUNT | ICOUNT_M | ICOUNT_PENDING | ICOUNT_HIT);
  TEST_ASSERT_EQUAL_HEX64(ICOUNT | ICOUNT_M | ICOUNT_PENDING | ICOUNT_HIT,
                          read_csr(TDATA1));

  write_csr(TDATA1, ICOUNT | ICOUNT_M);
  TEST_ASSERT_EQUAL_HEX64(ICOUNT | ICOUNT_M, read_csr(TDATA1));
}

// Three instructions: x1 = 1, x2 = 2, x3 = 3.
static void start_counting(uxlen_t modes, unsigned count,
                           enum Privilege privilege) {
  static const uint32_t program[] = {LI(1, 1), LI(2, 2), LI(3, 3)};
  load_program(program, 3);
  write_csr(MSTATUS, MSTATUS_MIE);
  write_csr(MTVEC, TRAP_HANDLER);
  set_trigger(0, ICOUNT | modes | ICOUNT_COUNT(count), 0);
  cpu_set_privilege(cpu, privilege);
}

static uxlen_t icount_state(void) {
  return trigger_tdata1(0) & (ICOUNT_COUNT_FIELD | ICOUNT_PENDING | ICOUNT_HIT);
}

static void test_icount_counts_down_on_each_instruction(void) {
  start_counting(ICOUNT_M, 3, PRIV_M);

  run_steps(1);

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(ICOUNT_COUNT(2), icount_state());
}

static void test_icount_sets_pending_when_count_reaches_zero(void) {
  start_counting(ICOUNT_M, 1, PRIV_M);

  run_steps(1);

  TEST_ASSERT_EQUAL_HEX64(1, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 4, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL_HEX64(ICOUNT_PENDING, icount_state());
}

static void test_icount_fires_before_the_next_instruction(void) {
  start_counting(ICOUNT_M, 1, PRIV_M);

  run_steps(2);

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(2));
  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 4, read_csr(MEPC));
  TEST_ASSERT_EQUAL_HEX64(0, read_csr(MTVAL));
}

static void test_icount_firing_clears_pending_and_sets_hit(void) {
  start_counting(ICOUNT_M, 1, PRIV_M);

  run_steps(2);

  TEST_ASSERT_EQUAL_HEX64(ICOUNT_HIT, icount_state());
}

static void test_icount_fires_after_count_instructions(void) {
  start_counting(ICOUNT_M, 2, PRIV_M);

  run_steps(3);

  TEST_ASSERT_EQUAL_HEX64(2, get_reg(2));
  TEST_ASSERT_EQUAL_HEX64(0, get_reg(3));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 8, read_csr(MEPC));
}

static void test_icount_at_zero_never_fires(void) {
  start_counting(ICOUNT_M, 0, PRIV_M);

  run_steps(3);

  TEST_ASSERT_EQUAL_HEX64(3, get_reg(3));
  TEST_ASSERT_EQUAL_HEX64(0, icount_state());
}

static void test_icount_ignores_modes_it_is_not_enabled_in(void) {
  start_counting(ICOUNT_U, 1, PRIV_M);

  run_steps(1);

  TEST_ASSERT_EQUAL_HEX64(ICOUNT_COUNT(1), icount_state());
}

static void test_icount_does_not_count_in_machine_mode_with_mie_clear(void) {
  start_counting(ICOUNT_M, 1, PRIV_M);
  write_csr(MSTATUS, 0);

  run_steps(1);

  TEST_ASSERT_EQUAL_HEX64(ICOUNT_COUNT(1), icount_state());
}

// Single step over an ecall from U-mode: the trap counts, the M-mode handler
// (not enabled) runs undisturbed, and the trigger fires before the next U-mode
// instruction.
static void test_icount_counts_a_trap_and_fires_after_the_handler(void) {
  static const uint32_t program[] = {ECALL, LI(1, MARKER)};
  load_program(program, 2);
  bus_write(bus, TRAP_HANDLER, 4, MRET);
  set_reg(1, 0);
  write_csr(MTVEC, TRAP_HANDLER);
  set_trigger(0, ICOUNT | ICOUNT_U | ICOUNT_COUNT(1), 0);
  cpu_set_privilege(cpu, PRIV_U);

  run_steps(1);
  TEST_ASSERT_EQUAL_HEX64(CAUSE_ECALL_FROM_U, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(ICOUNT_PENDING, icount_state());
  write_csr(MEPC, RAM_BASE + 4);

  run_steps(1);
  TEST_ASSERT_EQUAL(PRIV_U, cpu_get_privilege(cpu));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 4, cpu_get_pc(cpu));

  run_steps(1);
  TEST_ASSERT_EQUAL_HEX64(0, get_reg(1));
  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 4, read_csr(MEPC));
  TEST_ASSERT_EQUAL_HEX64(MSTATUS_MPP_U, read_csr(MSTATUS) & MSTATUS_MPP);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_tselect_selects_each_trigger);
  RUN_TEST(test_tselect_ignores_an_index_past_the_last_trigger);
  RUN_TEST(test_tinfo_reports_version_and_supported_types);
  RUN_TEST(test_tinfo_ignores_writes);
  RUN_TEST(test_triggers_start_disabled);
  RUN_TEST(test_tdata1_holds_an_mcontrol_breakpoint);
  RUN_TEST(test_writing_zero_to_tdata1_disables_the_trigger);
  RUN_TEST(test_no_trigger_is_armed_after_reset);
  RUN_TEST(test_setting_any_trigger_arms_the_module);
  RUN_TEST(test_module_stays_armed_until_the_last_trigger_is_disabled);
  RUN_TEST(test_tdata1_with_an_unsupported_type_is_disabled);
  RUN_TEST(test_mcontrol_keeps_only_supported_settings);
  RUN_TEST(test_mcontrol_reports_no_napot_support);
  RUN_TEST(test_tdata2_holds_any_address);
  RUN_TEST(test_each_trigger_keeps_its_own_tdata);
  RUN_TEST(test_tdata3_reads_zero_without_context_matching);
  RUN_TEST(test_tcontrol_is_not_implemented);
  RUN_TEST(test_trigger_csrs_are_machine_only);
  RUN_TEST(test_hit_can_be_set_and_cleared_by_software);
  RUN_TEST(test_execute_breakpoint_stops_before_the_instruction);
  RUN_TEST(test_execute_breakpoint_reports_the_instruction_address);
  RUN_TEST(test_execute_breakpoint_sets_hit);
  RUN_TEST(test_stopped_instruction_does_not_retire);
  RUN_TEST(test_execute_breakpoint_at_another_address_does_not_fire);
  RUN_TEST(test_disabled_trigger_does_not_fire);
  RUN_TEST(test_load_store_trigger_does_not_fire_on_execute);
  RUN_TEST(test_every_trigger_is_checked_not_only_the_selected_one);
  RUN_TEST(test_every_matching_trigger_sets_hit);
  RUN_TEST(test_execute_breakpoint_ignores_modes_it_is_not_enabled_in);
  RUN_TEST(test_execute_breakpoint_fires_in_user_mode);
  RUN_TEST(test_execute_breakpoint_fires_in_supervisor_mode);
  RUN_TEST(test_execute_breakpoint_has_priority_over_fetch_access_fault);
  RUN_TEST(test_breakpoint_does_not_fire_in_machine_mode_with_mie_clear);
  RUN_TEST(test_undelegated_breakpoint_fires_in_supervisor_mode_with_sie_clear);
  RUN_TEST(
      test_delegated_breakpoint_does_not_fire_in_supervisor_mode_with_sie_clear);
  RUN_TEST(test_delegated_breakpoint_fires_in_supervisor_mode_with_sie_set);
  RUN_TEST(test_delegated_breakpoint_fires_in_user_mode_regardless_of_sie);
  RUN_TEST(test_load_breakpoint_stops_before_the_load);
  RUN_TEST(test_trigger_comparing_against_zero_fires);
  RUN_TEST(test_writing_another_trigger_keeps_this_one);
  RUN_TEST(test_load_breakpoint_reports_the_pc_and_the_data_address);
  RUN_TEST(test_load_breakpoint_sets_hit);
  RUN_TEST(test_stopped_load_does_not_retire);
  RUN_TEST(test_load_breakpoint_compares_the_computed_address);
  RUN_TEST(test_store_breakpoint_stops_before_memory_is_written);
  RUN_TEST(test_store_breakpoint_reports_the_pc_and_the_data_address);
  RUN_TEST(test_load_breakpoint_at_another_address_does_not_fire);
  RUN_TEST(test_load_breakpoint_does_not_fire_on_a_store);
  RUN_TEST(test_store_breakpoint_does_not_fire_on_a_load);
  RUN_TEST(test_execute_breakpoint_does_not_fire_on_a_load);
  RUN_TEST(test_load_breakpoint_does_not_fire_on_instruction_fetch);
  RUN_TEST(test_load_breakpoint_has_priority_over_load_access_fault);
  RUN_TEST(test_store_breakpoint_has_priority_over_store_access_fault);
  RUN_TEST(test_load_breakpoint_ignores_modes_it_is_not_enabled_in);
  RUN_TEST(test_load_breakpoint_fires_in_user_mode);
  RUN_TEST(test_load_breakpoint_does_not_fire_in_machine_mode_with_mie_clear);
  RUN_TEST(test_delegated_store_breakpoint_enters_supervisor_mode);
  RUN_TEST(test_lr_is_a_load);
  RUN_TEST(test_lr_is_not_a_store);
  RUN_TEST(test_lr_breakpoint_has_priority_over_misaligned_address);
  RUN_TEST(test_successful_sc_is_a_store);
  RUN_TEST(test_failing_sc_is_not_a_store);
  RUN_TEST(test_sc_is_not_a_load);
  RUN_TEST(test_amo_is_a_load);
  RUN_TEST(test_amo_is_a_store);
  RUN_TEST(test_amo_breakpoint_has_priority_over_misaligned_address);
  RUN_TEST(test_mcontrol_keeps_supported_match_modes);
  RUN_TEST(test_unsupported_match_modes_read_back_as_equal);
  RUN_TEST(test_match_ge_fires_above_tdata2);
  RUN_TEST(test_match_ge_fires_on_tdata2_itself);
  RUN_TEST(test_match_ge_ignores_addresses_below_tdata2);
  RUN_TEST(test_match_lt_fires_below_tdata2);
  RUN_TEST(test_match_lt_ignores_tdata2_itself);
  RUN_TEST(test_match_lt_compares_unsigned);
  RUN_TEST(test_match_not_equal_fires_on_other_addresses);
  RUN_TEST(test_match_not_equal_ignores_tdata2_itself);
  RUN_TEST(test_match_mask_low_ignores_bits_outside_the_mask);
  RUN_TEST(test_match_mask_low_ignores_a_different_low_half);
  RUN_TEST(test_match_mask_high_compares_the_high_half);
  RUN_TEST(test_match_mask_high_ignores_a_different_high_half);
  RUN_TEST(test_match_not_mask_low_fires_when_mask_low_would_not);
  RUN_TEST(test_match_not_mask_high_fires_when_mask_high_would_not);
  RUN_TEST(test_match_modes_apply_to_loads);
  RUN_TEST(test_load_trigger_fires_on_any_byte_of_the_access);
  RUN_TEST(test_store_trigger_fires_on_any_byte_of_the_access);
  RUN_TEST(test_amo_trigger_fires_on_any_byte_of_the_access);
  RUN_TEST(test_load_trigger_ignores_bytes_outside_the_access);
  RUN_TEST(test_range_trigger_reports_the_lowest_matching_byte);
  RUN_TEST(test_not_equal_fires_on_the_other_bytes_of_the_access);
  RUN_TEST(test_execute_trigger_compares_only_the_pc);
  RUN_TEST(test_mcontrol_keeps_supported_sizes);
#if XLEN == 64
  RUN_TEST(test_unsupported_sizes_read_back_as_any);
#endif
  RUN_TEST(test_8bit_trigger_fires_on_a_byte_load);
  RUN_TEST(test_8bit_trigger_ignores_a_word_load);
  RUN_TEST(test_16bit_trigger_fires_on_a_halfword_store);
  RUN_TEST(test_32bit_trigger_ignores_a_halfword_store);
  RUN_TEST(test_32bit_trigger_fires_on_a_word_amo);
  RUN_TEST(test_32bit_trigger_fires_on_a_32bit_instruction);
  RUN_TEST(test_16bit_trigger_ignores_a_32bit_instruction);
#if XLEN == 64
  RUN_TEST(test_64bit_trigger_fires_on_a_doubleword_load);
  RUN_TEST(test_64bit_trigger_ignores_a_word_load);
#endif
  RUN_TEST(test_mcontrol_keeps_chain);
  RUN_TEST(test_last_trigger_cannot_start_a_chain);
  RUN_TEST(test_chained_range_fires_inside_the_range);
  RUN_TEST(test_chained_range_ignores_an_access_below_the_range);
  RUN_TEST(test_chained_range_ignores_an_access_above_the_range);
  RUN_TEST(test_fired_chain_sets_hit_on_every_trigger_in_it);
  RUN_TEST(test_trigger_held_back_by_its_chain_does_not_set_hit);
  RUN_TEST(test_chain_can_start_at_any_trigger);
  RUN_TEST(test_trigger_before_a_chain_is_not_part_of_it);
  RUN_TEST(test_chain_of_three_fires_when_every_trigger_matches);
  RUN_TEST(test_chain_of_three_needs_every_trigger);
  RUN_TEST(test_chained_triggers_must_match_the_same_access);
  RUN_TEST(test_mcontrol_keeps_select);
  RUN_TEST(test_store_data_trigger_fires_on_the_value_written);
  RUN_TEST(test_store_data_trigger_ignores_other_values);
  RUN_TEST(test_store_data_is_cut_to_the_access_size);
  RUN_TEST(test_store_data_trigger_ignores_the_address);
  RUN_TEST(test_load_data_trigger_fires_on_the_value_read);
  RUN_TEST(test_load_data_trigger_ignores_other_values);
  RUN_TEST(test_load_data_is_compared_before_sign_extension);
  RUN_TEST(test_load_data_trigger_does_not_hide_a_load_access_fault);
  RUN_TEST(test_match_modes_apply_to_data);
  RUN_TEST(test_execute_data_trigger_fires_on_the_instruction);
  RUN_TEST(test_execute_data_trigger_compares_16_bits_of_compressed);
  RUN_TEST(test_execute_data_trigger_ignores_other_instructions);
  RUN_TEST(test_execute_data_trigger_ignores_the_pc);
  RUN_TEST(test_lr_data_trigger_fires_on_the_value_read);
  RUN_TEST(test_sc_data_trigger_fires_on_the_value_written);
  RUN_TEST(test_amo_load_data_is_the_old_value);
  RUN_TEST(test_amo_store_data_is_the_new_value);
  RUN_TEST(test_address_and_data_chain_fires_on_both);
  RUN_TEST(test_address_and_data_chain_ignores_the_value_elsewhere);
  RUN_TEST(test_address_and_data_chain_ignores_another_value);
  RUN_TEST(test_address_and_data_chain_works_on_loads);
  RUN_TEST(test_data_compare_ignores_tdata2_bits_past_the_access_size);
  RUN_TEST(test_tdata1_holds_an_mcontrol6_breakpoint);
  RUN_TEST(test_mcontrol6_keeps_its_supported_fields);
  RUN_TEST(test_mcontrol6_drops_unsupported_fields);
  RUN_TEST(test_mcontrol6_keeps_supported_sizes);
  RUN_TEST(test_mcontrol6_unsupported_sizes_read_back_as_any);
  RUN_TEST(test_mcontrol6_unsupported_match_modes_read_back_as_equal);
  RUN_TEST(test_mcontrol6_hit_can_be_set_and_cleared_by_software);
  RUN_TEST(test_last_mcontrol6_trigger_cannot_start_a_chain);
  RUN_TEST(test_mcontrol6_execute_breakpoint_stops_before_the_instruction);
  RUN_TEST(test_mcontrol6_reports_firing_before_the_instruction);
  RUN_TEST(test_mcontrol6_firing_overwrites_an_old_hit_value);
  RUN_TEST(test_mcontrol6_ignores_modes_it_is_not_enabled_in);
  RUN_TEST(test_mcontrol6_load_trigger_fires_on_any_byte_of_the_access);
  RUN_TEST(test_mcontrol6_8bit_trigger_ignores_a_word_load);
  RUN_TEST(test_mcontrol6_8bit_trigger_fires_on_a_byte_load);
  RUN_TEST(test_mcontrol6_store_data_trigger_fires_on_the_value_written);
  RUN_TEST(test_mcontrol6_data_trigger_ignores_the_address);
  RUN_TEST(test_mcontrol6_data_compare_ignores_tdata2_bits_past_the_size);
  RUN_TEST(test_mcontrol6_chained_range_fires_inside_the_range);
  RUN_TEST(test_mcontrol6_chained_range_ignores_an_access_above_it);
  RUN_TEST(test_tdata1_holds_an_etrigger);
  RUN_TEST(test_etrigger_drops_unsupported_fields);
  RUN_TEST(test_etrigger_hit_can_be_set_and_cleared_by_software);
  RUN_TEST(test_etrigger_keeps_only_exception_codes_this_hart_raises);
  RUN_TEST(test_etrigger_fires_before_the_handler_runs);
  RUN_TEST(test_etrigger_keeps_the_state_of_the_original_trap);
  RUN_TEST(test_etrigger_sets_hit);
  RUN_TEST(test_etrigger_ignores_other_exceptions);
  RUN_TEST(test_etrigger_ignores_modes_it_is_not_enabled_in);
  RUN_TEST(test_etrigger_fires_on_an_exception_from_supervisor_mode);
  RUN_TEST(test_etrigger_does_not_fire_in_a_machine_mode_handler);
  RUN_TEST(test_etrigger_does_not_fire_when_breakpoints_are_delegated_too);
  RUN_TEST(test_tdata1_holds_an_icount);
  RUN_TEST(test_icount_drops_unsupported_fields);
  RUN_TEST(test_icount_count_holds_fourteen_bits);
  RUN_TEST(test_icount_pending_and_hit_can_be_set_and_cleared);
  RUN_TEST(test_icount_counts_down_on_each_instruction);
  RUN_TEST(test_icount_sets_pending_when_count_reaches_zero);
  RUN_TEST(test_icount_fires_before_the_next_instruction);
  RUN_TEST(test_icount_firing_clears_pending_and_sets_hit);
  RUN_TEST(test_icount_fires_after_count_instructions);
  RUN_TEST(test_icount_at_zero_never_fires);
  RUN_TEST(test_icount_ignores_modes_it_is_not_enabled_in);
  RUN_TEST(test_icount_does_not_count_in_machine_mode_with_mie_clear);
  RUN_TEST(test_icount_counts_a_trap_and_fires_after_the_handler);
  return UNITY_END();
}
