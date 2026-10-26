#include "riscv/csr.h"
#include "riscv/bits.h"
#include "riscv/trap_cause.h"
#include <stdlib.h>

enum Counteren { COUNTEREN_CY = 1u << 0, COUNTEREN_IR = 1u << 2 };

struct ModeRegs {
  uxlen_t tvec, scratch, epc, cause, tval;
  uint32_t counteren;
};

#define SSIP (1u << 1)
#define STIP (1u << 5)
#define SEIP (1u << 9)
#define MIDELEG_WRITABLE (SSIP | STIP | SEIP)

#define CAUSE_BIT(cause) ((uxlen_t)1 << (cause))
#define MEDELEG_WRITABLE                                                       \
  (CAUSE_BIT(TRAP_INSTRUCTION_ADDRESS_MISALIGNED) |                            \
   CAUSE_BIT(TRAP_INSTRUCTION_ACCESS_FAULT) |                                  \
   CAUSE_BIT(TRAP_ILLEGAL_INSTRUCTION) | CAUSE_BIT(TRAP_BREAKPOINT) |          \
   CAUSE_BIT(TRAP_LOAD_ADDRESS_MISALIGNED) |                                   \
   CAUSE_BIT(TRAP_LOAD_ACCESS_FAULT) |                                         \
   CAUSE_BIT(TRAP_STORE_ADDRESS_MISALIGNED) |                                  \
   CAUSE_BIT(TRAP_STORE_ACCESS_FAULT) | CAUSE_BIT(TRAP_ECALL_FROM_U) |         \
   CAUSE_BIT(TRAP_ECALL_FROM_S) | CAUSE_BIT(TRAP_INSTRUCTION_PAGE_FAULT) |     \
   CAUSE_BIT(TRAP_LOAD_PAGE_FAULT) | CAUSE_BIT(TRAP_STORE_PAGE_FAULT))

#if XLEN == 64
#define MSTATUS_UXL_MASK ((uxlen_t)3 << 32)
#define MSTATUS_SXL_MASK ((uxlen_t)3 << 34)
#define MSTATUS_UXL_64 ((uxlen_t)2 << 32)
#define MSTATUS_SXL_64 ((uxlen_t)2 << 34)
#endif

struct Csr {
  struct ModeRegs m, s;
  uxlen_t mhartid;
  uxlen_t mstatus;
  uxlen_t mie;
  uxlen_t misa;
  uxlen_t mvendorid;
  uxlen_t marchid;
  uxlen_t mimpid;
  uint64_t minstret;
  uint64_t mcycle;
  uxlen_t medeleg;
  uxlen_t mideleg;
  bool minstret_written;
};

static void mode_regs_init(struct ModeRegs *regs) {
  regs->tvec = 0;
  regs->scratch = 0;
  regs->epc = 0;
  regs->cause = 0;
  regs->tval = 0;
  regs->counteren = 0;
}

Csr *csr_create(void) {
  Csr *csr = (Csr *)malloc(sizeof(Csr));
  csr->mhartid = 0;
  mode_regs_init(&csr->m);
  mode_regs_init(&csr->s);
#if XLEN == 64
  csr->mstatus = MSTATUS_UXL_64 | MSTATUS_SXL_64;
#else
  csr->mstatus = 0;
#endif
  csr->mie = 0;
  csr->misa = MISA_MXL | MISA_I | MISA_M | MISA_U | MISA_S;
  csr->mvendorid = 0;
  csr->marchid = 0;
  csr->mimpid = 0;
  csr->minstret = 0;
  csr->mcycle = 0;
  csr->minstret_written = false;
  csr->medeleg = 0;
  csr->mideleg = 0;
  return csr;
}

static struct ModeRegs *mode_regs(Csr *csr, uint16_t addr) {
  return bits(addr, 9, 8) == PRIV_M ? &csr->m : &csr->s;
}

static inline bool counteren_accessible(Csr *csr, enum Counteren counter,
                                        enum Privilege privilege) {
  uxlen_t men = mode_regs(csr, CSR_REG_MCOUNTEREN)->counteren;
  uxlen_t sen = mode_regs(csr, CSR_REG_SCOUNTEREN)->counteren;

  switch (privilege) {
  case PRIV_M:
    return true;
  case PRIV_S:
    return men & counter;
  case PRIV_U:
    return men & sen & counter;
  }
  return false;
}

