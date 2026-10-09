# Mein OS

Das ist dein eigenes Betriebssystem. Es fängt bei null an: In der virtuellen Maschine
läuft nur das, was in diesem Ordner steht. In der App wählst du es oben mit **Mein OS** aus.

## Schritt 0: ein Buchstabe

[`start.S`](start.S) ist das ganze Programm. Es besteht aus 5 Befehlen für den Prozessor:

1. `mov x0, #0x09000000` legt die Adresse der Textausgabe in das Register `x0`.
2. `mov w1, #65` legt die Zahl 65 in das Register `w1`. 65 ist der Code für den Buchstaben „A“.
3. `strb w1, [x0]` schreibt diese Zahl an die Adresse aus `x0`. Dadurch erscheint ein „A“ in der App.
4. `wfi` legt den Prozessor schlafen.
5. `b 1b` springt zurück zu Befehl 4. Danach passiert nichts mehr.

Ein Register ist ein kleiner Speicherplatz direkt im Prozessor.
`link.ld` und `Makefile` legen fest, wo das Programm im Speicher liegt und wie es gebaut wird.
Die musst du erst mal nicht ändern.

## So änderst du etwas

1. Öffne die Datei auf GitHub, zum Beispiel `mein-os/start.S`, und tippe auf den Stift (Bearbeiten).
2. Ändere den Text und tippe auf **Commit changes**.
3. GitHub baut die App neu. Nach etwa 10 Minuten liegt unter **Releases** eine neue APK.
   Die installierst du über die alte.

Wenn dein Programm einen Fehler hat, baut GitHub keine neue App und meldet den Fehler.

## Nächster Schritt

**Schritt 1:** Statt „A“ soll das ganze Wort „Hallo“ erscheinen.
Überlege zuerst: Welche der 5 Befehle brauchst du mehrmals, und was ändert sich dabei jedes Mal?
