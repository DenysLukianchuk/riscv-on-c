#include "riscv/cpu/csr.h"
#include "riscv_spec.h"
#include "unity.h"

// The F and D extensions keep their state in the f registers and fcsr.
// mstatus.FS tells software whether that state is in use: Off (any
// floating-point instruction or fcsr access is illegal), Initial, Clean or
// Dirty. An operating system saves the f registers on a context switch only
// when FS is Dirty, and SD sums this up in the top bit of mstatus.

static Csr *csrs;

void setUp(void) { csrs = csr_create(); }

void tearDown(void) { csr_destroy(csrs); }

// Failures are reported at the caller's line.
#define write_as(privilege, addr, value)                                       \
  write_as_at(__LINE__, privilege, addr, value)
#define read_as(privilege, addr) read_as_at(__LINE__, privilege, addr)

static void write_as_at(int line, enum Privilege privilege, uint16_t addr,
                        uxlen_t value) {
  UNITY_TEST_ASSERT_EQUAL_INT(CSR_OK, csr_write(csrs, addr, privilege, value),
                              line, "CSR write rejected");
}

static uxlen_t read_as_at(int line, enum Privilege privilege, uint16_t addr) {
  uxlen_t value = 0;
  UNITY_TEST_ASSERT_EQUAL_INT(CSR_OK, csr_read(csrs, addr, privilege, &value),
                              line, "CSR read rejected");
  return value;
}

static void set_fs(uxlen_t fs) { write_as(PRIV_M, MSTATUS, fs); }

static uxlen_t fs(void) { return read_as(PRIV_M, MSTATUS) & MSTATUS_FS; }

// mstatus.FS and SD.

static void test_fp_state_is_off_after_reset(void) {
  TEST_ASSERT_EQUAL_HEX64(MSTATUS_FS_OFF, fs());
}

static void test_mstatus_fs_holds_each_state(void) {
  const uxlen_t states[] = {MSTATUS_FS_OFF, MSTATUS_FS_INITIAL,
                            MSTATUS_FS_CLEAN, MSTATUS_FS_DIRTY};
  for (unsigned i = 0; i < sizeof states / sizeof states[0]; i++) {
    set_fs(states[i]);

    TEST_ASSERT_EQUAL_HEX64(states[i], fs());
  }
}

// The supervisor manages the f registers of its processes, so FS is an
// S-mode field as well.
static void test_sstatus_shows_and_writes_fs(void) {
  set_fs(MSTATUS_FS_CLEAN);
  TEST_ASSERT_EQUAL_HEX64(MSTATUS_FS_CLEAN,
                          read_as(PRIV_S, SSTATUS) & MSTATUS_FS);

  write_as(PRIV_S, SSTATUS, MSTATUS_FS_INITIAL);

  TEST_ASSERT_EQUAL_HEX64(MSTATUS_FS_INITIAL, fs());
}

static void test_sd_is_set_while_fs_is_dirty(void) {
  set_fs(MSTATUS_FS_DIRTY);
  TEST_ASSERT_EQUAL_HEX64(MSTATUS_SD, read_as(PRIV_M, MSTATUS) & MSTATUS_SD);
  TEST_ASSERT_EQUAL_HEX64(MSTATUS_SD, read_as(PRIV_S, SSTATUS) & MSTATUS_SD);

  set_fs(MSTATUS_FS_CLEAN);
  TEST_ASSERT_EQUAL_HEX64(0, read_as(PRIV_M, MSTATUS) & MSTATUS_SD);
}

// SD only reports FS; it cannot be written.
static void test_sd_cannot_be_written(void) {
  write_as(PRIV_M, MSTATUS, MSTATUS_SD | MSTATUS_FS_CLEAN);

  TEST_ASSERT_EQUAL_HEX64(0, read_as(PRIV_M, MSTATUS) & MSTATUS_SD);
}

// fcsr: the rounding mode frm (bits 7:5) and the accrued exception flags
// fflags (bits 4:0). frm and fflags are views of the same register.

static void test_fp_csrs_are_illegal_while_fs_is_off(void) {
  const uint16_t fp_csrs[] = {FFLAGS, FRM, FCSR};
  for (unsigned i = 0; i < sizeof fp_csrs / sizeof fp_csrs[0]; i++) {
    uxlen_t value = 0;
    TEST_ASSERT_EQUAL(CSR_ILLEGAL, csr_read(csrs, fp_csrs[i], PRIV_M, &value));
    TEST_ASSERT_EQUAL(CSR_ILLEGAL, csr_write(csrs, fp_csrs[i], PRIV_M, 0));
  }
}

