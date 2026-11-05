# Mod-Archive packen (PackMod)

Neue Spieldaten (Quests, Infos, Vorlagen, Texturen, Töne) liefert das Spiel in Archiven aus (`Data\Quests.pak`, `Data\Templates.pak`, ...). Ein Mod bringt eigene Dateien als
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

- Spätere Archive **überschreiben** frühere, auch Dateien des Originals (so beschreibt es die Anleitung des Community-Patches; im Spiel nicht bestätigt, siehe „Grenzen“).
- Die Nummerierung muss **lückenlos** sein: Die Engine hört bei der ersten fehlenden Nummer auf (`.m00`, `.m02` ohne `.m01` lädt nur `.m00`).
- Der **Archivname** bestimmt das Volume: `Quests.mod` enthält Dateien wie `G3_World_01/Abbas_Artefakt_quest_G3_World_01.quest`, `Templates.mod` Dateien wie
  `CMB/BodyParts/Male/HERO/CMB_MaleBody.lrtpl`. Pfade im Archiv sind relativ zum Volume.
- Mehrere Mods für dasselbe Volume legen deshalb **eigene Generationen** an, statt eine gemeinsame `Quests.mod` zu überschreiben.

## Werkzeug: `og3_packmod.exe`

```
og3_packmod pack    <Ordner> <Archiv>                            Ordner als Archiv schreiben
og3_packmod install <Ordner> <Spielordner> <Volume> [m|n]        als nächste freie Generation ablegen
og3_packmod next    <Data-Ordner> <Volume> [m|n]                 nächsten freien Dateinamen anzeigen
og3_packmod list    <Archiv>                                     Inhalt anzeigen
```

`install` wählt die erste freie Datei der Reihe `Volume.mod`, `Volume.m00`, `Volume.m01`, ... (mit `n`: `.nod`, `.n00`, ...). Eine Lücke wird zuerst gefüllt. Beispiel:

```
mods\mein_mod\ModFiles\Quests\G3_World_01\Meine_quest_G3_World_01.quest
og3_packmod install mods\mein_mod\ModFiles\Quests G:\Gothic3 Quests      ->  G:\Gothic3\Data\Quests.mod
```

Optionen: `--comments` speichert den Quellpfad je Datei (wie `G3PakDir`; verrät lokale Pfade, deshalb standardmäßig aus), `--no-deleted` packt `.deleted`-Dateien als normale Dateien.

`copy` spielt ein **fertiges** Archiv als nächste Generation ein (`og3_packmod copy ModFiles\Quests.m0x G:\Gothic3 Quests`). Das ist der Schritt, den der Union Starter bei der
Installation macht: Er nummeriert `ModFiles\<Volume>.m0x` / `.n0x` selbst als `.m00`, `.m01`, ... Vorher prüft `copy`, ob die Datei ein gültiges Archiv ist.

## Mod-Ordner im Union-Starter-Layout

Ein Mod-Ordner kann dieselbe Aufteilung wie beim Union Starter haben. Der OG3-Patch ignoriert, was er nicht braucht, und der Union Starter ignoriert `main.as`:

```
mein_mod\
  Mod.ini                 [Main] Name=..., Category=...   (Union Starter)
  main.as                 Skript (OG3-Patch)
  ModFiles\
    Quests.m0x            fertiges Archiv (Union Starter nummeriert es als .m00, .m01, ...)
    stringtable.ini       Mod-Texte (der OG3-Patch liest sie auch hier; Mod-Ordner oder ModFiles sind gleichwertig)
  Source\Quests\...       Quelle des Archivs: og3_packmod pack Source\Quests ModFiles\Quests.m0x
```

**Beispiel `jens_dialog`:** Die Quest `OG3_Jens_Wolfsfell` ist eine echte Quest-Datei (`Source\Quests\G3_World_01\OG3_Jens_Wolfsfell_quest_G3_World_01.quest`: XP 100,
Belohnung `DEX` +1, Titel und Beschreibung als Text-IDs `OG3_JENS_WOLFSFELL` und `OG3_JENS_WOLFSFELL_TEXT` aus `ModFiles\stringtable.ini`). Die Dialoge und die Logik bleiben im Skript.
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
entpacken mit einem unabhängigen Python-Leser, mit `og3_packmod` neu packen, vergleichen. Namen, Verzeichnisse, Inhalt, Attribute und Kopf stimmen überein (`tests/packmod_roundtrip.py`).
Das Format entspricht `G3PakDir`: Kopf mit `G3V0` und `G3PD`, Pfade relativ mit `/`, Verzeichnisse mit `/` am Ende, Eintragsattribut „Packed“ (`0x20000`), `.dds`/`.ogg`/`.wav` als „Stream“.

## Grenzen

- **Keine Komprimierung:** Dateien werden gespeichert. Das Archiv ist größer als ein gepacktes (Vorlagenarchive bis etwa Faktor 3), die Engine liest es trotzdem. Der Union Starter packt beim „Neu verpacken“ ebenfalls ohne Komprimierung.
- **Lesen komprimierter Einträge** (`list` zeigt sie, den Inhalt nicht): nötig für `Strings.pak\stringtable.ini`, siehe [Patches](patches.md).
- **Unsere Datei-Patches kennen die Mod-Schichten noch nicht:** Sie lesen als Basis nur das `.pak`. Hat ein anderes Mod-Archiv (`.mod`, `.m00`, ...) dieselbe Datei ersetzt, patcht der Diff die Fassung aus dem `.pak`.
- **Im Spiel bestätigt (Stand 2026-11-05):** Eine **neue** Datei aus einem von `og3_packmod` geschriebenen Archiv (`Quests.m00`: die Quest `OG3_Jens_Wolfsfell`) wird gelesen, die Quest kommt aus dem Archiv und läuft durch.
- **Noch nicht bestätigt:** dass ein Archiv eine **vorhandene** Datei des Spiels überschreibt. Im Test blieb `Agenak_ArenaDefeatDimitar` bei 300 XP, obwohl `Quests.mod` die Datei mit 4242 XP enthielt. Das Dateisystem-Modul (im Differenztest
  gegen das Original geprüft) lässt das später gemountete Archiv gewinnen; offen ist, ob die Engine `Quests.mod` überhaupt mountet oder ob die Quest-Verwaltung vorhandene Namen auslässt. Ein Gegentest mit `Quests.m01` läuft.
  Bis dahin: eigene Dateien als **neue Namen** anlegen, vorhandene Dateien des Spiels mit [Datei-Patches](patches.md) ändern.
