#include "riscv/cpu/cpu.h"
#include "riscv/cpu/csr.h"
#include "riscv/cpu/mmu.h"
#include "riscv/devices/memory.h"
#include "riscv/system/board.h"
#include "riscv/system/bus.h"
#include "riscv_spec.h"
#include "unity.h"

// Address translation: Sv39 on RV64, Sv32 on RV32. RAM holds the page tables
// as well as the pages they map.
#define MMU_RAM_SIZE (16 * PAGE_SIZE)
#define DATA_ADDR (RAM_BASE + 0x1234)

static Memory *mem;
static Bus *bus;
static Cpu *cpu;

void setUp(void) {
  mem = memory_create(MMU_RAM_SIZE);
  bus = bus_create();
  bus_attach(bus, memory_bus_device(mem, RAM_BASE));
  cpu = cpu_create(bus);
}

void tearDown(void) {
  cpu_destroy(cpu);
  bus_destroy(bus);
  memory_destroy(mem);
}

#include "page_table.h"

// Every VPN field is non-zero, so an index taken from the wrong bits misses:
// Sv39 VPN[2..0] = 1, 2, 3; Sv32 VPN[1..0] = 0x101, 3.
#define VADDR 0x40403678
#define PAGE_OFFSET 0x678
#define DATA_PAGE (RAM_BASE + 8 * PAGE_SIZE)

static void test_bare_mode_leaves_the_address_unchanged(void) {
  cpu_set_privilege(cpu, PRIV_S);
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_OK, mmu_translate(cpu, DATA_ADDR, MMU_LOAD, &paddr));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, paddr);
}

// M-mode never translates its own accesses. The root table is empty, so a
// walk would end in a page fault.
static void test_machine_mode_is_not_translated(void) {
  clear_page(ROOT_TABLE);
  enable_paging(ROOT_TABLE);
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_OK, mmu_translate(cpu, DATA_ADDR, MMU_LOAD, &paddr));
  TEST_ASSERT_EQUAL_HEX64(DATA_ADDR, paddr);
}

// With V = 0 in the root entry the walk stops at once.
static void test_invalid_entry_is_a_page_fault(void) {
  clear_page(ROOT_TABLE);
  enable_paging(ROOT_TABLE);
  cpu_set_privilege(cpu, PRIV_S);
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    mmu_translate(cpu, VADDR, MMU_LOAD, &paddr));
}

// The walk goes root -> ... -> leaf; the leaf's PPN plus the page offset is
// the physical address.
static void test_page_is_translated_through_every_level(void) {
  clear_page(ROOT_TABLE);
  map_page(VADDR, DATA_PAGE, PTE_V | PTE_R | PTE_W | PTE_A | PTE_D);
  enable_paging(ROOT_TABLE);
  cpu_set_privilege(cpu, PRIV_S);
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_OK, mmu_translate(cpu, VADDR, MMU_LOAD, &paddr));
  TEST_ASSERT_EQUAL_HEX64(DATA_PAGE + PAGE_OFFSET, paddr);
}

// Fills a page with valid leaves, so a walk that wrongly reads it as a table
// finds one instead of failing by luck.
static void fill_with_leaves(uxlen_t page) {
  for (uxlen_t i = 0; i < PAGE_SIZE / PTE_SIZE; i++)
    write_pte(page, i,
              pte(page, PTE_V | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D));
}

// A pointer (R = X = 0) at the last level has no table below it to go to.
static void test_pointer_at_the_last_level_is_a_page_fault(void) {
  clear_page(ROOT_TABLE);
  map_page(VADDR, DATA_PAGE, PTE_V);
  fill_with_leaves(DATA_PAGE);
  enable_paging(ROOT_TABLE);
  cpu_set_privilege(cpu, PRIV_S);
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    mmu_translate(cpu, VADDR, MMU_LOAD, &paddr));
}

