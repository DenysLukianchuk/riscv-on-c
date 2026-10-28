#ifndef TESTS_CPU_FIXTURE_H
#define TESTS_CPU_FIXTURE_H

#include "riscv/bus.h"
#include "riscv/cpu.h"
#include "riscv/csr.h"
#include "riscv/decoder.h"
#include "riscv/execute.h"
#include "riscv/hart.h"
#include "riscv/memory.h"
#include "riscv/registers.h"
#include "riscv_asm.h"
#include "riscv_spec.h"
#include "unity.h"

#define TEST_RAM_SIZE 4096
#define TRAP_HANDLER (RAM_BASE + 0x400)
#define UNMAPPED_ADDR 0x10

static Memory *mem;
static Bus *bus;
static Cpu *cpu;

void setUp(void) {
  mem = memory_create(TEST_RAM_SIZE);
  bus = bus_create();
  bus_attach(bus, memory_bus_device(mem, RAM_BASE));
  cpu = cpu_create(bus);
}

void tearDown(void) {
  cpu_destroy(cpu);
  bus_destroy(bus);
  memory_destroy(mem);
}

static inline void execute_raw(uint32_t raw) {
  struct Instruction inst = instruction_decode(raw);
  cpu_execute(cpu, &inst);
}

// Copies a program to the start of RAM and points pc at it.
static inline void load_program(const uint32_t *program, size_t count) {
  for (size_t i = 0; i < count; i++)
    bus_write(bus, RAM_BASE + 4 * i, 4, program[i]);
  cpu_set_pc(cpu, RAM_BASE);
}

static inline void run_steps(unsigned steps) {
  for (unsigned i = 0; i < steps; i++)
    cpu_step(cpu);
}

static inline uxlen_t get_reg(uint8_t index) {
  return registers_read(cpu_get_registers(cpu), index);
}

static inline void set_reg(uint8_t index, uxlen_t value) {
  registers_write(cpu_get_registers(cpu), index, value);
}

static inline uxlen_t read_csr(uint16_t addr) {
  uxlen_t value = 0;
  csr_read(cpu_get_csr(cpu), addr, PRIV_M, &value);
  return value;
}

static inline void write_csr(uint16_t addr, uxlen_t value) {
  csr_write(cpu_get_csr(cpu), addr, PRIV_M, value);
}

// Macros rather than functions so that a failure points at the caller's line.
// mcause and mtval are cleared first so a trap left over from an earlier check
// cannot make this one pass.
#define TEST_ASSERT_TRAP(raw, cause, tval)                                     \
  do {                                                                         \
    write_csr(MCAUSE, 0);                                                      \
    write_csr(MTVAL, 0);                                                       \
    execute_raw(raw);                                                          \
    TEST_ASSERT_EQUAL_HEX64((cause), read_csr(MCAUSE));                        \
    TEST_ASSERT_EQUAL_HEX64((tval), read_csr(MTVAL));                          \
  } while (0)

#define TEST_ASSERT_ILLEGAL_INSTRUCTION(raw)                                   \
  TEST_ASSERT_TRAP(raw, CAUSE_ILLEGAL_INSTRUCTION, raw)

#endif // TESTS_CPU_FIXTURE_H
