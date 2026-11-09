# Kompatibilität

Der OG3-Patch ist ein **Alpha**-Stand. Getestet ist bisher nur eine Installation. Diese Seite sammelt, was auf welcher Installation geprüft wurde. Wer den Patch
auf einer anderen Installation ausprobiert, kann das Ergebnis gern melden (Protokolle siehe unten).

## Testmatrix

| Installation | Community Patch | PU-Patch | DXVK | Ergebnis | Stand |
|---|---|---|---|---|---|
| Entwicklerinstallation (Kopie, Windows 11) | 1.75 | ja | ja | alle Prüfungen bestanden | API 30, 2026-11 |
| Steam (stark gemoddet, Kopie) | Steam-Fassung mit eigenem Patchstand | ja | ja (anderer DXVK-Stand) | Start, Spielstand laden, Weltwechsel, Beenden ohne Fehler (nur Mini-Mod); alle 8 Spiel-Hooks und 2 Statuseffekt-Hooks aktiv, x87-Steuerwort durchgehend 007F; Tooltip-Hook ausgelassen, weil `Game.dll+0xA48B0` dort abweicht (erwartet). Offen: Beispiel-Mods, Speichern/Laden, Ausbau | 2026-11 |
| GOG | offen | offen | offen | nicht getestet | |
| CD/DVD-Version | offen | offen | offen | nicht getestet | |

Weitere Zeilen für Kombinationen: ohne Community Patch, mit/ohne PU-Patch, mit/ohne DXVK.

## Bekannte Unterschiede zwischen Installationen

Gleiche Dateigröße und gleicher PE-Zeitstempel heißen **nicht** gleicher Inhalt. Die `Game.dll` einer Steam-Installation und die der Entwicklerinstallation haben
beide 4.640.768 Bytes und den Zeitstempel `4F959E2D`, unterscheiden sich aber in 15.701 Bytes (427 Bereiche, überwiegend Sprünge in nachträglich eingefügten Code);
`Engine.dll` und `Script.dll` ebenso (1.860 bzw. 461 Bytes). Deshalb schreibt der Patch zu jedem Modul auch eine CRC32-Prüfsumme ins Protokoll, und Hooks an festen
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
2. **Starten**, Spielstand laden, ein paar Minuten spielen, beenden. Dann `<Kopie>\og3_patch.log` ansehen:
   - sieben Zeilen `Modul ...: <Bytes> Bytes, PE-Zeitstempel ..., CRC32 ..., Spielordner ja/nein` (Gothic3.exe, Game, Engine, Script, Script_Game, SharedBase, d3d9);
   - alle Hooks als `eingehaengt`, `Qualitaets-Tooltip (Lebensraub): eingehaengt`;
   - das x87-Steuerwort steht bei jeder Zeile `FPU-Zustand ...` auf `007F`;
   - keine Zeile mit `nicht eingehaengt`, `FEHLER` oder `WARNUNG`. Steht dort `nicht eingehaengt`, ist die Spielversion eine andere als erwartet: Der Patch
     lässt diese Funktion aus und läuft weiter. Das ist kein Absturz, aber bitte melden.
3. **Mods einschalten:** in `<Kopie>\og3\mods\load_order.txt` die Zeilen `+perk_lifesteal` und `+beliars_klaue` eintragen (Beispiel-Mods liegen im Zip unter
   `examples\mods`, in den Ordner `<Kopie>\og3\mods\` kopieren). Spiel starten, kämpfen, Charakterbogen öffnen.
4. **Speichern und Laden** mit eingeschalteten Mods, **in einen neuen Slot**.
5. **Ausbauen:** `... install_loader.ps1 -Game "<Kopie>" -Uninstall`, danach das Spiel **ohne Patch** starten.

**Achtung bei Schritt 5:** `perk_lifesteal` legt einen Gegenstandsstapel im Inventar des Helden an. Er steht im Spielstand. Lädt man einen Spielstand mit diesem
Stapel **ohne** die Mod und öffnet den Charakterbogen, stürzt das Spiel ab (gemessen, siehe [Grenzen und Stand](modkit/limitations.md)). Zum Ausbau-Test deshalb
einen Spielstand laden, der **vor** dem Einschalten der Mods entstand, oder `ENTFERNEN = true` in `perk_lifesteal` setzen, einmal laden, speichern und erst dann
die Mod abschalten.

## Was bei einem Bericht helfen würde
- Installation (Steam/GOG/CD), Version des Community Patch, PU-Patch ja/nein, DXVK ja/nein.
- `og3_patch.log` und `og3_loader.log` aus dem Spielordner. Der Loader schreibt in der ersten Zeile den Pfad zur Einstellungsdatei; vor dem Weitergeben
  Benutzernamen im Pfad ggf. unkenntlich machen.
- Was zuletzt im Spiel passiert ist, wenn es abstürzt.