// Permissions. A leaf's R, W and X allow loads, stores and instruction
// fetches; each access needs its own bit. A and D are set throughout so that
// only the permission bits decide.

#define LEAF (PTE_V | PTE_A | PTE_D)

// Maps VADDR to DATA_PAGE with the given flags and translates it from the
// given mode.
static enum MmuResult access_page_as(enum Privilege privilege, uint64_t flags,
                                     enum MmuAccess access) {
  clear_page(ROOT_TABLE);
  map_page(VADDR, DATA_PAGE, flags);
  enable_paging(ROOT_TABLE);
  cpu_set_privilege(cpu, privilege);
  uint64_t paddr = 0;
  return mmu_translate(cpu, VADDR, access, &paddr);
}

static enum MmuResult access_page(uint64_t flags, enum MmuAccess access) {
  return access_page_as(PRIV_S, flags, access);
}

static void test_each_access_is_allowed_by_its_permission(void) {
  TEST_ASSERT_EQUAL(MMU_OK, access_page(LEAF | PTE_R, MMU_LOAD));
  TEST_ASSERT_EQUAL(MMU_OK, access_page(LEAF | PTE_R | PTE_W, MMU_STORE));
  TEST_ASSERT_EQUAL(MMU_OK, access_page(LEAF | PTE_X, MMU_FETCH));
}

static void test_load_needs_read_permission(void) {
  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT, access_page(LEAF | PTE_X, MMU_LOAD));
}

static void test_store_needs_write_permission(void) {
  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT, access_page(LEAF | PTE_R, MMU_STORE));
  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    access_page(LEAF | PTE_R | PTE_X, MMU_STORE));
}

static void test_fetch_needs_execute_permission(void) {
  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    access_page(LEAF | PTE_R | PTE_W, MMU_FETCH));
}

// W = 1 with R = 0 is reserved: the walk stops with a page fault at any
// level. Here it sits in the root entry, and the page it names is full of
// leaves, so reading it as a pointer would find one.
static void test_write_without_read_is_reserved(void) {
  clear_page(ROOT_TABLE);
  write_pte(ROOT_TABLE, vpn(VADDR, PT_LEVELS - 1),
            pte(DATA_PAGE, PTE_V | PTE_W));
  fill_with_leaves(DATA_PAGE);
  enable_paging(ROOT_TABLE);
  cpu_set_privilege(cpu, PRIV_S);
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    mmu_translate(cpu, VADDR, MMU_STORE, &paddr));
}

// The U bit marks a page as user memory. U-mode may only touch user pages;
// S-mode may not touch them unless mstatus.SUM is set, and even then never
// executes from them.

static void set_mstatus(uxlen_t value) {
  csr_write(cpu_get_csr(cpu), MSTATUS, PRIV_M, value);
}

static void test_user_mode_accesses_user_pages(void) {
  TEST_ASSERT_EQUAL(MMU_OK,
                    access_page_as(PRIV_U, LEAF | PTE_R | PTE_U, MMU_LOAD));
}

static void test_user_mode_cannot_access_supervisor_pages(void) {
  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    access_page_as(PRIV_U, LEAF | PTE_R, MMU_LOAD));
  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    access_page_as(PRIV_U, LEAF | PTE_X, MMU_FETCH));
}

static void test_supervisor_cannot_access_user_pages_by_default(void) {
  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    access_page(LEAF | PTE_R | PTE_U, MMU_LOAD));
  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    access_page(LEAF | PTE_R | PTE_W | PTE_U, MMU_STORE));
}

static void test_sum_lets_supervisor_load_and_store_user_pages(void) {
  set_mstatus(MSTATUS_SUM);

  TEST_ASSERT_EQUAL(MMU_OK, access_page(LEAF | PTE_R | PTE_U, MMU_LOAD));
  TEST_ASSERT_EQUAL(MMU_OK,
                    access_page(LEAF | PTE_R | PTE_W | PTE_U, MMU_STORE));
}

