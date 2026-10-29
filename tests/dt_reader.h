#ifndef TESTS_DT_READER_H
#define TESTS_DT_READER_H

// Reads a flattened device tree the way a guest would, so tests can ask for
// a property by node path instead of counting words. Written from the
// Devicetree Specification v0.4, chapter 5, independently of the emulator.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define DT_BEGIN_NODE 0x1
#define DT_END_NODE 0x2
#define DT_PROP 0x3
#define DT_NOP 0x4
#define DT_END 0x9
#define DT_MAX_PATH 256
#define DT_MAX_DEPTH 16

struct DtProperty {
  const uint8_t *value;
  uint32_t len;
};

static inline uint32_t dt_be32(const uint8_t *p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 |
         p[3];
}

static inline uint32_t dt_align(uint32_t len) { return (len + 3) & ~3u; }

// Walks the structure block. With name NULL, reports whether the node at
// path exists; otherwise finds that node's property.
static inline bool dt_lookup(const uint8_t *blob, const char *path,
                             const char *name, struct DtProperty *out) {
  const uint8_t *p = blob + dt_be32(blob + 8);
  const char *strings = (const char *)blob + dt_be32(blob + 12);
  char current[DT_MAX_PATH] = "";
  size_t parent_length[DT_MAX_DEPTH];
  int depth = 0;
  for (;;) {
    uint32_t token = dt_be32(p);
    p += 4;
    switch (token) {
    case DT_BEGIN_NODE: {
      const char *node = (const char *)p;
      parent_length[depth++] = strlen(current);
      if (depth == 1)
        strcpy(current, "/");
      else {
        if (strcmp(current, "/") != 0)
          strcat(current, "/");
        strcat(current, node);
      }
      if (!name && strcmp(current, path) == 0)
        return true;
      p += dt_align(strlen(node) + 1);
      break;
    }
    case DT_END_NODE:
      current[parent_length[--depth]] = '\0';
      break;
    case DT_PROP: {
      uint32_t len = dt_be32(p);
      const char *prop_name = strings + dt_be32(p + 4);
      if (name && strcmp(current, path) == 0 && strcmp(prop_name, name) == 0) {
        out->value = p + 8;
        out->len = len;
        return true;
      }
      p += 8 + dt_align(len);
      break;
    }
    case DT_NOP:
      break;
    default:
      return false;
    }
  }
}

static inline bool dt_has_node(const uint8_t *blob, const char *path) {
  return dt_lookup(blob, path, NULL, NULL);
}

// The n-th 32-bit cell of a property.
static inline uint32_t dt_cell(struct DtProperty property, unsigned n) {
  return dt_be32(property.value + 4 * n);
}

#endif // TESTS_DT_READER_H
