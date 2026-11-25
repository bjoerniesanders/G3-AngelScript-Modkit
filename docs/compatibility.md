# Kompatibilität

**Deutsch** | [English](en/compatibility.md)

Ich habe G3ASM bisher auf wenigen Installationen getestet. Diese Seite sammelt, was ich auf welcher Installation geprüft habe. Probierst du es auf einer
anderen Installation aus, kannst du das Ergebnis gern melden (Protokolle siehe unten).

## Voraussetzungen

G3ASM braucht den **Community Patch 1.75** und den **Parallel-Universe-Patch** (PU). Ohne einen der beiden ist es nicht getestet und wird nicht
unterstützt.

- `tools/install_loader.ps1` prüft beim Einbau beide Patches. Fehlt ein Patch, bricht das Skript ab. Mit `-Force` baut es G3ASM trotzdem ein, aber
  das ist ungetestet.
- Beim Start schreibt das Modkit die Zeile `Voraussetzungen: Community Patch 1.75 ja/NEIN, Parallel-Universe-Patch ja/NEIN` ins Protokoll. Bei einem
  `NEIN` schreibt es zusätzlich eine `WARNUNG`. Es läuft trotzdem weiter. Hooks an festen Adressen prüfen ihre Bytes selbst.
- Erkennung: der Community Patch 1.75 am PE-Zeitstempel `4F959F4F` der `Gothic3.exe`. Die `Game.dll` taugt dafür nicht, weil Mods sie binär patchen.
  Der PU-Patch an `paru_s1.dll` und `scripts\Script_paru.dll` im Spielordner.

## Testmatrix

| Installation | Community Patch | PU-Patch | DXVK | Ergebnis | Stand |
|---|---|---|---|---|---|
| Entwicklerinstallation (Kopie, Windows 11; `FileSystem.dll` und `Dialogs.dll` sind dort eigene Testfassungen) | 1.75 | Ja | Ja | Alle Prüfungen bestanden. Hier entstehen alle Messungen | API 47, 2026-11-25 |
| Steam (stark gemoddet, Kopie) | 1.75 (Mods haben die Spiel-DLLs binär verändert) | Ja | Ja (anderer DXVK-Stand) | Start, Spielstand laden, Weltwechsel, Beenden ohne Fehler (nur Mini-Mod). Alle 8 Spiel-Hooks und 2 Statuseffekt-Hooks aktiv. x87-Steuerwort durchgehend 007F. Tooltip-Hook ausgelassen, weil `Game.dll+0xA48B0` dort abweicht (erwartet). 2026-11-08: Blackscreen im Intro durch einen gemeinsamen `Shader.Cache` (getrennt: behoben). Ein Spielstand mit `perk_lifesteal` stürzte nach Neustart und Laden beim Öffnen des Charakterbogens ab. Das passierte auch ohne Mods (Ursache offen) | 2026-11 |
| Steam (sauber, Kopie; nur Community Patch und PU) | 1.75 | Ja | Nein | Spiel-DLLs byte-gleich mit der Entwicklerinstallation (außer `FileSystem.dll` und `Dialogs.dll`). Ohne Patch (Loader eingebaut, `enable=0`) und mit Patch (nur Mini-Mod): Start, Spielstand laden, Charakterbogen, Speichern in neuen Slot, erneut laden, Beenden ohne Fehler. Voraussetzungen erkannt. Alle Hooks aktiv (auch Tooltip und die festen Stellen Monsterschaden und Lernkosten). x87-Steuerwort durchgehend 007F. Mod-Test mit `perk_lifesteal` und `beliars_klaue` nach dem Prüfablauf unten: Lebensraub heilt, Kachel nach jedem Laden und Speichern vorhanden, Blitze mit Besitzer und Optik. Abschalten mit `ENTFERNEN = true`, danach Mods aus und zuletzt Patch ausgebaut: Spielstand laden, Charakterbogen, Waffe ab- und anlegen, jeweils ohne Absturz | API 46, 2026-11-13 |
| GOG | 1.75 nötig | Nötig | Offen | Nicht getestet | |
| CD/DVD-Version | 1.75 nötig | Nötig | Offen | Nicht getestet | |

Installationen ohne Community Patch 1.75 oder ohne PU-Patch prüfe ich nicht (siehe Voraussetzungen). Offen ist noch: mit und ohne DXVK.

## Bekannte Unterschiede zwischen Installationen