// Round to nearest, ties to even, and no exception flags.
static void test_fcsr_is_zero_after_reset(void) {
  set_fs(MSTATUS_FS_INITIAL);

  TEST_ASSERT_EQUAL_HEX64(0, read_as(PRIV_M, FCSR));
}

static void test_fcsr_holds_rounding_mode_and_flags(void) {
  set_fs(MSTATUS_FS_INITIAL);

  write_as(PRIV_M, FCSR, 0xFF);

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_as(PRIV_M, FCSR));
  TEST_ASSERT_EQUAL_HEX64(0x7, read_as(PRIV_M, FRM));
  TEST_ASSERT_EQUAL_HEX64(0x1F, read_as(PRIV_M, FFLAGS));
}

static void test_fcsr_bits_above_frm_read_zero(void) {
  set_fs(MSTATUS_FS_INITIAL);

  write_as(PRIV_M, FCSR, ~(uxlen_t)0);

  TEST_ASSERT_EQUAL_HEX64(0xFF, read_as(PRIV_M, FCSR));
}

static void test_fflags_and_frm_write_their_part_of_fcsr(void) {
  set_fs(MSTATUS_FS_INITIAL);

  write_as(PRIV_M, FRM, 3);
  write_as(PRIV_M, FFLAGS, 0x15);

  TEST_ASSERT_EQUAL_HEX64(3 << 5 | 0x15, read_as(PRIV_M, FCSR));
}

static void test_fflags_write_leaves_frm(void) {
  set_fs(MSTATUS_FS_INITIAL);
  write_as(PRIV_M, FRM, 3);

  write_as(PRIV_M, FFLAGS, ~(uxlen_t)0);

  TEST_ASSERT_EQUAL_HEX64(3 << 5 | 0x1F, read_as(PRIV_M, FCSR));
}

static void test_frm_write_leaves_fflags(void) {
  set_fs(MSTATUS_FS_INITIAL);
  write_as(PRIV_M, FFLAGS, 0x15);

  write_as(PRIV_M, FRM, ~(uxlen_t)0);

  TEST_ASSERT_EQUAL_HEX64(7 << 5 | 0x15, read_as(PRIV_M, FCSR));
}

// fcsr belongs to the program: U-mode reads and writes it.
static void test_fp_csrs_are_accessible_from_user_mode(void) {
  set_fs(MSTATUS_FS_INITIAL);

  write_as(PRIV_U, FCSR, 0x21);

  TEST_ASSERT_EQUAL_HEX64(0x21, read_as(PRIV_U, FCSR));
}

// Changing fcsr changes the floating-point state, which then has to be saved.
static void test_writing_an_fp_csr_makes_fs_dirty(void) {
  const uint16_t fp_csrs[] = {FFLAGS, FRM, FCSR};
  for (unsigned i = 0; i < sizeof fp_csrs / sizeof fp_csrs[0]; i++) {
    set_fs(MSTATUS_FS_CLEAN);

    write_as(PRIV_M, fp_csrs[i], 0);

    TEST_ASSERT_EQUAL_HEX64(MSTATUS_FS_DIRTY, fs());
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_fp_state_is_off_after_reset);
  RUN_TEST(test_mstatus_fs_holds_each_state);
  RUN_TEST(test_sstatus_shows_and_writes_fs);
  RUN_TEST(test_sd_is_set_while_fs_is_dirty);
  RUN_TEST(test_sd_cannot_be_written);
  RUN_TEST(test_fp_csrs_are_illegal_while_fs_is_off);
  RUN_TEST(test_fcsr_is_zero_after_reset);
  RUN_TEST(test_fcsr_holds_rounding_mode_and_flags);
  RUN_TEST(test_fcsr_bits_above_frm_read_zero);
  RUN_TEST(test_fflags_and_frm_write_their_part_of_fcsr);
  RUN_TEST(test_fflags_write_leaves_frm);
  RUN_TEST(test_frm_write_leaves_fflags);
  RUN_TEST(test_fp_csrs_are_accessible_from_user_mode);
  RUN_TEST(test_writing_an_fp_csr_makes_fs_dirty);
  return UNITY_END();
}
