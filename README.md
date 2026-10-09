# HAKONIWA 箱庭

Android-App, die auf dem Handy eine virtuelle Maschine startet. Darin läuft ein eigenes,
kleines Betriebssystem. Dein echtes Android wird dabei nicht verändert, und die App braucht kein Root.

## Download

Die neueste APK gibt es unter [Releases](../../releases/latest). Auf dem Handy öffnen und installieren.
Android warnt dabei vor einer unbekannten App, weil sie nicht aus dem Play Store kommt.
Neue Versionen einfach über die alte installieren.

## Benutzen

- Beim Öffnen der App startet die VM automatisch.
- Unten einen Befehl eintippen und auf **Senden** tippen, oder einen Schnellbefehl antippen.
- **Stopp** beendet die VM, **Neustart** startet sie frisch, **Start** schaltet sie wieder ein.

| Befehl | Was er macht |
| --- | --- |
| `hilfe` | zeigt alle Befehle |
| `info` | Daten der VM: CPU, Speicher, Timer |
| `zeit` | Zeit seit dem Start |
| `speicher` | wo der Kernel im Speicher liegt |
| `echo Text` | gibt den Text aus |
| `rechne 6 * 7` | rechnet mit `+ - * / :` |
| `absturz` | löst absichtlich einen Fehler aus (danach Enter = Neustart) |
| `neustart` | startet die VM neu |
| `aus` | schaltet die VM aus |

## Aufbau

- `os/` – das Mini-Betriebssystem (C und Assembler). Es startet ohne fremden Code direkt auf
  der virtuellen CPU (AArch64, Stufe EL1) und bringt eine kleine Kommandozeile mit.
- `app/` – die Android-App (Kotlin). Sie startet QEMU und zeigt an, was das Mini-Betriebssystem ausgibt.
- `qemu/` – baut [QEMU](https://www.qemu.org) für Android. QEMU stellt die virtuelle Maschine
  bereit: Prozessor, 128 MB Speicher und eine serielle Schnittstelle.
- `tests/` – automatische Tests für das Mini-Betriebssystem und die App.

## Selber bauen

Jeder Push auf `main` baut über GitHub Actions alles neu:
Mini-Betriebssystem (mit Test in QEMU), QEMU für Android (arm64 und x86_64), APK,
Test der App im Android-Emulator, danach ein Release mit der APK.
Dafür braucht der Build das Secret `KEYSTORE_PASSWORD`. Wer das Projekt kopiert, braucht einen eigenen Schlüssel.

Das Mini-Betriebssystem allein lässt sich auf einem Linux-Rechner testen:

```sh
sudo apt install clang lld qemu-system-arm
make -C os run        # Beenden: Strg+A, dann X
```

## Lizenz

GPL-2.0, wie QEMU. Die App enthält QEMU 11.0.3
([Quellcode](https://download.qemu.org/qemu-11.0.3.tar.xz)) und GLib 2.88.4
([Quellcode](https://download.gnome.org/sources/glib/2.88/glib-2.88.4.tar.xz), LGPL-2.1).
Die Änderungen für Android stehen in `qemu/`. Auf Anfrage gibt es den vollständigen Quellcode
jeder veröffentlichten Version.
