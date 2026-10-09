// HAKONIWA OS – ein kleines Betriebssystem für die virtuelle Maschine.
//
// Ablauf: boot.S richtet Stack und Ausnahme-Tabelle ein und ruft dann
// kernel_main() auf. Danach läuft eine kleine Kommandozeile (Shell).

#include <stdint.h>
#include "dtb.h"
#include "gic.h"
#include "hw.h"
#include "lib.h"
#include "psci.h"
#include "uart.h"

#define OS_VERSION "0.1"

// Symbole aus link.ld (ihre Adresse ist der Wert, der Inhalt ist egal)
extern char __kernel_start[], __kernel_end[], __stack_bottom[], __stack_top[];
extern char vector_table[];

static struct dtb_info dtb;
static uint64_t boot_ticks;

// ---------------------------------------------------------------------------
// Hilfsfunktionen

static uint64_t timer_frequency(void)
{
    return read_sysreg(cntfrq_el0);
}

static uint64_t timer_ticks(void)
{
    return read_sysreg(cntvct_el0);
}

static unsigned current_el(void)
{
    return (unsigned)((read_sysreg(currentel) >> 2) & 3);
}

static const char *cpu_name(uint64_t midr)
{
    unsigned implementer = (unsigned)((midr >> 24) & 0xff);
    unsigned part = (unsigned)((midr >> 4) & 0xfff);
    if (implementer != 0x41) {
        return "unbekannte CPU";
    }
    switch (part) {
    case 0xd03: return "Cortex-A53";
    case 0xd04: return "Cortex-A35";
    case 0xd05: return "Cortex-A55";
    case 0xd07: return "Cortex-A57";
    case 0xd08: return "Cortex-A72";
    case 0xd09: return "Cortex-A73";
    case 0xd0b: return "Cortex-A76";
    case 0xd0c: return "Neoverse-N1";
    case 0xd41: return "Cortex-A78";
    case 0xd47: return "Cortex-A710";
    case 0xd81: return "Cortex-A720";
    case 0xd87: return "Cortex-A725";
    default:    return "ARM-CPU";
    }
}

static uint64_t ram_end(void)
{
    if (dtb.found && dtb.ram_size) {
        return dtb.ram_base + dtb.ram_size;
    }
    return RAM_BASE + 128ul * 1024 * 1024;
}

// ---------------------------------------------------------------------------
// Befehle der Shell

static void cmd_hilfe(const char *args);

static void cmd_hallo(const char *args)
{
    (void)args;
    kputs("Hallo! Ich bin dein eigenes Mini-Betriebssystem.\n");
    kputs("Ich laufe in einer virtuellen Maschine auf deinem Handy.\n");
}

static void cmd_info(const char *args)
{
    (void)args;
    uint64_t midr = read_sysreg(midr_el1);
    uint64_t psci = psci_call(PSCI_VERSION);

    kprintf("Maschine:  %s\n", dtb.compatible ? dtb.compatible : "QEMU virt");
    kprintf("CPU:       %s (MIDR 0x%lx)\n", cpu_name(midr), midr);
    kprintf("Kerne:     %d\n", dtb.cpu_count ? dtb.cpu_count : 1);
    kprintf("Stufe:     EL%u (Kernel-Rechte)\n", current_el());
    kprintf("Timer:     %lu Hz\n", timer_frequency());
    kprintf("RAM:       %lu MiB ab 0x%lx\n",
            (ram_end() - (dtb.found ? dtb.ram_base : RAM_BASE)) >> 20,
            dtb.found ? dtb.ram_base : RAM_BASE);
    kprintf("PSCI:      Version %lu.%lu\n", (psci >> 16) & 0xffff, psci & 0xffff);
    if (dtb.found) {
        kprintf("Device Tree: %d Einträge, %u Bytes\n", dtb.node_count, dtb.total_size);
    } else {
        kputs("Device Tree: nicht gefunden\n");
    }
}

static void cmd_zeit(const char *args)
{
    (void)args;
    uint64_t freq = timer_frequency();
    uint64_t ticks = timer_ticks() - boot_ticks;
    uint64_t seconds = ticks / freq;
    uint64_t hundredths = (ticks % freq) * 100 / freq;
    kprintf("Seit dem Start vergangen: %lu,%02lu Sekunden\n", seconds, hundredths);
    kprintf("(%lu Timer-Ticks bei %lu Hz)\n", ticks, freq);
}

