#include "riscv/machine.h"
#include "riscv_asm.h"
#include "riscv_spec.h"
#include "unity.h"

// The memory map follows QEMU's virt board.
#define FINISHER_ADDR 0x00100000
#define FINISHER_SIZE 0x1000
#define CLINT_ADDR 0x02000000
#define PLIC_ADDR 0x0C000000
#define PLIC_SIZE 0x600000
#define UART_ADDR 0x10000000
#define UART_SIZE 0x100
#define UART_IRQ 10
#define UART_IER 1
#define UART_LSR 5
#define IER_RX_DATA 0x01
#define LSR_DR 0x01
#define LSR_THRE 0x20
#define RAM_ADDR 0x80000000
#define RAM_SIZE 4096
#define HANDLER (RAM_ADDR + 0x400)
#define INTERRUPT(code) (CAUSE_INTERRUPT | (code))

static Machine *machine;
static Cpu *cpu;
static Bus *bus;
static char console[64];
static unsigned console_length;

static void print(void *context, uint8_t byte) {
  (void)context;
  console[console_length++] = (char)byte;
  console[console_length] = '\0';
}

void setUp(void) {
  console_length = 0;
  console[0] = '\0';
  machine = machine_create(RAM_SIZE, print, NULL);
  cpu = machine_get_cpu(machine);
  bus = machine_get_bus(machine);
}

void tearDown(void) { machine_destroy(machine); }

static void load(const uint32_t *program, size_t count) {
  for (size_t i = 0; i < count; i++)
    bus_write(bus, RAM_ADDR + 4 * i, 4, program[i]);
  cpu_set_pc(cpu, RAM_ADDR);
}

static void load_nops(void) {
  static const uint32_t program[] = {NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP};
  load(program, 8);
}

static uxlen_t csr(uint16_t addr) {
  uxlen_t value = 0;
  csr_read(cpu_get_csr(cpu), addr, PRIV_M, &value);
  return value;
}

static void set_csr(uint16_t addr, uxlen_t value) {
  csr_write(cpu_get_csr(cpu), addr, PRIV_M, value);
}

static uint64_t clint(uxlen_t offset) {
  uint64_t value = 0;
  TEST_ASSERT_EQUAL(BUS_OK, bus_read(bus, CLINT_ADDR + offset, 8, &value));
  return value;
}

static void set_plic(uxlen_t offset, uint32_t value) {
  TEST_ASSERT_EQUAL(BUS_OK, bus_write(bus, PLIC_ADDR + offset, 4, value));
}

static uint32_t plic(uxlen_t offset) {
  uint64_t value = 0;
  TEST_ASSERT_EQUAL(BUS_OK, bus_read(bus, PLIC_ADDR + offset, 4, &value));
  return (uint32_t)value;
}

static void run(unsigned steps) {
  for (unsigned i = 0; i < steps; i++)
    machine_step(machine);
}

// The memory map.

static void test_ram_is_at_its_address(void) {
  TEST_ASSERT_EQUAL(BUS_OK, bus_write(bus, RAM_ADDR, 4, 0xCAFEBABE));

  uint64_t value = 0;
  TEST_ASSERT_EQUAL(BUS_OK, bus_read(bus, RAM_ADDR + RAM_SIZE - 4, 4, &value));
  TEST_ASSERT_EQUAL(BUS_OK, bus_read(bus, RAM_ADDR, 4, &value));
  TEST_ASSERT_EQUAL_HEX64(0xCAFEBABE, value);
}

static void test_finisher_is_at_its_address(void) {
  uint64_t value = 0xFF;

  TEST_ASSERT_EQUAL(BUS_OK, bus_read(bus, FINISHER_ADDR, 4, &value));
  TEST_ASSERT_EQUAL_HEX64(0, value);
}

static void test_clint_is_at_its_address(void) {
  TEST_ASSERT_EQUAL_HEX64(0, clint(CLINT_MTIME));
}

static void test_plic_is_at_its_address(void) {
  set_plic(PLIC_PRIORITY(UART_IRQ), 5);

  TEST_ASSERT_EQUAL_HEX32(5, plic(PLIC_PRIORITY(UART_IRQ)));
}

static void test_uart_is_at_its_address(void) {
  uint64_t value = 0;

  TEST_ASSERT_EQUAL(BUS_OK, bus_read(bus, UART_ADDR + UART_LSR, 1, &value));
  TEST_ASSERT_BITS(LSR_THRE, LSR_THRE, value);
}

