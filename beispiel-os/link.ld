/*
 * Speicherplan des Kernels.
 *
 * Bei QEMUs Maschine "virt" beginnt der RAM bei 0x40000000.
 * Dort legt QEMU die Hardwarebeschreibung (Device Tree) ab.
 * Der Kernel beginnt deshalb 1 MiB dahinter, bei 0x40100000.
 */
ENTRY(_start)

SECTIONS
{
    . = 0x40100000;
    __kernel_start = .;

    .text : {
        KEEP(*(.text.boot))
        *(.text.vectors)
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

    /* 64 KiB Stack direkt hinter dem Kernel */
    . = ALIGN(16);
    __stack_bottom = .;
    . = . + 0x10000;
    __stack_top = .;

    __kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame_hdr)
    }
}
