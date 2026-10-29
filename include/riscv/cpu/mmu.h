#ifndef RISCV_CPU_MMU_H
#define RISCV_CPU_MMU_H

#include "riscv/cpu/cpu.h"
#include "riscv/cpu/trap_cause.h"

enum MmuAccess { MMU_FETCH, MMU_LOAD, MMU_STORE };
enum MmuResult { MMU_OK, MMU_PAGE_FAULT, MMU_ACCESS_FAULT };

enum MmuResult mmu_translate(Cpu *cpu, uxlen_t vaddr, enum MmuAccess access,
                             uint64_t *paddr);

enum Trap mmu_fault_cause(enum MmuResult mmu_result,
                          enum MmuAccess access_type);

#endif // RISCV_CPU_MMU_H