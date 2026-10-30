#include "riscv/cpu/trigger.h"
#include "riscv/cpu/decoder.h"
#include "riscv/cpu/trap_cause.h"
#include "riscv/util/bits.h"
#include <stddef.h>

enum TriggerType {
  TRIGGER_TYPE_MCONTROL = 2,
  TRIGGER_TYPE_ICOUNT = 3,
  TRIGGER_TYPE_ETRIGGER = 5,
  TRIGGER_TYPE_MCONTROL6 = 6,
  TRIGGER_TYPE_DISABLED = 15
};

#define TDATA1_TYPE_SHIFT (XLEN - 4)
#define TDATA1_TYPE(type) ((uxlen_t)(type) << TDATA1_TYPE_SHIFT)
#define TDATA1_TYPE_OF(tdata1)                                                 \
  ((enum TriggerType)((tdata1) >> TDATA1_TYPE_SHIFT))
#define TDATA1_TYPE_MCONTROL TDATA1_TYPE(TRIGGER_TYPE_MCONTROL)
#define TDATA1_TYPE_MCONTROL6 TDATA1_TYPE(TRIGGER_TYPE_MCONTROL6)
#define TDATA1_TYPE_ICOUNT TDATA1_TYPE(TRIGGER_TYPE_ICOUNT)
#define TDATA1_TYPE_ETRIGGER TDATA1_TYPE(TRIGGER_TYPE_ETRIGGER)
#define TDATA1_TYPE_DISABLED TDATA1_TYPE(TRIGGER_TYPE_DISABLED)

#define TINFO_TYPE(type) (1u << (type))
#define TINFO_VERSION_1 (1u << 24)
#define TINFO_SUPPORTED                                                        \
  (TINFO_VERSION_1 | TINFO_TYPE(TRIGGER_TYPE_MCONTROL) |                       \
   TINFO_TYPE(TRIGGER_TYPE_MCONTROL6) | TINFO_TYPE(TRIGGER_TYPE_DISABLED) |    \
   TINFO_TYPE(TRIGGER_TYPE_ICOUNT) | TINFO_TYPE(TRIGGER_TYPE_ETRIGGER))

// Fields mcontrol and mcontrol6 share.

#define MCONTROL_CHAIN ((uxlen_t)1 << 11)
#define MCONTROL_M ((uxlen_t)1 << 6)
#define MCONTROL_S ((uxlen_t)1 << 4)
#define MCONTROL_U ((uxlen_t)1 << 3)

enum McontrolAccess {
  MCONTROL_ACCESS_LOAD = 1 << 0,
  MCONTROL_ACCESS_STORE = 1 << 1,
  MCONTROL_ACCESS_EXECUTE = 1 << 2,
};

#define MCONTROL_COMMON                                                        \
  (MCONTROL_CHAIN | MCONTROL_M | MCONTROL_S | MCONTROL_U |                     \
   MCONTROL_ACCESS_EXECUTE | MCONTROL_ACCESS_STORE | MCONTROL_ACCESS_LOAD)

#define MCONTROL_MATCH_SHIFT 7
#define MCONTROL_MATCH_FIELD 0xFu
#define MCONTROL_MATCH(match) ((uxlen_t)(match) << MCONTROL_MATCH_SHIFT)
#define MCONTROL_MATCH_OF(tdata1)                                              \
  ((unsigned)((tdata1) >> MCONTROL_MATCH_SHIFT) & MCONTROL_MATCH_FIELD)

enum MatchType {
  MATCH_EQUAL = 0,
  MATCH_NAPOT = 1,
  MATCH_GE = 2,
  MATCH_LT = 3,
  MATCH_MASK_LOW = 4,
  MATCH_MASK_HIGH = 5,
  MATCH_NOT_EQUAL = 8,
  MATCH_NOT_NAPOT = 9,
  MATCH_NOT_MASK_LOW = 12,
  MATCH_NOT_MASK_HIGH = 13,
};

#define MATCH_NOT 8u
#define MATCH_BASE(match) ((match) & 7u)

enum SizeType {
  SIZE_ANY = 0,
  SIZE_8BIT = 1,
  SIZE_16BIT = 2,
  SIZE_32BIT = 3,
  SIZE_64BIT = 5,
};

