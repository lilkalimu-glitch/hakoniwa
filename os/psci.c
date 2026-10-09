#include "psci.h"

// Bei QEMUs "virt" ohne EL2/EL3 beantwortet QEMU selbst den Befehl "hvc".
uint64_t psci_call(uint32_t function)
{
    register uint64_t x0 __asm__("x0") = function;
    __asm__ volatile("hvc #0"
                     : "+r"(x0)
                     :
                     : "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10",
                       "x11", "x12", "x13", "x14", "x15", "x16", "x17", "memory");
    return x0;
}
