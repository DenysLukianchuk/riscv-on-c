#include "riscv/cpu/decoder.h"
#include "riscv_asm.h"
#include "unity.h"

// The C extension: each 16-bit instruction stands for one 32-bit instruction
// and expands to it. The 16-bit encodings below come from the GNU assembler
// (riscv64-linux-gnu-as -march=rv64imafdc / rv32imafc, objdump -M no-aliases),
// not from the emulator. A reserved encoding, or one that needs an extension
// the hart lacks, expands to 0, which the decoder treats as illegal.

void setUp(void) {}
void tearDown(void) {}

#define ASSERT_EXPANDS(compressed, expected)                                   \
  TEST_ASSERT_EQUAL_HEX32((expected), instruction_expand(compressed))
#define ASSERT_ILLEGAL(compressed) ASSERT_EXPANDS(compressed, 0)

// The all-zero halfword is defined to be illegal, so that running into
// zeroed memory traps.
static void test_zero_halfword_is_illegal(void) { ASSERT_ILLEGAL(0x0000); }

// Quadrant 0 (bits 1:0 = 00): stack-pointer offsets and loads/stores through
// the 3-bit registers x8..x15 (s0, s1, a0..a5).

static void test_c_addi4spn_expands_to_addi_from_sp(void) {
  ASSERT_EXPANDS(0x0800, ADDI(8, 2, 16));    // c.addi4spn s0, sp, 16
  ASSERT_EXPANDS(0x1ffc, ADDI(15, 2, 1020)); // c.addi4spn a5, sp, 1020
}

// The zero halfword is this case too: c.addi4spn with an offset of 0.
static void test_c_addi4spn_with_zero_offset_is_reserved(void) {
  ASSERT_ILLEGAL(0x0004); // rd' = s1, nzuimm = 0
}

static void test_c_lw_expands_to_lw(void) {
  ASSERT_EXPANDS(0x41c8, LW(10, 4, 11));  // c.lw a0, 4(a1)
  ASSERT_EXPANDS(0x5fe4, LW(9, 124, 15)); // c.lw s1, 124(a5)
}

static void test_c_sw_expands_to_sw(void) {
  ASSERT_EXPANDS(0xc1c8, SW(10, 4, 11));  // c.sw a0, 4(a1)
  ASSERT_EXPANDS(0xdfe4, SW(9, 124, 15)); // c.sw s1, 124(a5)
}

static void test_quadrant_0_funct3_100_is_reserved(void) {
  ASSERT_ILLEGAL(0x8000);
}

// The hart has no F or D: their compressed loads and stores are illegal.
static void test_c_fld_and_c_fsd_are_illegal(void) {
  ASSERT_ILLEGAL(0x2588); // c.fld fa0, 8(a1)
  ASSERT_ILLEGAL(0xa588); // c.fsd fa0, 8(a1)
  ASSERT_ILLEGAL(0x2522); // c.fldsp fa0, 8(sp)
  ASSERT_ILLEGAL(0xa42a); // c.fsdsp fa0, 8(sp)
}

// Quadrant 1 (bits 1:0 = 01): immediates, arithmetic and branches.

static void test_c_addi_expands_to_addi(void) {
  ASSERT_EXPANDS(0x12f5, ADDI(5, 5, -3)); // c.addi t0, -3
  ASSERT_EXPANDS(0x02fd, ADDI(5, 5, 31)); // c.addi t0, 31
}

static void test_c_nop_expands_to_nop(void) {
  ASSERT_EXPANDS(0x0001, NOP); // c.nop
}

static void test_c_li_expands_to_addi_from_x0(void) {
  ASSERT_EXPANDS(0x44c5, ADDI(9, 0, 17)); // c.li s1, 17
}

// Writing x0 makes these hints, not reserved encodings: they expand as usual
// and the result is discarded.
static void test_hints_writing_x0_expand_normally(void) {
  ASSERT_EXPANDS(0x4015, ADDI(0, 0, 5)); // c.li x0, 5
  ASSERT_EXPANDS(0x802e, ADD(0, 0, 11)); // c.mv x0, a1
}