// mcontrol (type 2).

#define MCONTROL_HIT ((uxlen_t)1 << 20)
#define MCONTROL_SELECT ((uxlen_t)1 << 19)
#define MCONTROL_WRITABLE (MCONTROL_COMMON | MCONTROL_HIT | MCONTROL_SELECT)

// The size code is split in two: sizelo holds its low two bits, sizehi (RV64
// only) the high two.
#define MCONTROL_SIZELO_SHIFT 16
#define MCONTROL_SIZEHI_SHIFT 21
#define SIZE_PART(value, shift) ((unsigned)((uxlen_t)(value) >> (shift)) & 3u)
#if XLEN == 64
#define MCONTROL_SIZE(code)                                                    \
  (((uxlen_t)SIZE_PART(code, 0) << MCONTROL_SIZELO_SHIFT) |                    \
   ((uxlen_t)SIZE_PART(code, 2) << MCONTROL_SIZEHI_SHIFT))
#define MCONTROL_SIZE_OF(tdata1)                                               \
  ((SIZE_PART(tdata1, MCONTROL_SIZEHI_SHIFT) << 2) |                           \
   SIZE_PART(tdata1, MCONTROL_SIZELO_SHIFT))
#else
#define MCONTROL_SIZE(code)                                                    \
  ((uxlen_t)SIZE_PART(code, 0) << MCONTROL_SIZELO_SHIFT)
#define MCONTROL_SIZE_OF(tdata1) SIZE_PART(tdata1, MCONTROL_SIZELO_SHIFT)
#endif

// mcontrol6 (type 6).

#define MCONTROL6_HIT1 ((uxlen_t)1 << 25)
#define MCONTROL6_HIT0 ((uxlen_t)1 << 22)
#define MCONTROL6_SELECT ((uxlen_t)1 << 21)
#define MCONTROL6_WRITABLE                                                     \
  (MCONTROL_COMMON | MCONTROL6_HIT1 | MCONTROL6_HIT0 | MCONTROL6_SELECT)

#define MCONTROL6_SIZE_SHIFT 16
#define MCONTROL6_SIZE_FIELD 0x7u
#define MCONTROL6_SIZE(code) ((uxlen_t)(code) << MCONTROL6_SIZE_SHIFT)
#define MCONTROL6_SIZE_OF(tdata1)                                              \
  ((unsigned)((tdata1) >> MCONTROL6_SIZE_SHIFT) & MCONTROL6_SIZE_FIELD)

// icount (type 3).

#define ICOUNT_HIT ((uxlen_t)1 << 24)
#define ICOUNT_COUNT_SHIFT 10
#define ICOUNT_COUNT_FIELD 0x3FFFu
#define ICOUNT_COUNT_MASK ((uxlen_t)ICOUNT_COUNT_FIELD << ICOUNT_COUNT_SHIFT)
#define ICOUNT_M ((uxlen_t)1 << 9)
#define ICOUNT_PENDING ((uxlen_t)1 << 8)
#define ICOUNT_S ((uxlen_t)1 << 7)
#define ICOUNT_U ((uxlen_t)1 << 6)
#define ICOUNT_WRITABLE                                                        \
  (ICOUNT_HIT | ICOUNT_COUNT_MASK | ICOUNT_M | ICOUNT_PENDING | ICOUNT_S |     \
   ICOUNT_U)

// etrigger (type 5).

#define ETRIGGER_HIT ((uxlen_t)1 << (XLEN - 6))
#define ETRIGGER_M ((uxlen_t)1 << 9)
#define ETRIGGER_S ((uxlen_t)1 << 7)
#define ETRIGGER_U ((uxlen_t)1 << 6)
#define ETRIGGER_WRITABLE (ETRIGGER_HIT | ETRIGGER_M | ETRIGGER_S | ETRIGGER_U)

