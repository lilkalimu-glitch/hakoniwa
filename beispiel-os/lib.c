#include <stdarg.h>
#include "lib.h"
#include "uart.h"

// Die Schleifen benutzen "volatile", damit der Compiler sie nicht selbst
// wieder in einen Aufruf von memset/memcpy umwandelt.

void *memset(void *dest, int value, size_t count)
{
    volatile uint8_t *d = dest;
    while (count--) {
        *d++ = (uint8_t)value;
    }
    return dest;
}

void *memcpy(void *dest, const void *src, size_t count)
{
    volatile uint8_t *d = dest;
    const volatile uint8_t *s = src;
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t count)
{
    volatile uint8_t *d = dest;
    const volatile uint8_t *s = src;
    if (d < s) {
        while (count--) {
            *d++ = *s++;
        }
    } else {
        d += count;
        s += count;
        while (count--) {
            *--d = *--s;
        }
    }
    return dest;
}

int memcmp(const void *a, const void *b, size_t count)
{
    const uint8_t *x = a;
    const uint8_t *y = b;
    for (size_t i = 0; i < count; i++) {
        if (x[i] != y[i]) {
            return x[i] < y[i] ? -1 : 1;
        }
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (uint8_t)*a - (uint8_t)*b;
}

int strncmp(const char *a, const char *b, size_t count)
{
    while (count && *a && *a == *b) {
        a++;
        b++;
        count--;
    }
    if (count == 0) {
        return 0;
    }
    return (uint8_t)*a - (uint8_t)*b;
}

const char *skip_spaces(const char *s)
{
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

int parse_int(const char **text, int64_t *out)
{
    const char *s = skip_spaces(*text);
    int negative = 0;
    if (*s == '-' || *s == '+') {
        negative = (*s == '-');
        s++;
    }
    if (*s < '0' || *s > '9') {
        return 0;
    }
    int64_t value = 0;
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        s++;
    }
    *out = negative ? -value : value;
    *text = s;
    return 1;
}

void kputs(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

// Schreibt eine Zahl als Text in buf (rückwärts) und gibt die Länge zurück.
static int format_number(char *buf, uint64_t value, unsigned base, int upper)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int n = 0;
    do {
        buf[n++] = digits[value % base];
        value /= base;
    } while (value != 0);
    return n;
}

static void put_padded(const char *text, int length, int reversed,
                       int width, int left_align, char pad)
{
    int padding = width > length ? width - length : 0;
    if (!left_align) {
        while (padding-- > 0) {
            uart_putc(pad);
        }
    }
    for (int i = 0; i < length; i++) {
        uart_putc(reversed ? text[length - 1 - i] : text[i]);
    }
    if (left_align) {
        while (padding-- > 0) {
            uart_putc(' ');
        }
    }
}

void kprintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    for (const char *f = format; *f; f++) {
        if (*f != '%') {
            uart_putc(*f);
            continue;
        }
        f++;

        int left_align = 0;
        char pad = ' ';
        if (*f == '-') {
            left_align = 1;
            f++;
        }
        if (*f == '0') {
            pad = '0';
            f++;
        }
        int width = 0;
        while (*f >= '0' && *f <= '9') {
            width = width * 10 + (*f - '0');
            f++;
        }
        int is_long = 0;
        while (*f == 'l') {
            is_long = 1;
            f++;
        }

        char buf[24];
        switch (*f) {
        case 's': {
            const char *s = va_arg(args, const char *);
            if (!s) {
                s = "(null)";
            }
            put_padded(s, (int)strlen(s), 0, width, left_align, ' ');
            break;
        }
        case 'c':
            buf[0] = (char)va_arg(args, int);
            put_padded(buf, 1, 0, width, left_align, ' ');
            break;
        case 'd':
        case 'i': {
            int64_t v = is_long ? va_arg(args, int64_t) : va_arg(args, int);
            uint64_t magnitude = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
            int n = format_number(buf, magnitude, 10, 0);
            if (v < 0) {
                buf[n++] = '-';
            }
            put_padded(buf, n, 1, width, left_align, pad);
            break;
        }
        case 'u': {
            uint64_t v = is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
            int n = format_number(buf, v, 10, 0);
            put_padded(buf, n, 1, width, left_align, pad);
            break;
        }
        case 'x':
        case 'X': {
            uint64_t v = is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
            int n = format_number(buf, v, 16, *f == 'X');
            put_padded(buf, n, 1, width, left_align, pad);
            break;
        }
        case 'p': {
            uint64_t v = (uint64_t)(uintptr_t)va_arg(args, void *);
            kputs("0x");
            int n = format_number(buf, v, 16, 0);
            put_padded(buf, n, 1, width, left_align, '0');
            break;
        }
        case '%':
            uart_putc('%');
            break;
        case '\0':
            va_end(args);
            return;
        default:
            uart_putc('%');
            uart_putc(*f);
            break;
        }
    }

    va_end(args);
}
