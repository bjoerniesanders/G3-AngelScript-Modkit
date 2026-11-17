# Mod-Archive packen (PackMod)

**Deutsch** | [English](../en/modkit/archives.md)

Das Spiel liefert seine Daten (Quests, Infos, Vorlagen, Texturen, Töne) in Archiven aus (`Data\Quests.pak`, `Data\Templates.pak`, ...). Eine Mod
bringt eigene Dateien als **Mod-Archiv** mit. `PackMod` schreibt dieses Archiv aus einem Ordner. Die Engine liest das Ergebnis wie ein `.pak`. Das
Ergebnis entspricht dem, was `G3PakDir` (Union Starter) schreibt. Deshalb kann ein Werkzeug das andere ersetzen.

## Wie die Engine Mod-Archive lädt

Dieselbe Regel gilt für alle Hersteller: Community Patch, Union Starter und G3ASM.

| Reihenfolge | Endungen | Bedeutung |
|---|---|---|
| 1 | `.pak`, `.p00`, `.p01`, ... | Spiel und offizielle Patches |
| 2 | `.cpt`, `.c00`, ... | Nur bei aktivem „Alternativen Balancing“ |
| 3 | **`.mod`, `.m00`, `.m01`, ...** | Mods. Die Engine lädt sie immer |
| 4 | **`.nod`, `.n00`, ...** | Mods nur bei aktivem „Alternativen Balancing“ |

- Spätere Archive **überschreiben** frühere. Das gilt auch für Dateien des Originals. So beschreibt es die Anleitung des Community Patch. Im Spiel
  habe ich es bestätigt, aber nur, solange keine vom Spiel angelegte physische Kopie unter `Data\<Volume>\` liegt (siehe „Grenzen“).
- Die Nummerierung muss **lückenlos** sein. Die Engine hört bei der ersten fehlenden Nummer auf. Gibt es `.m00` und `.m02` ohne `.m01`, lädt sie nur
  `.m00`.
- Der **Archivname** bestimmt das Volume. `Quests.mod` enthält Dateien wie `G3_World_01/Abbas_Artefakt_quest_G3_World_01.quest`. `Templates.mod`
  enthält Dateien wie `CMB/BodyParts/Male/HERO/CMB_MaleBody.lrtpl`. Pfade im Archiv sind relativ zum Volume.
- Haben mehrere Mods Dateien für dasselbe Volume, legt deshalb jede Mod **ihre eigene Generation** an. Sie überschreiben keine gemeinsame
  `Quests.mod`.

## Werkzeug: `g3asm_packmod.exe`

```
g3asm_packmod pack    <Ordner> <Archiv>                            Ordner als Archiv schreiben
g3asm_packmod install <Ordner> <Spielordner> <Volume> [m|n]        als nächste freie Generation ablegen
g3asm_packmod next    <Data-Ordner> <Volume> [m|n]                 nächsten freien Dateinamen anzeigen
g3asm_packmod list    <Archiv>                                     Inhalt anzeigen
g3asm_packmod copy    <Archiv> <Spielordner> <Volume> [m|n]        fertiges Archiv als nächste Generation einspielen
g3asm_packmod release <Mod-Ordner> <Zielordner>                    Paket für Spieler, siehe Mods weitergeben
```

`release` beschreibt [Mods weitergeben](mod-package.md#mods-weitergeben).

`install` wählt die erste freie Datei der Reihe `Volume.mod`, `Volume.m00`, `Volume.m01`, ... (mit `n`: `.nod`, `.n00`, ...). Eine Lücke füllt es
zuerst. Beispiel:

```
mods\meine_mod\ModFiles\Quests\G3_World_01\Meine_quest_G3_World_01.quest
g3asm_packmod install mods\meine_mod\ModFiles\Quests G:\Gothic3 Quests      ->  G:\Gothic3\Data\Quests.mod
```

Optionen:
- `--comments` speichert den Quellpfad jeder Datei, wie `G3PakDir`. Das verrät lokale Pfade, deshalb ist es standardmäßig aus.
- `--no-deleted` packt `.deleted`-Dateien als normale Dateien.

`copy` spielt ein **fertiges** Archiv als nächste Generation ein (`g3asm_packmod copy ModFiles\Quests.m0x G:\Gothic3 Quests`). Diesen Schritt macht
der Union Starter bei der Installation: Er nummeriert `ModFiles\<Volume>.m0x` und `.n0x` selbst als `.m00`, `.m01`, ... Vorher prüft `copy`, ob die
Datei ein gültiges Archiv ist.

## Mod-Ordner im Union-Starter-Layout

Ein Mod-Ordner kann dieselbe Aufteilung wie beim Union Starter haben. G3ASM ignoriert, was es nicht braucht. Der Union Starter ignoriert `main.as`:

```
meine_mod\
  Mod.ini                 [Main] Name=..., Category=...   (Union Starter)
  main.as                 Skript (G3ASM)
  ModFiles\
    Quests.m0x            fertiges Archiv (der Union Starter nummeriert es als .m00, .m01, ...)
    stringtable.ini       Mod-Texte (G3ASM liest sie auch hier; Mod-Ordner und ModFiles sind gleichwertig)
  Source\Quests\...       Quelle des Archivs: g3asm_packmod pack Source\Quests ModFiles\Quests.m0x
