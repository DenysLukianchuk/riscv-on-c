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
#include "unity.h"

#define TEST_RAM_SIZE 4096

static Memory *mem;
static Bus *bus;
static Cpu *cpu;

void setUp(void) {
  mem = memory_create(TEST_RAM_SIZE);
  bus = bus_create(mem);
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

static inline uxlen_t get_reg(uint8_t index) {
  return registers_read(cpu_get_registers(cpu), index);
}

static inline void set_reg(uint8_t index, uxlen_t value) {
  registers_write(cpu_get_registers(cpu), index, value);
}

static inline uxlen_t read_csr(uint16_t addr) {
  uxlen_t value = 0;
  csr_read(cpu_get_csr(cpu), addr, &value);
  return value;
}

static inline void write_csr(uint16_t addr, uxlen_t value) {
  csr_write(cpu_get_csr(cpu), addr, value);
}

// A macro rather than a function so that a failure points at the caller's
// line. mcause and mtval are cleared first so a trap left over from an earlier
// check cannot make this one pass.
#define TEST_ASSERT_ILLEGAL_INSTRUCTION(raw)                                   \
  do {                                                                         \
    write_csr(CSR_REG_MCAUSE, 0);                                              \
    write_csr(CSR_REG_MTVAL, 0);                                               \
    execute_raw(raw);                                                          \
    TEST_ASSERT_EQUAL_HEX64(2, read_csr(CSR_REG_MCAUSE));                      \
    TEST_ASSERT_EQUAL_HEX64((raw), read_csr(CSR_REG_MTVAL));                   \
  } while (0)

#endif // TESTS_CPU_FIXTURE_H
