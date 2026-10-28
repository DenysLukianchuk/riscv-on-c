#ifndef TESTS_RISCV_SPEC_H
#define TESTS_RISCV_SPEC_H

// Architectural numbers written out from the RISC-V specifications. Tests use
// these instead of the emulator's own headers, so a wrong constant there makes
// the tests fail instead of agreeing with it.

#include "riscv/xlen.h"

// CSR addresses.
#define SSTATUS 0x100
#define SIE 0x104
#define STVEC 0x105
#define SCOUNTEREN 0x106
#define SSCRATCH 0x140
#define SEPC 0x141
#define SCAUSE 0x142
#define STVAL 0x143
#define SIP 0x144
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
#define MIP 0x344
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
#define TSELECT 0x7A0
#define TDATA1 0x7A1
#define TDATA2 0x7A2
#define TDATA3 0x7A3
#define TINFO 0x7A4
#define TCONTROL 0x7A5

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
#define MSTATUS_MPP_H (2u << 11)
#define MSTATUS_UBE (1u << 6)
#define MSTATUS_VS (3u << 9)
#define MSTATUS_FS (3u << 13)
#define MSTATUS_XS (3u << 15)
#define MSTATUS_TW (1u << 21)
#define MSTATUS_SD ((uxlen_t)1 << (XLEN - 1))
#define SSTATUS_SIE MSTATUS_SIE
#define SSTATUS_SPIE MSTATUS_SPIE
#define SSTATUS_SPP MSTATUS_SPP
#define SSTATUS_SPP_U 0u
#if XLEN == 64
#define MSTATUS_UXL ((uxlen_t)3 << 32)
#define MSTATUS_SXL ((uxlen_t)3 << 34)
#define MSTATUS_UXL_64 ((uxlen_t)2 << 32)
#define MSTATUS_SXL_64 ((uxlen_t)2 << 34)
#define MSTATUS_SBE ((uxlen_t)1 << 36)
#define MSTATUS_MBE ((uxlen_t)1 << 37)
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

// Trigger module (Sdtrig 1.0). tdata1 fields shared by every trigger type:
#define TDATA1_TYPE(type) ((uxlen_t)(type) << (XLEN - 4))
#define TDATA1_DMODE ((uxlen_t)1 << (XLEN - 5))
#define TRIGGER_TYPE_MCONTROL 2
#define TRIGGER_TYPE_ICOUNT 3
#define TRIGGER_TYPE_ETRIGGER 5
#define TRIGGER_TYPE_TMEXTTRIGGER 7
#define TRIGGER_TYPE_DISABLED 15
// mcontrol (type 2) fields:
#define MCONTROL_MASKMAX ((uxlen_t)0x3F << (XLEN - 11))
#if XLEN == 64
#define MCONTROL_SIZE(size)                                                    \
  (((uxlen_t)((size) >> 2 & 3) << 21) | ((uxlen_t)((size) & 3) << 16))
#else
#define MCONTROL_SIZE(size) ((uxlen_t)((size) & 3) << 16)
#endif
#define MCONTROL_SIZE_FIELD MCONTROL_SIZE(0xF)
#define SIZE_ANY 0
#define SIZE_8BIT 1
#define SIZE_16BIT 2
#define SIZE_32BIT 3
#define SIZE_48BIT 4
#define SIZE_64BIT 5
#define MCONTROL_HIT (1u << 20)
#define MCONTROL_SELECT (1u << 19)
#define MCONTROL_TIMING (1u << 18)
#define MCONTROL_ACTION(action) ((uxlen_t)(action) << 12)
#define MCONTROL_CHAIN (1u << 11)
#define MCONTROL_MATCH(match) ((uxlen_t)(match) << 7)
#define MCONTROL_MATCH_FIELD MCONTROL_MATCH(0xF)
#define MATCH_EQUAL 0
#define MATCH_NAPOT 1
#define MATCH_GE 2
#define MATCH_LT 3
#define MATCH_MASK_LOW 4
#define MATCH_MASK_HIGH 5
#define MATCH_NOT_EQUAL 8
#define MATCH_NOT_NAPOT 9
#define MATCH_NOT_MASK_LOW 12
#define MATCH_NOT_MASK_HIGH 13
#define MCONTROL_M (1u << 6)
#define MCONTROL_S (1u << 4)
#define MCONTROL_U (1u << 3)
#define MCONTROL_EXECUTE (1u << 2)
#define MCONTROL_STORE (1u << 1)
#define MCONTROL_LOAD (1u << 0)
// mcontrol6 (type 6) fields; chain, match, action, m, s, u, execute, store
// and load sit where they are in mcontrol.
#define TRIGGER_TYPE_MCONTROL6 6
#define MCONTROL6_UNCERTAIN (1u << 26)
#define MCONTROL6_HIT1 (1u << 25)
#define MCONTROL6_VS (1u << 24)
#define MCONTROL6_VU (1u << 23)
#define MCONTROL6_HIT0 (1u << 22)
#define MCONTROL6_SELECT (1u << 21)
#define MCONTROL6_SIZE(size) ((uxlen_t)(size) << 16)
#define MCONTROL6_SIZE_FIELD MCONTROL6_SIZE(7)
#define MCONTROL6_UNCERTAINEN (1u << 5)
#define SIZE_128BIT 6
// icount (type 3) fields:
#define ICOUNT_VS (1u << 26)
#define ICOUNT_VU (1u << 25)
#define ICOUNT_HIT (1u << 24)
#define ICOUNT_COUNT(count) ((uxlen_t)(count) << 10)
#define ICOUNT_COUNT_FIELD ICOUNT_COUNT(0x3FFF)
#define ICOUNT_M (1u << 9)
#define ICOUNT_PENDING (1u << 8)
#define ICOUNT_S (1u << 7)
#define ICOUNT_U (1u << 6)
#define ICOUNT_ACTION(action) ((uxlen_t)(action))
// etrigger (type 5) fields; tdata2 holds one bit per exception code:
#define ETRIGGER_HIT ((uxlen_t)1 << (XLEN - 6))
#define ETRIGGER_VS (1u << 12)
#define ETRIGGER_VU (1u << 11)
#define ETRIGGER_M (1u << 9)
#define ETRIGGER_S (1u << 7)
#define ETRIGGER_U (1u << 6)
#define ETRIGGER_ACTION(action) ((uxlen_t)(action))
#define TINFO_VERSION_1 (1u << 24)
#define TINFO_TYPE(type) (1u << (type))

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

// Interrupts in mcause and scause: the top bit set, the code below it.
#define CAUSE_INTERRUPT ((uxlen_t)1 << (XLEN - 1))
#define IRQ_S_SOFTWARE 1
#define IRQ_M_SOFTWARE 3
#define IRQ_S_TIMER 5
#define IRQ_M_TIMER 7
#define IRQ_S_EXTERNAL 9
#define IRQ_M_EXTERNAL 11

// CLINT (SiFive CLINT / RISC-V ACLINT): register offsets inside the device,
// for hart 0.
#define CLINT_MSIP 0x0000
#define CLINT_MTIMECMP 0x4000
#define CLINT_MTIME 0xBFF8
#define CLINT_SIZE 0x10000

#endif // TESTS_RISCV_SPEC_H
