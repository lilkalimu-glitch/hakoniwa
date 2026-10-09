# Baut den Kernel (kernel.elf) für QEMUs Maschine "virt" (AArch64).
#   make            -> kernel.elf
#   make run        -> startet ihn in QEMU (Beenden: Strg+A, dann X)

# Eigene Namen, damit make nicht seine Standardwerte (cc, ld) benutzt
KCC     ?= clang
KLD     ?= ld.lld

CFLAGS  := --target=aarch64-none-elf -mcpu=cortex-a72 -O2 -g0 \
           -ffreestanding -fno-builtin -fno-stack-protector -fno-pic \
           -mgeneral-regs-only -mstrict-align \
           -Wall -Wextra -Werror -std=gnu11
ASFLAGS := --target=aarch64-none-elf -mcpu=cortex-a72
LDFLAGS := -T link.ld -nostdlib -static -z max-page-size=4096 --no-rosegment

OBJS := boot.o vectors.o kernel.o lib.o uart.o gic.o dtb.o psci.o

kernel.elf: $(OBJS) link.ld
	$(KLD) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c *.h
	$(KCC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(KCC) $(ASFLAGS) -c -o $@ $<

run: kernel.elf
	qemu-system-aarch64 -M virt,gic-version=2 -cpu cortex-a72 -m 128M \
		-nodefaults -display none -serial stdio -kernel kernel.elf

clean:
	rm -f *.o kernel.elf

.PHONY: run clean
