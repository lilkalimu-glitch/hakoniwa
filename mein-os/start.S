// MEIN OS – Schritt 0: ein Buchstabe
//
// Das ist das ganze Programm. Es läuft direkt auf dem virtuellen Prozessor,
// ohne irgendein Betriebssystem darunter.
//
// Ein Register ist ein kleiner Speicherplatz im Prozessor (x0, x1, ...).
// w1 ist dasselbe Register wie x1, nur mit 32 statt 64 Bit.

    .section .text.start, "ax"
    .global _start
_start:
    mov  x0, #0x09000000   // 1. Adresse der Textausgabe in Register x0 legen
    mov  w1, #65           // 2. Die Zahl 65 in Register w1 legen (65 bedeutet "A")
    strb w1, [x0]          // 3. Diese Zahl an die Adresse aus x0 schreiben: "A" erscheint
1:  wfi                    // 4. Prozessor schlafen legen
    b    1b                // 5. Zurück zu Befehl 4 springen (für immer)
