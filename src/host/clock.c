// clock_gettime is POSIX, which strict C11 leaves out of the headers.
#define _POSIX_C_SOURCE 200809L

#include "riscv/host/clock.h"
#include <time.h>

uint64_t host_clock_ns(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t)now.tv_sec * 1000000000u + (uint64_t)now.tv_nsec;
}
