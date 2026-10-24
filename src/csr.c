#include "riscv/csr.h"
#include "riscv/bits.h"
#include <stdbool.h>
#include <stdlib.h>

struct Csr {
  uxlen_t mscratch;
  uxlen_t mhartid;
  uxlen_t mepc;
  uxlen_t mcause;
  uxlen_t mtval;
  uxlen_t mtvec;
  uxlen_t mstatus;
  uxlen_t mie;
};

Csr *csr_create(void) {
  Csr *csr = (Csr *)malloc(sizeof(Csr));
  csr->mscratch = 0;
  csr->mhartid = 0;
  csr->mepc = 0;
  csr->mcause = 0;
  csr->mtval = 0;
  csr->mtvec = 0;
  csr->mstatus = 0;
  csr->mie = 0;
  return csr;
}

enum CsrResult csr_read(Csr *csr, uint16_t addr, uxlen_t *value) {
  switch (addr) {
  case CSR_REG_MSCRATCH:
    *value = csr->mscratch;
    return CSR_OK;
  case CSR_REG_MHARTID:
    *value = csr->mhartid;
    return CSR_OK;
  case CSR_REG_MEPC:
    *value = csr->mepc;
    return CSR_OK;
  case CSR_REG_MCAUSE:
    *value = csr->mcause;
    return CSR_OK;
  case CSR_REG_MTVAL:
    *value = csr->mtval;
    return CSR_OK;
  case CSR_REG_MTVEC:
    *value = csr->mtvec;
    return CSR_OK;
  case CSR_REG_MSTATUS:
    *value = csr->mstatus;
    return CSR_OK;
  case CSR_REG_MIE:
    *value = csr->mie;
    return CSR_OK;
  default:
    return CSR_ILLEGAL;
  }
}

static inline bool csr_is_read_only(uint16_t addr) {
  return bits(addr, 11, 10) == 0x3;
}

enum CsrResult csr_write(Csr *csr, uint16_t addr, uxlen_t value) {
  if (csr_is_read_only(addr))
    return CSR_ILLEGAL;
  switch (addr) {
  case CSR_REG_MSCRATCH:
    csr->mscratch = value;
    return CSR_OK;
  case CSR_REG_MEPC:
    csr->mepc = clear_bits(value, 0, 0);
    return CSR_OK;
  case CSR_REG_MCAUSE:
    csr->mcause = value;
    return CSR_OK;
  case CSR_REG_MTVAL:
    csr->mtval = value;
    return CSR_OK;
  case CSR_REG_MTVEC: {
    uxlen_t mode = bits(value, 1, 0);
    if (mode > 1)
      mode = bits(csr->mtvec, 1, 0);
    csr->mtvec = clear_bits(value, 1, 0) | mode;
    return CSR_OK;
  };
  case CSR_REG_MSTATUS:
    csr->mstatus = value;
    return CSR_OK;
  case CSR_REG_MIE:
    csr->mie = value;
    return CSR_OK;
  default:
    return CSR_ILLEGAL;
  }
}

void csr_destroy(Csr *csr) { free(csr); }
