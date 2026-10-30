#define TEST_RAM_SIZE (16 * 4096)
#include "cpu_fixture.h"
#include "page_table.h"

// The CPU's own accesses go through the MMU: instruction fetches, loads and
// stores use virtual addresses once S-mode turns paging on. A failed
// translation is a page fault of the access's kind, with the virtual address
// in mtval.

// VADDR and CODE_VADDR share their upper VPNs, so they share the tables.
#define VADDR 0x40403678
#define PAGE_OFFSET 0x678
#define CODE_VADDR 0x40404000
#define DATA_PAGE (RAM_BASE + 8 * PAGE_SIZE)
#define CODE_PAGE (RAM_BASE + 9 * PAGE_SIZE)
#define LEAF (PTE_V | PTE_A | PTE_D)

static void start_paging_in_s_mode(void) {
  clear_page(ROOT_TABLE);
  enable_paging(ROOT_TABLE);
  cpu_set_privilege(cpu, PRIV_S);
}

static uint32_t read_physical(uxlen_t addr) {
  uint64_t value = 0;
  bus_read(bus, addr, 4, &value);
  return (uint32_t)value;
}

static void test_load_reads_the_mapped_page(void) {
  start_paging_in_s_mode();
  map_page(VADDR, DATA_PAGE, LEAF | PTE_R);
  bus_write(bus, DATA_PAGE + PAGE_OFFSET, 4, 0x12345678);
  set_reg(1, VADDR);

  execute_raw(LW(2, 0, 1));

  TEST_ASSERT_EQUAL_HEX64(0x12345678, get_reg(2));
}

static void test_store_writes_the_mapped_page(void) {
  start_paging_in_s_mode();
  map_page(VADDR, DATA_PAGE, LEAF | PTE_R | PTE_W);
  set_reg(1, VADDR);
  set_reg(2, 0x0BADCAFE);

  execute_raw(SW(2, 0, 1));

  TEST_ASSERT_EQUAL_HEX32(0x0BADCAFE, read_physical(DATA_PAGE + PAGE_OFFSET));
}

static void test_load_from_an_unmapped_page_is_a_load_page_fault(void) {
  start_paging_in_s_mode();
  set_reg(1, VADDR);

  TEST_ASSERT_TRAP(LW(2, 0, 1), CAUSE_LOAD_PAGE_FAULT, VADDR);
}

static void test_store_to_a_read_only_page_is_a_store_page_fault(void) {
  start_paging_in_s_mode();
  map_page(VADDR, DATA_PAGE, LEAF | PTE_R);
  set_reg(1, VADDR);

  TEST_ASSERT_TRAP(SW(2, 0, 1), CAUSE_STORE_PAGE_FAULT, VADDR);
}

static void test_instruction_is_fetched_through_the_page_table(void) {
  start_paging_in_s_mode();
  map_page(CODE_VADDR, CODE_PAGE, LEAF | PTE_X);
  bus_write(bus, CODE_PAGE, 4, ADDI(5, 0, 42));
  cpu_set_pc(cpu, CODE_VADDR);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(42, get_reg(5));
  TEST_ASSERT_EQUAL_HEX64(CODE_VADDR + 4, cpu_get_pc(cpu));
}

static void
test_fetch_from_an_unmapped_page_is_an_instruction_page_fault(void) {
  start_paging_in_s_mode();
  cpu_set_pc(cpu, CODE_VADDR);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_FETCH_PAGE_FAULT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(CODE_VADDR, read_csr(MTVAL));
  TEST_ASSERT_EQUAL_HEX64(CODE_VADDR, read_csr(MEPC));
}

// Atomics. lr is a load; sc and AMOs are stores, and an AMO that cannot
// translate reports a store page fault even though it reads first.

