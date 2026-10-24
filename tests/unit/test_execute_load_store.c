#include "cpu_fixture.h"

#define LW_X1_0_X2 0x00012083

static void test_lw_loads_word_from_rs1_address(void) {
  bus_write(bus, RAM_BASE + 0x100, 0x12345678, 4);
  set_reg(2, RAM_BASE + 0x100);

  execute_raw(LW_X1_0_X2);

  TEST_ASSERT_EQUAL_HEX64(0x12345678, get_reg(1));
}

static void test_lw_sign_extends_loaded_word_to_xlen(void) {
  bus_write(bus, RAM_BASE + 0x100, 0x80000000, 4);
  set_reg(2, RAM_BASE + 0x100);

  execute_raw(LW_X1_0_X2);

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-1 << 31, get_reg(1));
}

#define LB_X1_0_X2 0x00010083

static void test_lb_loads_sign_extended_byte(void) {
  bus_write(bus, RAM_BASE + 0x100, 0x80, 1);
  set_reg(2, RAM_BASE + 0x100);

  execute_raw(LB_X1_0_X2);

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-128, get_reg(1));
}

#define LH_X1_0_X2 0x00011083
#define LBU_X1_0_X2 0x00014083
#define LHU_X1_0_X2 0x00015083
#define LW_X1_MINUS_4_X2 0xFFC12083

static void test_lh_loads_sign_extended_halfword(void) {
  bus_write(bus, RAM_BASE + 0x100, 0x8000, 2);
  set_reg(2, RAM_BASE + 0x100);

  execute_raw(LH_X1_0_X2);

  TEST_ASSERT_EQUAL_HEX64((uxlen_t)-32768, get_reg(1));
}

static void test_lbu_loads_zero_extended_byte(void) {
  bus_write(bus, RAM_BASE + 0x100, 0x80, 1);
  set_reg(2, RAM_BASE + 0x100);

  execute_raw(LBU_X1_0_X2);

  TEST_ASSERT_EQUAL_HEX64(0x80, get_reg(1));
}

static void test_lhu_loads_zero_extended_halfword(void) {
  bus_write(bus, RAM_BASE + 0x100, 0x8000, 2);
  set_reg(2, RAM_BASE + 0x100);

  execute_raw(LHU_X1_0_X2);

  TEST_ASSERT_EQUAL_HEX64(0x8000, get_reg(1));
}

static void test_load_adds_negative_offset_to_rs1_value(void) {
  bus_write(bus, RAM_BASE + 0xFC, 0x12345678, 4);
  set_reg(2, RAM_BASE + 0x100);

  execute_raw(LW_X1_MINUS_4_X2);

  TEST_ASSERT_EQUAL_HEX64(0x12345678, get_reg(1));
}

#if XLEN == 64
#define LWU_X1_0_X2 0x00016083
#define LD_X1_0_X2 0x00013083

static void test_lwu_loads_zero_extended_word(void) {
  bus_write(bus, RAM_BASE + 0x100, 0x80000000, 4);
  set_reg(2, RAM_BASE + 0x100);

  execute_raw(LWU_X1_0_X2);

  TEST_ASSERT_EQUAL_HEX64(0x80000000, get_reg(1));
}

static void test_ld_loads_doubleword(void) {
  bus_write(bus, RAM_BASE + 0x100, 0x8000000000000001, 8);
  set_reg(2, RAM_BASE + 0x100);

  execute_raw(LD_X1_0_X2);

  TEST_ASSERT_EQUAL_HEX64(0x8000000000000001, get_reg(1));
}
#endif

#define SW_X3_0_X2 0x00312023

static void test_sw_stores_word_to_rs1_address(void) {
  set_reg(2, RAM_BASE + 0x100);
  set_reg(3, 0x12345678);

  execute_raw(SW_X3_0_X2);

  uint64_t stored = 0;
  bus_read(bus, RAM_BASE + 0x100, 4, &stored);
  TEST_ASSERT_EQUAL_HEX64(0x12345678, stored);
}

