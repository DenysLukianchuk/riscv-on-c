#include "riscv/cpu/mmu.h"
#include "riscv/util/bits.h"
#include <assert.h>

#if XLEN == 64
#define PT_LEVELS 3
#define PTE_SIZE 8
#define VPN_BITS 9
#define VA_BITS 39
#define SATP_PPN_SIZE 44
#else
#define PT_LEVELS 2
#define PTE_SIZE 4
#define VPN_BITS 10
#define VA_BITS 32
#define SATP_PPN_SIZE 22
#endif
#if XLEN == 64
// Sv39 PTE bits 63..54: N, PBMT and reserved bits, none of them implemented.
#define PTE_RESERVED_HIGH 63
#define PTE_RESERVED_LOW 54
#endif
#define PTE_PPN_SHIFT 10
#define PAGE_SHIFT 12
#define PAGE_SIZE (1 << PAGE_SHIFT)
#define PAGE_OFFSET_MASK (PAGE_SIZE - 1)
#define PTE_V (1u << 0)
#define PTE_R (1u << 1)
#define PTE_W (1u << 2)
#define PTE_X (1u << 3)
#define PTE_U (1u << 4)
#define PTE_G (1u << 5)
#define PTE_A (1u << 6)
#define PTE_D (1u << 7)

struct Access {
  enum MmuAccess type;
  enum Privilege priv;
  bool sum, mxr;
};

static uint64_t vpn(uxlen_t vaddr, int level) {
  return (vaddr >> (PAGE_SHIFT + level * VPN_BITS)) & (low_mask(VPN_BITS));
}

static uint64_t pte_address(uint64_t pte) {
  return (pte >> PTE_PPN_SHIFT) * PAGE_SIZE;
}

// Bits XLEN-1 down to VA_BITS-1 must all be equal: all 0 or all 1.
static bool vaddr_is_canonical(uxlen_t vaddr) {
#if XLEN == 64
  uxlen_t sign = bits(vaddr, XLEN - 1, VA_BITS - 1);
  return sign == 0 || sign == low_mask(XLEN - VA_BITS + 1);
#else
  (void)vaddr;
  return true;
#endif
}

static bool pte_has_reserved_bits(uint64_t pte) {
#if XLEN == 64
  return bits(pte, PTE_RESERVED_HIGH, PTE_RESERVED_LOW) != 0;
#else
  (void)pte;
  return false;
#endif
}

static bool pte_is_valid(uint64_t pte) {
  return (pte & PTE_V) && !((pte & PTE_W) && !(pte & PTE_R)) &&
         !pte_has_reserved_bits(pte);
}

static bool pte_is_leaf(uint64_t pte) { return (pte & (PTE_R | PTE_X)); }

static bool accessed_and_dirty(uint64_t pte, enum MmuAccess access) {
  if (!(pte & PTE_A))
    return false;
  if (access == MMU_STORE)
    return pte & PTE_D;
  return true;
}

static bool pte_allows(uint64_t pte, const struct Access *access_info) {
  switch (access_info->type) {
  case MMU_LOAD:
    return (pte & PTE_R) || (access_info->mxr && pte & PTE_X);
  case MMU_STORE:
    return (pte & PTE_W);
  case MMU_FETCH:
    return (pte & PTE_X);
  }
  return false;
}

static bool privilege_allows(uint64_t pte, const struct Access *access_info) {
  if ((pte & PTE_U) == 0 && access_info->priv == PRIV_U)
    return false;
  if ((pte & PTE_U) && access_info->priv == PRIV_S) {
    if (access_info->type == MMU_FETCH || !access_info->sum)
      return false;
  }
  return true;
}

static bool leaf_allows(uint64_t pte, const struct Access *access_info) {
  return accessed_and_dirty(pte, access_info->type) &&
         privilege_allows(pte, access_info) && pte_allows(pte, access_info);
}