static inline bool csr_is_accessible(Csr *csr, uint16_t addr,
                                     enum Privilege privilege) {
  if (privilege < bits(addr, 9, 8))
    return false;
  switch (addr) {
  case CSR_REG_CYCLE:
#if XLEN == 32
  case CSR_REG_CYCLEH:
#endif
    return counteren_accessible(csr, COUNTEREN_CY, privilege);
  case CSR_REG_INSTRET:
#if XLEN == 32
  case CSR_REG_INSTRETH:
#endif
    return counteren_accessible(csr, COUNTEREN_IR, privilege);
  }
  return true;
}

enum CsrResult csr_read(Csr *csr, uint16_t addr, enum Privilege privilege,
                        uxlen_t *value) {
  if (!csr_is_accessible(csr, addr, privilege))
    return CSR_ILLEGAL;
  switch (addr) {
  case CSR_REG_MSCRATCH:
  case CSR_REG_SSCRATCH:
    *value = mode_regs(csr, addr)->scratch;
    return CSR_OK;
  case CSR_REG_MHARTID:
    *value = csr->mhartid;
    return CSR_OK;
  case CSR_REG_MEPC:
  case CSR_REG_SEPC:
    *value = mode_regs(csr, addr)->epc;
    return CSR_OK;
  case CSR_REG_MCAUSE:
  case CSR_REG_SCAUSE:
    *value = mode_regs(csr, addr)->cause;
    return CSR_OK;
  case CSR_REG_MTVAL:
  case CSR_REG_STVAL:
    *value = mode_regs(csr, addr)->tval;
    return CSR_OK;
  case CSR_REG_MTVEC:
  case CSR_REG_STVEC:
    *value = mode_regs(csr, addr)->tvec;
    return CSR_OK;
  case CSR_REG_MSTATUS:
    *value = csr->mstatus;
    return CSR_OK;
  case CSR_REG_SSTATUS:
#if XLEN == 64
    *value = csr->mstatus & (SSTATUS_MASK | MSTATUS_UXL_MASK);
#else
    *value = csr->mstatus & (SSTATUS_MASK);
#endif
    return CSR_OK;
  case CSR_REG_MIE:
    *value = csr->mie;
    return CSR_OK;
  case CSR_REG_MISA:
    *value = csr->misa;
    return CSR_OK;
  case CSR_REG_MVENDORID:
    *value = csr->mvendorid;
    return CSR_OK;
  case CSR_REG_MARCHID:
    *value = csr->marchid;
    return CSR_OK;
  case CSR_REG_MIMPID:
    *value = csr->mimpid;
    return CSR_OK;
  case CSR_REG_MINSTRET:
  case CSR_REG_INSTRET:
    *value = (uxlen_t)csr->minstret;
    return CSR_OK;
#if XLEN == 32
  case CSR_REG_MINSTRETH:
  case CSR_REG_INSTRETH:
    *value = (uxlen_t)(csr->minstret >> 32);
    return CSR_OK;
#endif
  case CSR_REG_MCYCLE:
  case CSR_REG_CYCLE:
    *value = (uxlen_t)csr->mcycle;
    return CSR_OK;
#if XLEN == 32
  case CSR_REG_MCYCLEH:
  case CSR_REG_CYCLEH:
    *value = (uxlen_t)(csr->mcycle >> 32);
    return CSR_OK;
#endif
  case CSR_REG_MCOUNTEREN:
  case CSR_REG_SCOUNTEREN:
    *value = mode_regs(csr, addr)->counteren;
    return CSR_OK;
  case CSR_REG_MEDELEG:
    *value = csr->medeleg;
    return CSR_OK;
  case CSR_REG_MIDELEG:
    *value = csr->mideleg;
    return CSR_OK;
  default:
    return CSR_ILLEGAL;
  }
}

static inline bool csr_is_read_only(uint16_t addr) {
  return bits(addr, 11, 10) == 0x3;
}