#define SB_X3_0_X2 0x00310023
#define SH_X3_0_X2 0x00311023
#define SW_X3_MINUS_4_X2 0xFE312E23

#define STORE_ADDR (RAM_BASE + 0x100)
#define BACKGROUND 0xAAAAAAAAAAAAAAAA

static uint64_t store_over_background(uint32_t raw, uxlen_t value) {
  bus_write(bus, STORE_ADDR, BACKGROUND, 8);
  set_reg(2, STORE_ADDR);
  set_reg(3, value);

  execute_raw(raw);

  uint64_t doubleword = 0;
  bus_read(bus, STORE_ADDR, 8, &doubleword);
  return doubleword;
}

static void test_sb_stores_only_low_byte_of_rs2(void) {
  TEST_ASSERT_EQUAL_HEX64(0xAAAAAAAAAAAAAA78,
                          store_over_background(SB_X3_0_X2, 0x12345678));
}

static void test_sh_stores_only_low_halfword_of_rs2(void) {
  TEST_ASSERT_EQUAL_HEX64(0xAAAAAAAAAAAA5678,
                          store_over_background(SH_X3_0_X2, 0x12345678));
}

static void test_sw_leaves_following_bytes_untouched(void) {
  TEST_ASSERT_EQUAL_HEX64(0xAAAAAAAA12345678,
                          store_over_background(SW_X3_0_X2, 0x12345678));
}

static void test_store_adds_negative_offset_to_rs1_value(void) {
  set_reg(2, STORE_ADDR + 4);
  set_reg(3, 0x12345678);

  execute_raw(SW_X3_MINUS_4_X2);

  uint64_t stored = 0;
  bus_read(bus, STORE_ADDR, 4, &stored);
  TEST_ASSERT_EQUAL_HEX64(0x12345678, stored);
}

static void test_store_advances_pc(void) {
  cpu_set_pc(cpu, 0x80000000);

  store_over_background(SW_X3_0_X2, 0x12345678);

  TEST_ASSERT_EQUAL_HEX64(0x80000004, cpu_get_pc(cpu));
}

#if XLEN == 64
#define SD_X3_0_X2 0x00313023

static void test_sd_stores_doubleword(void) {
  uint64_t stored = store_over_background(SD_X3_0_X2, 0x8000000000000001);

  TEST_ASSERT_EQUAL_HEX64(0x8000000000000001, stored);
}
#endif

#define UNMAPPED_ADDR 0x10

static void test_lw_from_unmapped_address_raises_load_access_fault(void) {
  set_reg(2, UNMAPPED_ADDR);

  execute_raw(LW_X1_0_X2);

  TEST_ASSERT_EQUAL_HEX64(5, read_csr(CSR_REG_MCAUSE));
}

#define TRAP_HANDLER (RAM_BASE + 0x400)

static void test_load_access_fault_jumps_to_trap_handler(void) {
  write_csr(CSR_REG_MTVEC, TRAP_HANDLER);
  cpu_set_pc(cpu, RAM_BASE);
  set_reg(2, UNMAPPED_ADDR);

  execute_raw(LW_X1_0_X2);

  TEST_ASSERT_EQUAL_HEX64(TRAP_HANDLER, cpu_get_pc(cpu));
}

static void raise_load_access_fault(void) {
  write_csr(CSR_REG_MTVEC, TRAP_HANDLER);
  cpu_set_pc(cpu, RAM_BASE + 0x20);
  set_reg(1, 0xDEAD);
  set_reg(2, UNMAPPED_ADDR);

  execute_raw(LW_X1_0_X2);
}

static void test_load_access_fault_saves_faulting_pc_to_mepc(void) {
  raise_load_access_fault();

  TEST_ASSERT_EQUAL_HEX64(RAM_BASE + 0x20, read_csr(CSR_REG_MEPC));
}

static void test_load_access_fault_saves_address_to_mtval(void) {
  raise_load_access_fault();

  TEST_ASSERT_EQUAL_HEX64(UNMAPPED_ADDR, read_csr(CSR_REG_MTVAL));
}