static void test_supervisor_never_executes_user_pages(void) {
  set_mstatus(MSTATUS_SUM);

  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    access_page(LEAF | PTE_X | PTE_U, MMU_FETCH));
}

// MXR (make executable readable): a load may use X in place of R.
static void test_mxr_makes_executable_pages_readable(void) {
  set_mstatus(MSTATUS_MXR);

  TEST_ASSERT_EQUAL(MMU_OK, access_page(LEAF | PTE_X, MMU_LOAD));
}

// A and D (Svade). A says the page has been accessed, D that it has been
// written. This hart does not set them itself: an access to a page with
// A = 0, or a store to a page with D = 0, is a page fault, and the OS sets
// the bit in its handler.

static void test_access_to_a_page_without_a_is_a_page_fault(void) {
  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    access_page(PTE_V | PTE_R | PTE_D, MMU_LOAD));
  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    access_page(PTE_V | PTE_R | PTE_W | PTE_D, MMU_STORE));
  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    access_page(PTE_V | PTE_X | PTE_D, MMU_FETCH));
}

static void test_store_to_a_page_without_d_is_a_page_fault(void) {
  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    access_page(PTE_V | PTE_R | PTE_W | PTE_A, MMU_STORE));
}

// D only matters to stores.
static void test_load_and_fetch_need_no_d(void) {
  TEST_ASSERT_EQUAL(MMU_OK, access_page(PTE_V | PTE_R | PTE_A, MMU_LOAD));
  TEST_ASSERT_EQUAL(MMU_OK, access_page(PTE_V | PTE_X | PTE_A, MMU_FETCH));
}

// Superpages. A leaf above level 0 maps a whole aligned region: 2 MiB at
// level 1 and 1 GiB at level 2 in Sv39, 4 MiB at level 1 in Sv32. The VPN
// fields below the leaf's level pass through to the physical address like
// the page offset does.

// Translates VADDR from S-mode through a superpage leaf at the given level.
static enum MmuResult access_superpage(uxlen_t paddr, int level,
                                       uint64_t *translated) {
  clear_page(ROOT_TABLE);
  map_at_level(VADDR, paddr, level, LEAF | PTE_R);
  enable_paging(ROOT_TABLE);
  cpu_set_privilege(cpu, PRIV_S);
  return mmu_translate(cpu, VADDR, MMU_LOAD, translated);
}

// Aligned for 2 MiB and 4 MiB superpages; VADDR is 0x3678 into either.
#define SUPERPAGE 0x80400000
#define VADDR_IN_SUPERPAGE 0x3678

static void test_superpage_keeps_the_lower_vpn_of_the_address(void) {
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_OK, access_superpage(SUPERPAGE, 1, &paddr));
  TEST_ASSERT_EQUAL_HEX64(SUPERPAGE + VADDR_IN_SUPERPAGE, paddr);
}

// The leaf's PPN bits below its level must be 0, or the superpage is
// misaligned: a page fault.
static void test_misaligned_superpage_is_a_page_fault(void) {
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    access_superpage(SUPERPAGE + PAGE_SIZE, 1, &paddr));
}

#if XLEN == 64
// A 1 GiB gigapage at the root; VADDR is 0x403678 into it.
#define GIGAPAGE 0x80000000
#define VADDR_IN_GIGAPAGE 0x403678

static void test_gigapage_keeps_both_lower_vpns_of_the_address(void) {
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_OK, access_superpage(GIGAPAGE, 2, &paddr));
  TEST_ASSERT_EQUAL_HEX64(GIGAPAGE + VADDR_IN_GIGAPAGE, paddr);
}

static void test_misaligned_gigapage_is_a_page_fault(void) {
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    access_superpage(GIGAPAGE + 0x200000, 2, &paddr));
}
#endif

