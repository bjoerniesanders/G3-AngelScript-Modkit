# Datei-Patches (ScriptPatches)

Wie beim G1R-Modkit kann ein Mod **Unified-Diff-Dateien** mitbringen, die die Datendateien des Spiels ändern, **bevor das Spiel sie liest**. Gothic 3 hat keine Spielskripte,
seine Quests, Dialoge und Tabellen sind Textdateien in Archiven (`Data\Quests.pak`, `Data\Infos.pak`, ...). Genau auf diese Textdateien wirken die Patches.

```
<Gothic 3>\g3asm\mods\mein_mod\ScriptPatches\wolfsfell_xp.patch
```

Ein Mod kann **nur** aus Patches bestehen (ohne `main.as`). Er braucht nur den Ordner `ScriptPatches` mit `.patch`-Dateien.

## Format

Standard-Unified-Diff (`diff -u`, `git diff`). Pfade stehen mit `a/` und `b/` davor:

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

- Der Pfad lautet `Data/<Archiv>/<Pfad im Archiv>`: `Data/Quests/...` liegt im Archiv `Data\Quests.pak`, `Data/Infos/...` in `Data\Infos.pak`. Hat das Archiv einen Unterordner (`G3_World_01/`), gehört er zum Pfad.
- `--- /dev/null` legt eine **neue Datei** an.
- Der Patch darf gegenüber der erwarteten Zeile bis zu 300 Zeilen verschoben sein (Kontext wird gesucht). Zeilenenden (CRLF) und Leerraum am Zeilenende spielen beim Vergleich keine Rolle.
- Mehrere Dateien in einer Patchdatei sind erlaubt. Patchdateien eines Mods werden alphabetisch angewendet (`01_...`, `02_...`), die Mods in Ladereihenfolge: Patches **stapeln sich**.

## Wie es wirkt

Das Dateisystem des Spiels bevorzugt **physische Dateien** vor Archiveinträgen. Das Modkit berechnet die geänderte Fassung und legt sie als physische Datei unter
`<Spiel>\Data\<Archiv>\<Pfad>` ab. Das Archiv selbst bleibt unverändert. Bei jedem Start werden die zuletzt erzeugten Dateien (Liste `g3asm\patched_files.txt`) gelöscht und alles aus dem Original neu berechnet.
Entfernst du einen Mod, ist beim nächsten Start der Originalzustand zurück.

Liegt am Ziel schon eine **lose Datei** (von einem anderen Mod oder selbst geändert), ist sie die Basis des Patches. Das Modkit sichert sie vorher nach
`g3asm\patch_backup\<Pfad>` und spielt sie beim nächsten Start zurück. Wird die gepatchte Datei zwischen zwei Starts ersetzt (etwa durch ein Update des anderen Mods),
gehört sie nicht mehr dem Modkit: Sie bleibt, wie sie ist, und die Sicherung entfällt. Das Protokoll meldet das mit `wurde seit dem letzten Start geaendert`.

Zielpfade sind relativ zum Spielordner. Pfade mit `..`, einem Laufwerk oder einem führenden Trenner lehnt das Modkit ab (`Zielpfad nicht erlaubt`).

## Fehler

Ein Patch, der nicht passt (Datei hat sich geändert), wird **ganz übersprungen** und im Protokoll gemeldet; die anderen laufen weiter:

```
Patch zweit\ScriptPatches\a_kaputt.patch (Data/Quests/...): Hunk 1 (ab Zeile 1) passt nicht: der Inhalt der Datei weicht ab - dieser Patch wird uebersprungen
```

Protokollzeilen: `Datei-Patch angewendet: <Datei> (Basis: Archiv Quests.pak, 2 Patch(es))` und `Datei-Patches: 4 Patchdatei(en), 2 Datei(en) gepatcht`.

## Grenzen

- **Komprimierte Archiveinträge** werden nicht gelesen (`Strings.pak\stringtable.ini`). Quests (`Quests.pak`) und Infos (`Infos.pak`) sind unkomprimiert.
- Die Patches wirken, wenn das Spiel die Datei **von der Platte** liest. Quests werden beim Spielstart und Laden aus den `.quest`-Dateien gelesen. Dialoge (Infos) lädt das Spiel beim Laden
  aus einer kompilierten Datei (`Infos.cpt`); ob dort `.info`-Patches ankommen, ist ungeprüft.
- Binärdateien (Templates, Meshes) lassen sich nicht per Diff ändern.
- Die Datei wird beim **Start des Spiels** gepatcht; Änderungen an Patches brauchen einen Neustart.
- `ini [g3asm] filepatches=0` schaltet das Feature ab.

## Beispiel

`examples/mods/patch_demo`: ein Patch für `Abbas_Artefakt` (Quest-XP 300 → 600) und ein kleines Skript, das nach dem Laden `GetQuestXP("Abbas_Artefakt")` ins Protokoll schreibt
(600 = Patch wirkt, 300 = Original).
