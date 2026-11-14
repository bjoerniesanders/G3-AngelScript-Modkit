# Mod-Archive packen (PackMod)

Neue Spieldaten (Quests, Infos, Vorlagen, Texturen, Töne) liefert das Spiel in Archiven aus (`Data\Quests.pak`, `Data\Templates.pak`, ...). Eine Mod bringt eigene Dateien als
**Mod-Archiv** mit. `PackMod` schreibt dieses Archiv aus einem Ordner. Das Ergebnis liest die Engine wie ein `.pak`, und es entspricht dem, was `G3PakDir` (Union Starter)
schreibt, sodass beide Werkzeuge einander ersetzen können.

## Wie die Engine Mod-Archive lädt

Dieselbe Regel gilt für alle Hersteller (Community-Patch, Union Starter, wir):

| Reihenfolge | Endungen | Bedeutung |
|---|---|---|
| 1 | `.pak`, `.p00`, `.p01`, ... | Spiel und offizielle Patches |
| 2 | `.cpt`, `.c00`, ... | nur bei aktivem „Alternativen Balancing“ |
| 3 | **`.mod`, `.m00`, `.m01`, ...** | Mods, werden immer geladen |
| 4 | **`.nod`, `.n00`, ...** | Mods nur bei aktivem „Alternativen Balancing“ |

