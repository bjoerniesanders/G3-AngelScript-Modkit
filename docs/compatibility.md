# Kompatibilität

Getestet ist G3ASM bisher auf wenigen Installationen. Diese Seite sammelt, was auf welcher Installation geprüft wurde. Wer es auf einer anderen Installation
ausprobiert, kann das Ergebnis gern melden (Protokolle siehe unten).

## Voraussetzungen

G3ASM setzt **Community Patch 1.75** und den **Parallel-Universe-Patch** (PU) voraus. Ohne einen der beiden ist es nicht getestet und wird nicht
unterstützt.

- `tools/install_loader.ps1` prüft beim Einbau beides und bricht ab, wenn etwas fehlt. Mit `-Force` baut er trotzdem ein (ungetestet).
- Das Modkit schreibt beim Start die Zeile `Voraussetzungen: Community Patch 1.75 ja/NEIN, Parallel-Universe-Patch ja/NEIN` ins Protokoll, bei einem `NEIN`
  zusätzlich eine `WARNUNG`. Es läuft trotzdem weiter; Hooks an festen Adressen prüfen ihre Bytes selbst.
- Erkennung: Community Patch 1.75 am PE-Zeitstempel `4F959F4F` der `Gothic3.exe` (die `Game.dll` taugt dafür nicht, Mods patchen sie binär), PU an
  `paru_s1.dll` und `scripts\Script_paru.dll` im Spielordner.

## Testmatrix

| Installation | Community Patch | PU-Patch | DXVK | Ergebnis | Stand |
|---|---|---|---|---|---|
| Entwicklerinstallation (Kopie, Windows 11; `FileSystem.dll`/`Dialogs.dll` sind dort eigene Testfassungen) | 1.75 | ja | ja | alle Prüfungen bestanden; hier entstehen alle Messungen | API 46, 2026-11 |
| Steam (stark gemoddet, Kopie) | 1.75 (Spiel-DLLs von Mods binär verändert) | ja | ja (anderer DXVK-Stand) | Start, Spielstand laden, Weltwechsel, Beenden ohne Fehler (nur Mini-Mod); alle 8 Spiel-Hooks und 2 Statuseffekt-Hooks aktiv, x87-Steuerwort durchgehend 007F; Tooltip-Hook ausgelassen, weil `Game.dll+0xA48B0` dort abweicht (erwartet). 2026-11-08: Blackscreen im Intro durch gemeinsamen `Shader.Cache` (getrennt: behoben); ein Spielstand mit `perk_lifesteal` stürzte nach Neustart und Laden beim Öffnen des Charakterbogens ab, auch ohne Mods (Ursache offen) | 2026-11 |
| Steam (sauber, Kopie; nur Community Patch und PU) | 1.75 | ja | nein | Spiel-DLLs byte-gleich mit der Entwicklerinstallation (außer `FileSystem.dll`/`Dialogs.dll`). Ohne Patch (Loader eingebaut, `enable=0`) und mit Patch (nur Mini-Mod): Start, Spielstand laden, Charakterbogen, Speichern in neuen Slot, erneut laden, Beenden ohne Fehler; Voraussetzungen erkannt, alle Hooks aktiv (auch Tooltip und die festen Stellen Monsterschaden/Lernkosten), x87-Steuerwort durchgehend 007F. Mod-Test mit `perk_lifesteal` und `beliars_klaue` nach dem Prüfablauf unten: Lebensraub heilt, Kachel nach jedem Laden und Speichern vorhanden, Blitze mit Besitzer und Optik; Abschalten mit `ENTFERNEN = true`, danach Mods aus und zuletzt Patch ausgebaut: Spielstand laden, Charakterbogen, Waffe ab- und anlegen jeweils ohne Absturz | API 46, 2026-11-13 |
| GOG | 1.75 nötig | nötig | offen | nicht getestet | |
| CD/DVD-Version | 1.75 nötig | nötig | offen | nicht getestet | |

Installationen ohne Community Patch 1.75 oder ohne PU-Patch werden nicht geprüft (siehe Voraussetzungen). Offen ist noch: mit/ohne DXVK.

## Bekannte Unterschiede zwischen Installationen

