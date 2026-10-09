// PSCI: Schnittstelle, über die ein Kernel die Maschine aus- oder neu startet.
#pragma once
#include <stdint.h>

#define PSCI_VERSION      0x84000000u
#define PSCI_SYSTEM_OFF   0x84000008u
#define PSCI_SYSTEM_RESET 0x84000009u

uint64_t psci_call(uint32_t function);
