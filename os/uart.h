// Treiber für die serielle Schnittstelle (PL011 UART).
#pragma once

void uart_init(void);
void uart_putc(char c);
int uart_getc(void);       // wartet, bis ein Zeichen da ist
int uart_has_data(void);
