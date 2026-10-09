// Hier startet die virtuelle CPU, sobald QEMU den Kernel geladen hat.
// Die CPU läuft auf Stufe EL1 (Kernel-Rechte), die MMU ist noch aus.

    .section .text.boot, "ax"
    .global _start
_start:
    // Nur Kern 0 arbeitet. Weitere Kerne (falls vorhanden) schlafen.
    mrs     x1, mpidr_el1
    and     x1, x1, #0xff
    cbnz    x1, park

    // Stack einrichten (Ende des reservierten Bereichs, siehe link.ld)
    ldr     x1, =__stack_top
    mov     sp, x1

    // Gleitkomma- und SIMD-Befehle erlauben (CPACR_EL1.FPEN = 0b11)
    mov     x1, #(3 << 20)
    msr     cpacr_el1, x1
    isb

    // Tabelle für Ausnahmen (Fehler, Interrupts) eintragen
    ldr     x1, =vector_table
    msr     vbar_el1, x1
    isb

    // Den .bss-Bereich mit Nullen füllen
    ldr     x1, =__bss_start
    ldr     x2, =__bss_end
1:  cmp     x1, x2
    b.hs    2f
    str     xzr, [x1], #8
    b       1b
2:
    bl      kernel_main

    // kernel_main kehrt nie zurück. Falls doch: anhalten.
park:
    wfi
    b       park

    .ltorg
