// Adressen der virtuellen Hardware und kleine Helfer für den Zugriff.
#pragma once
#include <stdint.h>

// Speicherplan der QEMU-Maschine "virt" (Gerätebausteine)
#define RAM_BASE        0x40000000UL   // hier beginnt der RAM, dort liegt auch der Device Tree
#define UART_BASE       0x09000000UL   // serielle Schnittstelle PL011
#define GICD_BASE       0x08000000UL   // Interrupt-Verteiler (GICv2)
#define GICC_BASE       0x08010000UL   // Interrupt-Schnittstelle der CPU (GICv2)
#define UART_IRQ        33             // Interrupt-Nummer des UART (SPI 1 = 32 + 1)

static inline void mmio_write32(uintptr_t addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}

static inline uint32_t mmio_read32(uintptr_t addr)
{
    return *(volatile uint32_t *)addr;
}

static inline void mmio_write8(uintptr_t addr, uint8_t value)
{
    *(volatile uint8_t *)addr = value;
}

// Systemregister der CPU lesen, z. B. read_sysreg(midr_el1)
#define read_sysreg(reg) ({                                   \
    uint64_t _value;                                          \
    __asm__ volatile("mrs %0, " #reg : "=r"(_value));         \
    _value;                                                   \
})