Gleiche Dateigröße und gleicher PE-Zeitstempel heißen **nicht** gleicher Inhalt. Die `Game.dll` einer Steam-Installation und die der
Entwicklerinstallation haben beide 4.640.768 Bytes und den Zeitstempel `4F959E2D`. Sie unterscheiden sich aber in 15.701 Bytes (427 Bereiche,
überwiegend Sprünge in nachträglich eingefügten Code). Bei `Engine.dll` und `Script.dll` ist es ebenso (1.860 und 461 Bytes). Deshalb schreibt das
Modkit zu jedem Modul auch eine CRC32-Prüfsumme ins Protokoll. Hooks an festen Adressen hängt es nur ein, wenn die Bytes passen. Weichen sie ab, fehlt
nur die betroffene Funktion. Alles andere läuft weiter.

| Datei | Entwicklerinstallation (CRC32) | Steam-Installation (CRC32) |
|---|---|---|
| Game.dll | AA8B3F6D | 148CEEE1 |
| Engine.dll | 37DB85F7 | 2D240B9E |
| Script.dll | 906D2266 | 6DC635DE |
| Gothic3.exe | 9A6E7A58 | 9A6E7A58 |
| SharedBase.dll | 022D320C | 022D320C |

## Prüfablauf

**Arbeite immer auf einer Kopie der Installation, nie im Original.** Kopiere den Spielordner (mit `Gothic3.exe`) und verändere nur die Kopie.

**Spielstände:** Alle Kopien benutzen denselben Ordner `Documents\gothic3`. Sichere den Ordner vorher. Speichere nur in **neue, freie Slots**. Lösche
die Testspielstände danach.

1. **Einbauen** (Spiel vorher beenden): aus dem Release-Zip, im entpackten Ordner:
   `powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_loader.ps1 -Game "<Kopie>" -BuildDir bin`
2. **Starten**, Spielstand laden, ein paar Minuten spielen, beenden. Sieh dann in `<Kopie>\g3asm.log` nach:
   - Die Zeile `Voraussetzungen: Community Patch 1.75 ja, Parallel-Universe-Patch ja`.
   - Sieben Zeilen `Modul ...: <Bytes> Bytes, PE-Zeitstempel ..., CRC32 ..., Spielordner ja/nein` (Gothic3.exe, Game, Engine, Script, Script_Game,
     SharedBase, d3d9).
   - Alle Hook-Zeilen (`Spielereignisse: 8 von 8 Hooks aktiv` und ähnliche, `Feste Stellen: 2 von 2 aktiv`), dazu
     `Qualitaets-Tooltip (Lebensraub): eingehaengt`.
   - Das x87-Steuerwort steht in jeder Zeile `FPU-Zustand ...` auf `007F`.
   - Keine Zeile mit `nicht eingehaengt`, `FEHLER` oder `WARNUNG`. Steht dort `nicht eingehaengt`, ist die Spielversion eine andere als erwartet. Das
     Modkit lässt diese Funktion aus und läuft weiter. Das ist kein Absturz, aber bitte melde es.
3. **Mods einschalten:** Trage in `<Kopie>\g3asm\mods\load_order.txt` die Zeilen `+perk_lifesteal` und `+beliars_klaue` ein. Die Beispiel-Mods liegen
   im Zip unter `examples\mods`. Kopiere sie in den Ordner `<Kopie>\g3asm\mods\`. Starte das Spiel, kämpfe, öffne den Charakterbogen.
4. **Speichern und Laden** mit eingeschalteten Mods, **in einen neuen Slot**.
5. **Ausbauen:** `... install_loader.ps1 -Game "<Kopie>" -Uninstall`. Starte danach das Spiel **ohne Modkit**.

**Achtung bei Schritt 5:** `perk_lifesteal` legt einen Gegenstandsstapel im Inventar des Helden an. Lädst du einen Spielstand mit diesem Stapel **ohne**
die Mod und öffnest den Charakterbogen, stürzt das Spiel ab (gemessen, siehe [Grenzen](modkit/limitations.md)). Seit Version 31 nimmt die Mod den
Stapel vor jedem Speichern heraus und legt ihn danach und nach jedem Laden neu an. Spielstände dieser Fassung enthalten ihn also nicht. Ältere
Spielstände können ihn enthalten. Setze dann `ENTFERNEN = true` in `perk_lifesteal`, lade einmal, speichere und schalte erst dann die Mod ab.

## Was bei einem Bericht helfen würde
- Installation (Steam, GOG, CD), DXVK ja oder nein, und die Zeile `Voraussetzungen: ...` aus dem Protokoll.
- `g3asm.log` und `g3asm_loader.log` aus dem Spielordner. Der Loader schreibt in die erste Zeile den Pfad zur Einstellungsdatei. Mache vor dem
  Weitergeben den Benutzernamen im Pfad bei Bedarf unkenntlich.
- Wenn das Spiel abstürzt: was zuletzt im Spiel passiert ist.
