// Liest die Hardwarebeschreibung (Device Tree), die QEMU an den RAM-Anfang legt.
#pragma once
#include <stdint.h>

struct dtb_info {
    int found;               // 1, wenn ein gültiger Device Tree gefunden wurde
    uint32_t total_size;     // Größe in Bytes
    uint64_t ram_base;       // Anfang des RAM
    uint64_t ram_size;       // Größe des RAM in Bytes
    int node_count;          // Anzahl der Einträge (Knoten)
    int cpu_count;           // Anzahl der CPU-Kerne
    const char *compatible;  // Name der Maschine
};

void dtb_read(uintptr_t address, struct dtb_info *info);