#define EXCEPTION_BIT(cause) ((uxlen_t)1 << (cause))
#define ETRIGGER_EXCEPTIONS                                                    \
  (EXCEPTION_BIT(TRAP_INSTRUCTION_ADDRESS_MISALIGNED) |                        \
   EXCEPTION_BIT(TRAP_INSTRUCTION_ACCESS_FAULT) |                              \
   EXCEPTION_BIT(TRAP_ILLEGAL_INSTRUCTION) | EXCEPTION_BIT(TRAP_BREAKPOINT) |  \
   EXCEPTION_BIT(TRAP_LOAD_ADDRESS_MISALIGNED) |                               \
   EXCEPTION_BIT(TRAP_LOAD_ACCESS_FAULT) |                                     \
   EXCEPTION_BIT(TRAP_STORE_ADDRESS_MISALIGNED) |                              \
   EXCEPTION_BIT(TRAP_STORE_ACCESS_FAULT) | EXCEPTION_BIT(TRAP_ECALL_FROM_U) | \
   EXCEPTION_BIT(TRAP_ECALL_FROM_S) | EXCEPTION_BIT(TRAP_ECALL_FROM_M))

#define HALF_BITS (XLEN / 2)
#define HALF_ONES (((uxlen_t)1 << HALF_BITS) - 1)

struct MatchControl {
  unsigned modes; // one bit per enum Privilege
  unsigned access;
  unsigned match;
  unsigned size;
  bool chain;
  bool select;
};

struct TriggerAccess {
  enum McontrolAccess kind;
  enum Privilege privilege;
  uxlen_t addr;
  uint8_t size;
  const uxlen_t *data;
};

static void trigger_data_init(struct TriggerData *data) {
  data->tdata1 = TDATA1_TYPE_DISABLED;
  data->tdata2 = 0;
}

static void update_armed(struct TriggerModule *module) {
  module->armed = false;
  for (int i = 0; i < TRIGGERS_COUNT; i++)
    if (TDATA1_TYPE_OF(module->triggers[i].tdata1) != TDATA1_TYPE_DISABLED)
      module->armed = true;
}

void trigger_regs_init(struct TriggerModule *module) {
  module->tselect = 0;
  for (int i = 0; i < TRIGGERS_COUNT; i++)
    trigger_data_init(&module->triggers[i]);
  update_armed(module);
}

static struct TriggerData *selected_data(struct TriggerModule *module) {
  return &module->triggers[module->tselect];
}

// icount and etrigger keep m, s and u at the same bits.
static bool enabled_in(const struct TriggerData *trigger,
                       enum Privilege privilege) {
  static const uxlen_t MODE_BITS[] = {
      [PRIV_U] = ICOUNT_U, [PRIV_S] = ICOUNT_S, [PRIV_M] = ICOUNT_M};
  return trigger->tdata1 & MODE_BITS[privilege];
}

static bool has_type(const struct TriggerData *trigger, enum TriggerType type) {
  return TDATA1_TYPE_OF(trigger->tdata1) == type;
}

uxlen_t trigger_regs_read(struct TriggerModule *module, uint16_t addr) {
  switch (addr) {
  case CSR_REG_TSELECT:
    return module->tselect;
  case CSR_REG_TDATA1:
    return selected_data(module)->tdata1;
  case CSR_REG_TDATA2:
    return selected_data(module)->tdata2;
  case CSR_REG_TDATA3:
    return 0;
  case CSR_REG_TINFO:
    return TINFO_SUPPORTED;
  default:
    return 0;
  }
}

static bool match_supported(unsigned match) {
  switch (match) {
  case MATCH_EQUAL:
  case MATCH_GE:
  case MATCH_LT:
  case MATCH_MASK_LOW:
  case MATCH_MASK_HIGH:
  case MATCH_NOT_EQUAL:
  case MATCH_NOT_MASK_LOW:
  case MATCH_NOT_MASK_HIGH:
    return true;
  default:
    return false;
  }
}

static bool size_supported(unsigned size) {
  switch (size) {
  case SIZE_ANY:
  case SIZE_8BIT:
  case SIZE_16BIT:
  case SIZE_32BIT:
#if XLEN == 64
  case SIZE_64BIT:
#endif
    return true;
  default:
    return false;
  }
}

