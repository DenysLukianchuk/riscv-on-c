#include "riscv/cpu/csr.h"
#include "riscv/cpu/trap_cause.h"
#include "riscv/cpu/trigger.h"
#include "riscv/util/bits.h"
#include <stdlib.h>

enum Counteren {
  COUNTEREN_CY = 1u << 0,
  COUNTEREN_TM = 1u << 1,
  COUNTEREN_IR = 1u << 2,
};

struct ModeRegs {
  uxlen_t tvec, scratch, epc, cause, tval;
  uint32_t counteren;
};

#define SSIP INTERRUPT_BIT(INTERRUPT_S_SOFTWARE)
#define MSIP INTERRUPT_BIT(INTERRUPT_M_SOFTWARE)
#define STIP INTERRUPT_BIT(INTERRUPT_S_TIMER)
#define MTIP INTERRUPT_BIT(INTERRUPT_M_TIMER)
#define SEIP INTERRUPT_BIT(INTERRUPT_S_EXTERNAL)
#define MEIP INTERRUPT_BIT(INTERRUPT_M_EXTERNAL)
#define MIDELEG_WRITABLE (SSIP | STIP | SEIP)
#define MIP_WRITABLE (SSIP | STIP | SEIP)
#define MIE_WRITABLE (SSIP | MSIP | STIP | MTIP | SEIP | MEIP)

#define MSTATUS_WRITABLE                                                       \
  (SSTATUS_SIE_BIT | MSTATUS_MIE_BIT | SSTATUS_SPIE_BIT | MSTATUS_MPIE_BIT |   \
   SSTATUS_SPP_BIT | MSTATUS_MPP_MASK | MSTATUS_TW_BIT | SSTATUS_SUM_BIT |     \
   SSTATUS_MXR_BIT | MSTATUS_MPRV_BIT | MSTATUS_TVM_BIT)
// MPP value 2 would name H-mode, which this hart does not implement.
#define MSTATUS_MPP_RESERVED ((uxlen_t)2 << MSTATUS_MPP_SHIFT)

#define SSTATUS_MASK                                                           \
  (SSTATUS_SIE_BIT | SSTATUS_SPIE_BIT | SSTATUS_SPP_BIT | SSTATUS_SUM_BIT |    \
   SSTATUS_MXR_BIT)

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
#define SATP_MODE_SV39 8
#define SATP_MODE_PAGED SATP_MODE_SV39
#else
#define SATP_MODE_SV32 1
#define SATP_MODE_PAGED SATP_MODE_SV32
#endif

#if XLEN == 64
#define MSTATUS_UXL_MASK ((uxlen_t)3 << 32)
#define MSTATUS_SXL_MASK ((uxlen_t)3 << 34)
#define MSTATUS_UXL_64 ((uxlen_t)2 << 32)
#define MSTATUS_SXL_64 ((uxlen_t)2 << 34)
#endif

struct Csr {
  struct ModeRegs m, s;
  struct TriggerModule trigger;
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
  uxlen_t mip;
  bool minstret_written;
  bool seip_line;
  uxlen_t satp;
  uint64_t time;
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
  csr_reset(csr);
  return csr;
}

void csr_reset(Csr *csr) {
  csr->mhartid = 0;
  mode_regs_init(&csr->m);
  mode_regs_init(&csr->s);
#if XLEN == 64
  csr->mstatus = MSTATUS_UXL_64 | MSTATUS_SXL_64;
#else
  csr->mstatus = 0;
#endif
  csr->mie = 0;
  csr->misa = MISA_MXL | MISA_I | MISA_M | MISA_U | MISA_S | MISA_A | MISA_C;
  csr->mvendorid = 0;
  csr->marchid = 0;
  csr->mimpid = 0;
  csr->minstret = 0;
  csr->mcycle = 0;
  csr->minstret_written = false;
  csr->medeleg = 0;
  csr->mideleg = 0;
  csr->mip = 0;
  csr->seip_line = false;
  csr->satp = 0;
  csr->time = 0;
  trigger_regs_init(&csr->trigger);
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
  case CSR_REG_TIME:
#if XLEN == 32
  case CSR_REG_TIMEH:
#endif
    return counteren_accessible(csr, COUNTEREN_TM, privilege);
  case CSR_REG_INSTRET:
#if XLEN == 32
  case CSR_REG_INSTRETH:
#endif
    return counteren_accessible(csr, COUNTEREN_IR, privilege);
  case CSR_REG_SATP:
    return (privilege == PRIV_M) || !(csr->mstatus & MSTATUS_TVM_BIT);
  }
  return true;
}

