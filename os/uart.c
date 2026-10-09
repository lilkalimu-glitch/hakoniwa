#include "uart.h"
#include "hw.h"

// Register des PL011 (Abstand zur Basisadresse)
#define UART_DR    (UART_BASE + 0x00)  // Daten: schreiben = senden, lesen = empfangen
#define UART_FR    (UART_BASE + 0x18)  // Status
#define UART_IBRD  (UART_BASE + 0x24)  // Baudrate, ganzzahliger Teil
#define UART_FBRD  (UART_BASE + 0x28)  // Baudrate, Bruchteil
#define UART_LCRH  (UART_BASE + 0x2C)  // Zeichenformat
#define UART_CR    (UART_BASE + 0x30)  // Steuerung (an/aus)
#define UART_IMSC  (UART_BASE + 0x38)  // welche Interrupts aktiv sind
#define UART_ICR   (UART_BASE + 0x44)  // Interrupts löschen

#define FR_RXFE    (1u << 4)           // Empfangspuffer leer
#define FR_TXFF    (1u << 5)           // Sendepuffer voll

void uart_init(void)
{
    mmio_write32(UART_CR, 0);                 // aus, während wir einstellen
    mmio_write32(UART_ICR, 0x7FF);            // alte Interrupts löschen
    mmio_write32(UART_IBRD, 13);              // 115200 Baud bei 24 MHz Takt
    mmio_write32(UART_FBRD, 1);
    mmio_write32(UART_LCRH, (3u << 5) | (1u << 4));  // 8 Bit pro Zeichen, Puffer an
    mmio_write32(UART_IMSC, (1u << 4) | (1u << 6));  // Interrupt bei empfangenen Zeichen
    mmio_write32(UART_CR, (1u << 0) | (1u << 8) | (1u << 9));  // an, Senden an, Empfangen an
}

static void uart_raw_putc(char c)
{
    while (mmio_read32(UART_FR) & FR_TXFF) {
        // warten, bis im Sendepuffer Platz ist
    }
    mmio_write32(UART_DR, (uint8_t)c);
}

void uart_putc(char c)
{
    if (c == '\n') {
        uart_raw_putc('\r');
    }
    uart_raw_putc(c);
}

int uart_has_data(void)
{
    return !(mmio_read32(UART_FR) & FR_RXFE);
}

int uart_getc(void)
{
    while (!uart_has_data()) {
        // Die CPU schläft, bis ein Interrupt anliegt (hier: ein neues Zeichen).
        // So braucht die VM keine Rechenzeit, solange du nichts tippst.
        __asm__ volatile("wfi");
    }
    return (int)(mmio_read32(UART_DR) & 0xFF);
}
