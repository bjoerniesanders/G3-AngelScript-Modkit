# OG3 Modkit: Modding für Gothic 3 mit AngelScript

Mit dem **OG3-Patch** schreibst du Mods für Gothic 3 in einer Skriptsprache, ohne das Spiel neu zu kompilieren und ohne Spieldateien zu verändern.
Die Skripte sind **AngelScript** (C++-ähnliche Syntax, `int`, `float`, `bCString`, Klassen, Arrays). Der Patch lädt sie beim Spielstart, führt sie
bei Spielereignissen aus und lädt sie im laufenden Spiel neu, sobald du die Datei speicherst.

Aktuelle Skript-Schnittstelle: **Version 28** (`GetApiVersion()`).

## Inhalt

| Seite | Inhalt |
|---|---|
| [Mod-Ordner und Installation](mod-package.md) | Wo ein Mod liegt, Ladereihenfolge, ini-Einstellungen, Protokoll, Hot-Reload |
| [Scripting-Guide](scripting-guide.md) | Die Sprache, Eigenheiten, Fallstricke, Fehlersuche |
| [Ereignisse](events.md) | Alle Funktionen, die ein Mod definieren kann (Engine- und Spielereignisse) |
| [API-Referenz](api-reference.md) | Alle Typen und Funktionen, die ein Skript aufrufen kann |
| [Quests](quests.md) | Quests anlegen, steuern, mit Texten und Belohnung versehen |
| [Hooks](hooks.md) | Spielfunktionen mit Override und Super() ändern oder abbrechen |
| [Datei-Patches](patches.md) | Unified-Diff-Patches auf Quest- und Info-Dateien des Spiels |
| [Mod-Archive packen](archives.md) | `PackMod`: Ordner als `.mod`/`.m00`-Archiv schreiben, kompatibel zu G3PakDir und Union Starter |
| [Dialoge](dialogs.md) | Gespräche (Infos) lesen und eigene anlegen, Befehle, Questlog-Verbindung |
| [Speichern und Laden](saving.md) | Lade-Ereignisse, Schalter im Spielstand, Wiederherstellung |
| [Beispiele](examples.md) | Fertige Mods zum Lernen und Kopieren |
| [Referenz](reference.md) | Geprüfte Werte, Namen und Konstanten: Schadensarten, Statuseffekte, Qualitäten, Slots, KI-Modi, Attribute, Namen aus dem Spiel |
| [Grenzen und Stand](limitations.md) | Was funktioniert, was ungetestet ist, was noch fehlt |

## Schnellstart

1. Patch einbauen (Spiel beenden): `powershell -NoProfile -File tools\install_patch.ps1` (siehe [Mod-Ordner](mod-package.md)).
2. Ordner `G:\Gothic3\og3\mods\hallo\` anlegen, darin `main.as`:
   ```angelscript
   void main() {
       print("Hallo aus meinem ersten Mod");
   }

   void OnKey(int vk, bool down) {
       if (down && vk == 0x91)                       // Rollen-Taste (Scroll Lock)
           PrintScreen("Hallo!", 3, 255, 255, 255);  // Text, Sekunden, R, G, B
   }
   ```
3. Spiel starten, die Rollen-Taste (Scroll Lock) drücken: der Text erscheint auf dem Bildschirm. In `G:\Gothic3\og3_patch.log` steht `[hallo] Hallo aus meinem ersten Mod`.
4. Datei ändern und speichern: nach einer Sekunde ist die neue Fassung aktiv, ohne Neustart.

## Wie es funktioniert (Kurzfassung)

- Der Patch (`og3_patch.dll`) wird vom `dbghelp.dll`-Proxy im Spielordner geladen und hängt sich über die Importtabelle von `Gothic3.exe` ein
  (kein Code-Eingriff, verträglich mit dem Parallel-Universe-Patch).
- Für Spielereignisse (Kills, Items, Zauber, ...) werden einzelne Funktionen der `Game.dll` im Speicher abgefangen (MinHook). Auf der Platte bleibt alles unverändert.
- Skripte rufen dieselben Engine-Funktionen auf wie das Spiel selbst, zum Beispiel für Attribute, Inventar und Quests. Das Verhalten ist deshalb das Originalverhalten.
