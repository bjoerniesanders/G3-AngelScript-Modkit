# G3 AngelScript Modkit (G3ASM): Mods für Gothic 3

**Deutsch** | [English](../en/modkit/index.md)

Mit **G3ASM** schreibst du Mods für Gothic 3 in einer Skriptsprache. Du kompilierst das Spiel nicht neu und veränderst keine Spieldateien.
Die Skripte sind **AngelScript**. Die Syntax ähnelt C++: `int`, `float`, `bCString`, Klassen, Arrays. Das Modkit lädt die Skripte beim Spielstart.
Es führt sie aus, wenn Spielereignisse eintreten. Speicherst du eine Skriptdatei im laufenden Spiel, lädt das Modkit das Skript neu.

**Für wen:** Modder mit Grundkenntnissen im Programmieren.

Aktuelle Skript-Schnittstelle: **Version 47** (`GetApiVersion()`).

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
| [Mod-Archive packen](archives.md) | `PackMod`: einen Ordner als `.mod`- oder `.m00`-Archiv schreiben, kompatibel zu G3PakDir und Union Starter |
| [Kommandozeile](cli.md) | `g3asm.exe`: neue Mod anlegen, Skript ohne Spiel prüfen, packen, Protokoll lesen, jeweils auch als JSON |
| [Dialoge](dialogs.md) | Gespräche (Infos) lesen und eigene anlegen, Befehle, Verbindung zum Questlog |
| [Speichern und Laden](saving.md) | Lade-Ereignisse, Schalter im Spielstand, Wiederherstellung |
| [Beispiele](examples.md) | Fertige Mods zum Lernen und Kopieren |
| [Referenz](reference.md) | Geprüfte Werte, Namen und Konstanten: Schadensarten, Statuseffekte, Qualitäten, Slots, KI-Modi, Attribute, Namen aus dem Spiel |
| [Grenzen](limitations.md) | Was geht, was nicht geht und was ungetestet ist |

## Schnellstart

1. Baue das Modkit ein. Beende vorher das Spiel. Das Spiel braucht den Community Patch 1.75 und den Parallel-Universe-Patch. Gib im
   entpackten Release-Ordner ein:
   `powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_loader.ps1 -Game "<Spielordner>" -BuildDir bin`.
   Einzelheiten stehen unter [Mod-Ordner](mod-package.md).
2. Lege den Ordner `<Spielordner>\g3asm\mods\hallo\` an. Lege darin eine Datei `main.as` an:
   ```angelscript
   void main() {
       print("Hallo aus meiner ersten Mod");
   }

   void OnKey(int vk, bool down) {
       if (down && vk == 0x91)                       // Rollen-Taste (Scroll Lock)
           PrintScreen("Hallo!", 3, 255, 255, 255);  // Text, Sekunden, R, G, B
   }
   ```
3. Starte das Spiel. Drücke die Rollen-Taste. Der Text erscheint auf dem Bildschirm. In `<Spielordner>\g3asm.log` steht
   `[hallo] Hallo aus meiner ersten Mod`.
4. Ändere die Datei und speichere sie. Nach 1 Sekunde ist die neue Fassung aktiv. Du musst das Spiel nicht neu starten.

Weiter geht es mit [Erste Mod Schritt für Schritt](erste-mod.md).

## Wie es funktioniert (Kurzfassung)

- Der Loader ist eine Ersatz-`dbghelp.dll` im Spielordner. Er lädt das Modkit (`g3asm.dll`). Das Modkit hängt sich über die Importtabelle von
  `Gothic3.exe` ein. Es greift nicht in den Code ein und ist deshalb verträglich mit dem Parallel-Universe-Patch.
- Für Spielereignisse (Kills, Items, Zauber, ...) fängt das Modkit einzelne Funktionen der `Game.dll` im Speicher ab (MinHook). Die Dateien auf der
  Platte bleiben unverändert.
- Skripte rufen dieselben Engine-Funktionen auf wie das Spiel selbst, zum Beispiel für Attribute, Inventar und Quests. Das Verhalten ist deshalb
  das Originalverhalten.

## Lizenz deiner Mod

Das Modkit steht unter GPL-3.0-or-later. Eine Zusatzerlaubnis (`LICENSE-EXCEPTIONS.md` im Hauptordner) stellt klar: Nutzt eine Mod das Modkit nur
über die Skript-Schnittstelle, darfst du sie unter jeder Lizenz veröffentlichen. Zur Skript-Schnittstelle gehören Skripte, `mod.ini`,
Stringtabellen, Datei-Patches und Archive. Die Beispiel-Mods sind gemeinfrei (CC0). Du kannst sie ohne Bedingungen in eigene Mods übernehmen.