Gleiche Dateigröße und gleicher PE-Zeitstempel heißen **nicht** gleicher Inhalt. Die `Game.dll` einer Steam-Installation und die der Entwicklerinstallation haben
beide 4.640.768 Bytes und den Zeitstempel `4F959E2D`, unterscheiden sich aber in 15.701 Bytes (427 Bereiche, überwiegend Sprünge in nachträglich eingefügten Code);
`Engine.dll` und `Script.dll` ebenso (1.860 bzw. 461 Bytes). Deshalb schreibt das Modkit zu jedem Modul auch eine CRC32-Prüfsumme ins Protokoll, und Hooks an festen
Adressen werden nur eingehängt, wenn die Bytes passen. Bei Abweichung fehlt nur die betroffene Funktion, der Rest läuft weiter.

| Datei | Entwicklerinstallation (CRC32) | Steam-Installation (CRC32) |
|---|---|---|
| Game.dll | AA8B3F6D | 148CEEE1 |
| Engine.dll | 37DB85F7 | 2D240B9E |
| Script.dll | 906D2266 | 6DC635DE |
| Gothic3.exe | 9A6E7A58 | 9A6E7A58 |
| SharedBase.dll | 022D320C | 022D320C |

## Prüfablauf

**Immer auf einer Kopie der Installation arbeiten, nie im Original.** Den Spielordner (mit `Gothic3.exe`) kopieren und nur die Kopie verändern.
**Spielstände:** Alle Kopien benutzen denselben Ordner `Documents\gothic3`. Vorher den Ordner sichern, nur in **neue, freie Slots** speichern und Testspielstände
danach löschen.

1. **Einbauen** (Spiel beendet): aus dem Release-Zip, im entpackten Ordner:
   `powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_loader.ps1 -Game "<Kopie>" -BuildDir bin`
2. **Starten**, Spielstand laden, ein paar Minuten spielen, beenden. Dann `<Kopie>\g3asm.log` ansehen:
   - die Zeile `Voraussetzungen: Community Patch 1.75 ja, Parallel-Universe-Patch ja`;
   - sieben Zeilen `Modul ...: <Bytes> Bytes, PE-Zeitstempel ..., CRC32 ..., Spielordner ja/nein` (Gothic3.exe, Game, Engine, Script, Script_Game, SharedBase, d3d9);
   - die Hook-Zeilen vollständig (`Spielereignisse: 8 von 8 Hooks aktiv` und ähnliche, `Feste Stellen: 2 von 2 aktiv`), dazu `Qualitaets-Tooltip (Lebensraub): eingehaengt`;
   - das x87-Steuerwort steht bei jeder Zeile `FPU-Zustand ...` auf `007F`;
   - keine Zeile mit `nicht eingehaengt`, `FEHLER` oder `WARNUNG`. Steht dort `nicht eingehaengt`, ist die Spielversion eine andere als erwartet: Das Modkit
     lässt diese Funktion aus und läuft weiter. Das ist kein Absturz, aber bitte melden.
3. **Mods einschalten:** in `<Kopie>\g3asm\mods\load_order.txt` die Zeilen `+perk_lifesteal` und `+beliars_klaue` eintragen (Beispiel-Mods liegen im Zip unter
   `examples\mods`, in den Ordner `<Kopie>\g3asm\mods\` kopieren). Spiel starten, kämpfen, Charakterbogen öffnen.
4. **Speichern und Laden** mit eingeschalteten Mods, **in einen neuen Slot**.
5. **Ausbauen:** `... install_loader.ps1 -Game "<Kopie>" -Uninstall`, danach das Spiel **ohne Modkit** starten.

**Achtung bei Schritt 5:** `perk_lifesteal` legt einen Gegenstandsstapel im Inventar des Helden an. Lädt man einen Spielstand mit diesem Stapel **ohne** die Mod
und öffnet den Charakterbogen, stürzt das Spiel ab (gemessen, siehe [Grenzen](modkit/limitations.md)). Seit Version 31 nimmt die Mod den Stapel vor
jedem Speichern heraus und legt ihn danach und nach jedem Laden neu an; Spielstände dieser Fassung enthalten ihn also nicht. Ältere Spielstände können ihn
enthalten: dann `ENTFERNEN = true` in `perk_lifesteal` setzen, einmal laden, speichern und erst dann die Mod abschalten.

## Was bei einem Bericht helfen würde
- Installation (Steam/GOG/CD), DXVK ja/nein und die Zeile `Voraussetzungen: ...` aus dem Protokoll.
- `g3asm.log` und `g3asm_loader.log` aus dem Spielordner. Der Loader schreibt in der ersten Zeile den Pfad zur Einstellungsdatei; vor dem Weitergeben
  Benutzernamen im Pfad ggf. unkenntlich machen.
- Was zuletzt im Spiel passiert ist, wenn es abstürzt.