static uxlen_t pending_bits(const Csr *csr) {
  uxlen_t pending = csr->mip;
  if (csr->seip_line)
    pending |= SEIP;
  return pending;
}

enum CsrResult csr_read(Csr *csr, uint16_t addr, enum Privilege privilege,
                        uxlen_t *value) {
  if (!csr_is_accessible(csr, addr, privilege))
    return CSR_ILLEGAL;
  if (is_trigger_register(addr)) {
    *value = trigger_regs_read(&csr->trigger, addr);
    return CSR_OK;
  }
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
  case CSR_REG_SIE:
    *value = csr->mie & csr->mideleg;
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
  case CSR_REG_MIP:
    *value = pending_bits(csr);
    return CSR_OK;
  case CSR_REG_SIP:
    *value = pending_bits(csr) & csr->mideleg;
    return CSR_OK;
  case CSR_REG_SATP:
    *value = csr->satp;
    return CSR_OK;
  case CSR_REG_TIME:
    *value = (uxlen_t)csr->time;
    return CSR_OK;
#if XLEN == 32
  case CSR_REG_TIMEH:
    *value = (uxlen_t)(csr->time >> XLEN);
    return CSR_OK;
#endif
  default:
    return CSR_ILLEGAL;
  }
}

enum CsrResult csr_modify(Csr *csr, uint16_t addr, enum Privilege privilege,
                          uxlen_t value, uxlen_t mask, bool write,
                          uxlen_t *old) {
  enum CsrResult result = csr_read(csr, addr, privilege, old);
  if (result != CSR_OK || !write)
    return result;
  // Only the software SEIP bit takes part, not the PLIC's line.
  uxlen_t base = addr == CSR_REG_MIP ? csr->mip : *old;
  return csr_write(csr, addr, privilege, (base & ~mask) | (value & mask));
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

static inline uxlen_t masked_write(uxlen_t old, uxlen_t new, uxlen_t mask) {
  return (old & ~mask) | (new & mask);
}

static uxlen_t legalize_status(uxlen_t old_value, uxlen_t value,
                               uint16_t addr) {
  if (addr != CSR_REG_MSTATUS)
    return masked_write(old_value, value, SSTATUS_MASK);
  uxlen_t status = masked_write(old_value, value, MSTATUS_WRITABLE);
  if ((status & MSTATUS_MPP_MASK) == MSTATUS_MPP_RESERVED)
    status &= ~MSTATUS_MPP_MASK;
  return status;
}

uxlen_t satp_mode(uxlen_t value) {
#if XLEN == 64
  return bits(value, 63, 60);
#else
  return bits(value, 31, 31);
#endif
}

static uxlen_t legalize_satp(uxlen_t old_value, uxlen_t value) {
  uxlen_t mode = satp_mode(value);
  if (mode != SATP_MODE_PAGED && mode != SATP_MODE_BARE)
    return old_value;
  return value;
}

enum CsrResult csr_write(Csr *csr, uint16_t addr, enum Privilege privilege,
                         uxlen_t value) {
  if (!csr_is_accessible(csr, addr, privilege) || csr_is_read_only(addr))
    return CSR_ILLEGAL;
  if (is_trigger_register(addr)) {
    trigger_regs_write(&csr->trigger, addr, value);
    return CSR_OK;
  }
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
    csr->mie = masked_write(csr->mie, value, MIE_WRITABLE);
    return CSR_OK;
  case CSR_REG_SIE:
    csr->mie = masked_write(csr->mie, value, csr->mideleg);
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
  case CSR_REG_MISA:
    // misa is WARL; this hart cannot turn extensions off.
    return CSR_OK;
  case CSR_REG_MIP:
    csr->mip = masked_write(csr->mip, value, MIP_WRITABLE);
    return CSR_OK;
  case CSR_REG_SIP:
    csr->mip = masked_write(csr->mip, value, csr->mideleg & SSIP);
    return CSR_OK;
  case CSR_REG_SATP:
    csr->satp = legalize_satp(csr->satp, value);
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

static bool breakpoint_enabled(Csr *csr, enum Privilege privilege) {
  switch (privilege) {
  case PRIV_M:
    return csr->mstatus & MSTATUS_MIE_BIT;
  case PRIV_S:
    return !(csr->medeleg & CAUSE_BIT(TRAP_BREAKPOINT)) ||
           (csr->mstatus & SSTATUS_SIE_BIT);
  case PRIV_U:
    return true;
  }
  return false;
}

bool csr_breakpoint_on_execute(Csr *csr, enum Privilege privilege, uxlen_t pc,
                               const uxlen_t *data, uxlen_t *tval) {
  return breakpoint_enabled(csr, privilege) &&
         trigger_match_execute(&csr->trigger, privilege, pc, data, tval);
}

bool csr_breakpoint_on_load(Csr *csr, enum Privilege privilege, uxlen_t addr,
                            uint8_t size, const uxlen_t *data, uxlen_t *tval) {
  return breakpoint_enabled(csr, privilege) &&
         trigger_match_load(&csr->trigger, privilege, addr, size, data, tval);
}

bool csr_breakpoint_on_store(Csr *csr, enum Privilege privilege, uxlen_t addr,
                             uint8_t size, const uxlen_t *data, uxlen_t *tval) {
  return breakpoint_enabled(csr, privilege) &&
         trigger_match_store(&csr->trigger, privilege, addr, size, data, tval);
}

bool csr_breakpoint_on_exception(Csr *csr, enum Privilege handler,
                                 enum Privilege from, unsigned cause) {
  return breakpoint_enabled(csr, handler) &&
         trigger_match_exception(&csr->trigger, from, cause);
}

bool csr_breakpoints_enabled(Csr *csr, enum Privilege privilege) {
  return breakpoint_enabled(csr, privilege);
}

bool csr_breakpoint_on_icount(Csr *csr, enum Privilege privilege) {
  return breakpoint_enabled(csr, privilege) &&
         trigger_icount_fire(&csr->trigger, privilege);
}

void csr_count_icount(Csr *csr, enum Privilege privilege) {
  trigger_icount_count(&csr->trigger, privilege);
}

void csr_set_interrupt_pending(Csr *csr, uxlen_t interrupts, bool pending) {
  if (pending)
    csr->mip |= interrupts;
  else
    csr->mip &= ~interrupts;
}

void csr_set_seip_line(Csr *csr, bool raised) { csr->seip_line = raised; }

// Highest priority first.
static const enum Interrupt INTERRUPT_PRIORITY[] = {
    INTERRUPT_M_EXTERNAL, INTERRUPT_M_SOFTWARE, INTERRUPT_M_TIMER,
    INTERRUPT_S_EXTERNAL, INTERRUPT_S_SOFTWARE, INTERRUPT_S_TIMER};

bool csr_pending_interrupt(Csr *csr, enum Privilege privilege, unsigned *code) {
  uxlen_t pending = pending_bits(csr) & csr->mie;
  uxlen_t to_m = pending & ~csr->mideleg;
  uxlen_t to_s = pending & csr->mideleg;

  // A mode can mask the interrupts that target it, never those of a higher
  // mode; M-mode never takes the ones delegated to S.
  bool m_enabled = privilege < PRIV_M || (csr->mstatus & MSTATUS_MIE_BIT);
  bool s_enabled = privilege < PRIV_S ||
                   (privilege == PRIV_S && (csr->mstatus & SSTATUS_SIE_BIT));
  if (!m_enabled)
    to_m = 0;
  if (!s_enabled)
    to_s = 0;

  // Interrupts for M-mode are taken before those for S-mode.
  uxlen_t ready = to_m ? to_m : to_s;
  for (unsigned i = 0;
       i < sizeof INTERRUPT_PRIORITY / sizeof INTERRUPT_PRIORITY[0]; i++) {
    if (ready & INTERRUPT_BIT(INTERRUPT_PRIORITY[i])) {
      *code = INTERRUPT_PRIORITY[i];
      return true;
    }
  }
  return false;
}

void csr_set_time(Csr *csr, uint64_t mtime) { csr->time = mtime; }