static void test_lr_reads_the_mapped_page(void) {
  start_paging_in_s_mode();
  map_page(VADDR, DATA_PAGE, LEAF | PTE_R);
  bus_write(bus, DATA_PAGE + PAGE_OFFSET, 4, 0x12345678);
  set_reg(1, VADDR);

  execute_raw(LR_W(2, 1));

  TEST_ASSERT_EQUAL_HEX64(0x12345678, get_reg(2));
}

static void test_sc_writes_the_mapped_page(void) {
  start_paging_in_s_mode();
  map_page(VADDR, DATA_PAGE, LEAF | PTE_R | PTE_W);
  set_reg(1, VADDR);
  set_reg(4, 0x0BADCAFE);

  execute_raw(LR_W(2, 1));
  execute_raw(SC_W(3, 4, 1));

  TEST_ASSERT_EQUAL_HEX64(0, get_reg(3));
  TEST_ASSERT_EQUAL_HEX32(0x0BADCAFE, read_physical(DATA_PAGE + PAGE_OFFSET));
}

static void test_amo_updates_the_mapped_page(void) {
  start_paging_in_s_mode();
  map_page(VADDR, DATA_PAGE, LEAF | PTE_R | PTE_W);
  bus_write(bus, DATA_PAGE + PAGE_OFFSET, 4, 5);
  set_reg(1, VADDR);
  set_reg(4, 3);

  execute_raw(AMO_W(AMO_ADD, 2, 4, 1));

  TEST_ASSERT_EQUAL_HEX64(5, get_reg(2));
  TEST_ASSERT_EQUAL_HEX32(8, read_physical(DATA_PAGE + PAGE_OFFSET));
}

static void test_lr_from_an_unmapped_page_is_a_load_page_fault(void) {
  start_paging_in_s_mode();
  set_reg(1, VADDR);

  TEST_ASSERT_TRAP(LR_W(2, 1), CAUSE_LOAD_PAGE_FAULT, VADDR);
}

static void test_sc_to_a_read_only_page_is_a_store_page_fault(void) {
  start_paging_in_s_mode();
  map_page(VADDR, DATA_PAGE, LEAF | PTE_R);
  set_reg(1, VADDR);
  execute_raw(LR_W(2, 1));

  TEST_ASSERT_TRAP(SC_W(3, 4, 1), CAUSE_STORE_PAGE_FAULT, VADDR);
}

static void test_amo_on_an_unmapped_page_is_a_store_page_fault(void) {
  start_paging_in_s_mode();
  set_reg(1, VADDR);

  TEST_ASSERT_TRAP(AMO_W(AMO_ADD, 2, 4, 1), CAUSE_STORE_PAGE_FAULT, VADDR);
}

// Readable is not enough: the AMO also writes.
static void test_amo_on_a_read_only_page_is_a_store_page_fault(void) {
  start_paging_in_s_mode();
  map_page(VADDR, DATA_PAGE, LEAF | PTE_R);
  set_reg(1, VADDR);

  TEST_ASSERT_TRAP(AMO_W(AMO_ADD, 2, 4, 1), CAUSE_STORE_PAGE_FAULT, VADDR);
}

// A misaligned access that crosses into the next virtual page reads or
// writes two pages, which need not be next to each other in physical
// memory: here the page after VADDR's maps to NEXT_PAGE, two pages further
// on. The physical page right after DATA_PAGE holds a marker that a straight
// physical access would hit instead.
#define PAGE_END (VADDR - PAGE_OFFSET + PAGE_SIZE)
#define ACROSS (PAGE_END - 2) // a word: two bytes in each page
#define NEXT_PAGE (RAM_BASE + 10 * PAGE_SIZE)
#define WRONG_PAGE (DATA_PAGE + PAGE_SIZE)
#define MARKER 0xEEEE

static void map_both_pages(uint64_t second_flags) {
  start_paging_in_s_mode();
  map_page(ACROSS, DATA_PAGE, LEAF | PTE_R | PTE_W);
  if (second_flags)
    map_page(PAGE_END, NEXT_PAGE, second_flags);
  bus_write(bus, WRONG_PAGE, 2, MARKER);
  set_reg(1, ACROSS);
}

