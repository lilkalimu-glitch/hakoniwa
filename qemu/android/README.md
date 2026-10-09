# Android-Anpassungen für QEMU

- `compat.h` / `compat.c`: Ersatz für `shm_open()`/`shm_unlink()` (fehlen in Android)
  und Abschalten der Heap-Zeiger-Markierung auf arm64.
- `setjmp-aarch64/`: `setjmp`/`longjmp` ohne die Prüfungen von Android 12+, an denen
  QEMU sonst abstürzt. Übernommen aus dem QEMU-Paket von
  [Termux](https://github.com/termux/termux-packages/tree/master/packages/qemu-system-x86-64-headless)
  (ursprünglich aus dem Android Open Source Project, BSD-Lizenz).
