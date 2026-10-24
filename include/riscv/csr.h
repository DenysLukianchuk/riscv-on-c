#ifndef RISCV_CSR_H
#define RISCV_CSR_H

#include "riscv/xlen.h"
enum CsrRegister {
  CSR_REG_MSTATUS = 0x300,
  CSR_REG_MIE = 0x304,
  CSR_REG_MTVEC = 0x305,
  CSR_REG_MSCRATCH = 0x340,
  CSR_REG_MEPC = 0x341,
  CSR_REG_MCAUSE = 0x342,
  CSR_REG_MTVAL = 0x343,
  CSR_REG_MHARTID = 0xF14,
};

enum CsrResult { CSR_OK, CSR_ILLEGAL };

enum Mstatus {
  MSTATUS_MIE_BIT = 0x8,
  MSTATUS_MPIE_BIT = 0x80,
  MSTATUS_MPP_MASK = 0x1800,
  MSTATUS_MPP_MACHINE = 0x1800
};

typedef struct Csr Csr;

Csr *csr_create(void);
enum CsrResult csr_read(Csr *csr, uint16_t addr, uxlen_t *value);
enum CsrResult csr_write(Csr *csr, uint16_t addr, uxlen_t value);
void csr_destroy(Csr *csr);

#endif // RISCV_CSR_H
