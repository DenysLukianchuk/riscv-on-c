#ifndef RISCV_CPU_TRIGGER_H
#define RISCV_CPU_TRIGGER_H

#include "riscv/cpu/privilege.h"
#include "riscv/xlen.h"
#include <stdbool.h>

enum TriggerRegister {
  CSR_REG_TSELECT = 0x7A0,
  CSR_REG_TDATA1 = 0x7A1,
  CSR_REG_TDATA2 = 0x7A2,
  CSR_REG_TDATA3 = 0x7A3,
  CSR_REG_TINFO = 0x7A4
};

struct TriggerData {
  uxlen_t tdata1;
  uxlen_t tdata2;
};

#define TRIGGERS_COUNT 4

struct TriggerModule {
  uxlen_t tselect;
  struct TriggerData triggers[TRIGGERS_COUNT];
};

static inline bool is_trigger_register(uint16_t addr) {
  return addr >= CSR_REG_TSELECT && addr <= CSR_REG_TINFO;
}

void trigger_regs_init(struct TriggerModule *module);

uxlen_t trigger_regs_read(struct TriggerModule *module, uint16_t addr);
void trigger_regs_write(struct TriggerModule *module, uint16_t addr,
                        uxlen_t value);
bool trigger_match_execute(struct TriggerModule *module,
                           enum Privilege privilege, uxlen_t pc,
                           const uxlen_t *data, uxlen_t *tval);

bool trigger_match_load(struct TriggerModule *module, enum Privilege privilege,
                        uxlen_t addr, uint8_t size, const uxlen_t *data,
                        uxlen_t *tval);
bool trigger_match_store(struct TriggerModule *module, enum Privilege privilege,
                         uxlen_t addr, uint8_t size, const uxlen_t *data,
                         uxlen_t *tval);
bool trigger_match_exception(struct TriggerModule *module, enum Privilege from,
                             unsigned cause);
bool trigger_icount_fire(struct TriggerModule *module,
                         enum Privilege privilege);
void trigger_icount_count(struct TriggerModule *module,
                          enum Privilege privilege);

#endif // RISCV_CPU_TRIGGER_H