// Counters are 64-bit on every XLEN; on RV32 a CSR write changes one half.
static void write_low_half(uint64_t *counter, uxlen_t value) {
#if XLEN == 32
  *counter = (*counter & 0xFFFFFFFF00000000) | value;
#else
  *counter = value;
#endif
}

#if XLEN == 32
static void write_high_half(uint64_t *counter, uxlen_t value) {
  *counter = (*counter & 0xFFFFFFFF) | ((uint64_t)value << 32);
}
#endif

static uxlen_t legalize_tvec(uxlen_t old_value, uxlen_t value) {
  uxlen_t mode = bits(value, 1, 0);
  if (mode > 1)
    mode = bits(old_value, 1, 0);
  return clear_bits(value, 1, 0) | mode;
}

static uxlen_t legalize_status(uxlen_t old_value, uxlen_t value,
                               uint16_t addr) {
#if XLEN == 64
  value &= ~(MSTATUS_UXL_MASK | MSTATUS_SXL_MASK);
  value |= (old_value & (MSTATUS_UXL_MASK | MSTATUS_SXL_MASK));
#endif
  if (addr == CSR_REG_MSTATUS)
    return value;
  else
    return (old_value & ~SSTATUS_MASK) | (value & SSTATUS_MASK);
}

enum CsrResult csr_write(Csr *csr, uint16_t addr, enum Privilege privilege,
                         uxlen_t value) {
  if (!csr_is_accessible(csr, addr, privilege) || csr_is_read_only(addr))
    return CSR_ILLEGAL;
  switch (addr) {
  case CSR_REG_MSCRATCH:
  case CSR_REG_SSCRATCH:
    mode_regs(csr, addr)->scratch = value;
    return CSR_OK;
  case CSR_REG_MEPC:
  case CSR_REG_SEPC:
    mode_regs(csr, addr)->epc = clear_bits(value, 0, 0);
    return CSR_OK;
  case CSR_REG_MCAUSE:
  case CSR_REG_SCAUSE:
    mode_regs(csr, addr)->cause = value;
    return CSR_OK;
  case CSR_REG_MTVAL:
  case CSR_REG_STVAL:
    mode_regs(csr, addr)->tval = value;
    return CSR_OK;
  case CSR_REG_MTVEC:
  case CSR_REG_STVEC: {
    struct ModeRegs *regs = mode_regs(csr, addr);
    regs->tvec = legalize_tvec(regs->tvec, value);
    return CSR_OK;
  }
  case CSR_REG_MSTATUS:
  case CSR_REG_SSTATUS:
    csr->mstatus = legalize_status(csr->mstatus, value, addr);
    return CSR_OK;
  case CSR_REG_MIE:
    csr->mie = value;
    return CSR_OK;
  case CSR_REG_MCYCLE:
    write_low_half(&csr->mcycle, value);
    return CSR_OK;
  case CSR_REG_MINSTRET:
    write_low_half(&csr->minstret, value);
    csr->minstret_written = true;
    return CSR_OK;
#if XLEN == 32
  case CSR_REG_MCYCLEH:
    write_high_half(&csr->mcycle, value);
    return CSR_OK;
  case CSR_REG_MINSTRETH:
    write_high_half(&csr->minstret, value);
    csr->minstret_written = true;
    return CSR_OK;
#endif
  case CSR_REG_MCOUNTEREN:
  case CSR_REG_SCOUNTEREN:
    mode_regs(csr, addr)->counteren = value;
    return CSR_OK;
  case CSR_REG_MEDELEG:
    csr->medeleg = value & MEDELEG_WRITABLE;
    return CSR_OK;
  case CSR_REG_MIDELEG:
    csr->mideleg = value & MIDELEG_WRITABLE;
    return CSR_OK;
  default:
    return CSR_ILLEGAL;
  }
}

void csr_destroy(Csr *csr) { free(csr); }

static void csr_count_cycle(Csr *csr) { csr->mcycle++; }

static void csr_count_instret(Csr *csr) {
  if (csr->minstret_written) {
    csr->minstret_written = false;
    return;
  }
  csr->minstret++;
}

void csr_begin_instruction(Csr *csr) { csr->minstret_written = false; }

void csr_end_instruction(Csr *csr, bool retired) {
  if (retired)
    csr_count_instret(csr);
  csr_count_cycle(csr);
}