static void cmd_speicher(const char *args)
{
    (void)args;
    uintptr_t kstart = (uintptr_t)__kernel_start;
    uintptr_t kend = (uintptr_t)__kernel_end;
    uintptr_t sbottom = (uintptr_t)__stack_bottom;
    uintptr_t stop = (uintptr_t)__stack_top;
    uint64_t end = ram_end();

    kprintf("RAM-Anfang:  0x%lx\n", (uint64_t)RAM_BASE);
    if (dtb.found) {
        kprintf("Device Tree: 0x%lx (%u Bytes)\n", (uint64_t)RAM_BASE, dtb.total_size);
    }
    kprintf("Kernel:      0x%lx - 0x%lx (%lu KB)\n", (uint64_t)kstart, (uint64_t)kend,
            (uint64_t)(kend - kstart) / 1024);
    kprintf("  Stack:     0x%lx - 0x%lx (%lu KB)\n", (uint64_t)sbottom, (uint64_t)stop,
            (uint64_t)(stop - sbottom) / 1024);
    kprintf("Frei:        0x%lx - 0x%lx (%lu MiB)\n", (uint64_t)kend, end,
            (end - kend) >> 20);
}

static void cmd_echo(const char *args)
{
    kprintf("%s\n", args);
}

static void cmd_rechne(const char *args)
{
    const char *s = args;
    int64_t a, b;
    if (!parse_int(&s, &a)) {
        kputs("Beispiel: rechne 12 * 7   (Zeichen: + - * / :)\n");
        return;
    }
    s = skip_spaces(s);
    char op = *s;
    if (op) {
        s++;
    }
    if (!op || !parse_int(&s, &b)) {
        kputs("Beispiel: rechne 12 * 7   (Zeichen: + - * / :)\n");
        return;
    }

    switch (op) {
    case '+':
        kprintf("%ld + %ld = %ld\n", a, b, a + b);
        break;
    case '-':
        kprintf("%ld - %ld = %ld\n", a, b, a - b);
        break;
    case '*':
    case 'x':
        kprintf("%ld * %ld = %ld\n", a, b, a * b);
        break;
    case '/':
    case ':':
        if (b == 0) {
            kputs("Durch 0 kann man nicht teilen.\n");
        } else if (a % b == 0) {
            kprintf("%ld : %ld = %ld\n", a, b, a / b);
        } else {
            kprintf("%ld : %ld = %ld Rest %ld\n", a, b, a / b, a % b);
        }
        break;
    default:
        kprintf("Das Zeichen '%c' kenne ich nicht. Erlaubt: + - * / :\n", op);
        break;
    }
}

static void cmd_absturz(const char *args)
{
    (void)args;
    kputs("Ich löse jetzt absichtlich eine Ausnahme aus ...\n");
    __asm__ volatile("brk #0x1234");
}

static void cmd_neustart(const char *args)
{
    (void)args;
    kputs("Neustart ...\n\n");
    psci_call(PSCI_SYSTEM_RESET);
    kputs("Neustart hat nicht geklappt.\n");
}

static void cmd_aus(const char *args)
{
    (void)args;
    kputs("Tschüss! Die VM schaltet sich aus.\n");
    psci_call(PSCI_SYSTEM_OFF);
    kputs("Ausschalten hat nicht geklappt.\n");
}

struct command {
    const char *name;
    const char *help;
    void (*run)(const char *args);
};

static const struct command commands[] = {
    { "hilfe",    "alle Befehle anzeigen",     cmd_hilfe },
    { "hallo",    "Begrüßung",                 cmd_hallo },
    { "info",     "Daten der VM",              cmd_info },
    { "zeit",     "Zeit seit dem Start",       cmd_zeit },
    { "speicher", "Speicherplan",              cmd_speicher },
    { "echo",     "Text ausgeben",             cmd_echo },
    { "rechne",   "z. B. rechne 6 * 7",        cmd_rechne },
    { "absturz",  "Fehler testen",             cmd_absturz },
    { "neustart", "VM neu starten",            cmd_neustart },
    { "aus",      "VM ausschalten",            cmd_aus },
};

#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

static void cmd_hilfe(const char *args)
{
    (void)args;
    kputs("Befehle:\n");
    for (unsigned i = 0; i < COMMAND_COUNT; i++) {
        kprintf("  %-9s %s\n", commands[i].name, commands[i].help);
    }
}

// ---------------------------------------------------------------------------
// Shell

// Liest eine Zeile von der Tastatur. Gibt die Länge zurück, -1 bei Strg+C.
static int read_line(char *buf, int size)
{
    static int last_was_cr;
    int len = 0;

    for (;;) {
        int c = uart_getc();

        if (c == '\n' && last_was_cr) {   // "\r\n" zählt nur als ein Enter
            last_was_cr = 0;
            continue;
        }
        last_was_cr = (c == '\r');

        if (c == '\r' || c == '\n') {
            uart_putc('\n');
            buf[len] = '\0';
            return len;
        }
        if (c == 0x7f || c == 0x08) {     // Rücktaste
            if (len > 0) {
                do {
                    len--;                // bei Umlauten mehrere Bytes entfernen
                } while (len > 0 && ((uint8_t)buf[len] & 0xC0) == 0x80);
                kputs("\b \b");
            }
            continue;
        }
        if (c == 0x03) {                  // Strg+C
            kputs("^C\n");
            buf[0] = '\0';
            return -1;
        }
        if (c < 0x20) {
            continue;                     // andere Steuerzeichen ignorieren
        }
        if (len < size - 1) {
            buf[len++] = (char)c;
            uart_putc((char)c);
        }
    }
}

