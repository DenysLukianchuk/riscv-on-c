// Written by Claude Code.

#include "riscv/cpu/fpu.h"
#include "riscv/util/float.h"
#include <fenv.h>
#include <math.h>
#include <string.h>

// The host FPU does the arithmetic. Operands and results go through volatile
// variables so that the compiler cannot move an operation across the calls
// that set the rounding mode and read the exception flags.

__extension__ typedef _Float128 wide;

static float to_float(uint32_t bits) {
  float value;
  memcpy(&value, &bits, sizeof value);
  return value;
}

static double to_double(uint64_t bits) {
  double value;
  memcpy(&value, &bits, sizeof value);
  return value;
}

static uint32_t from_float(float value) {
  if (isnan(value))
    return NAN_SINGLE;
  uint32_t bits;
  memcpy(&bits, &value, sizeof bits);
  return bits;
}

static uint64_t from_double(double value) {
  if (isnan(value))
    return NAN_DOUBLE;
  uint64_t bits;
  memcpy(&bits, &value, sizeof bits);
  return bits;
}

static bool is_signaling_s(uint32_t a) {
  return fclass(a, SINGLE_EXP_BITS, SINGLE_MANT_BITS) == FCLASS_SIGNALING_NAN;
}

static bool is_signaling_d(uint64_t a) {
  return fclass(a, DOUBLE_EXP_BITS, DOUBLE_MANT_BITS) == FCLASS_SIGNALING_NAN;
}

static int host_rounding(enum FpRounding rm) {
  switch (rm) {
  case FP_RTZ:
    return FE_TOWARDZERO;
  case FP_RDN:
    return FE_DOWNWARD;
  case FP_RUP:
    return FE_UPWARD;
  case FP_RNE:
  case FP_RMM:
    break;
  }
  return FE_TONEAREST;
}

static void begin(enum FpRounding rm) {
  feclearexcept(FE_ALL_EXCEPT);
  fesetround(host_rounding(rm));
}

static uint8_t end(void) {
  int raised = fetestexcept(FE_ALL_EXCEPT);
  fesetround(FE_TONEAREST);

  uint8_t flags = 0;
  if (raised & FE_INEXACT)
    flags |= FP_FLAG_NX;
  if (raised & FE_UNDERFLOW)
    flags |= FP_FLAG_UF;
  if (raised & FE_OVERFLOW)
    flags |= FP_FLAG_OF;
  if (raised & FE_DIVBYZERO)
    flags |= FP_FLAG_DZ;
  if (raised & FE_INVALID)
    flags |= FP_FLAG_NV;
  return flags;
}

// The host has no RMM, so it runs as RNE and the ties RNE broke toward zero
// are moved away. exact must hold the unrounded result whenever a tie is
// possible; quotients and square roots never are ties.
static float tie_away_s(wide exact, float nearest) {
  if (!isfinite(nearest))
    return nearest;
  wide diff = exact - (wide)nearest;
  if (diff == 0 || (diff < 0) != (bool)signbit(nearest))
    return nearest;
  float away = nextafterf(nearest, copysignf(INFINITY, nearest));
  return diff * 2 == (wide)away - (wide)nearest ? away : nearest;
}

static double tie_away_d(wide exact, double nearest) {
  if (!isfinite(nearest))
    return nearest;
  wide diff = exact - (wide)nearest;
  if (diff == 0 || (diff < 0) != (bool)signbit(nearest))
    return nearest;
  double away = nextafter(nearest, copysign(INFINITY, nearest));
  return diff * 2 == (wide)away - (wide)nearest ? away : nearest;
}

static wide exact_arith(enum FpOp op, wide a, wide b) {
  switch (op) {
  case FP_ADD:
    return a + b;
  case FP_SUB:
    return a - b;
  case FP_MUL:
  case FP_DIV:
    break;
  }
  return a * b;
}

uint32_t fpu_arith_s(enum FpOp op, uint32_t a, uint32_t b, enum FpRounding rm,
                     uint8_t *flags) {
  volatile float x = to_float(a), y = to_float(b);
  volatile float result;
  begin(rm);
  switch (op) {
  case FP_ADD:
    result = x + y;
    break;
  case FP_SUB:
    result = x - y;
    break;
  case FP_MUL:
    result = x * y;
    break;
  case FP_DIV:
    result = x / y;
    break;
  }
  *flags = end();
  if (rm == FP_RMM && op != FP_DIV)
    result = tie_away_s(exact_arith(op, x, y), result);
  return from_float(result);
}

