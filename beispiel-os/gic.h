// Interrupt-Controller (GICv2): leitet Interrupts der Geräte an die CPU weiter.
#pragma once

void gic_init(void);
void gic_enable_interrupt(unsigned id);