// c.lui carries bits 17:12 of the value; lui takes bits 31:12.
static void test_c_lui_expands_to_lui(void) {
  ASSERT_EXPANDS(0x6505, LUI(10, 0x1));    // c.lui a0, 0x1
  ASSERT_EXPANDS(0x74fd, LUI(9, 0xfffff)); // c.lui s1, 0xfffff
  ASSERT_EXPANDS(0x7281, LUI(5, 0xfffe0)); // c.lui t0, 0xfffe0
}

static void test_c_lui_with_zero_is_reserved(void) {
  ASSERT_ILLEGAL(0x6501); // c.lui a0, 0
}

// With rd = sp the c.lui encoding is c.addi16sp.
static void test_c_addi16sp_expands_to_addi_on_sp(void) {
  ASSERT_EXPANDS(0x7101, ADDI(2, 2, -512)); // c.addi16sp sp, -512
  ASSERT_EXPANDS(0x617d, ADDI(2, 2, 496));  // c.addi16sp sp, 496
  ASSERT_EXPANDS(0x6141, ADDI(2, 2, 16));   // c.addi16sp sp, 16
}

static void test_c_addi16sp_with_zero_is_reserved(void) {
  ASSERT_ILLEGAL(0x6101);
}

static void test_c_srli_and_c_srai_expand_to_shifts(void) {
  ASSERT_EXPANDS(0x800d, SRLI(8, 8, 3));   // c.srli s0, 3
  ASSERT_EXPANDS(0x8785, SRAI(15, 15, 1)); // c.srai a5, 1
}

static void test_c_andi_expands_to_andi(void) {
  ASSERT_EXPANDS(0x997d, ANDI(10, 10, -1)); // c.andi a0, -1
  ASSERT_EXPANDS(0x893d, ANDI(10, 10, 15)); // c.andi a0, 15
}

static void test_register_arithmetic_expands(void) {
  ASSERT_EXPANDS(0x8c05, SUB(8, 8, 9));    // c.sub s0, s1
  ASSERT_EXPANDS(0x8d2d, XOR(10, 10, 11)); // c.xor a0, a1
  ASSERT_EXPANDS(0x8e55, OR(12, 12, 13));  // c.or a2, a3
  ASSERT_EXPANDS(0x8f7d, AND(14, 14, 15)); // c.and a4, a5
}

// Bit 12 set selects the word forms; only subw and addw are defined.
static void test_unused_word_arithmetic_is_reserved(void) {
  ASSERT_ILLEGAL(0x9c45);
  ASSERT_ILLEGAL(0x9c65);
}

// The jump offset is scattered over bits 12:2; the offsets below set
// alternating bits so that each piece lands in its own place.
static void test_c_j_expands_to_jal_without_link(void) {
  ASSERT_EXPANDS(0xb001, JAL(0, -2048)); // c.j .-2048
  ASSERT_EXPANDS(0xaffd, JAL(0, 2046));  // c.j .+2046
  ASSERT_EXPANDS(0xab91, JAL(0, 1364));  // c.j .+1364
  ASSERT_EXPANDS(0xa46d, JAL(0, 682));   // c.j .+682
}

static void test_c_beqz_and_c_bnez_compare_with_x0(void) {
  ASSERT_EXPANDS(0xd001, BEQ(8, 0, -256)); // c.beqz s0, .-256
  ASSERT_EXPANDS(0xcffd, BEQ(15, 0, 254)); // c.beqz a5, .+254
  ASSERT_EXPANDS(0xe54d, BNE(10, 0, 170)); // c.bnez a0, .+170
  ASSERT_EXPANDS(0xe931, BNE(10, 0, 84));  // c.bnez a0, .+84
}

// Quadrant 2 (bits 1:0 = 10): full registers and the stack pointer.

static void test_c_mv_expands_to_add_from_x0(void) {
  ASSERT_EXPANDS(0x852e, ADD(10, 0, 11)); // c.mv a0, a1
}

static void test_c_add_expands_to_add(void) {
  ASSERT_EXPANDS(0x9636, ADD(12, 12, 13)); // c.add a2, a3
}

// With rs2 = 0 the same funct4 values mean jumps through a register.

static void test_c_jr_expands_to_jalr_without_link(void) {
  ASSERT_EXPANDS(0x8282, JALR(0, 0, 5)); // c.jr t0
}

static void test_c_jr_from_x0_is_reserved(void) {
  ASSERT_ILLEGAL(0x8002); // c.jr x0
}

