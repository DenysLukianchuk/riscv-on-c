#ifndef TESTS_PAGE_TABLE_H
#define TESTS_PAGE_TABLE_H

// Builds Sv39 / Sv32 page tables in RAM through the bus. The including test
// defines the globals `bus` and `cpu`. Tables sit right after the root: the
// one below the root at ROOT_TABLE + 1 page, and so on.

#include "riscv/cpu/cpu.h"
#include "riscv/cpu/csr.h"
#include "riscv/system/board.h"
#include "riscv/system/bus.h"
#include "riscv_spec.h"

#define ROOT_TABLE RAM_BASE

// RAM is not zeroed, so a table starts out as garbage until cleared.
static inline void clear_page(uxlen_t addr) {
  for (uxlen_t i = 0; i < PAGE_SIZE; i++)
    bus_write(bus, addr + i, 1, 0);
}

static inline void enable_paging(uxlen_t root_table) {
  csr_write(cpu_get_csr(cpu), SATP, PRIV_M,
            SATP_PAGED | root_table / PAGE_SIZE);
}

static inline uxlen_t vpn(uxlen_t vaddr, int level) {
  return (vaddr >> (12 + level * VPN_BITS)) & ((1u << VPN_BITS) - 1);
}

static inline uint64_t pte(uxlen_t paddr, uint64_t flags) {
  return (uint64_t)(paddr / PAGE_SIZE) << PTE_PPN_SHIFT | flags;
}

static inline void write_pte(uxlen_t table, uxlen_t index, uint64_t entry) {
  bus_write(bus, table + index * PTE_SIZE, PTE_SIZE, entry);
}

// Puts a leaf with the given flags at leaf_level. A table below the root is
// created on first use and reused afterwards, so pages mapped in one test
// must share their VPNs above leaf_level.
static inline void map_at_level(uxlen_t vaddr, uxlen_t paddr, int leaf_level,
                                uint64_t flags) {
  uxlen_t table = ROOT_TABLE;
  for (int level = PT_LEVELS - 1; level > leaf_level; level--) {
    uxlen_t next = ROOT_TABLE + (PT_LEVELS - level) * PAGE_SIZE;
    uint64_t entry = 0;
    bus_read(bus, table + vpn(vaddr, level) * PTE_SIZE, PTE_SIZE, &entry);
    if (!(entry & PTE_V)) {
      clear_page(next);
      write_pte(table, vpn(vaddr, level), pte(next, PTE_V));
    }
    table = next;
  }
  write_pte(table, vpn(vaddr, leaf_level), pte(paddr, flags));
}

// Maps one 4 KiB page: the leaf is at level 0.
static inline void map_page(uxlen_t vaddr, uxlen_t paddr, uint64_t flags) {
  map_at_level(vaddr, paddr, 0, flags);
}

#endif // TESTS_PAGE_TABLE_H
