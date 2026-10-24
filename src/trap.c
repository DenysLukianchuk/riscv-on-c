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

uxlen_t trap_raise(Csr *csr, uxlen_t pc, uxlen_t cause, uxlen_t tval) {
  csr_write(csr, CSR_REG_MEPC, pc);
  csr_write(csr, CSR_REG_MCAUSE, cause);
  csr_write(csr, CSR_REG_MTVAL, tval);
  mstatus_trap_enter(csr);
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
