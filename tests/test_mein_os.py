#!/usr/bin/env python3
"""Prüft, ob "Mein OS" in QEMU startet, ohne dass QEMU abstürzt.

Absichtlich ohne feste Erwartung an die Ausgabe: Was "Mein OS" ausgibt, ändert
sich mit jedem Schritt. Optional kann man einen Text angeben, der erscheinen muss.

Aufruf: python3 tests/test_mein_os.py [qemu-befehl] [mein-os/kernel.elf] [erwarteter-text]
"""
import os
import shlex
import subprocess
import sys
import threading
import time

QEMU = shlex.split(sys.argv[1] if len(sys.argv) > 1 else "qemu-system-aarch64")
KERNEL = sys.argv[2] if len(sys.argv) > 2 else "mein-os/kernel.elf"
EXPECTED = sys.argv[3] if len(sys.argv) > 3 else None
SECONDS = 6

ARGS = [
    "-M", "virt,gic-version=2", "-cpu", "cortex-a72", "-m", "128M",
    "-nodefaults", "-display", "none", "-serial", "stdio",
    "-kernel", KERNEL,
]


def read_all(stream, target):
    while True:
        data = os.read(stream.fileno(), 4096)
        if not data:
            break
        target.extend(data)


def main():
    proc = subprocess.Popen(QEMU + ARGS, stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = bytearray(), bytearray()
    readers = [threading.Thread(target=read_all, args=(proc.stdout, out), daemon=True),
               threading.Thread(target=read_all, args=(proc.stderr, err), daemon=True)]
    for r in readers:
        r.start()

    deadline = time.time() + SECONDS
    while time.time() < deadline and proc.poll() is None:
        time.sleep(0.1)

    code = proc.poll()
    if code is None:
        proc.kill()
        proc.wait()
    for r in readers:
        r.join(2)

    text = out.decode("utf-8", "replace")
    print("========== Ausgabe von Mein OS ==========")
    print(text)
    if err.strip():
        print("========== QEMU-Meldungen ==========")
        print(err.decode("utf-8", "replace"))

    if code is not None and code != 0:
        print(f"TEST FEHLGESCHLAGEN: QEMU endete mit Code {code}")
        return 1
    if EXPECTED is not None and EXPECTED not in text:
        print(f"TEST FEHLGESCHLAGEN: {EXPECTED!r} kam nicht vor")
        return 1
    state = "läuft noch" if code is None else "hat sich ausgeschaltet"
    print(f"MEIN OS OK ({state} nach {SECONDS} s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