static void test_nothing_else_is_mapped(void) {
  uint64_t value = 0;

  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT,
                    bus_read(bus, UART_ADDR + UART_SIZE, 1, &value));

  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT,
                    bus_read(bus, FINISHER_ADDR + FINISHER_SIZE, 4, &value));
  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT,
                    bus_read(bus, CLINT_ADDR + CLINT_SIZE, 4, &value));
  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT,
                    bus_read(bus, PLIC_ADDR + PLIC_SIZE, 4, &value));
  TEST_ASSERT_EQUAL(BUS_ACCESS_FAULT,
                    bus_read(bus, RAM_ADDR + RAM_SIZE, 4, &value));
}

// Time: mtime counts the steps the machine has run.

static void test_mtime_advances_once_per_step(void) {
  load_nops();

  run(3);

  TEST_ASSERT_EQUAL_HEX64(3, clint(CLINT_MTIME));
}

// Wires: the CLINT drives MTIP and MSIP in mip. A step sees the lines as they
// are when it starts.

static void enable_machine_interrupts(uxlen_t interrupts) {
  set_csr(MTVEC, HANDLER);
  set_csr(MIE, interrupts);
  set_csr(MSTATUS, MSTATUS_MIE);
}

static void test_timer_interrupt_is_taken_once_mtime_reaches_mtimecmp(void) {
  load_nops();
  bus_write(bus, CLINT_ADDR + CLINT_MTIMECMP, 8, 2);
  enable_machine_interrupts(MTIP);

  run(3);

  TEST_ASSERT_EQUAL_HEX64(INTERRUPT(IRQ_M_TIMER), csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_ADDR + 8, csr(MEPC));
  TEST_ASSERT_EQUAL_HEX64(HANDLER, cpu_get_pc(cpu));
}

static void test_timer_is_not_taken_before_mtimecmp(void) {
  load_nops();
  bus_write(bus, CLINT_ADDR + CLINT_MTIMECMP, 8, 2);
  enable_machine_interrupts(MTIP);

  run(2);

  TEST_ASSERT_EQUAL_HEX64(0, csr(MIP) & MTIP);
  TEST_ASSERT_EQUAL_HEX64(RAM_ADDR + 8, cpu_get_pc(cpu));
}

static void test_moving_mtimecmp_ahead_clears_mtip(void) {
  load_nops();
  bus_write(bus, CLINT_ADDR + CLINT_MTIMECMP, 8, 0);
  run(1);
  TEST_ASSERT_EQUAL_HEX64(MTIP, csr(MIP) & MTIP);

  bus_write(bus, CLINT_ADDR + CLINT_MTIMECMP, 8, 100);
  run(1);

  TEST_ASSERT_EQUAL_HEX64(0, csr(MIP) & MTIP);
}

static void test_msip_raises_the_software_interrupt(void) {
  load_nops();
  enable_machine_interrupts(MSIP);
  bus_write(bus, CLINT_ADDR + CLINT_MSIP, 4, 1);

  run(1);

  TEST_ASSERT_EQUAL_HEX64(INTERRUPT(IRQ_M_SOFTWARE), csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_ADDR, csr(MEPC));
}

// A whole program: it programs the timer through the CLINT, enables the
// interrupt and spins; the handler sets x10.
static const uint32_t TIMER_PROGRAM[] = {
    LUI(5, (CLINT_ADDR + CLINT_MTIMECMP) >> 12), // x5 = &mtimecmp
    LI(6, 20),
    SW(6, 0, 5), // mtimecmp = 20
    SW(0, 4, 5),
    LI(7, MTIP),
    CSRRS(0, MIE, 7),
    CSRRSI(0, MSTATUS, MSTATUS_MIE),
    JAL(0, 0), // spin
};

static void test_program_receives_a_timer_interrupt(void) {
  load(TIMER_PROGRAM, 8);
  bus_write(bus, HANDLER, 4, LI(10, 1));
  bus_write(bus, HANDLER + 4, 4, JAL(0, 0));
  set_csr(MTVEC, HANDLER);

  run(30);

  TEST_ASSERT_EQUAL_HEX64(1, registers_read(cpu_get_registers(cpu), 10));
  TEST_ASSERT_EQUAL_HEX64(INTERRUPT(IRQ_M_TIMER), csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_ADDR + 7 * 4, csr(MEPC));
}

// The console: what the guest writes to the UART comes out, and what the
// host types goes in.

static const uint32_t HELLO_PROGRAM[] = {
    LUI(5, UART_ADDR >> 12), // x5 = &THR
    LI(6, 'H'),
    SB(6, 0, 5),
    LI(6, 'i'),
    SB(6, 0, 5),
    JAL(0, 0), // spin
};

