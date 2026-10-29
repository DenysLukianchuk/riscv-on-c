#include "riscv/devices/plic.h"
#include <stdlib.h>

#define PLIC_SOURCES 32
#define PLIC_CONTEXTS 2
#define PRIORITY_MASK 0x7
#define SOURCE_BIT(source) (1u << (source))
#define REGISTER_SIZE 4

#define PENDING_BASE 0x1000
#define ENABLE_BASE 0x2000
#define ENABLE_STRIDE 0x80
#define CONTEXT_BASE 0x200000
#define CONTEXT_STRIDE 0x1000

#define PLIC_SIZE 0x600000

struct Plic {
  uint8_t priority[PLIC_SOURCES];
  uint32_t raised;
  uint32_t claimed;
  uint32_t enable[PLIC_CONTEXTS];
  uint8_t threshold[PLIC_CONTEXTS];
};

enum PlicRegister { PRIORITY, PENDING, ENABLE, THRESHOLD, CLAIM, RESERVED };

Plic *plic_create(void) {
  Plic *plic = (Plic *)malloc(sizeof(Plic));
  *plic = (Plic){0};
  return plic;
}

void plic_destroy(Plic *plic) { free(plic); }

static enum PlicRegister register_at(uxlen_t offset) {
  if (offset < PENDING_BASE)
    return PRIORITY;
  if (offset < ENABLE_BASE)
    return offset == PENDING_BASE ? PENDING : RESERVED;
  if (offset < CONTEXT_BASE)
    return (offset - ENABLE_BASE) % ENABLE_STRIDE == 0 ? ENABLE : RESERVED;
  switch ((offset - CONTEXT_BASE) % CONTEXT_STRIDE) {
  case 0:
    return THRESHOLD;
  case REGISTER_SIZE:
    return CLAIM;
  default:
    return RESERVED;
  }
}

static unsigned source_at(uxlen_t offset) { return offset / REGISTER_SIZE; }

static unsigned enable_context_at(uxlen_t offset) {
  return (offset - ENABLE_BASE) / ENABLE_STRIDE;
}

static unsigned context_at(uxlen_t offset) {
  return (offset - CONTEXT_BASE) / CONTEXT_STRIDE;
}

static uint32_t pending(const Plic *plic) {
  return plic->raised & ~plic->claimed;
}

static unsigned best_source(const Plic *plic, unsigned context) {
  uint32_t candidates = pending(plic) & plic->enable[context];
  unsigned best = 0;
  unsigned best_priority = plic->threshold[context];
  for (unsigned source = 1; source < PLIC_SOURCES; source++) {
    if ((candidates & SOURCE_BIT(source)) &&
        plic->priority[source] > best_priority) {
      best = source;
      best_priority = plic->priority[source];
    }
  }
  return best;
}

static uint32_t claim(Plic *plic, unsigned context) {
  unsigned source = best_source(plic, context);
  if (source != 0)
    plic->claimed |= SOURCE_BIT(source);
  return source;
}

// Ignored unless the source is enabled for the context; source 0 never is.
static void complete(Plic *plic, unsigned context, uint32_t source) {
  if (source < PLIC_SOURCES && (plic->enable[context] & SOURCE_BIT(source)))
    plic->claimed &= ~SOURCE_BIT(source);
}

enum BusStatus plic_read(Plic *plic, uxlen_t offset, unsigned size,
                         uint64_t *value) {
  if (size != REGISTER_SIZE || offset % REGISTER_SIZE)
    return BUS_ERROR;
  unsigned source = source_at(offset);
  unsigned enable_context = enable_context_at(offset);
  unsigned context = context_at(offset);
  *value = 0;
  switch (register_at(offset)) {
  case PRIORITY:
    if (source < PLIC_SOURCES)
      *value = plic->priority[source];
    break;
  case PENDING:
    *value = pending(plic);
    break;
  case ENABLE:
    if (enable_context < PLIC_CONTEXTS)
      *value = plic->enable[enable_context];
    break;
  case THRESHOLD:
    if (context < PLIC_CONTEXTS)
      *value = plic->threshold[context];
    break;
  case CLAIM:
    if (context < PLIC_CONTEXTS)
      *value = claim(plic, context);
    break;
  case RESERVED:
    break;
  }
  return BUS_OK;
}

enum BusStatus plic_write(Plic *plic, uxlen_t offset, unsigned size,
                          uint64_t value) {
  if (size != REGISTER_SIZE || offset % REGISTER_SIZE)
    return BUS_ERROR;
  unsigned source = source_at(offset);
  unsigned enable_context = enable_context_at(offset);
  unsigned context = context_at(offset);
  switch (register_at(offset)) {
  case PRIORITY:
    // Source 0 does not exist; its priority stays 0.
    if (source != 0 && source < PLIC_SOURCES)
      plic->priority[source] = value & PRIORITY_MASK;
    break;
  case ENABLE:
    if (enable_context < PLIC_CONTEXTS)
      plic->enable[enable_context] = value & ~SOURCE_BIT(0);
    break;
  case THRESHOLD:
    if (context < PLIC_CONTEXTS)
      plic->threshold[context] = value & PRIORITY_MASK;
    break;
  case CLAIM:
    if (context < PLIC_CONTEXTS)
      complete(plic, context, value);
    break;
  case PENDING: // read-only: only the sources change it
  case RESERVED:
    break;
  }
  return BUS_OK;
}

void plic_set_source(Plic *plic, unsigned source, bool raised) {
  if (source == 0 || source >= PLIC_SOURCES)
    return;
  if (raised)
    plic->raised |= SOURCE_BIT(source);
  else
    plic->raised &= ~SOURCE_BIT(source);
}

bool plic_interrupt_pending(const Plic *plic, unsigned context) {
  return context < PLIC_CONTEXTS && best_source(plic, context) != 0;
}

static enum BusStatus bus_read(void *plic, uxlen_t offset, unsigned size,
                               uint64_t *value) {
  return plic_read(plic, offset, size, value);
}

static enum BusStatus bus_write(void *plic, uxlen_t offset, unsigned size,
                                uint64_t value) {
  return plic_write(plic, offset, size, value);
}

struct BusDevice plic_bus_device(Plic *plic, uxlen_t base) {
  return (struct BusDevice){.base = base,
                            .size = PLIC_SIZE,
                            .device = plic,
                            .read = bus_read,
                            .write = bus_write};
}