// A page table the bus cannot read is an access fault, not a page fault:
// the walk itself failed. satp points at physical page 0, where there is
// no memory.
static void test_unreadable_page_table_is_an_access_fault(void) {
  enable_paging(0);
  cpu_set_privilege(cpu, PRIV_S);
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_ACCESS_FAULT,
                    mmu_translate(cpu, VADDR, MMU_LOAD, &paddr));
}

#if XLEN == 64
// Sv39 uses only the low 39 bits, and bits 63..39 must all equal bit 38.
// Addresses are either low (bit 38 = 0, upper bits 0) or high (bit 38 = 1,
// upper bits 1); anything else is a page fault.

// VADDR with bit 39 set: the same VPNs, but bits 63..39 no longer copy bit 38.
#define NON_CANONICAL (VADDR | (uxlen_t)1 << 39)
// VADDR with bits 63..38 set: VPN[2] = 0x101, VPN[1] and VPN[0] unchanged.
#define HIGH_VADDR (VADDR | (uxlen_t)0xFFFFFFC0 << 32)

static void test_non_canonical_address_is_a_page_fault(void) {
  clear_page(ROOT_TABLE);
  map_page(NON_CANONICAL, DATA_PAGE, LEAF | PTE_R);
  enable_paging(ROOT_TABLE);
  cpu_set_privilege(cpu, PRIV_S);
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    mmu_translate(cpu, NON_CANONICAL, MMU_LOAD, &paddr));
}

// The top of the address space, where Linux keeps the kernel.
static void test_high_canonical_address_is_translated(void) {
  clear_page(ROOT_TABLE);
  map_page(HIGH_VADDR, DATA_PAGE, LEAF | PTE_R);
  enable_paging(ROOT_TABLE);
  cpu_set_privilege(cpu, PRIV_S);
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_OK, mmu_translate(cpu, HIGH_VADDR, MMU_LOAD, &paddr));
  TEST_ASSERT_EQUAL_HEX64(DATA_PAGE + PAGE_OFFSET, paddr);
}

// Sv39 PTE bits 63..54: N (63, Svnapot), PBMT (62..61, Svpbmt) and reserved
// bits (60..54). None of these extensions is implemented, so any of them set
// is a page fault.
static void test_reserved_pte_bits_are_a_page_fault(void) {
  const unsigned bits[] = {54, 60, 61, 62, 63};
  for (unsigned i = 0; i < sizeof bits / sizeof bits[0]; i++)
    TEST_ASSERT_EQUAL_MESSAGE(
        MMU_PAGE_FAULT,
        access_page(LEAF | PTE_R | (uint64_t)1 << bits[i], MMU_LOAD),
        "reserved PTE bit accepted");
}
#endif

// MPRV: with mstatus.MPRV set, M-mode loads and stores are translated and
// checked as if they ran in the mode held in MPP. Fetches are not affected.
// OpenSBI uses this to read S-mode memory on the kernel's behalf.

static void set_mprv_with_mpp(uxlen_t mpp) { set_mstatus(MSTATUS_MPRV | mpp); }

// Paging on, but the hart stays in M-mode.
static void map_for_machine_mode(uint64_t flags) {
  clear_page(ROOT_TABLE);
  map_page(VADDR, DATA_PAGE, flags);
  enable_paging(ROOT_TABLE);
}

static void test_mprv_translates_machine_loads_as_mpp(void) {
  set_mprv_with_mpp(MSTATUS_MPP_S);
  map_for_machine_mode(LEAF | PTE_R);
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_OK, mmu_translate(cpu, VADDR, MMU_LOAD, &paddr));
  TEST_ASSERT_EQUAL_HEX64(DATA_PAGE + PAGE_OFFSET, paddr);
}

static void test_mprv_checks_stores_as_mpp(void) {
  set_mprv_with_mpp(MSTATUS_MPP_S);

  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    access_page_as(PRIV_M, LEAF | PTE_R, MMU_STORE));
}

