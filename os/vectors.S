// Ausnahme-Tabelle (Exception Vector Table) für EL1.
//
// Die CPU springt bei einer Ausnahme an eine feste Stelle dieser Tabelle.
// Es gibt 16 Einträge zu je 0x80 Bytes:
//   0- 3: aktuelle Stufe, Stack SP_EL0     (synchron, IRQ, FIQ, SError)
//   4- 7: aktuelle Stufe, Stack SP_EL1
//   8-11: niedrigere Stufe in AArch64
//  12-15: niedrigere Stufe in AArch32
// Jeder Eintrag ruft exception_handler(nummer, esr, elr, far) in C auf.

    .section .text.vectors, "ax"
    .balign 2048
    .global vector_table
vector_table:
    .irp idx, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
    .balign 0x80
    mov     x0, #\idx
    b       exception_entry
    .endr

exception_entry:
    mrs     x1, esr_el1     // Grund der Ausnahme
    mrs     x2, elr_el1     // Adresse des Befehls, der sie ausgelöst hat
    mrs     x3, far_el1     // betroffene Speicheradresse (bei Speicherfehlern)
    bl      exception_handler
1:  wfi
    b       1b
