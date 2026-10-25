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
  uxlen_t misa;
  uxlen_t mvendorid;
  uxlen_t marchid;
  uxlen_t mimpid;
  uint64_t minstret;
  uint64_t mcycle;
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
  csr->misa = MISA_MXL | MISA_I | MISA_M | MISA_U;
  csr->mvendorid = 0;
  csr->marchid = 0;
  csr->mimpid = 0;
  csr->minstret = 0;
  csr->mcycle = 0;
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
  case CSR_REG_MCYCLE:
    write_low_half(&csr->mcycle, value);
    return CSR_OK;
  case CSR_REG_MINSTRET:
    write_low_half(&csr->minstret, value);
    return CSR_OK;
#if XLEN == 32
  case CSR_REG_MCYCLEH:
    write_high_half(&csr->mcycle, value);
    return CSR_OK;
  case CSR_REG_MINSTRETH:
    write_high_half(&csr->minstret, value);
    return CSR_OK;
#endif
  default:
    return CSR_ILLEGAL;
  }
}

void csr_destroy(Csr *csr) { free(csr); }

void csr_count_cycle(Csr *csr) { csr->mcycle++; }

void csr_count_instret(Csr *csr) { csr->minstret++; }