static uint8_t size_bytes(unsigned code) {
  switch (code) {
  case SIZE_8BIT:
    return 1;
  case SIZE_16BIT:
    return 2;
  case SIZE_32BIT:
    return 4;
  case SIZE_64BIT:
    return 8;
  default:
    return 0;
  }
}

static uxlen_t legalize_mcontrol(uxlen_t value) {
  uxlen_t tdata = TDATA1_TYPE_MCONTROL | (value & MCONTROL_WRITABLE);
  unsigned size = MCONTROL_SIZE_OF(value);
  if (size_supported(size))
    tdata |= MCONTROL_SIZE(size);
  return tdata;
}

static uxlen_t legalize_mcontrol6(uxlen_t value) {
  uxlen_t tdata = TDATA1_TYPE_MCONTROL6 | (value & MCONTROL6_WRITABLE);
  unsigned size = MCONTROL6_SIZE_OF(value);
  if (size_supported(size))
    tdata |= MCONTROL6_SIZE(size);
  return tdata;
}

// Chain and match sit at the same bits in mcontrol and mcontrol6.
static uxlen_t legalize_match_fields(uxlen_t tdata, uxlen_t value,
                                     unsigned index) {
  // There is no next trigger to chain to.
  if (index == TRIGGERS_COUNT - 1)
    tdata &= ~MCONTROL_CHAIN;
  unsigned match = MCONTROL_MATCH_OF(value);
  if (match_supported(match))
    tdata |= MCONTROL_MATCH(match);
  return tdata;
}

static uxlen_t legalize_tdata1(uxlen_t value, unsigned index) {
  switch (TDATA1_TYPE_OF(value)) {
  case TRIGGER_TYPE_MCONTROL:
    return legalize_match_fields(legalize_mcontrol(value), value, index);
  case TRIGGER_TYPE_MCONTROL6:
    return legalize_match_fields(legalize_mcontrol6(value), value, index);
  case TRIGGER_TYPE_ICOUNT:
    return TDATA1_TYPE_ICOUNT | (value & ICOUNT_WRITABLE);
  case TRIGGER_TYPE_ETRIGGER:
    return TDATA1_TYPE_ETRIGGER | (value & ETRIGGER_WRITABLE);
  default:
    return TDATA1_TYPE_DISABLED;
  }
}

static uxlen_t legalize_tdata2(struct TriggerModule *module, uxlen_t value) {
  if (has_type(selected_data(module), TRIGGER_TYPE_ETRIGGER))
    return value & ETRIGGER_EXCEPTIONS;
  return value;
}

void trigger_regs_write(struct TriggerModule *module, uint16_t addr,
                        uxlen_t value) {
  switch (addr) {
  case CSR_REG_TSELECT:
    if (value < TRIGGERS_COUNT)
      module->tselect = value;
    break;
  case CSR_REG_TDATA1:
    selected_data(module)->tdata1 = legalize_tdata1(value, module->tselect);
    update_armed(module);
    break;
  case CSR_REG_TDATA2:
    selected_data(module)->tdata2 = legalize_tdata2(module, value);
    break;
  case CSR_REG_TDATA3:
  case CSR_REG_TINFO:
    // No context matching in tdata3; tinfo is read-only.
    break;
  default:
    break;
  }
}

static bool decode_match_control(uxlen_t tdata1, struct MatchControl *mc) {
  switch (TDATA1_TYPE_OF(tdata1)) {
  case TRIGGER_TYPE_MCONTROL:
    mc->size = MCONTROL_SIZE_OF(tdata1);
    mc->select = tdata1 & MCONTROL_SELECT;
    break;
  case TRIGGER_TYPE_MCONTROL6:
    mc->size = MCONTROL6_SIZE_OF(tdata1);
    mc->select = tdata1 & MCONTROL6_SELECT;
    break;
  default:
    return false;
  }
  mc->modes = (tdata1 & MCONTROL_M ? 1u << PRIV_M : 0) |
              (tdata1 & MCONTROL_S ? 1u << PRIV_S : 0) |
              (tdata1 & MCONTROL_U ? 1u << PRIV_U : 0);
  mc->access = tdata1 & (MCONTROL_ACCESS_LOAD | MCONTROL_ACCESS_STORE |
                         MCONTROL_ACCESS_EXECUTE);
  mc->match = MCONTROL_MATCH_OF(tdata1);
  mc->chain = tdata1 & MCONTROL_CHAIN;
  return true;
}