- Spätere Archive **überschreiben** frühere, auch Dateien des Originals (so beschreibt es die Anleitung des Community-Patches; im Spiel bestätigt, aber nur, solange keine vom Spiel angelegte physische Kopie unter `Data\<Volume>\` liegt, siehe „Grenzen“).
- Die Nummerierung muss **lückenlos** sein: Die Engine hört bei der ersten fehlenden Nummer auf (`.m00`, `.m02` ohne `.m01` lädt nur `.m00`).
- Der **Archivname** bestimmt das Volume: `Quests.mod` enthält Dateien wie `G3_World_01/Abbas_Artefakt_quest_G3_World_01.quest`, `Templates.mod` Dateien wie
  `CMB/BodyParts/Male/HERO/CMB_MaleBody.lrtpl`. Pfade im Archiv sind relativ zum Volume.
- Mehrere Mods für dasselbe Volume legen deshalb **eigene Generationen** an, statt eine gemeinsame `Quests.mod` zu überschreiben.

## Werkzeug: `g3asm_packmod.exe`

```
g3asm_packmod pack    <Ordner> <Archiv>                            Ordner als Archiv schreiben
g3asm_packmod install <Ordner> <Spielordner> <Volume> [m|n]        als nächste freie Generation ablegen
g3asm_packmod next    <Data-Ordner> <Volume> [m|n]                 nächsten freien Dateinamen anzeigen
g3asm_packmod list    <Archiv>                                     Inhalt anzeigen
```

`install` wählt die erste freie Datei der Reihe `Volume.mod`, `Volume.m00`, `Volume.m01`, ... (mit `n`: `.nod`, `.n00`, ...). Eine Lücke wird zuerst gefüllt. Beispiel:

```
mods\mein_mod\ModFiles\Quests\G3_World_01\Meine_quest_G3_World_01.quest
g3asm_packmod install mods\mein_mod\ModFiles\Quests G:\Gothic3 Quests      ->  G:\Gothic3\Data\Quests.mod
```

Optionen: `--comments` speichert den Quellpfad je Datei (wie `G3PakDir`; verrät lokale Pfade, deshalb standardmäßig aus), `--no-deleted` packt `.deleted`-Dateien als normale Dateien.

`copy` spielt ein **fertiges** Archiv als nächste Generation ein (`g3asm_packmod copy ModFiles\Quests.m0x G:\Gothic3 Quests`). Das ist der Schritt, den der Union Starter bei der
Installation macht: Er nummeriert `ModFiles\<Volume>.m0x` / `.n0x` selbst als `.m00`, `.m01`, ... Vorher prüft `copy`, ob die Datei ein gültiges Archiv ist.

## Mod-Ordner im Union-Starter-Layout

Ein Mod-Ordner kann dieselbe Aufteilung wie beim Union Starter haben. G3ASM ignoriert, was er nicht braucht, und der Union Starter ignoriert `main.as`:

```
mein_mod\
  Mod.ini                 [Main] Name=..., Category=...   (Union Starter)
  main.as                 Skript (G3ASM)
  ModFiles\
    Quests.m0x            fertiges Archiv (Union Starter nummeriert es als .m00, .m01, ...)
    stringtable.ini       Mod-Texte (G3ASM liest sie auch hier; Mod-Ordner oder ModFiles sind gleichwertig)
  Source\Quests\...       Quelle des Archivs: g3asm_packmod pack Source\Quests ModFiles\Quests.m0x
```

**Beispiel `jens_dialog`:** Die Quest `G3ASM_Jens_Wolfsfell` ist eine echte Quest-Datei (`Source\Quests\G3_World_01\G3ASM_Jens_Wolfsfell_quest_G3_World_01.quest`: XP 100,
Belohnung `DEX` +1, Titel und Beschreibung als Text-IDs `G3ASM_JENS_WOLFSFELL` und `G3ASM_JENS_WOLFSFELL_TEXT` aus `ModFiles\stringtable.ini`). Die Dialoge und die Logik bleiben im Skript.
Ist das Archiv nicht installiert, legt das Skript die Quest wie bisher zur Laufzeit an. Im Protokoll steht, woher sie kam: `OnQuestsLoaded: Quest vorbereitet (aus dem Mod-Archiv, XP 100)`
oder `(zur Laufzeit angelegt)`.

### Dateien des Spiels entfernen

Eine Datei `<name>.deleted` im Ordner (Inhalt egal) wird zu einem Eintrag „Datei gelöscht“. Das Archiv entfernt damit die Datei `<name>` aus den darunterliegenden Schichten.
Das ist dieselbe Schreibweise wie bei `G3PakDir`.

## Skript-Schnittstelle (ab Version 24)

| Funktion | Bedeutung |
|---|---|
| `bCString PackMod(const bCString &in ordner, const bCString &in volume, bool alternativeBalancing = false)` | Ordner als nächste freie Generation in `<Spiel>\Data` ablegen; Rückgabe: Pfad der Datei, leer bei Fehler |
| `bool PackFolder(const bCString &in ordner, const bCString &in archivDatei)` | Archiv an beliebigem Ort schreiben |
| `bCString NextModFile(const bCString &in volume, bool alternativeBalancing = false)` | Welche Datei `PackMod` als Nächstes anlegen würde; leer bei ungültigem Namen |
| `bCString GetPackError()` | Fehlertext des letzten `PackMod`/`PackFolder`; leer bei Erfolg |

Ein neu angelegtes Archiv liest das Spiel erst **beim nächsten Start**.

## Kompatibilität

Geprüft mit 8 Archiven der Community (`Quests.mod`, `Infos.m00`, `Infos.mod`, `Quests.m05`, `Templates.m02`, `Templates.mod`, `Library.mod`, `Materials.mod`, zusammen rund 1 600 Dateien):
entpacken mit einem unabhängigen Python-Leser, mit `g3asm_packmod` neu packen, vergleichen. Namen, Verzeichnisse, Inhalt, Attribute und Kopf stimmen überein (`tests/packmod_roundtrip.py`).
Das Format entspricht `G3PakDir`: Kopf mit `G3V0` und `G3PD`, Pfade relativ mit `/`, Verzeichnisse mit `/` am Ende, Eintragsattribut „Packed“ (`0x20000`), `.dds`/`.ogg`/`.wav` als „Stream“.

## Grenzen

- **Keine Komprimierung:** Dateien werden gespeichert. Das Archiv ist größer als ein gepacktes (Vorlagenarchive bis etwa Faktor 3), die Engine liest es trotzdem. Der Union Starter packt beim „Neu verpacken“ ebenfalls ohne Komprimierung.
- **Lesen komprimierter Einträge** (`list` zeigt sie, den Inhalt nicht): nötig für `Strings.pak\stringtable.ini`, siehe [Patches](patches.md).
- **Unsere Datei-Patches kennen die Mod-Schichten noch nicht:** Sie lesen als Basis nur das `.pak`. Hat ein anderes Mod-Archiv (`.mod`, `.m00`, ...) dieselbe Datei ersetzt, patcht der Diff die Fassung aus dem `.pak`.
- **Im Spiel bestätigt (2026-11-05, wiederholt 2026-11-06 ohne physische Kopie):** Eine **neue** Datei aus einem von `g3asm_packmod` geschriebenen Archiv (`Quests.m00`: die Quest `G3ASM_Jens_Wolfsfell`) wird gelesen, die Quest kommt aus dem Archiv und läuft durch.
- **Überschreiben vorhandener Dateien funktioniert (Stand 2026-11-06):** `Quests.mod` und `Quests.m01` mit der Datei `Agenak_ArenaDefeatDimitar` (4242 XP statt 300) setzen die Quest im Spiel auf 4242, sobald **keine physische Kopie** der Datei unter `Data\<Volume>\` liegt (im Spiel bestätigt, `demo_packmod`).
- **Falle: physische Kopien verdecken Archive.** Das Dateisystem des Spiels bevorzugt Dateien im Ordner `Data\<Volume>\` (z. B. `Data\Quests\G3_World_01\…quest`) vor allen Archiven. Und das Spiel legt diese Dateien selbst an: Beim Laden hat es alle 571 Quest-Dateien dorthin geschrieben, ohne dass sie vorher da waren, mit den Werten, die gerade galten. Eine solche Kopie aus einem früheren Lauf überdeckt jedes später installierte Mod-Archiv; so ist der frühere Fehlschlag (Quest blieb bei 300 XP) entstanden. Folge: Ein Mod-Archiv, das Dateien eines Volumes ersetzt, wirkt erst, wenn der Ordner `Data\<Volume>\` (nur die vom Spiel erzeugten Kopien, nicht Overlay-Dateien eigener Datei-Patches) entfernt wird; das Spiel erzeugt ihn beim nächsten Laden aus den Archiven neu. Eine Änderung am Mod-Archiv wirkt ebenso erst nach dem Entfernen.
- **Hauptmenü:** Beim ersten `OnQuestsLoaded` (im Hauptmenü) ist der Quest-Manager leer: `GetQuestCount()` meldet -1 und auch die Quests des Spiels fehlen. Das hat nichts mit dem Archiv zu tun. Skripte legen dann nichts an (Prüfung `GetQuestCount() < 0`); der echte Aufbau folgt beim Laden eines Spielstands.