static void test_c_jalr_expands_to_jalr_linking_ra(void) {
  ASSERT_EXPANDS(0x9782, JALR(1, 0, 15)); // c.jalr a5
}

// c.jalr with rs1 = 0 is c.ebreak.
static void test_c_ebreak_expands_to_ebreak(void) {
  ASSERT_EXPANDS(0x9002, EBREAK); // c.ebreak
}

// The shift amount sits where c.addi keeps its immediate, but unsigned.
static void test_c_slli_expands_to_slli(void) {
  ASSERT_EXPANDS(0x040e, SLLI(8, 8, 3));    // c.slli s0, 3
  ASSERT_EXPANDS(0x057e, SLLI(10, 10, 31)); // c.slli a0, 31
}

static void test_c_lwsp_expands_to_lw_from_sp(void) {
  ASSERT_EXPANDS(0x557e, LW(10, 252, 2)); // c.lwsp a0, 252(sp)
  ASSERT_EXPANDS(0x4312, LW(6, 4, 2));    // c.lwsp t1, 4(sp)
}

static void test_c_lwsp_into_x0_is_reserved(void) { ASSERT_ILLEGAL(0x4002); }

static void test_c_swsp_expands_to_sw_to_sp(void) {
  ASSERT_EXPANDS(0xdfaa, SW(10, 252, 2)); // c.swsp a0, 252(sp)
  ASSERT_EXPANDS(0xc21a, SW(6, 4, 2));    // c.swsp t1, 4(sp)
}

#if XLEN == 64

// RV64 spends the RV32 c.flw/c.fsw encodings on doubleword loads and stores,
// and the RV32 c.jal encoding on c.addiw.

static void test_c_ld_and_c_sd_expand_to_doubleword_access(void) {
  ASSERT_EXPANDS(0x6588, LD(10, 8, 11));  // c.ld a0, 8(a1)
  ASSERT_EXPANDS(0x7fe4, LD(9, 248, 15)); // c.ld s1, 248(a5)
  ASSERT_EXPANDS(0xe588, SD(10, 8, 11));  // c.sd a0, 8(a1)
  ASSERT_EXPANDS(0xffe4, SD(9, 248, 15)); // c.sd s1, 248(a5)
}

static void test_c_ldsp_and_c_sdsp_expand_to_doubleword_access(void) {
  ASSERT_EXPANDS(0x757e, LD(10, 504, 2)); // c.ldsp a0, 504(sp)
  ASSERT_EXPANDS(0x6322, LD(6, 8, 2));    // c.ldsp t1, 8(sp)
  ASSERT_EXPANDS(0xffaa, SD(10, 504, 2)); // c.sdsp a0, 504(sp)
  ASSERT_EXPANDS(0xe41a, SD(6, 8, 2));    // c.sdsp t1, 8(sp)
}

static void test_c_ldsp_into_x0_is_reserved(void) { ASSERT_ILLEGAL(0x6002); }

static void test_c_addiw_expands_to_addiw(void) {
  ASSERT_EXPANDS(0x357d, ADDIW(10, 10, -1)); // c.addiw a0, -1
  ASSERT_EXPANDS(0x24fd, ADDIW(9, 9, 31));   // c.addiw s1, 31
}

static void test_c_addiw_into_x0_is_reserved(void) { ASSERT_ILLEGAL(0x2005); }

static void test_word_arithmetic_expands(void) {
  ASSERT_EXPANDS(0x9c05, SUBW(8, 8, 9));    // c.subw s0, s1
  ASSERT_EXPANDS(0x9d2d, ADDW(10, 10, 11)); // c.addw a0, a1
}

// Bit 12 is shamt[5]: RV64 shifts by up to 63.
static void test_shifts_reach_63(void) {
  ASSERT_EXPANDS(0x907d, SRLI(8, 8, 63));   // c.srli s0, 63
  ASSERT_EXPANDS(0x9781, SRAI(15, 15, 32)); // c.srai a5, 32
  ASSERT_EXPANDS(0x157e, SLLI(10, 10, 63)); // c.slli a0, 63
}

#else

