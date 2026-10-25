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
  CSR_REG_MISA = 0x301,
  CSR_REG_MVENDORID = 0xF11,
  CSR_REG_MARCHID = 0xF12,
  CSR_REG_MIMPID = 0xF13,
  CSR_REG_MCYCLE = 0xB00,
  CSR_REG_MINSTRET = 0xB02,
#if XLEN == 32
  CSR_REG_MCYCLEH = 0xB80,
  CSR_REG_MINSTRETH = 0xB82,
#endif
  CSR_REG_CYCLE = 0xC00,
#if XLEN == 32
  CSR_REG_CYCLEH = 0xC80,
#endif
  CSR_REG_INSTRET = 0xC02,
#if XLEN == 32
  CSR_REG_INSTRETH = 0xC82,
#endif
};

enum CsrResult { CSR_OK, CSR_ILLEGAL };

enum Mstatus {
  MSTATUS_MIE_BIT = 0x8,
  MSTATUS_MPIE_BIT = 0x80,
  MSTATUS_MPP_MASK = 0x1800,
  MSTATUS_MPP_MACHINE = 0x1800
};

#define MSTATUS_MPP_SHIFT 11

#define MISA_MXL_SHIFT (XLEN - 2)
#define MISA_MXL_32 1
#define MISA_MXL_64 2
#if XLEN == 64
#define MISA_MXL ((uxlen_t)MISA_MXL_64 << MISA_MXL_SHIFT)
#else
#define MISA_MXL ((uxlen_t)MISA_MXL_32 << MISA_MXL_SHIFT)
#endif
#define MISA_EXTENSION(letter) ((uxlen_t)1 << ((letter) - 'A'))
#define MISA_I MISA_EXTENSION('I')
#define MISA_M MISA_EXTENSION('M')
#define MISA_U MISA_EXTENSION('U')

typedef struct Csr Csr;

Csr *csr_create(void);
enum CsrResult csr_read(Csr *csr, uint16_t addr, uxlen_t *value);
enum CsrResult csr_write(Csr *csr, uint16_t addr, uxlen_t value);
void csr_destroy(Csr *csr);

void csr_count_cycle(Csr *csr);
void csr_count_instret(Csr *csr);

#endif // RISCV_CSR_H