static void test_program_prints_through_the_uart(void) {
  load(HELLO_PROGRAM, 6);

  run(6);

  TEST_ASSERT_EQUAL_STRING("Hi", console);
}

static void test_console_input_reaches_the_uart(void) {
  machine_console_input(machine, 'x');

  uint64_t value = 0;
  bus_read(bus, UART_ADDR + UART_LSR, 1, &value);
  TEST_ASSERT_BITS(LSR_DR, LSR_DR, value);
  bus_read(bus, UART_ADDR, 1, &value);
  TEST_ASSERT_EQUAL_HEX64('x', value);
}

// External interrupts: the UART's line goes to PLIC source 10, and the PLIC
// drives MEIP from context 0 and SEIP from context 1.

static void route_uart_to(unsigned context) {
  set_plic(PLIC_PRIORITY(UART_IRQ), 1);
  set_plic(PLIC_ENABLE(context), 1u << UART_IRQ);
  bus_write(bus, UART_ADDR + UART_IER, 1, IER_RX_DATA);
}

static void test_uart_raises_meip_through_the_plic(void) {
  load_nops();
  route_uart_to(0);
  machine_console_input(machine, 'x');

  run(1);

  TEST_ASSERT_EQUAL_HEX64(MEIP, csr(MIP) & MEIP);
  TEST_ASSERT_EQUAL_HEX64(0, csr(MIP) & SEIP);
}

static void test_uart_raises_seip_through_the_plic(void) {
  load_nops();
  route_uart_to(1);
  machine_console_input(machine, 'x');

  run(1);

  TEST_ASSERT_EQUAL_HEX64(SEIP, csr(MIP) & SEIP);
  TEST_ASSERT_EQUAL_HEX64(0, csr(MIP) & MEIP);
}

static void test_meip_drops_once_the_byte_is_read(void) {
  load_nops();
  route_uart_to(0);
  machine_console_input(machine, 'x');
  run(1);

  uint64_t value = 0;
  bus_read(bus, UART_ADDR, 1, &value);
  run(1);

  TEST_ASSERT_EQUAL_HEX64(0, csr(MIP) & MEIP);
}

// SEIP as software reads it is its own writable bit ORed with the PLIC's
// line; the board drives the line without touching the bit.
static void test_software_seip_survives_a_quiet_plic(void) {
  load_nops();
  set_csr(MIP, SEIP);

  run(1);

  TEST_ASSERT_EQUAL_HEX64(SEIP, csr(MIP) & SEIP);
}

static void test_software_cannot_clear_the_plic_seip_line(void) {
  load_nops();
  route_uart_to(1);
  machine_console_input(machine, 'x');
  run(1);

  set_csr(MIP, 0);

  TEST_ASSERT_EQUAL_HEX64(SEIP, csr(MIP) & SEIP);
}

// csrrs reads mip and writes it back; only the software SEIP bit takes part
// in that, so the PLIC's line must not stick in it.
static void test_csrrs_on_mip_does_not_latch_the_plic_seip_line(void) {
  static const uint32_t program[] = {LI(5, SSIP), CSRRS(0, MIP, 5), NOP, NOP};
  load(program, 4);
  route_uart_to(1);
  machine_console_input(machine, 'x');
  run(2);

  uint64_t value = 0;
  bus_read(bus, UART_ADDR, 1, &value);
  run(1);

  TEST_ASSERT_EQUAL_HEX64(SSIP, csr(MIP) & (SSIP | SEIP));
}

// The line does not only show in mip: an S-mode hart takes the interrupt.
static void test_s_mode_takes_the_plic_interrupt(void) {
  load_nops();
  route_uart_to(1);
  set_csr(MIDELEG, SEIP);
  set_csr(STVEC, HANDLER);
  set_csr(MIE, SEIP);
  set_csr(MSTATUS, MSTATUS_SIE);
  cpu_set_privilege(cpu, PRIV_S);
  machine_console_input(machine, 'x');

  run(1);

  TEST_ASSERT_EQUAL_HEX64(INTERRUPT(IRQ_S_EXTERNAL), csr(SCAUSE));
  TEST_ASSERT_EQUAL_HEX64(RAM_ADDR, csr(SEPC));
  TEST_ASSERT_EQUAL_HEX64(HANDLER, cpu_get_pc(cpu));
  TEST_ASSERT_EQUAL(PRIV_S, cpu_get_privilege(cpu));
}

// A whole round trip: a typed byte interrupts the program, whose handler
// claims the source from the PLIC, reads the byte and completes.
static const uint32_t UART_HANDLER[] = {
    LUI(5, (PLIC_ADDR + PLIC_CLAIM(0)) >> 12),
    LW(6, PLIC_CLAIM(0) & 0xFFF, 5), // x6 = claimed source
    LUI(7, UART_ADDR >> 12),
    LBU(10, 0, 7),                   // x10 = the byte
    SW(6, PLIC_CLAIM(0) & 0xFFF, 5), // complete
    JAL(0, 0),                       // spin
};

