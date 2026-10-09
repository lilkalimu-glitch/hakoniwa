/*
 * Speicherplan von "Mein OS". Diese Datei musst du erst mal nicht ändern.
 *
 * Der RAM der virtuellen Maschine beginnt bei 0x40000000. Dort legt QEMU eine
 * Beschreibung der Hardware ab (Device Tree). Dein Programm beginnt deshalb
 * 1 MiB dahinter, bei 0x40100000.
 */
ENTRY(_start)

SECTIONS
{
    . = 0x40100000;

    .text : {
        KEEP(*(.text.start))
        *(.text .text.*)
    }

    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(16) {
        *(.data .data.*)
    }

    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
    }

    /* Platz für einen Stack (wird erst gebraucht, wenn du C benutzt) */
    . = ALIGN(16);
    . = . + 0x10000;
    __stack_top = .;

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame_hdr)
    }
}
