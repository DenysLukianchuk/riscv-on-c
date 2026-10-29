#include "riscv/fdt.h"
#include <stdlib.h>
#include <string.h>

// Devicetree Specification v0.4, chapter 5. Every number is big-endian.
#define FDT_MAGIC 0xD00DFEED
#define FDT_VERSION 17
#define FDT_LAST_COMP_VERSION 16

#define HEADER_SIZE 40
// The reservation map is empty: only its terminating zero entry.
#define RSVMAP_SIZE 16

#define FDT_BEGIN_NODE 0x1
#define FDT_END_NODE 0x2
#define FDT_PROP 0x3
#define FDT_END 0x9

#define TOKEN_SIZE 4

struct Buffer {
  uint8_t *data;
  uint32_t size;
};

struct Fdt {
  struct Buffer structure;
  struct Buffer strings;
  struct Buffer blob;
};

Fdt *fdt_create(void) {
  Fdt *fdt = (Fdt *)malloc(sizeof(Fdt));
  *fdt = (Fdt){0};
  return fdt;
}

void fdt_destroy(Fdt *fdt) {
  free(fdt->structure.data);
  free(fdt->strings.data);
  free(fdt->blob.data);
  free(fdt);
}

static uint8_t *grow(struct Buffer *buffer, uint32_t len) {
  buffer->data = (uint8_t *)realloc(buffer->data, buffer->size + len);
  uint8_t *end = buffer->data + buffer->size;
  buffer->size += len;
  return end;
}

static void store_be32(uint8_t *p, uint32_t value) {
  p[0] = value >> 24;
  p[1] = value >> 16;
  p[2] = value >> 8;
  p[3] = value;
}

static void put_be32(struct Buffer *buffer, uint32_t value) {
  store_be32(grow(buffer, sizeof(uint32_t)), value);
}

static void put_bytes(struct Buffer *buffer, const void *bytes, uint32_t len) {
  if (len)
    memcpy(grow(buffer, len), bytes, len);
}

static void put_padded(struct Buffer *buffer, const void *bytes, uint32_t len) {
  put_bytes(buffer, bytes, len);
  uint32_t padding = (TOKEN_SIZE - len % TOKEN_SIZE) % TOKEN_SIZE;
  if (padding)
    memset(grow(buffer, padding), 0, padding);
}

void fdt_begin_node(Fdt *fdt, const char *name) {
  put_be32(&fdt->structure, FDT_BEGIN_NODE);
  put_padded(&fdt->structure, name, strlen(name) + 1);
}

void fdt_end_node(Fdt *fdt) { put_be32(&fdt->structure, FDT_END_NODE); }

void fdt_property(Fdt *fdt, const char *name, const void *value, uint32_t len) {
  put_be32(&fdt->structure, FDT_PROP);
  put_be32(&fdt->structure, len);
  put_be32(&fdt->structure, fdt->strings.size);
  put_padded(&fdt->structure, value, len);
  put_bytes(&fdt->strings, name, strlen(name) + 1);
}

void fdt_property_u32(Fdt *fdt, const char *name, uint32_t value) {
  uint8_t cell[4];
  store_be32(cell, value);
  fdt_property(fdt, name, cell, sizeof cell);
}

void fdt_property_u64(Fdt *fdt, const char *name, uint64_t value) {
  uint8_t cells[8];
  store_be32(cells, value >> 32);
  store_be32(cells + 4, value);
  fdt_property(fdt, name, cells, sizeof cells);
}

void fdt_property_string(Fdt *fdt, const char *name, const char *value) {
  fdt_property(fdt, name, value, strlen(value) + 1);
}

void fdt_property_cells(Fdt *fdt, const char *name, const uint32_t *cells,
                        uint32_t count) {
  struct Buffer temp_buffer = {NULL, 0};
  for (uint32_t i = 0; i < count; i++) {
    uint8_t cell[4];
    store_be32(cell, cells[i]);
    put_bytes(&temp_buffer, cell, sizeof cell);
  }
  fdt_property(fdt, name, temp_buffer.data, count * 4);
  free(temp_buffer.data);
}

const uint8_t *fdt_finish(Fdt *fdt, uint32_t *size) {
  if (fdt->blob.size != 0) {
    *size = fdt->blob.size;
    return fdt->blob.data;
  }

  uint32_t struct_size = fdt->structure.size + TOKEN_SIZE; // with FDT_END
  uint32_t off_struct = HEADER_SIZE + RSVMAP_SIZE;
  uint32_t off_strings = off_struct + struct_size;
  uint32_t total = off_strings + fdt->strings.size;

  struct Buffer *blob = &fdt->blob;
  blob->size = 0;
  put_be32(blob, FDT_MAGIC);
  put_be32(blob, total);
  put_be32(blob, off_struct);
  put_be32(blob, off_strings);
  put_be32(blob, HEADER_SIZE);
  put_be32(blob, FDT_VERSION);
  put_be32(blob, FDT_LAST_COMP_VERSION);
  put_be32(blob, 0);
  put_be32(blob, fdt->strings.size);
  put_be32(blob, struct_size);
  memset(grow(blob, RSVMAP_SIZE), 0, RSVMAP_SIZE);
  put_bytes(blob, fdt->structure.data, fdt->structure.size);
  put_be32(blob, FDT_END);
  put_bytes(blob, fdt->strings.data, fdt->strings.size);

  *size = blob->size;
  return blob->data;
}
