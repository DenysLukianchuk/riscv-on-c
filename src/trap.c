#include "riscv/trap.h"
#include "riscv/bits.h"

static uxlen_t trap_vector_address(Csr *csr) {
  uxlen_t mtvec = 0;
  csr_read(csr, CSR_REG_MTVEC, &mtvec);
  return clear_bits(mtvec, 1, 0);
}

static void mstatus_trap_enter(Csr *csr) {
  uxlen_t mstatus = 0;
  csr_read(csr, CSR_REG_MSTATUS, &mstatus);
  if (mstatus & MSTATUS_MIE_BIT)
    mstatus |= MSTATUS_MPIE_BIT;
  else
    mstatus &= ~MSTATUS_MPIE_BIT;
  mstatus &= ~MSTATUS_MIE_BIT;
  mstatus |= MSTATUS_MPP_MACHINE;
  csr_write(csr, CSR_REG_MSTATUS, mstatus);
}

static void trap_set_mpp(Csr *csr, uint8_t mpp) {
  uxlen_t mstatus = 0;
  csr_read(csr, CSR_REG_MSTATUS, &mstatus);
  mstatus &= ~MSTATUS_MPP_MASK;
  mstatus |= ((uxlen_t)mpp << MSTATUS_MPP_SHIFT) & MSTATUS_MPP_MASK;
  csr_write(csr, CSR_REG_MSTATUS, mstatus);
}

uxlen_t trap_raise(Csr *csr, uxlen_t pc, uint8_t mpp, uxlen_t cause,
                   uxlen_t tval) {
  csr_write(csr, CSR_REG_MEPC, pc);
  csr_write(csr, CSR_REG_MCAUSE, cause);
  csr_write(csr, CSR_REG_MTVAL, tval);
  mstatus_trap_enter(csr);
  trap_set_mpp(csr, mpp);
  return trap_vector_address(csr);
}

static void mstatus_trap_return(Csr *csr) {
  uxlen_t mstatus = 0;
  csr_read(csr, CSR_REG_MSTATUS, &mstatus);
  if (mstatus & MSTATUS_MPIE_BIT)
    mstatus |= MSTATUS_MIE_BIT;
  else
    mstatus &= ~MSTATUS_MIE_BIT;
  mstatus |= MSTATUS_MPIE_BIT;
  mstatus &= ~MSTATUS_MPP_MASK;
  csr_write(csr, CSR_REG_MSTATUS, mstatus);
}

uxlen_t trap_return(Csr *csr) {
  mstatus_trap_return(csr);
  uxlen_t mepc = 0;
  csr_read(csr, CSR_REG_MEPC, &mepc);
  return mepc;
}

uint8_t trap_get_mpp(Csr *csr) {
  uxlen_t mstatus = 0;
  csr_read(csr, CSR_REG_MSTATUS, &mstatus);
  return (mstatus & MSTATUS_MPP_MASK) >> MSTATUS_MPP_SHIFT;
}

uxlen_t cpu_raise_exception(Cpu *cpu, uxlen_t cause, uxlen_t tval) {
  uxlen_t privilege = cpu_get_privilege(cpu);
  cpu_set_privilege(cpu, PRIV_M);
  return trap_raise(cpu_get_csr(cpu), cpu_get_pc(cpu), privilege, cause, tval);
}

uxlen_t cpu_return_from_trap(Cpu *cpu) {
  cpu_set_privilege(cpu, trap_get_mpp(cpu_get_csr(cpu)));
  return trap_return(cpu_get_csr(cpu));
}
