// The riscv-tests virtual-memory environment brings its own string functions
// (env/v/string.c) but includes the C library's headers for their
// declarations. A bare cross compiler has no C library, so these stand in.
#ifndef RISCV_TESTS_STRING_H
#define RISCV_TESTS_STRING_H

#include <stddef.h>

void *memcpy(void *dest, const void *src, size_t len);
void *memset(void *dest, int byte, size_t len);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
char *strcpy(char *dest, const char *src);

#endif