uint64_t fpu_arith_d(enum FpOp op, uint64_t a, uint64_t b, enum FpRounding rm,
                     uint8_t *flags) {
  volatile double x = to_double(a), y = to_double(b);
  volatile double result;
  begin(rm);
  switch (op) {
  case FP_ADD:
    result = x + y;
    break;
  case FP_SUB:
    result = x - y;
    break;
  case FP_MUL:
    result = x * y;
    break;
  case FP_DIV:
    result = x / y;
    break;
  }
  *flags = end();
  if (rm == FP_RMM && op != FP_DIV)
    result = tie_away_d(exact_arith(op, x, y), result);
  return from_double(result);
}

uint32_t fpu_sqrt_s(uint32_t a, enum FpRounding rm, uint8_t *flags) {
  volatile float x = to_float(a);
  begin(rm);
  volatile float result = sqrtf(x);
  *flags = end();
  return from_float(result);
}

uint64_t fpu_sqrt_d(uint64_t a, enum FpRounding rm, uint8_t *flags) {
  volatile double x = to_double(a);
  begin(rm);
  volatile double result = sqrt(x);
  *flags = end();
  return from_double(result);
}

// IEEE 754 leaves it to the implementation whether infinity times zero plus a
// quiet NaN is invalid; RISC-V says it is.
static bool infinity_times_zero(double x, double y) {
  return (isinf(x) && y == 0) || (x == 0 && isinf(y));
}

uint32_t fpu_fma_s(uint32_t a, uint32_t b, uint32_t c, enum FpRounding rm,
                   uint8_t *flags) {
  volatile float x = to_float(a), y = to_float(b), z = to_float(c);
  begin(rm);
  volatile float result = fmaf(x, y, z);
  *flags = end();
  if (infinity_times_zero(x, y))
    *flags |= FP_FLAG_NV;
  if (rm == FP_RMM)
    result = tie_away_s((wide)x * y + z, result);
  return from_float(result);
}

uint64_t fpu_fma_d(uint64_t a, uint64_t b, uint64_t c, enum FpRounding rm,
                   uint8_t *flags) {
  volatile double x = to_double(a), y = to_double(b), z = to_double(c);
  begin(rm);
  volatile double result = fma(x, y, z);
  *flags = end();
  if (infinity_times_zero(x, y))
    *flags |= FP_FLAG_NV;
  if (rm == FP_RMM)
    result = tie_away_d((wide)x * y + z, result);
  return from_double(result);
}

// Equal operands differ in their bits only when they are +0 and -0, and the
// minimum of the two is -0.
uint32_t fpu_min_max_s(uint32_t a, uint32_t b, bool max, uint8_t *flags) {
  *flags = is_signaling_s(a) || is_signaling_s(b) ? FP_FLAG_NV : 0;
  float x = to_float(a), y = to_float(b);
  if (isnan(x) && isnan(y))
    return NAN_SINGLE;
  if (isnan(x))
    return b;
  if (isnan(y))
    return a;
  if (x == y)
    return max ? a & b : a | b;
  return (x < y) != max ? a : b;
}

uint64_t fpu_min_max_d(uint64_t a, uint64_t b, bool max, uint8_t *flags) {
  *flags = is_signaling_d(a) || is_signaling_d(b) ? FP_FLAG_NV : 0;
  double x = to_double(a), y = to_double(b);
  if (isnan(x) && isnan(y))
    return NAN_DOUBLE;
  if (isnan(x))
    return b;
  if (isnan(y))
    return a;
  if (x == y)
    return max ? a & b : a | b;
  return (x < y) != max ? a : b;
}

static bool compare(enum FpCompare cmp, double x, double y) {
  switch (cmp) {
  case FP_LE:
    return x <= y;
  case FP_LT:
    return x < y;
  case FP_EQ:
    break;
  }
  return x == y;
}

// feq is quiet: only a signaling NaN is invalid. flt and fle are invalid for
// any NaN.
bool fpu_compare_s(enum FpCompare cmp, uint32_t a, uint32_t b, uint8_t *flags) {
  float x = to_float(a), y = to_float(b);
  if (isnan(x) || isnan(y)) {
    bool invalid = cmp != FP_EQ || is_signaling_s(a) || is_signaling_s(b);
    *flags = invalid ? FP_FLAG_NV : 0;
    return false;
  }
  *flags = 0;
  return compare(cmp, x, y);
}