```

**Beispiel `jens_dialog`:** Die Quest `G3ASM_Jens_Wolfsfell` ist eine echte Quest-Datei
(`Source\Quests\G3_World_01\G3ASM_Jens_Wolfsfell_quest_G3_World_01.quest`). Sie hat XP 100 und die Belohnung `DEX` +1. Titel und Beschreibung sind
die Text-IDs `G3ASM_JENS_WOLFSFELL` und `G3ASM_JENS_WOLFSFELL_TEXT` aus `ModFiles\stringtable.ini`. Die Dialoge und die Logik bleiben im Skript. Ist
das Archiv nicht installiert, legt das Skript die Quest wie bisher zur Laufzeit an. Im Protokoll steht, woher die Quest kam:
`OnQuestsLoaded: Quest vorbereitet (aus dem Mod-Archiv, XP 100)` oder `(zur Laufzeit angelegt)`.

### Dateien des Spiels entfernen

Eine Datei `<name>.deleted` im Ordner wird zu einem Eintrag „Datei gelöscht“. Ihr Inhalt spielt keine Rolle. Mit diesem Eintrag entfernt das Archiv
die Datei `<name>` aus den darunterliegenden Schichten. `G3PakDir` nutzt dieselbe Schreibweise.

## Skript-Schnittstelle (ab Version 24)

| Funktion | Bedeutung |
|---|---|
| `bCString PackMod(const bCString &in ordner, const bCString &in volume, bool alternativeBalancing = false)` | Legt den Ordner als nächste freie Generation in `<Spiel>\Data` ab. Rückgabe: Pfad der Datei, leer bei Fehler |
| `bool PackFolder(const bCString &in ordner, const bCString &in archivDatei)` | Schreibt ein Archiv an einen beliebigen Ort |
| `bCString NextModFile(const bCString &in volume, bool alternativeBalancing = false)` | Die Datei, die `PackMod` als Nächstes anlegen würde. Leer bei ungültigem Namen |
| `bCString GetPackError()` | Fehlertext des letzten `PackMod` oder `PackFolder`. Leer bei Erfolg |

Ein neu angelegtes Archiv liest das Spiel erst **beim nächsten Start**.

## Kompatibilität

Ich habe 8 Archive der Community geprüft: `Quests.mod`, `Infos.m00`, `Infos.mod`, `Quests.m05`, `Templates.m02`, `Templates.mod`, `Library.mod`,
`Materials.mod`. Zusammen enthalten sie rund 1600 Dateien. Der Test entpackt jedes Archiv mit einem unabhängigen Python-Leser, packt es mit
`g3asm_packmod` neu und vergleicht beide. Namen, Verzeichnisse, Inhalt, Attribute und Kopf stimmen überein (`tests/packmod_roundtrip.py`).

Das Format entspricht dem von `G3PakDir`:
- Kopf mit `G3V0` und `G3PD`.
- Relative Pfade mit `/`.
- Verzeichnisse mit `/` am Ende.
- Eintragsattribut „Packed“ (`0x20000`).
- `.dds`, `.ogg` und `.wav` als „Stream“.

## Grenzen

- **Keine Komprimierung:** Das Werkzeug speichert die Dateien. Das Archiv ist größer als ein komprimiertes (Vorlagenarchive bis etwa Faktor 3). Die
  Engine liest es trotzdem. Auch der Union Starter packt beim „Neu verpacken“ ohne Komprimierung.
- **Komprimierte Einträge lesen:** `list` zeigt sie, aber nicht ihren Inhalt. Das wäre nötig für `Strings.pak\stringtable.ini`. Siehe
  [Patches](patches.md).
- **Die Datei-Patches von G3ASM kennen die Mod-Schichten noch nicht.** Sie lesen als Basis nur das `.pak`. Hat ein anderes Mod-Archiv (`.mod`,
  `.m00`, ...) dieselbe Datei ersetzt, patcht der Diff die Fassung aus dem `.pak`.
- **Neue Dateien** aus einem Archiv von `g3asm_packmod` liest das Spiel. Geprüft habe ich das mit der Quest `G3ASM_Jens_Wolfsfell` in `Quests.m00`.
- **Vorhandene Dateien überschreiben** funktioniert ebenfalls, solange **keine lose Kopie** der Datei unter `Data\<Volume>\` liegt. Siehe den nächsten
  Punkt und das Beispiel `demo_packmod`.
- **Achtung: Lose Kopien verdecken Archive.** Liegt eine Datei lose im Ordner `Data\<Volume>\` (z. B. `Data\Quests\G3_World_01\…quest`), nimmt das
  Spiel diese und nicht die aus einem Archiv. Das Spiel legt solche Kopien beim Laden selbst an, für alle 571 Quest-Dateien. Eine Kopie aus einem
  früheren Lauf verdeckt also jedes später installierte Mod-Archiv. Abhilfe:
  1. Beende das Spiel.
  2. Lösche den Ordner `Data\<Volume>\` (z. B. `Data\Quests\`). Haben eigene Datei-Patches dort Dateien angelegt, legt das Modkit sie beim nächsten
     Start neu an.
  3. Starte das Spiel und lade einen Spielstand. Das Spiel legt den Ordner aus den Archiven neu an, jetzt mit den Dateien deiner Mod.

  Dasselbe gilt, wenn du ein schon installiertes Mod-Archiv änderst.
- **Hauptmenü:** Beim ersten `OnQuestsLoaded` im Hauptmenü gibt es noch keine Quests (`GetQuestCount()` liefert -1), auch nicht die des Spiels. Lege
  dann nichts an (`if (GetQuestCount() < 0) return;`). Der richtige Aufbau folgt, wenn der Spieler einen Spielstand lädt.
