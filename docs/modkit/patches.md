# Datei-Patches (ScriptPatches)

**Deutsch** | [English](../en/modkit/patches.md)

Wie beim G1R-Modkit kann eine Mod **Unified-Diff-Dateien** mitbringen. Diese Dateien ändern die Datendateien des Spiels, **bevor das Spiel sie
liest**. Gothic 3 hat keine Spielskripte. Seine Quests, Dialoge und Tabellen sind Textdateien in Archiven (`Data\Quests.pak`, `Data\Infos.pak`, ...).
Auf genau diese Textdateien wirken die Patches.

```
<Gothic 3>\g3asm\mods\meine_mod\ScriptPatches\wolfsfell_xp.patch
```

Eine Mod kann **nur** aus Patches bestehen, ohne `main.as`. Sie braucht dann nur den Ordner `ScriptPatches` mit `.patch`-Dateien.

## Format

Das Format ist ein Standard-Unified-Diff (`diff -u`, `git diff`). Die Pfade beginnen mit `a/` und `b/`:

```
--- a/Data/Quests/G3_World_01/Abbas_Artefakt_quest_G3_World_01.quest
+++ b/Data/Quests/G3_World_01/Abbas_Artefakt_quest_G3_World_01.quest
@@ -5,5 +5,5 @@
 LogTopic=QUEST_FILLER60
 LogText
-ExperiencePoints=300
+ExperiencePoints=600
 PoliticalSuccess=0
 PoliticalSuccessAmount=0
```

- Der Pfad lautet `Data/<Archiv>/<Pfad im Archiv>`. `Data/Quests/...` liegt im Archiv `Data\Quests.pak`. `Data/Infos/...` liegt in
  `Data\Infos.pak`. Hat das Archiv einen Unterordner (`G3_World_01/`), gehört er zum Pfad.
- `--- /dev/null` legt eine **neue Datei** an.
- Der Patch darf gegenüber der erwarteten Zeile bis zu 300 Zeilen verschoben sein. Das Modkit sucht den Kontext. Zeilenenden (CRLF) und Leerraum am
  Zeilenende spielen beim Vergleich keine Rolle.
- Eine Patchdatei darf mehrere Dateien enthalten. Das Modkit wendet die Patchdateien einer Mod alphabetisch an (`01_...`, `02_...`), die Mods in
  Ladereihenfolge. So **stapeln sich** die Patches.

## Wie es wirkt

Das Dateisystem des Spiels bevorzugt **physische Dateien** vor Archiveinträgen. Das Modkit berechnet die geänderte Fassung und schreibt sie als
physische Datei nach `<Spiel>\Data\<Archiv>\<Pfad>`. Das Archiv selbst bleibt unverändert. Bei jedem Start löscht das Modkit die Dateien, die es beim
letzten Mal erzeugt hat (Liste `g3asm\patched_files.txt`). Danach berechnet es alles neu aus dem Original. Entfernst du eine Mod, ist beim nächsten
Start der Originalzustand zurück.

Liegt am Ziel schon eine **lose Datei**, ist sie die Basis des Patches. Sie kann von einer anderen Mod stammen, oder du hast sie selbst geändert.
Das Modkit sichert sie vorher nach `g3asm\patch_backup\<Pfad>` und spielt sie beim nächsten Start zurück. Ersetzt etwas die gepatchte Datei zwischen
zwei Starts, zum Beispiel ein Update der anderen Mod, gehört die Datei nicht mehr dem Modkit. Sie bleibt dann, wie sie ist, und das Modkit verwirft
die Sicherung. Das Protokoll meldet das mit `wurde seit dem letzten Start geaendert`.

Zielpfade sind relativ zum Spielordner. Pfade mit `..`, mit einem Laufwerk oder mit einem führenden Trenner lehnt das Modkit ab
(`Zielpfad nicht erlaubt`).

## Fehler

Passt ein Patch nicht zur Datei (die Datei hat sich geändert), **überspringt das Modkit den ganzen Patch** und meldet ihn im Protokoll. Die anderen
Patches laufen weiter:

```
Patch zweit\ScriptPatches\a_kaputt.patch (Data/Quests/...): Hunk 1 (ab Zeile 1) passt nicht: der Inhalt der Datei weicht ab - dieser Patch wird uebersprungen
```

Weitere Protokollzeilen: `Datei-Patch angewendet: <Datei> (Basis: Archiv Quests.pak, 2 Patch(es))` und
`Datei-Patches: 4 Patchdatei(en), 2 Datei(en) gepatcht`.

## Grenzen

- **Komprimierte Archiveinträge** liest das Modkit nicht (`Strings.pak\stringtable.ini`). Quests (`Quests.pak`) und Infos (`Infos.pak`) sind nicht
  komprimiert.
- Die Patches wirken, wenn das Spiel die Datei **von der Platte** liest. Quests liest das Spiel beim Start und beim Laden aus den `.quest`-Dateien.
  Dialoge (Infos) lädt das Spiel aus einer kompilierten Datei (`Infos.cpt`). Ob dort `.info`-Patches ankommen, habe ich nicht geprüft.
- Binärdateien (Vorlagen, Meshes) lassen sich nicht per Diff ändern.
- Das Modkit patcht die Datei beim **Start des Spiels**. Änderst du einen Patch, starte das Spiel neu.
- `ini [g3asm] filepatches=0` schaltet das Feature ab.
- **Nur mit dem Loader.** Bei Spielern startet die Laufzeit als Script-DLL, nachdem das Spiel seine Dateiliste gelesen hat. Dann kommen Patches zu
  spät. `g3asm_packmod release` lässt `ScriptPatches\` deshalb weg. Liefere für Mod-Pakete die geänderte Datei als Archiv aus
  ([Mod-Archive](archives.md), [Mods weitergeben](mod-package.md#mods-weitergeben)).

## Beispiel

`examples/mods/demo_patch` enthält einen Patch für `Abbas_Artefakt` (Quest-XP 300 → 600) und ein kleines Skript. Nach dem Laden schreibt das Skript
`GetQuestXP("Abbas_Artefakt")` ins Protokoll. 600 heißt, der Patch wirkt. 300 heißt, es ist das Original.