// MPP = U: the page must have U set, as for a real U-mode access.
static void test_mprv_with_mpp_user_needs_a_user_page(void) {
  set_mprv_with_mpp(MSTATUS_MPP_U);

  TEST_ASSERT_EQUAL(MMU_PAGE_FAULT,
                    access_page_as(PRIV_M, LEAF | PTE_R, MMU_LOAD));
  TEST_ASSERT_EQUAL(MMU_OK,
                    access_page_as(PRIV_M, LEAF | PTE_R | PTE_U, MMU_LOAD));
}

// The root table is empty, so a translated access would fault.
static void test_mprv_leaves_fetches_untranslated(void) {
  set_mprv_with_mpp(MSTATUS_MPP_S);
  clear_page(ROOT_TABLE);
  enable_paging(ROOT_TABLE);
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_OK, mmu_translate(cpu, VADDR, MMU_FETCH, &paddr));
  TEST_ASSERT_EQUAL_HEX64(VADDR, paddr);
}

static void test_mprv_with_mpp_machine_is_not_translated(void) {
  set_mprv_with_mpp(MSTATUS_MPP_M);
  clear_page(ROOT_TABLE);
  enable_paging(ROOT_TABLE);
  uint64_t paddr = 0;

  TEST_ASSERT_EQUAL(MMU_OK, mmu_translate(cpu, VADDR, MMU_LOAD, &paddr));
  TEST_ASSERT_EQUAL_HEX64(VADDR, paddr);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_bare_mode_leaves_the_address_unchanged);
  RUN_TEST(test_machine_mode_is_not_translated);
  RUN_TEST(test_invalid_entry_is_a_page_fault);
  RUN_TEST(test_page_is_translated_through_every_level);
  RUN_TEST(test_pointer_at_the_last_level_is_a_page_fault);
  RUN_TEST(test_each_access_is_allowed_by_its_permission);
  RUN_TEST(test_load_needs_read_permission);
  RUN_TEST(test_store_needs_write_permission);
  RUN_TEST(test_fetch_needs_execute_permission);
  RUN_TEST(test_write_without_read_is_reserved);
  RUN_TEST(test_user_mode_accesses_user_pages);
  RUN_TEST(test_user_mode_cannot_access_supervisor_pages);
  RUN_TEST(test_supervisor_cannot_access_user_pages_by_default);
  RUN_TEST(test_sum_lets_supervisor_load_and_store_user_pages);
  RUN_TEST(test_supervisor_never_executes_user_pages);
  RUN_TEST(test_mxr_makes_executable_pages_readable);
  RUN_TEST(test_access_to_a_page_without_a_is_a_page_fault);
  RUN_TEST(test_store_to_a_page_without_d_is_a_page_fault);
  RUN_TEST(test_load_and_fetch_need_no_d);
  RUN_TEST(test_unreadable_page_table_is_an_access_fault);
  RUN_TEST(test_superpage_keeps_the_lower_vpn_of_the_address);
  RUN_TEST(test_misaligned_superpage_is_a_page_fault);
#if XLEN == 64
  RUN_TEST(test_gigapage_keeps_both_lower_vpns_of_the_address);
  RUN_TEST(test_misaligned_gigapage_is_a_page_fault);
  RUN_TEST(test_non_canonical_address_is_a_page_fault);
  RUN_TEST(test_high_canonical_address_is_translated);
  RUN_TEST(test_reserved_pte_bits_are_a_page_fault);
#endif
  RUN_TEST(test_mprv_translates_machine_loads_as_mpp);
  RUN_TEST(test_mprv_checks_stores_as_mpp);
  RUN_TEST(test_mprv_with_mpp_user_needs_a_user_page);
  RUN_TEST(test_mprv_leaves_fetches_untranslated);
  RUN_TEST(test_mprv_with_mpp_machine_is_not_translated);
  return UNITY_END();
}
