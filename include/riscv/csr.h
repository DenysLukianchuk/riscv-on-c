#ifndef RISCV_CSR_H
#define RISCV_CSR_H

#include "riscv/privilege.h"
#include "riscv/xlen.h"
#include <stdbool.h>

enum CsrRegister {
  CSR_REG_MSTATUS = 0x300,
  CSR_REG_SSTATUS = 0x100,
  CSR_REG_MIE = 0x304,
  CSR_REG_MTVEC = 0x305,
  CSR_REG_STVEC = 0x105,
  CSR_REG_MSCRATCH = 0x340,
  CSR_REG_SSCRATCH = 0x140,
  CSR_REG_MEPC = 0x341,
  CSR_REG_SEPC = 0x141,
  CSR_REG_MCAUSE = 0x342,
  CSR_REG_SCAUSE = 0x142,
  CSR_REG_MTVAL = 0x343,
  CSR_REG_STVAL = 0x143,
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
  CSR_REG_MCOUNTEREN = 0x306,
  CSR_REG_SCOUNTEREN = 0x106,
  CSR_REG_MEDELEG = 0x302,
  CSR_REG_MIDELEG = 0x303,
};

enum CsrResult { CSR_OK, CSR_ILLEGAL };

enum Mstatus {
  MSTATUS_MIE_BIT = 0x8,
  MSTATUS_MPIE_BIT = 0x80,
  MSTATUS_MPP_MASK = 0x1800,
  MSTATUS_MPP_MACHINE = 0x1800
};

enum Sstatus {
  SSTATUS_SIE_BIT = 0x2,
  SSTATUS_SPIE_BIT = 0x20,
  SSTATUS_SPP_BIT = 0x100,
  SSTATUS_MASK = SSTATUS_SIE_BIT | SSTATUS_SPIE_BIT | SSTATUS_SPP_BIT
};

#define MSTATUS_MPP_SHIFT 11
#define SSTATUS_SPP_SHIFT 8

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
#define MISA_S MISA_EXTENSION('S')

typedef struct Csr Csr;

Csr *csr_create(void);
enum CsrResult csr_read(Csr *csr, uint16_t addr, enum Privilege privilege,
                        uxlen_t *value);
enum CsrResult csr_write(Csr *csr, uint16_t addr, enum Privilege privilege,
                         uxlen_t value);
void csr_destroy(Csr *csr);

void csr_begin_instruction(Csr *csr);
void csr_end_instruction(Csr *csr, bool retired);

#endif // RISCV_CSR_H