static void run_command(char *line)
{
    char *name = (char *)skip_spaces(line);
    if (*name == '\0') {
        return;
    }

    // Befehlsnamen abtrennen und in Kleinbuchstaben umwandeln
    // (Handy-Tastaturen schreiben das erste Wort oft groß).
    char *args = name;
    while (*args && *args != ' ' && *args != '\t') {
        if (*args >= 'A' && *args <= 'Z') {
            *args = (char)(*args - 'A' + 'a');
        }
        args++;
    }
    if (*args) {
        *args++ = '\0';
    }
    args = (char *)skip_spaces(args);

    for (unsigned i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(name, commands[i].name) == 0) {
            commands[i].run(args);
            return;
        }
    }
    kprintf("Unbekannter Befehl: %s\nTippe \"hilfe\" für alle Befehle.\n", name);
}

static void shell(void)
{
    char line[128];
    for (;;) {
        kputs("hakoniwa> ");
        int len = read_line(line, (int)sizeof(line));
        if (len > 0) {
            run_command(line);
        }
    }
}

// ---------------------------------------------------------------------------
// Ausnahmen (wird aus vectors.S aufgerufen)

static const char *exception_reason(unsigned ec)
{
    switch (ec) {
    case 0x00: return "unbekannter Befehl";
    case 0x07: return "Gleitkomma/SIMD gesperrt";
    case 0x0e: return "ungültiger Ausführungszustand";
    case 0x15: return "Systemaufruf (SVC)";
    case 0x16: return "Hypervisor-Aufruf (HVC)";
    case 0x18: return "verbotener Systemregister-Zugriff";
    case 0x20:
    case 0x21: return "Befehl konnte nicht geladen werden";
    case 0x22: return "Befehlsadresse nicht ausgerichtet";
    case 0x24:
    case 0x25: return "Speicherzugriff fehlgeschlagen";
    case 0x26: return "Stack nicht ausgerichtet";
    case 0x2f: return "SError (Hardwarefehler)";
    case 0x3c: return "BRK-Befehl (Haltepunkt)";
    default:   return "anderer Grund";
    }
}

void exception_handler(uint64_t index, uint64_t esr, uint64_t elr, uint64_t far)
{
    static const char *const kinds[4] = { "synchron", "IRQ", "FIQ", "SError" };
    static const char *const origins[4] = {
        "aktuelle Stufe (SP_EL0)", "aktuelle Stufe (SP_EL1)",
        "niedrigere Stufe (AArch64)", "niedrigere Stufe (AArch32)",
    };
    unsigned ec = (unsigned)((esr >> 26) & 0x3f);

    kputs("\n!!! Ausnahme !!!\n");
    kprintf("Art:     %s, %s\n", kinds[index & 3], origins[(index >> 2) & 3]);
    kprintf("Grund:   %s (EC 0x%x)\n", exception_reason(ec), ec);
    kprintf("ESR:     0x%lx\n", esr);
    kprintf("Befehl:  0x%lx\n", elr);
    kprintf("Adresse: 0x%lx\n", far);
    kputs("Der Kernel ist angehalten. Drücke Enter für einen Neustart.\n");

    uart_getc();
    psci_call(PSCI_SYSTEM_RESET);
    for (;;) {
        __asm__ volatile("wfi");
    }
}

// ---------------------------------------------------------------------------
// Start

void kernel_main(void)
{
    uart_init();
    boot_ticks = timer_ticks();

    kputs("\n");
    kputs("+----------------------------+\n");
    kputs("|  HAKONIWA OS " OS_VERSION "           |\n");
    kputs("|  Dein Mini-Betriebssystem  |\n");
    kputs("+----------------------------+\n");
    kputs("\n");
    kputs("[ ok ] Serielle Schnittstelle bereit\n");

    if (read_sysreg(vbar_el1) == (uint64_t)(uintptr_t)vector_table) {
        kputs("[ ok ] Ausnahme-Tabelle geladen\n");
    } else {
        kputs("[FEHL] Ausnahme-Tabelle fehlt\n");
    }

    gic_init();
    gic_enable_interrupt(UART_IRQ);
    kputs("[ ok ] Interrupt-Controller bereit\n");

    dtb_read(RAM_BASE, &dtb);
    if (dtb.found) {
        kprintf("[ ok ] Device Tree gelesen (%d Einträge)\n", dtb.node_count);
    } else {
        kputs("[ -- ] Kein Device Tree gefunden\n");
    }

    uint64_t ram_base = dtb.found ? dtb.ram_base : RAM_BASE;
    kprintf("\nCPU:  %s, Stufe EL%u\n", cpu_name(read_sysreg(midr_el1)), current_el());
    kprintf("RAM:  %lu MiB ab 0x%lx\n", (ram_end() - ram_base) >> 20, ram_base);
    kputs("\nTippe \"hilfe\" für alle Befehle.\n\n");

    shell();
}