static void test_load_across_pages_reads_both_pages(void) {
  map_both_pages(LEAF | PTE_R);
  bus_write(bus, DATA_PAGE + PAGE_SIZE - 2, 2, 0x1234);
  bus_write(bus, NEXT_PAGE, 2, 0x5678);

  execute_raw(LW(2, 0, 1));

  TEST_ASSERT_EQUAL_HEX64(0x56781234, get_reg(2));
}

static void test_store_across_pages_writes_both_pages(void) {
  map_both_pages(LEAF | PTE_R | PTE_W);
  set_reg(2, 0x56781234);

  execute_raw(SW(2, 0, 1));

  TEST_ASSERT_EQUAL_HEX32(0x1234,
                          read_physical(DATA_PAGE + PAGE_SIZE - 4) >> 16);
  TEST_ASSERT_EQUAL_HEX32(0x5678, read_physical(NEXT_PAGE) & 0xFFFF);
  TEST_ASSERT_EQUAL_HEX32(MARKER, read_physical(WRONG_PAGE) & 0xFFFF);
}

// When only the second part faults, mtval holds the address of that part:
// the start of the second page.
static void test_load_into_an_unmapped_second_page_faults_there(void) {
  map_both_pages(0);

  TEST_ASSERT_TRAP(LW(2, 0, 1), CAUSE_LOAD_PAGE_FAULT, PAGE_END);
}

// Both pages are translated before anything is written, so a store that
// faults on the second page leaves the first one as it was.
static void test_store_into_a_read_only_second_page_writes_nothing(void) {
  map_both_pages(LEAF | PTE_R);
  bus_write(bus, DATA_PAGE + PAGE_SIZE - 2, 2, 0xAAAA);
  set_reg(2, 0x56781234);

  TEST_ASSERT_TRAP(SW(2, 0, 1), CAUSE_STORE_PAGE_FAULT, PAGE_END);
  TEST_ASSERT_EQUAL_HEX32(0xAAAA,
                          read_physical(DATA_PAGE + PAGE_SIZE - 4) >> 16);
}

// The same for instruction fetch. An instruction in the last 2 bytes of a
// page is fetched from both pages only when it is 4 bytes long.
#define CODE_PAGE_END (CODE_VADDR + PAGE_SIZE)
#define NEXT_CODE_PAGE (RAM_BASE + 11 * PAGE_SIZE)
#define WRONG_CODE_PAGE (CODE_PAGE + PAGE_SIZE)

static void map_code_pages(bool second) {
  start_paging_in_s_mode();
  map_page(CODE_VADDR, CODE_PAGE, LEAF | PTE_X);
  if (second)
    map_page(CODE_PAGE_END, NEXT_CODE_PAGE, LEAF | PTE_X);
  bus_write(bus, WRONG_CODE_PAGE, 2, MARKER);
  cpu_set_pc(cpu, CODE_PAGE_END - 2);
}

static void test_compressed_instruction_at_end_of_page_is_fetched_alone(void) {
  map_code_pages(false);
  bus_write(bus, CODE_PAGE + PAGE_SIZE - 2, 2, 0x44c5); // c.li s1, 17

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(17, get_reg(9));
  TEST_ASSERT_EQUAL_HEX64(CODE_PAGE_END, cpu_get_pc(cpu));
}

static void test_instruction_across_pages_is_fetched_from_both_pages(void) {
  map_code_pages(true);
  bus_write(bus, CODE_PAGE + PAGE_SIZE - 2, 2, ADDI(5, 0, 42) & 0xFFFF);
  bus_write(bus, NEXT_CODE_PAGE, 2, ADDI(5, 0, 42) >> 16);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(42, get_reg(5));
  TEST_ASSERT_EQUAL_HEX64(CODE_PAGE_END + 2, cpu_get_pc(cpu));
}