static enum MmuResult translate_leaf(uint64_t pte, int level, uint64_t vaddr,
                                     const struct Access *access_info,
                                     uint64_t *paddr) {
  int offset_bits = PAGE_SHIFT + level * VPN_BITS;
  if (!leaf_allows(pte, access_info))
    return MMU_PAGE_FAULT;
  uint64_t page = pte_address(pte);
  if (page & low_mask(offset_bits))
    return MMU_PAGE_FAULT;
  *paddr = page + (vaddr & low_mask(offset_bits));
  return MMU_OK;
}

static enum Privilege effective_privilege(Cpu *cpu, uxlen_t mstatus,
                                          enum MmuAccess access) {
  enum Privilege priv = cpu_get_privilege(cpu);
  if (access != MMU_FETCH && (mstatus & MSTATUS_MPRV_BIT))
    priv = mstatus_mpp(mstatus);
  return priv;
}

static enum MmuResult walk(Cpu *cpu, uxlen_t satp, uxlen_t vaddr,
                           const struct Access *access_info, uint64_t *paddr) {
  uint64_t ppn = satp & low_mask(SATP_PPN_SIZE);
  uint64_t table = ppn * PAGE_SIZE;
  for (int level = PT_LEVELS - 1; level >= 0; level--) {
    uint64_t index = vpn(vaddr, level);
    uint64_t pte_addr = table + index * PTE_SIZE;
    uint64_t pte;
    if (bus_read(cpu_get_bus(cpu), pte_addr, PTE_SIZE, &pte) != BUS_OK)
      return MMU_ACCESS_FAULT;
    if (!pte_is_valid(pte))
      return MMU_PAGE_FAULT;
    if (pte_is_leaf(pte))
      return translate_leaf(pte, level, vaddr, access_info, paddr);
    table = pte_address(pte);
  }
  return MMU_PAGE_FAULT;
}

enum MmuResult mmu_translate(Cpu *cpu, uxlen_t vaddr, enum MmuAccess access,
                             uint64_t *paddr) {
  uxlen_t mstatus;
  csr_read(cpu_get_csr(cpu), CSR_REG_MSTATUS, PRIV_M, &mstatus);
  enum Privilege priv = effective_privilege(cpu, mstatus, access);
  uxlen_t satp;
  csr_read(cpu_get_csr(cpu), CSR_REG_SATP, PRIV_M, &satp);
  uxlen_t mode = satp_mode(satp);
  if (priv == PRIV_M || mode == SATP_MODE_BARE) {
    *paddr = vaddr;
    return MMU_OK;
  }

  if (!vaddr_is_canonical(vaddr))
    return MMU_PAGE_FAULT;

  struct Access access_info = {
      .type = access,
      .priv = priv,
      .sum = mstatus & SSTATUS_SUM_BIT,
      .mxr = mstatus & SSTATUS_MXR_BIT,
  };
  return walk(cpu, satp, vaddr, &access_info, paddr);
}

enum Trap mmu_fault_cause(enum MmuResult mmu_result,
                          enum MmuAccess access_type) {
  switch (mmu_result) {
  case MMU_PAGE_FAULT:
    switch (access_type) {
    case MMU_LOAD:
      return TRAP_LOAD_PAGE_FAULT;
    case MMU_STORE:
      return TRAP_STORE_PAGE_FAULT;
    case MMU_FETCH:
      return TRAP_INSTRUCTION_PAGE_FAULT;
    }
    break;
  case MMU_ACCESS_FAULT:
    switch (access_type) {
    case MMU_LOAD:
      return TRAP_LOAD_ACCESS_FAULT;
    case MMU_STORE:
      return TRAP_STORE_ACCESS_FAULT;
    case MMU_FETCH:
      return TRAP_INSTRUCTION_ACCESS_FAULT;
    }
    break;
  case MMU_OK:
    break;
  }
  assert(0 && "Unhandled MMU result");
  return TRAP_ILLEGAL_INSTRUCTION; // unreachable; keeps release builds defined
}

unsigned mmu_bytes_to_page_end(uxlen_t vaddr) {
  return PAGE_SIZE - (vaddr & PAGE_OFFSET_MASK);
}