static bool compare(unsigned match, uxlen_t value, uxlen_t tdata2) {
  uxlen_t mask = tdata2 >> HALF_BITS;
  uxlen_t expected = tdata2 & HALF_ONES;
  bool result;
  switch (MATCH_BASE(match)) {
  case MATCH_EQUAL:
    result = value == tdata2;
    break;
  case MATCH_GE:
    result = value >= tdata2;
    break;
  case MATCH_LT:
    result = value < tdata2;
    break;
  case MATCH_MASK_LOW:
    result = (value & HALF_ONES & mask) == expected;
    break;
  case MATCH_MASK_HIGH:
    result = (value >> HALF_BITS & mask) == expected;
    break;
  default:
    result = false;
    break;
  }

  if (match & MATCH_NOT)
    result = !result;
  return result;
}

static bool applies_to(const struct MatchControl *mc,
                       const struct TriggerAccess *access) {
  uint8_t size = size_bytes(mc->size);
  return (mc->modes & (1u << access->privilege)) &&
         (mc->access & access->kind) && (size == 0 || size == access->size);
}

static bool matches_address(const struct MatchControl *mc, uxlen_t tdata2,
                            const struct TriggerAccess *access, uxlen_t *tval) {
  unsigned bytes = access->kind == MCONTROL_ACCESS_EXECUTE ? 1 : access->size;
  for (unsigned i = 0; i < bytes; i++) {
    uxlen_t addr = access->addr + i;
    if (compare(mc->match, addr, tdata2)) {
      *tval = addr;
      return true;
    }
  }
  return false;
}

static bool matches_data(const struct MatchControl *mc, uxlen_t tdata2,
                         const struct TriggerAccess *access, uxlen_t *tval) {
  if (access->data == NULL || !compare(mc->match, *access->data,
                                       truncate_to_size(tdata2, access->size)))
    return false;
  *tval = access->addr;
  return true;
}

static bool matches_access(const struct TriggerData *trigger,
                           const struct TriggerAccess *access, uxlen_t *tval) {
  struct MatchControl mc;
  if (!decode_match_control(trigger->tdata1, &mc) || !applies_to(&mc, access))
    return false;
  if (mc.select)
    return matches_data(&mc, trigger->tdata2, access, tval);
  return matches_address(&mc, trigger->tdata2, access, tval);
}

static bool chains_to_next(const struct TriggerData *trigger) {
  struct MatchControl mc;
  return decode_match_control(trigger->tdata1, &mc) && mc.chain;
}

static bool selects_data(const struct TriggerData *trigger) {
  struct MatchControl mc;
  return decode_match_control(trigger->tdata1, &mc) && mc.select;
}

static unsigned chain_end(const struct TriggerModule *module, unsigned start) {
  unsigned end = start;
  while (end + 1 < TRIGGERS_COUNT && chains_to_next(&module->triggers[end]))
    end++;
  return end;
}

static bool chain_has_data(const struct TriggerModule *module, unsigned start,
                           unsigned end) {
  for (unsigned i = start; i <= end; i++) {
    if (selects_data(&module->triggers[i]))
      return true;
  }
  return false;
}

static bool chain_matches(const struct TriggerModule *module, unsigned start,
                          unsigned end, const struct TriggerAccess *access,
                          uxlen_t *tval) {
  for (unsigned i = start; i <= end; i++) {
    if (!matches_access(&module->triggers[i], access, tval))
      return false;
  }
  return true;
}

static void mark_fired(struct TriggerData *trigger) {
  switch (TDATA1_TYPE_OF(trigger->tdata1)) {
  case TRIGGER_TYPE_MCONTROL:
    trigger->tdata1 |= MCONTROL_HIT;
    break;
  case TRIGGER_TYPE_MCONTROL6:
    trigger->tdata1 &= ~(MCONTROL6_HIT0 | MCONTROL6_HIT1);
    trigger->tdata1 |= MCONTROL6_HIT0;
    break;
  default:
    break;
  }
}