// mepc points at the instruction, mtval at the page that is missing.
static void test_instruction_into_an_unmapped_second_page_faults_there(void) {
  map_code_pages(false);
  bus_write(bus, CODE_PAGE + PAGE_SIZE - 2, 2, ADDI(5, 0, 42) & 0xFFFF);

  cpu_step(cpu);

  TEST_ASSERT_EQUAL_HEX64(CAUSE_FETCH_PAGE_FAULT, read_csr(MCAUSE));
  TEST_ASSERT_EQUAL_HEX64(CODE_PAGE_END, read_csr(MTVAL));
  TEST_ASSERT_EQUAL_HEX64(CODE_PAGE_END - 2, read_csr(MEPC));
}

// Triggers match virtual addresses. A chain of an address trigger (VADDR)
// and a data trigger can only fire once the data is loaded, and by then the
// address it is given must still be the virtual one.
#define S_LOAD_TRIGGER                                                         \
  (TDATA1_TYPE(TRIGGER_TYPE_MCONTROL) | MCONTROL_S | MCONTROL_LOAD)

static void set_trigger(uxlen_t index, uxlen_t tdata1, uxlen_t tdata2) {
  write_csr(TSELECT, index);
  write_csr(TDATA1, 0);
  write_csr(TDATA2, tdata2);
  write_csr(TDATA1, tdata1);
}

static void test_load_data_trigger_chain_matches_the_virtual_address(void) {
  start_paging_in_s_mode();
  map_page(VADDR, DATA_PAGE, LEAF | PTE_R);
  bus_write(bus, DATA_PAGE + PAGE_OFFSET, 4, 0x12345678);
  set_trigger(0, S_LOAD_TRIGGER | MCONTROL_CHAIN, VADDR);
  set_trigger(1, S_LOAD_TRIGGER | MCONTROL_SELECT, 0x12345678);
  set_reg(1, VADDR);

  execute_raw(LW(2, 0, 1));

  TEST_ASSERT_EQUAL_HEX64(CAUSE_BREAKPOINT, read_csr(MCAUSE));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_load_reads_the_mapped_page);
  RUN_TEST(test_store_writes_the_mapped_page);
  RUN_TEST(test_load_from_an_unmapped_page_is_a_load_page_fault);
  RUN_TEST(test_store_to_a_read_only_page_is_a_store_page_fault);
  RUN_TEST(test_instruction_is_fetched_through_the_page_table);
  RUN_TEST(test_fetch_from_an_unmapped_page_is_an_instruction_page_fault);
  RUN_TEST(test_lr_reads_the_mapped_page);
  RUN_TEST(test_sc_writes_the_mapped_page);
  RUN_TEST(test_amo_updates_the_mapped_page);
  RUN_TEST(test_lr_from_an_unmapped_page_is_a_load_page_fault);
  RUN_TEST(test_sc_to_a_read_only_page_is_a_store_page_fault);
  RUN_TEST(test_amo_on_an_unmapped_page_is_a_store_page_fault);
  RUN_TEST(test_amo_on_a_read_only_page_is_a_store_page_fault);
  RUN_TEST(test_load_across_pages_reads_both_pages);
  RUN_TEST(test_store_across_pages_writes_both_pages);
  RUN_TEST(test_load_into_an_unmapped_second_page_faults_there);
  RUN_TEST(test_store_into_a_read_only_second_page_writes_nothing);
  RUN_TEST(test_compressed_instruction_at_end_of_page_is_fetched_alone);
  RUN_TEST(test_instruction_across_pages_is_fetched_from_both_pages);
  RUN_TEST(test_instruction_into_an_unmapped_second_page_faults_there);
  RUN_TEST(test_load_data_trigger_chain_matches_the_virtual_address);
  return UNITY_END();
}
