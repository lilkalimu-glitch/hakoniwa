#!/usr/bin/env python3
"""Testet HAKONIWA OS in QEMU.

Startet den Kernel, tippt Befehle ein und prüft die Antworten:
Start, Ruhezustand (keine Rechenlast), Befehle, Ausnahme, Neustart, Ausschalten.

Aufruf: python3 tests/test_os.py [qemu-system-aarch64] [os/kernel.elf]
"""
import os
import shlex
import subprocess
import sys
import threading
import time

QEMU = shlex.split(sys.argv[1] if len(sys.argv) > 1 else "qemu-system-aarch64")
# Im Docker-Container lässt sich die Rechenzeit von QEMU nicht direkt messen
IDLE_CHECK = os.environ.get("IDLE_CHECK", "1") == "1"
KERNEL = sys.argv[2] if len(sys.argv) > 2 else "os/kernel.elf"

QEMU_ARGS = [
    "-M", "virt,gic-version=2", "-cpu", "cortex-a72", "-m", "128M",
    "-nodefaults", "-display", "none", "-serial", "stdio",
    "-kernel", KERNEL,
]

PROMPT = "hakoniwa> "


class VM:
    def __init__(self):
        self.proc = subprocess.Popen(
            QEMU + QEMU_ARGS,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        self.out = bytearray()
        self.err = bytearray()
        self.lock = threading.Lock()
        self.pos = 0  # ab hier wird nach neuer Ausgabe gesucht
        threading.Thread(target=self._read, args=(self.proc.stdout, self.out), daemon=True).start()
        threading.Thread(target=self._read, args=(self.proc.stderr, self.err), daemon=True).start()

    def _read(self, stream, target):
        while True:
            data = os.read(stream.fileno(), 4096)
            if not data:
                break
            with self.lock:
                target.extend(data)

    def text(self):
        with self.lock:
            return self.out.decode("utf-8", "replace").replace("\r", "")

    def expect(self, needle, timeout=20.0):
        """Wartet, bis `needle` nach der letzten Fundstelle in der Ausgabe auftaucht."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            text = self.text()
            index = text.find(needle, self.pos)
            if index >= 0:
                self.pos = index + len(needle)
                return text[index:self.pos]
            if self.proc.poll() is not None:
                break
            time.sleep(0.05)
        tail = self.text()[-2500:]
        err = self.err.decode("utf-8", "replace")[-1500:]
        raise AssertionError(
            f"Nicht gefunden: {needle!r}\n--- Ausgabe (Ende) ---\n{tail}\n--- QEMU-Fehler ---\n{err}"
        )

    def send(self, line):
        self.proc.stdin.write(line.encode("utf-8") + b"\r")
        self.proc.stdin.flush()

    def command(self, line, *expected, timeout=20.0):
        self.expect(PROMPT, timeout)
        self.send(line)
        for needle in expected:
            self.expect(needle, timeout)

    def cpu_seconds(self):
        with open(f"/proc/{self.proc.pid}/stat") as f:
            fields = f.read().rsplit(")", 1)[1].split()
        ticks = os.sysconf("SC_CLK_TCK")
        return (int(fields[11]) + int(fields[12])) / ticks  # utime + stime


def main():
    vm = VM()
    ok = False
    try:
        # 1. Start
        vm.expect("HAKONIWA OS", 30)
        vm.expect("[ ok ] Serielle Schnittstelle bereit")
        vm.expect("[ ok ] Ausnahme-Tabelle geladen")
        vm.expect("[ ok ] Interrupt-Controller bereit")
        vm.expect("[ ok ] Device Tree gelesen")
        vm.expect("RAM:  128 MiB ab 0x40000000")
        vm.expect(PROMPT)
        vm.pos -= len(PROMPT)
        print("Start: ok")

        # 2. Ruhezustand: Die VM soll schlafen, solange niemand tippt.
        if IDLE_CHECK:
            time.sleep(1.0)
            before = vm.cpu_seconds()
            time.sleep(4.0)
            used = vm.cpu_seconds() - before
            print(f"Rechenzeit in 4 s Ruhe: {used:.2f} s")
            assert used < 1.0, f"VM verbraucht im Leerlauf zu viel Rechenzeit ({used:.2f} s in 4 s)"

        # 3. Befehle
        vm.command("hilfe", "Befehle:", "VM ausschalten")
        vm.command("Info", "CPU:       Cortex-A72", "Stufe:     EL1", "Timer:", "PSCI:      Version")
        vm.command("rechne 6 * 7", "6 * 7 = 42")
        vm.command("rechne 17:5", "17 : 5 = 3 Rest 2")
        vm.command("rechne 1/0", "Durch 0 kann man nicht teilen.")
        vm.command("echo Hallo Welt", "\nHallo Welt\n")
        vm.command("zeit", "Sekunden")
        vm.command("speicher", "Kernel:      0x40100000")
        vm.command("hallo", "Hallo! Ich bin dein eigenes Mini-Betriebssystem.")
        vm.command("gibtsnicht", "Unbekannter Befehl: gibtsnicht")
        vm.command("   ")
        print("Befehle: ok")

        # 4. Ausnahme und Neustart danach
        vm.command("absturz", "!!! Ausnahme !!!", "BRK-Befehl", "Drücke Enter")
        vm.send("")
        vm.expect("HAKONIWA OS")
        vm.expect(PROMPT)
        vm.pos -= len(PROMPT)
        print("Ausnahme: ok")

        # 5. Neustart
        vm.command("neustart", "Neustart ...", "HAKONIWA OS")
        vm.expect(PROMPT)
        vm.pos -= len(PROMPT)
        print("Neustart: ok")

        # 6. Ausschalten
        vm.command("aus", "Tschüss")
        code = vm.proc.wait(timeout=15)
        assert code == 0, f"QEMU endete mit Code {code}"
        print("Ausschalten: ok")
        ok = True
    finally:
        if vm.proc.poll() is None:
            vm.proc.kill()
        print("\n========== Ausgabe der VM ==========")
        print(vm.text())
        err = vm.err.decode("utf-8", "replace").strip()
        if err:
            print("========== QEMU-Meldungen ==========")
            print(err)
    print("ALLE TESTS BESTANDEN" if ok else "TEST FEHLGESCHLAGEN")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
