# Baut "Mein OS" (kernel.elf) für QEMUs Maschine "virt" (AArch64).
# Jede .S- und .c-Datei in diesem Ordner wird automatisch mitgebaut.
#   make       -> kernel.elf
#   make run   -> startet es in QEMU (Beenden: Strg+A, dann X)

KCC     ?= clang
KLD     ?= ld.lld

CFLAGS  := --target=aarch64-none-elf -mcpu=cortex-a72 -O2 -g0 \
           -ffreestanding -fno-builtin -fno-stack-protector -fno-pic \
           -mgeneral-regs-only -mstrict-align -Wall -Wextra -std=gnu11
ASFLAGS := --target=aarch64-none-elf -mcpu=cortex-a72
LDFLAGS := -T link.ld -nostdlib -static -z max-page-size=4096 --no-rosegment

SRCS    := $(wildcard *.S) $(wildcard *.c)
OBJS    := $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(SRCS)))

kernel.elf: $(OBJS) link.ld
	$(KLD) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c $(wildcard *.h)
	$(KCC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(KCC) $(ASFLAGS) -c -o $@ $<

run: kernel.elf
	qemu-system-aarch64 -M virt,gic-version=2 -cpu cortex-a72 -m 128M \
		-nodefaults -display none -serial stdio -kernel kernel.elf

clean:
	rm -f *.o kernel.elf

.PHONY: run clean
