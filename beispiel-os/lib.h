// Kleine Standardfunktionen, die es ohne Betriebssystem nicht gibt.
#pragma once
#include <stddef.h>
#include <stdint.h>

void *memset(void *dest, int value, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
void *memmove(void *dest, const void *src, size_t count);
int memcmp(const void *a, const void *b, size_t count);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t count);

// Text ausgeben, ähnlich wie printf: %s %c %d %u %x %p %% (mit l für 64 Bit)
void kprintf(const char *format, ...);
void kputs(const char *s);

// Liest eine ganze Zahl ab *text (mit Vorzeichen). Gibt 0 zurück, wenn keine Zahl da ist.
int parse_int(const char **text, int64_t *out);
const char *skip_spaces(const char *s);
