#!/usr/bin/env bash
# Testet die App in einem Android-Emulator (läuft auf GitHub).
# Aufruf: tests/emulator_test.sh <apk> <ergebnis-ordner>
set -uo pipefail

APK="$1"
OUT="${2:-emulator-results}"
PKG=com.lilkalimu.hakoniwa
mkdir -p "$OUT"

fail() {
  echo "FEHLER: $*"
  adb logcat -d -s Hakoniwa:V AndroidRuntime:E DEBUG:V libc:V > "$OUT/logcat-fehler.txt" 2>&1 || true
  adb exec-out screencap -p > "$OUT/screenshot-fehler.png" 2>/dev/null || true
  exit 1
}

# Wartet, bis eine Zeile im Log auftaucht: wait_log <text> <sekunden>
wait_log() {
  local needle="$1" timeout="$2" waited=0
  while [ "$waited" -lt "$timeout" ]; do
    if adb logcat -d -s Hakoniwa:V 2>/dev/null | grep -qF -- "$needle"; then
      echo "  gefunden: $needle"
      return 0
    fi
    sleep 2
    waited=$((waited + 2))
  done
  fail "nicht im Log gefunden: $needle"
}

echo "== Gerät"
adb shell getprop ro.build.version.release
adb shell getprop ro.product.cpu.abi

echo "== Installieren"
adb install -r "$APK" || fail "Installation fehlgeschlagen"
adb logcat -c

echo "== App starten (VM startet automatisch) und Befehle senden"
adb shell "am start -W -n $PKG/.MainActivity --es hakoniwa.test.commands 'hilfe;Info;rechne 6 * 7;zeit;speicher;hallo'" \
  || fail "App ließ sich nicht starten"

wait_log "VM: |  HAKONIWA OS" 120
wait_log "VM: [ ok ] Interrupt-Controller bereit" 60
wait_log "VM: Befehle:" 60
wait_log "VM: CPU:       Cortex-A72" 60
wait_log "VM: 6 * 7 = 42" 60
wait_log "Sekunden" 60
wait_log "VM: Kernel:      0x40100000" 60
wait_log "VM: Hallo! Ich bin dein eigenes Mini-Betriebssystem." 60
sleep 2
adb exec-out screencap -p > "$OUT/screenshot-1-befehle.png"

echo "== Rechenlast im Leerlauf"
sleep 5
adb shell top -b -n 1 -o PID,%CPU,RES,NAME 2>/dev/null | grep -iE "qemu|PID" | tee "$OUT/top.txt" || true

echo "== Neustart in der VM"
adb shell "am start -n $PKG/.MainActivity --es hakoniwa.test.commands 'neustart'" || fail "Intent fehlgeschlagen"
wait_log "VM: Neustart ..." 60
sleep 3
count=$(adb logcat -d -s Hakoniwa:V | grep -cF "VM: |  HAKONIWA OS")
echo "  Startmeldungen: $count"
[ "$count" -ge 2 ] || fail "nach dem Neustart kam keine neue Startmeldung"

echo "== Ausschalten"
adb shell "am start -n $PKG/.MainActivity --es hakoniwa.test.commands 'aus'" || fail "Intent fehlgeschlagen"
wait_log "VM: Tschüss! Die VM schaltet sich aus." 60
wait_log "APP: VM beendet (Code 0" 60
sleep 2
adb exec-out screencap -p > "$OUT/screenshot-2-aus.png"

echo "== Wieder starten"
adb shell "am start -n $PKG/.MainActivity --es hakoniwa.test.commands 'rechne 100 : 7'" || fail "Intent fehlgeschlagen"
wait_log "VM: 100 : 7 = 14 Rest 2" 90
sleep 2
adb exec-out screencap -p > "$OUT/screenshot-3-neu.png"

adb logcat -d -s Hakoniwa:V > "$OUT/logcat.txt"
echo "ALLE APP-TESTS BESTANDEN"
