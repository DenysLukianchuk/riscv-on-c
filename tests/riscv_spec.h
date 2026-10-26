#ifndef TESTS_RISCV_SPEC_H
#define TESTS_RISCV_SPEC_H

// Architectural numbers written out from the RISC-V specifications. Tests use
// these instead of the emulator's own headers, so a wrong constant there makes
// the tests fail instead of agreeing with it.

#include "riscv/xlen.h"

// CSR addresses.
#define SSTATUS 0x100
#define STVEC 0x105
#define SCOUNTEREN 0x106
#define SSCRATCH 0x140
#define SEPC 0x141
#define SCAUSE 0x142
#define STVAL 0x143
#define MSTATUS 0x300
#define MISA 0x301
#define MEDELEG 0x302
#define MIDELEG 0x303
#define MIE 0x304
#define MTVEC 0x305
#define MCOUNTEREN 0x306
#define MSCRATCH 0x340
#define MEPC 0x341
#define MCAUSE 0x342
#define MTVAL 0x343
#define MCYCLE 0xB00
#define MINSTRET 0xB02
#define MCYCLEH 0xB80
#define MINSTRETH 0xB82
#define CYCLE 0xC00
#define INSTRET 0xC02
#define CYCLEH 0xC80
#define INSTRETH 0xC82
#define MVENDORID 0xF11
#define MARCHID 0xF12
#define MIMPID 0xF13
#define MHARTID 0xF14

// mstatus fields; sstatus shows the S-mode ones at the same positions.
#define MSTATUS_SIE (1u << 1)
#define MSTATUS_MIE (1u << 3)
#define MSTATUS_SPIE (1u << 5)
#define MSTATUS_MPIE (1u << 7)
#define MSTATUS_SPP (1u << 8)
#define MSTATUS_MPP (3u << 11)
#define MSTATUS_MPP_U (0u << 11)
#define MSTATUS_MPP_S (1u << 11)
#define MSTATUS_MPP_M (3u << 11)
#define SSTATUS_SIE MSTATUS_SIE
#define SSTATUS_SPIE MSTATUS_SPIE
#define SSTATUS_SPP MSTATUS_SPP
#define SSTATUS_SPP_U 0u
#if XLEN == 64
#define MSTATUS_UXL ((uxlen_t)3 << 32)
#define MSTATUS_SXL ((uxlen_t)3 << 34)
#define MSTATUS_UXL_64 ((uxlen_t)2 << 32)
#define MSTATUS_SXL_64 ((uxlen_t)2 << 34)
#endif

#define MISA_MXL_FIELD ((uxlen_t)(XLEN == 64 ? 2 : 1) << (XLEN - 2))
#define MISA_EXT(letter) ((uxlen_t)1 << ((letter) - 'A'))

// Interrupt bits of mip, mie and mideleg.
#define SSIP (1u << 1)
#define MSIP (1u << 3)
#define STIP (1u << 5)
#define MTIP (1u << 7)
#define SEIP (1u << 9)
#define MEIP (1u << 11)

// mcounteren and scounteren bits.
#define COUNTEREN_CY (1u << 0)
#define COUNTEREN_IR (1u << 2)

// Exception codes in mcause and scause.
#define CAUSE_MISALIGNED_FETCH 0
#define CAUSE_FETCH_ACCESS 1
#define CAUSE_ILLEGAL_INSTRUCTION 2
#define CAUSE_BREAKPOINT 3
#define CAUSE_MISALIGNED_LOAD 4
#define CAUSE_LOAD_ACCESS 5
#define CAUSE_MISALIGNED_STORE 6
#define CAUSE_STORE_ACCESS 7
#define CAUSE_ECALL_FROM_U 8
#define CAUSE_ECALL_FROM_S 9
#define CAUSE_ECALL_FROM_M 11

#endif // TESTS_RISCV_SPEC_H