bool fpu_compare_d(enum FpCompare cmp, uint64_t a, uint64_t b, uint8_t *flags) {
  double x = to_double(a), y = to_double(b);
  if (isnan(x) || isnan(y)) {
    bool invalid = cmp != FP_EQ || is_signaling_d(a) || is_signaling_d(b);
    *flags = invalid ? FP_FLAG_NV : 0;
    return false;
  }
  *flags = 0;
  return compare(cmp, x, y);
}

static double round_to_integral(double x, enum FpRounding rm) {
  switch (rm) {
  case FP_RTZ:
    return trunc(x);
  case FP_RDN:
    return floor(x);
  case FP_RUP:
    return ceil(x);
  case FP_RMM:
    return round(x);
  case FP_RNE:
    break;
  }
  return nearbyint(x);
}

static uint64_t to_int(double x, enum FpInt type, enum FpRounding rm,
                       uint8_t *flags) {
  double r = round_to_integral(x, rm);
  bool fits = false;
  uint64_t low = 0, high = 0;
  switch (type) {
  case FP_INT_W:
    fits = r >= -0x1p31 && r <= 0x1p31 - 1;
    low = (uint64_t)INT32_MIN;
    high = INT32_MAX;
    break;
  case FP_INT_WU:
    fits = r >= 0 && r <= 0x1p32 - 1;
    high = UINT64_MAX;
    break;
  case FP_INT_L:
    fits = r >= -0x1p63 && r < 0x1p63;
    low = (uint64_t)INT64_MIN;
    high = INT64_MAX;
    break;
  case FP_INT_LU:
    fits = r >= 0 && r < 0x1p64;
    high = UINT64_MAX;
    break;
  }
  if (!fits) {
    *flags = FP_FLAG_NV;
    return x < 0 ? low : high;
  }
  *flags = r != x ? FP_FLAG_NX : 0;
  switch (type) {
  case FP_INT_W:
    return (uint64_t)(int64_t)(int32_t)r;
  case FP_INT_WU:
    return (uint64_t)(int64_t)(int32_t)(uint32_t)r;
  case FP_INT_L:
    return (uint64_t)(int64_t)r;
  case FP_INT_LU:
    break;
  }
  return (uint64_t)r;
}

uint64_t fpu_s_to_int(uint32_t a, enum FpInt type, enum FpRounding rm,
                      uint8_t *flags) {
  return to_int(to_float(a), type, rm, flags);
}

uint64_t fpu_d_to_int(uint64_t a, enum FpInt type, enum FpRounding rm,
                      uint8_t *flags) {
  return to_int(to_double(a), type, rm, flags);
}

static wide int_value(uint64_t value, enum FpInt type) {
  switch (type) {
  case FP_INT_W:
    return (int32_t)value;
  case FP_INT_WU:
    return (uint32_t)value;
  case FP_INT_L:
    return (int64_t)value;
  case FP_INT_LU:
    break;
  }
  return value;
}

uint32_t fpu_int_to_s(uint64_t value, enum FpInt type, enum FpRounding rm,
                      uint8_t *flags) {
  volatile uint64_t v = value;
  volatile float result;
  begin(rm);
  switch (type) {
  case FP_INT_W:
    result = (int32_t)v;
    break;
  case FP_INT_WU:
    result = (uint32_t)v;
    break;
  case FP_INT_L:
    result = (int64_t)v;
    break;
  case FP_INT_LU:
    result = v;
    break;
  }
  *flags = end();
  if (rm == FP_RMM)
    result = tie_away_s(int_value(value, type), result);
  return from_float(result);
}

uint64_t fpu_int_to_d(uint64_t value, enum FpInt type, enum FpRounding rm,
                      uint8_t *flags) {
  volatile uint64_t v = value;
  volatile double result;
  begin(rm);
  switch (type) {
  case FP_INT_W:
    result = (int32_t)v;
    break;
  case FP_INT_WU:
    result = (uint32_t)v;
    break;
  case FP_INT_L:
    result = (int64_t)v;
    break;
  case FP_INT_LU:
    result = v;
    break;
  }
  *flags = end();
  if (rm == FP_RMM)
    result = tie_away_d(int_value(value, type), result);
  return from_double(result);
}

uint64_t fpu_s_to_d(uint32_t a, uint8_t *flags) {
  volatile float x = to_float(a);
  begin(FP_RNE);
  volatile double result = x;
  *flags = end();
  return from_double(result);
}

uint32_t fpu_d_to_s(uint64_t a, enum FpRounding rm, uint8_t *flags) {
  volatile double x = to_double(a);
  begin(rm);
  volatile float result = (float)x;
  *flags = end();
  if (rm == FP_RMM)
    result = tie_away_s(x, result);
  return from_float(result);
}