static bool trigger_match_access(struct TriggerModule *module,
                                 const struct TriggerAccess *access,
                                 uxlen_t *tval) {
  if (!module->armed)
    return false;
  bool fired = false;
  unsigned start = 0;
  while (start < TRIGGERS_COUNT) {
    unsigned end = chain_end(module, start);
    uxlen_t chain_tval = 0;
    if (chain_has_data(module, start, end) == (access->data != NULL) &&
        chain_matches(module, start, end, access, &chain_tval)) {
      for (unsigned i = start; i <= end; i++)
        mark_fired(&module->triggers[i]);
      if (!fired || chain_tval < *tval)
        *tval = chain_tval;
      fired = true;
    }
    start = end + 1;
  }
  return fired;
}

bool trigger_match_execute(struct TriggerModule *module,
                           enum Privilege privilege, uxlen_t pc,
                           const uxlen_t *data, uxlen_t *tval) {
  struct TriggerAccess access = {.kind = MCONTROL_ACCESS_EXECUTE,
                                 .privilege = privilege,
                                 .addr = pc,
                                 .size = INSTRUCTION_WIDTH_BYTES,
                                 .data = data};
  return trigger_match_access(module, &access, tval);
}

bool trigger_match_load(struct TriggerModule *module, enum Privilege privilege,
                        uxlen_t addr, uint8_t size, const uxlen_t *data,
                        uxlen_t *tval) {
  struct TriggerAccess access = {.kind = MCONTROL_ACCESS_LOAD,
                                 .privilege = privilege,
                                 .addr = addr,
                                 .size = size,
                                 .data = data};
  return trigger_match_access(module, &access, tval);
}

bool trigger_match_store(struct TriggerModule *module, enum Privilege privilege,
                         uxlen_t addr, uint8_t size, const uxlen_t *data,
                         uxlen_t *tval) {
  struct TriggerAccess access = {.kind = MCONTROL_ACCESS_STORE,
                                 .privilege = privilege,
                                 .addr = addr,
                                 .size = size,
                                 .data = data};
  return trigger_match_access(module, &access, tval);
}

bool trigger_match_exception(struct TriggerModule *module, enum Privilege from,
                             unsigned cause) {
  if (!module->armed)
    return false;
  bool fired = false;
  for (unsigned i = 0; i < TRIGGERS_COUNT; i++) {
    struct TriggerData *trigger = &module->triggers[i];
    if (has_type(trigger, TRIGGER_TYPE_ETRIGGER) && enabled_in(trigger, from) &&
        (trigger->tdata2 & EXCEPTION_BIT(cause))) {
      trigger->tdata1 |= ETRIGGER_HIT;
      fired = true;
    }
  }
  return fired;
}

bool trigger_icount_fire(struct TriggerModule *module,
                         enum Privilege privilege) {
  bool fired = false;
  for (unsigned i = 0; i < TRIGGERS_COUNT; i++) {
    struct TriggerData *trigger = &module->triggers[i];
    if (has_type(trigger, TRIGGER_TYPE_ICOUNT) &&
        (trigger->tdata1 & ICOUNT_PENDING) && enabled_in(trigger, privilege)) {
      trigger->tdata1 = (trigger->tdata1 & ~ICOUNT_PENDING) | ICOUNT_HIT;
      fired = true;
    }
  }
  return fired;
}

void trigger_icount_count(struct TriggerModule *module,
                          enum Privilege privilege) {
  for (unsigned i = 0; i < TRIGGERS_COUNT; i++) {
    struct TriggerData *trigger = &module->triggers[i];
    if (!has_type(trigger, TRIGGER_TYPE_ICOUNT) ||
        !enabled_in(trigger, privilege))
      continue;
    unsigned count =
        (trigger->tdata1 >> ICOUNT_COUNT_SHIFT) & ICOUNT_COUNT_FIELD;
    if (count == 0)
      continue;
    if (count == 1)
      trigger->tdata1 |= ICOUNT_PENDING;
    trigger->tdata1 = (trigger->tdata1 & ~ICOUNT_COUNT_MASK) |
                      ((uxlen_t)(count - 1) << ICOUNT_COUNT_SHIFT);
  }
}