static void test_load_access_fault_leaves_rd_unchanged(void) {
  raise_load_access_fault();

  TEST_ASSERT_EQUAL_HEX64(0xDEAD, get_reg(1));
}

static void test_sw_to_unmapped_address_raises_store_access_fault(void) {
  set_reg(2, UNMAPPED_ADDR);

  execute_raw(SW_X3_0_X2);

  TEST_ASSERT_EQUAL_HEX64(7, read_csr(CSR_REG_MCAUSE));
}

#define LOAD_FUNCT3_7_X1_0_X2 0x00017083

static void test_load_with_reserved_funct3_raises_illegal_instruction(void) {
  set_reg(2, RAM_BASE + 0x100);

  TEST_ASSERT_ILLEGAL_INSTRUCTION(LOAD_FUNCT3_7_X1_0_X2);
}

#define STORE_FUNCT3_4_X3_0_X2 0x00314023
#define STORE_FUNCT3_5_X3_0_X2 0x00315023
#define STORE_FUNCT3_6_X3_0_X2 0x00316023
#define STORE_FUNCT3_7_X3_0_X2 0x00317023

static void test_store_with_reserved_funct3_raises_illegal_instruction(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(STORE_FUNCT3_4_X3_0_X2);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(STORE_FUNCT3_5_X3_0_X2);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(STORE_FUNCT3_6_X3_0_X2);
  TEST_ASSERT_ILLEGAL_INSTRUCTION(STORE_FUNCT3_7_X3_0_X2);
}

#if XLEN == 32
#define LD_X1_0_X2 0x00013083
#define LWU_X1_0_X2 0x00016083
#define SD_X3_0_X2 0x00313023

static void test_ld_is_illegal_on_rv32(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(LD_X1_0_X2);
}

static void test_lwu_is_illegal_on_rv32(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(LWU_X1_0_X2);
}

static void test_sd_is_illegal_on_rv32(void) {
  TEST_ASSERT_ILLEGAL_INSTRUCTION(SD_X3_0_X2);
}
#endif

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_lw_loads_word_from_rs1_address);
  RUN_TEST(test_lw_sign_extends_loaded_word_to_xlen);
  RUN_TEST(test_lb_loads_sign_extended_byte);
  RUN_TEST(test_lh_loads_sign_extended_halfword);
  RUN_TEST(test_lbu_loads_zero_extended_byte);
  RUN_TEST(test_lhu_loads_zero_extended_halfword);
  RUN_TEST(test_load_adds_negative_offset_to_rs1_value);
  RUN_TEST(test_sw_stores_word_to_rs1_address);
  RUN_TEST(test_sb_stores_only_low_byte_of_rs2);
  RUN_TEST(test_sh_stores_only_low_halfword_of_rs2);
  RUN_TEST(test_sw_leaves_following_bytes_untouched);
  RUN_TEST(test_store_adds_negative_offset_to_rs1_value);
  RUN_TEST(test_store_advances_pc);
  RUN_TEST(test_lw_from_unmapped_address_raises_load_access_fault);
  RUN_TEST(test_load_access_fault_jumps_to_trap_handler);
  RUN_TEST(test_load_access_fault_saves_faulting_pc_to_mepc);
  RUN_TEST(test_load_access_fault_saves_address_to_mtval);
  RUN_TEST(test_load_access_fault_leaves_rd_unchanged);
  RUN_TEST(test_sw_to_unmapped_address_raises_store_access_fault);
  RUN_TEST(test_load_with_reserved_funct3_raises_illegal_instruction);
#if XLEN == 64
  RUN_TEST(test_lwu_loads_zero_extended_word);
  RUN_TEST(test_ld_loads_doubleword);
  RUN_TEST(test_sd_stores_doubleword);
#endif
  RUN_TEST(test_store_with_reserved_funct3_raises_illegal_instruction);
#if XLEN == 32
  RUN_TEST(test_ld_is_illegal_on_rv32);
  RUN_TEST(test_lwu_is_illegal_on_rv32);
  RUN_TEST(test_sd_is_illegal_on_rv32);
#endif
  return UNITY_END();
}
