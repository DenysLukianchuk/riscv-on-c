#ifndef RISCV_FDT_H
#define RISCV_FDT_H

#include <stdint.h>

typedef struct Fdt Fdt;

Fdt *fdt_create(void);
void fdt_destroy(Fdt *fdt);
void fdt_begin_node(Fdt *fdt, const char *name);
void fdt_end_node(Fdt *fdt);
void fdt_property(Fdt *fdt, const char *name, const void *value, uint32_t len);
void fdt_property_u32(Fdt *fdt, const char *name, uint32_t value);
void fdt_property_u64(Fdt *fdt, const char *name, uint64_t value);
void fdt_property_string(Fdt *fdt, const char *name, const char *value);
void fdt_property_cells(Fdt *fdt, const char *name, const uint32_t *cells,
                        uint32_t count);
const uint8_t *fdt_finish(Fdt *fdt, uint32_t *size);

#endif // RISCV_FDT_H
