#include "riscv/trap.h"
#include "riscv/bits.h"

struct TrapMode {
  uint16_t epc, cause, tval, tvec;
  uxlen_t ie, pie, pp_mask;
  unsigned pp_shift;
  enum Privilege privilege;
};

static const struct TrapMode MODE_M = {
    CSR_REG_MEPC,     CSR_REG_MCAUSE,    CSR_REG_MTVAL,
    CSR_REG_MTVEC,    MSTATUS_MIE_BIT,   MSTATUS_MPIE_BIT,
    MSTATUS_MPP_MASK, MSTATUS_MPP_SHIFT, PRIV_M};

static const struct TrapMode MODE_S = {
    CSR_REG_SEPC,    CSR_REG_SCAUSE,    CSR_REG_STVAL,
    CSR_REG_STVEC,   SSTATUS_SIE_BIT,   SSTATUS_SPIE_BIT,
    SSTATUS_SPP_BIT, SSTATUS_SPP_SHIFT, PRIV_S};

static uxlen_t csr_get(Csr *csr, uint16_t addr) {
  uxlen_t value = 0;
  csr_read(csr, addr, PRIV_M, &value);
  return value;
}

static void csr_set(Csr *csr, uint16_t addr, uxlen_t value) {
  csr_write(csr, addr, PRIV_M, value);
}

static uxlen_t with_bit(uxlen_t value, uxlen_t mask, bool on) {
  return on ? value | mask : value & ~mask;
}

static uxlen_t trap_enter(Csr *csr, const struct TrapMode *mode, uxlen_t pc,
                          enum Privilege from, uxlen_t cause, uxlen_t tval) {
  csr_set(csr, mode->epc, pc);
  csr_set(csr, mode->cause, cause);
  csr_set(csr, mode->tval, tval);

  uxlen_t status = csr_get(csr, CSR_REG_MSTATUS);
  status = with_bit(status, mode->pie, status & mode->ie);
  status = with_bit(status, mode->ie, false);
  status = (status & ~mode->pp_mask) |
           (((uxlen_t)from << mode->pp_shift) & mode->pp_mask);
  csr_set(csr, CSR_REG_MSTATUS, status);

  return clear_bits(csr_get(csr, mode->tvec), 1, 0);
}

static uxlen_t trap_leave(Csr *csr, const struct TrapMode *mode,
                          enum Privilege *to) {
  uxlen_t status = csr_get(csr, CSR_REG_MSTATUS);
  *to = (enum Privilege)((status & mode->pp_mask) >> mode->pp_shift);
  status = with_bit(status, mode->ie, status & mode->pie);
  status = with_bit(status, mode->pie, true);
  status &= ~mode->pp_mask;
  csr_set(csr, CSR_REG_MSTATUS, status);

  return csr_get(csr, mode->epc);
}

static uxlen_t take_trap(Cpu *cpu, uxlen_t cause, uxlen_t tval, uxlen_t epc) {
  enum Privilege from = cpu_get_privilege(cpu);
  Csr *csr = cpu_get_csr(cpu);
  uxlen_t medeleg;
  csr_read(csr, CSR_REG_MEDELEG, PRIV_M, &medeleg);
  const struct TrapMode *target_mode;
  if (from <= PRIV_S && (medeleg & ((uxlen_t)1 << cause)))
    target_mode = &MODE_S;
  else
    target_mode = &MODE_M;

  cpu_set_privilege(cpu, target_mode->privilege);
  return trap_enter(csr, target_mode, epc, from, cause, tval);
}

uxlen_t cpu_raise_exception(Cpu *cpu, uxlen_t cause, uxlen_t tval) {
  enum Privilege from = cpu_get_privilege(cpu);
  uxlen_t handler = take_trap(cpu, cause, tval, cpu_get_pc(cpu));
  // An etrigger fires right after the trap, before the handler's first
  // instruction.
  if (csr_breakpoint_on_exception(cpu_get_csr(cpu), cpu_get_privilege(cpu),
                                  from, cause))
    handler = take_trap(cpu, TRAP_BREAKPOINT, 0, handler);
  return handler;
}

uxlen_t cpu_return_from_trap(Cpu *cpu, enum Privilege xret) {
  const struct TrapMode *mode = xret == PRIV_M ? &MODE_M : &MODE_S;
  enum Privilege to;
  uxlen_t pc = trap_leave(cpu_get_csr(cpu), mode, &to);
  cpu_set_privilege(cpu, to);
  return pc;
}
