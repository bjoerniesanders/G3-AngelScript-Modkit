# G3 AngelScript Modkit (G3ASM): Modding für Gothic 3

Mit **G3ASM** schreibst du Mods für Gothic 3 in einer Skriptsprache, ohne das Spiel neu zu kompilieren und ohne Spieldateien zu verändern.
Die Skripte sind **AngelScript** (C++-ähnliche Syntax, `int`, `float`, `bCString`, Klassen, Arrays). Das Modkit lädt sie beim Spielstart, führt sie
bei Spielereignissen aus und lädt sie im laufenden Spiel neu, sobald du die Datei speicherst.

**Für wen:** Modder mit Grundkenntnissen im Programmieren.

Aktuelle Skript-Schnittstelle: **Version 46** (`GetApiVersion()`).

## Inhalt

| Seite | Inhalt |
|---|---|
| [Erste Mod Schritt für Schritt](erste-mod.md) | **Hier anfangen:** eine kleine Mod mit Quest und Gespräch, Kapitel für Kapitel |
| [Mod-Ordner und Installation](mod-package.md) | Wo eine Mod liegt, Ladereihenfolge, ini-Einstellungen, Protokoll, Hot-Reload |
| [Scripting-Guide](scripting-guide.md) | Die Sprache, Eigenheiten, Fallstricke, Fehlersuche |
| [Ereignisse](events.md) | Alle Funktionen, die eine Mod definieren kann (Engine- und Spielereignisse) |
| [API-Referenz](api-reference.md) | Alle Typen und Funktionen, die ein Skript aufrufen kann, mit Vermerken (experimentell, veraltet, ...) |
| [Versionsregeln](api-policy.md) | Was sich zwischen zwei Versionen der Skript-Schnittstelle ändern darf |
| [Quests](quests.md) | Quests anlegen, steuern, mit Texten und Belohnung versehen |
| [Hooks](hooks.md) | Spielfunktionen mit Override und Super() ändern oder abbrechen |
| [Datei-Patches](patches.md) | Unified-Diff-Patches auf Quest- und Info-Dateien des Spiels |
| [Mod-Archive packen](archives.md) | `PackMod`: Ordner als `.mod`/`.m00`-Archiv schreiben, kompatibel zu G3PakDir und Union Starter |
| [Dialoge](dialogs.md) | Gespräche (Infos) lesen und eigene anlegen, Befehle, Questlog-Verbindung |
| [Speichern und Laden](saving.md) | Lade-Ereignisse, Schalter im Spielstand, Wiederherstellung |
| [Beispiele](examples.md) | Fertige Mods zum Lernen und Kopieren |
| [Referenz](reference.md) | Geprüfte Werte, Namen und Konstanten: Schadensarten, Statuseffekte, Qualitäten, Slots, KI-Modi, Attribute, Namen aus dem Spiel |
| [Grenzen](limitations.md) | Was geht, was nicht geht und was ungetestet ist |

## Schnellstart

1. Modkit einbauen (Spiel beenden; Voraussetzung Community Patch 1.75 und Parallel-Universe-Patch), im entpackten Release-Ordner:
   `powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_loader.ps1 -Game "<Spielordner>" -BuildDir bin` (siehe [Mod-Ordner](mod-package.md)).
2. Ordner `<Spielordner>\g3asm\mods\hallo\` anlegen, darin `main.as`:
   ```angelscript
   void main() {
       print("Hallo aus meiner ersten Mod");
   }

   void OnKey(int vk, bool down) {
       if (down && vk == 0x91)                       // Rollen-Taste (Scroll Lock)
           PrintScreen("Hallo!", 3, 255, 255, 255);  // Text, Sekunden, R, G, B
   }
   ```
3. Spiel starten, die Rollen-Taste (Scroll Lock) drücken: der Text erscheint auf dem Bildschirm. In `<Spielordner>\g3asm.log` steht `[hallo] Hallo aus meiner ersten Mod`.
4. Datei ändern und speichern: nach einer Sekunde ist die neue Fassung aktiv, ohne Neustart.

Weiter geht es mit [Erste Mod Schritt für Schritt](erste-mod.md).

## Wie es funktioniert (Kurzfassung)

- Das Modkit (`g3asm.dll`) wird vom Loader (einer Ersatz-`dbghelp.dll` im Spielordner) geladen und hängt sich über die Importtabelle von `Gothic3.exe` ein
  (kein Code-Eingriff, verträglich mit dem Parallel-Universe-Patch).
- Für Spielereignisse (Kills, Items, Zauber, ...) werden einzelne Funktionen der `Game.dll` im Speicher abgefangen (MinHook). Auf der Platte bleibt alles unverändert.
- Skripte rufen dieselben Engine-Funktionen auf wie das Spiel selbst, zum Beispiel für Attribute, Inventar und Quests. Das Verhalten ist deshalb das Originalverhalten.

## Lizenz deiner Mod

Das Modkit steht unter GPL-3.0-or-later. Eine Zusatzerlaubnis (`LICENSE-EXCEPTIONS.md` im Hauptordner) stellt klar: Mods, die das Modkit nur über die
Skript-Schnittstelle nutzen (Skripte, `mod.ini`, Stringtabellen, Datei-Patches, Archive), darfst du unter jeder Lizenz veröffentlichen. Die Beispiel-Mods
sind gemeinfrei (CC0), du kannst sie ohne Bedingungen in eigene Mods übernehmen.
