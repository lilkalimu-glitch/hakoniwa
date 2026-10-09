#include "gic.h"
#include "hw.h"

#define GICD_CTLR       (GICD_BASE + 0x000)
#define GICD_ISENABLER  (GICD_BASE + 0x100)
#define GICD_IPRIORITYR (GICD_BASE + 0x400)
#define GICD_ITARGETSR  (GICD_BASE + 0x800)
#define GICD_ICFGR      (GICD_BASE + 0xC00)

#define GICC_CTLR       (GICC_BASE + 0x000)
#define GICC_PMR        (GICC_BASE + 0x004)

void gic_init(void)
{
    mmio_write32(GICD_CTLR, 3);     // Verteiler an
    mmio_write32(GICC_PMR, 0xF0);   // alle Prioritäten bis 0xF0 durchlassen
    mmio_write32(GICC_CTLR, 3);     // CPU-Schnittstelle an
}

void gic_enable_interrupt(unsigned id)
{
    mmio_write8(GICD_IPRIORITYR + id, 0x80);   // mittlere Priorität
    mmio_write8(GICD_ITARGETSR + id, 0x01);    // an CPU 0 schicken

    // Pegel-gesteuert (level): Bit 2*(id%16)+1 im Konfigurationsregister löschen
    uintptr_t cfg = GICD_ICFGR + 4 * (id / 16);
    mmio_write32(cfg, mmio_read32(cfg) & ~(1u << (2 * (id % 16) + 1)));

    mmio_write32(GICD_ISENABLER + 4 * (id / 32), 1u << (id % 32));
}
