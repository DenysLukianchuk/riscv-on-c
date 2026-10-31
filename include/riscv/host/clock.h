#ifndef RISCV_HOST_CLOCK_H
#define RISCV_HOST_CLOCK_H

#include <stdint.h>

// Nanoseconds on a monotonic clock.
uint64_t host_clock_ns(void);

#endif // RISCV_HOST_CLOCK_H