static void test_program_receives_a_byte_through_the_plic(void) {
  load_nops();
  for (size_t i = 0; i < 6; i++)
    bus_write(bus, HANDLER + 4 * i, 4, UART_HANDLER[i]);
  route_uart_to(0);
  enable_machine_interrupts(MEIP);
  machine_console_input(machine, 'x');

  run(10);

  Registers *regs = cpu_get_registers(cpu);
  TEST_ASSERT_EQUAL_HEX64(INTERRUPT(IRQ_M_EXTERNAL), csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(UART_IRQ, registers_read(regs, 6));
  TEST_ASSERT_EQUAL_HEX64('x', registers_read(regs, 10));
  TEST_ASSERT_EQUAL_HEX64(0, csr(MIP) & MEIP);
  TEST_ASSERT_EQUAL_HEX32(0, plic(PLIC_PENDING));
}

// Power: the guest turns the board off through the finisher, and the board
// keeps the exit code for the host.

static void test_board_is_on_after_reset(void) {
  TEST_ASSERT_FALSE(machine_finished(machine));
}

static const uint32_t PASS_PROGRAM[] = {
    LUI(5, FINISHER_ADDR >> 12),
    LUI(6, 0x5),
    ADDI(6, 6, 0x555), // x6 = 0x5555, pass
    SW(6, 0, 5),
    LI(10, 1), // never runs: the board is off
    JAL(0, 0),
};

static void test_program_powers_off_with_pass(void) {
  load(PASS_PROGRAM, 6);

  run(4);

  TEST_ASSERT_TRUE(machine_finished(machine));
  TEST_ASSERT_EQUAL(0, machine_exit_code(machine));
}

static void test_program_powers_off_with_a_failure_code(void) {
  static const uint32_t program[] = {
      LUI(5, FINISHER_ADDR >> 12),
      LUI(6, 0x33),
      ADDI(6, 6, 0x333), // x6 = 0x33333: fail with code 3
      SW(6, 0, 5),
  };
  load(program, 4);

  run(4);

  TEST_ASSERT_TRUE(machine_finished(machine));
  TEST_ASSERT_EQUAL(3, machine_exit_code(machine));
}

static void test_board_that_is_off_runs_nothing(void) {
  load(PASS_PROGRAM, 6);

  run(10);

  TEST_ASSERT_EQUAL_HEX64(0, registers_read(cpu_get_registers(cpu), 10));
  TEST_ASSERT_EQUAL_HEX64(RAM_ADDR + 4 * 4, cpu_get_pc(cpu));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_ram_is_at_its_address);
  RUN_TEST(test_finisher_is_at_its_address);
  RUN_TEST(test_clint_is_at_its_address);
  RUN_TEST(test_plic_is_at_its_address);
  RUN_TEST(test_uart_is_at_its_address);
  RUN_TEST(test_nothing_else_is_mapped);
  RUN_TEST(test_mtime_advances_once_per_step);
  RUN_TEST(test_timer_interrupt_is_taken_once_mtime_reaches_mtimecmp);
  RUN_TEST(test_timer_is_not_taken_before_mtimecmp);
  RUN_TEST(test_moving_mtimecmp_ahead_clears_mtip);
  RUN_TEST(test_msip_raises_the_software_interrupt);
  RUN_TEST(test_program_receives_a_timer_interrupt);
  RUN_TEST(test_program_prints_through_the_uart);
  RUN_TEST(test_console_input_reaches_the_uart);
  RUN_TEST(test_uart_raises_meip_through_the_plic);
  RUN_TEST(test_uart_raises_seip_through_the_plic);
  RUN_TEST(test_meip_drops_once_the_byte_is_read);
  RUN_TEST(test_software_seip_survives_a_quiet_plic);
  RUN_TEST(test_software_cannot_clear_the_plic_seip_line);
  RUN_TEST(test_csrrs_on_mip_does_not_latch_the_plic_seip_line);
  RUN_TEST(test_s_mode_takes_the_plic_interrupt);
  RUN_TEST(test_program_receives_a_byte_through_the_plic);
  RUN_TEST(test_board_is_on_after_reset);
  RUN_TEST(test_program_powers_off_with_pass);
  RUN_TEST(test_program_powers_off_with_a_failure_code);
  RUN_TEST(test_board_that_is_off_runs_nothing);
  return UNITY_END();
}