// The same encodings as c.ld/c.sd/c.ldsp/c.sdsp on RV64.
static void test_c_flw_and_c_fsw_are_illegal(void) {
  ASSERT_ILLEGAL(0x61c8); // c.flw fa0, 4(a1)
  ASSERT_ILLEGAL(0xe1c8); // c.fsw fa0, 4(a1)
  ASSERT_ILLEGAL(0x6512); // c.flwsp fa0, 4(sp)
  ASSERT_ILLEGAL(0xe22a); // c.fswsp fa0, 4(sp)
}

// The c.addiw encoding on RV64.
static void test_c_jal_expands_to_jal_linking_ra(void) {
  ASSERT_EXPANDS(0x3001, JAL(1, -2048)); // c.jal .-2048
  ASSERT_EXPANDS(0x246d, JAL(1, 682));   // c.jal .+682
}

// The c.subw and c.addw encodings on RV64.
static void test_word_arithmetic_is_reserved(void) {
  ASSERT_ILLEGAL(0x9c05);
  ASSERT_ILLEGAL(0x9d2d);
}

// A shift by 32 or more does not fit an RV32 register.
static void test_shifts_by_32_are_reserved(void) {
  ASSERT_ILLEGAL(0x9001); // c.srli s0, 32
  ASSERT_ILLEGAL(0x9401); // c.srai s0, 32
  ASSERT_ILLEGAL(0x1502); // c.slli a0, 32
}

#endif

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_zero_halfword_is_illegal);
  RUN_TEST(test_c_addi4spn_expands_to_addi_from_sp);
  RUN_TEST(test_c_addi4spn_with_zero_offset_is_reserved);
  RUN_TEST(test_c_lw_expands_to_lw);
  RUN_TEST(test_c_sw_expands_to_sw);
  RUN_TEST(test_quadrant_0_funct3_100_is_reserved);
  RUN_TEST(test_c_fld_and_c_fsd_are_illegal);
  RUN_TEST(test_c_addi_expands_to_addi);
  RUN_TEST(test_c_nop_expands_to_nop);
  RUN_TEST(test_c_li_expands_to_addi_from_x0);
  RUN_TEST(test_hints_writing_x0_expand_normally);
  RUN_TEST(test_c_lui_expands_to_lui);
  RUN_TEST(test_c_lui_with_zero_is_reserved);
  RUN_TEST(test_c_addi16sp_expands_to_addi_on_sp);
  RUN_TEST(test_c_addi16sp_with_zero_is_reserved);
  RUN_TEST(test_c_srli_and_c_srai_expand_to_shifts);
  RUN_TEST(test_c_andi_expands_to_andi);
  RUN_TEST(test_register_arithmetic_expands);
  RUN_TEST(test_unused_word_arithmetic_is_reserved);
  RUN_TEST(test_c_j_expands_to_jal_without_link);
  RUN_TEST(test_c_beqz_and_c_bnez_compare_with_x0);
  RUN_TEST(test_c_mv_expands_to_add_from_x0);
  RUN_TEST(test_c_add_expands_to_add);
  RUN_TEST(test_c_jr_expands_to_jalr_without_link);
  RUN_TEST(test_c_jr_from_x0_is_reserved);
  RUN_TEST(test_c_jalr_expands_to_jalr_linking_ra);
  RUN_TEST(test_c_ebreak_expands_to_ebreak);
  RUN_TEST(test_c_slli_expands_to_slli);
  RUN_TEST(test_c_lwsp_expands_to_lw_from_sp);
  RUN_TEST(test_c_lwsp_into_x0_is_reserved);
  RUN_TEST(test_c_swsp_expands_to_sw_to_sp);
#if XLEN == 64
  RUN_TEST(test_c_ld_and_c_sd_expand_to_doubleword_access);
  RUN_TEST(test_c_ldsp_and_c_sdsp_expand_to_doubleword_access);
  RUN_TEST(test_c_ldsp_into_x0_is_reserved);
  RUN_TEST(test_c_addiw_expands_to_addiw);
  RUN_TEST(test_c_addiw_into_x0_is_reserved);
  RUN_TEST(test_word_arithmetic_expands);
  RUN_TEST(test_shifts_reach_63);
#else
  RUN_TEST(test_c_flw_and_c_fsw_are_illegal);
  RUN_TEST(test_c_jal_expands_to_jal_linking_ra);
  RUN_TEST(test_word_arithmetic_is_reserved);
  RUN_TEST(test_shifts_by_32_are_reserved);
#endif
  return UNITY_END();
}
