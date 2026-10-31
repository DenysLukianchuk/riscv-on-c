#include "riscv/host/ram_size.h"
#include "riscv/system/board.h"
#include "riscv/xlen.h"
#include <ctype.h>

static unsigned suffix_shift(char suffix) {
  switch (toupper((unsigned char)suffix)) {
  case 'K':
    return 10;
  case '\0':
  case 'M':
    return 20;
  case 'G':
    return 30;
  }
  return 0;
}

bool ram_size_parse(const char *text, uint64_t *bytes) {
  uint64_t count = 0;
  const char *p = text;
  for (; isdigit((unsigned char)*p); p++) {
    if (count > (UINT64_MAX - 9) / 10)
      return false;
    count = count * 10 + (uint64_t)(*p - '0');
  }
  if (p == text || (*p != '\0' && p[1] != '\0'))
    return false;
  unsigned shift = suffix_shift(*p);
  if (shift == 0 || count == 0 || count > UINT64_MAX >> shift)
    return false;

  uint64_t size = count << shift;
  uint64_t room = (uint64_t)(uxlen_t)-1 - RAM_BASE + 1;
  if (size > room)
    return false;
  *bytes = size;
  return true;
}
