# File patches (ScriptPatches)

[Deutsch](../../modkit/patches.md) | **English**

As in the G1R modkit, a mod can contain **unified diff files**. These files change the data files of the game **before the game reads them**.
Gothic 3 has no game scripts. Its quests, dialogs and tables are text files in archives (`Data\Quests.pak`, `Data\Infos.pak`, ...). The patches
apply to these text files.

```
<Gothic 3>\g3asm\mods\my_mod\ScriptPatches\wolfskin_xp.patch
```

A mod can consist **only** of patches, without `main.as`. It then needs only the folder `ScriptPatches` with `.patch` files.

## Format

The format is a standard unified diff (`diff -u`, `git diff`). The paths start with `a/` and `b/`:

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

- The path is `Data/<archive>/<path in the archive>`. `Data/Quests/...` is in the archive `Data\Quests.pak`. `Data/Infos/...` is in
  `Data\Infos.pak`. If the archive has a subfolder (`G3_World_01/`), the subfolder is part of the path.
- `--- /dev/null` makes a **new file**.
- The patch can have an offset of up to 300 lines from the expected line. The modkit searches for the context. Line ends (CRLF) and spaces at
  the end of a line have no effect on the comparison.
- One patch file can contain more than one file. The modkit applies the patch files of a mod in alphabetical sequence (`01_...`, `02_...`), and
  the mods in the load order. Thus the patches **stack**.

## How it works

The file system of the game prefers **physical files** to archive entries. The modkit calculates the changed version and writes it as a physical
file to `<game>\Data\<archive>\<path>`. The archive itself does not change. At each start, the modkit deletes the files that it made the last time
(list `g3asm\patched_files.txt`). Then it calculates all files again from the original. If you remove a mod, the original condition comes back at
the next start.

If a **loose file** is already at the target, this file is the base of the patch. It can come from another mod, or you changed it yourself. The
modkit first makes a backup of it in `g3asm\patch_backup\<path>` and restores it at the next start. If something replaces the patched file
between two starts, for example an update of the other mod, the file does not belong to the modkit anymore. The file then stays as it is, and the
modkit discards the backup. The log reports this with `wurde seit dem letzten Start geaendert` (was changed since the last start).

Target paths are relative to the game folder. The modkit refuses paths with `..`, with a drive, or with a separator at the start
(`Zielpfad nicht erlaubt`, target path not permitted).

## Errors

If a patch does not agree with the file (the file changed), the modkit **skips the full patch** and reports it in the log. The other patches
continue:

```
Patch zweit\ScriptPatches\a_kaputt.patch (Data/Quests/...): Hunk 1 (ab Zeile 1) passt nicht: der Inhalt der Datei weicht ab - dieser Patch wird uebersprungen
```

This line means: hunk 1 (from line 1) does not agree, the contents of the file are different, the modkit skips this patch.

Other log lines: `Datei-Patch angewendet: <Datei> (Basis: Archiv Quests.pak, 2 Patch(es))` (file patch applied, base: archive Quests.pak) and
`Datei-Patches: 4 Patchdatei(en), 2 Datei(en) gepatcht` (4 patch files, 2 files patched).

## Limits

- The modkit does not read **compressed archive entries** (`Strings.pak\stringtable.ini`). Quests (`Quests.pak`) and infos (`Infos.pak`) are not
  compressed.
- The patches have an effect when the game reads the file **from the disk**. The game reads quests from the `.quest` files when it starts and when
  it loads. The game loads dialogs (infos) from a compiled file (`Infos.cpt`). I did not check if `.info` patches have an effect there.
- You cannot change binary files (templates, meshes) with a diff.
- The modkit patches the file when the **game starts**. If you change a patch, start the game again.
- `ini [g3asm] filepatches=0` turns the feature off.
- **Only with the loader.** On the computer of a player, the runtime starts as a script DLL after the game has read its file list. At that time,
  patches come too late. Thus `g3asm_packmod release` does not include `ScriptPatches\`. For mod packages, give the changed file as an archive
  ([Mod archives](archives.md), [Share mods](mod-package.md#share-mods)).

## Example

`examples/mods/demo_patch` contains a patch for `Abbas_Artefakt` (quest XP 300 → 600) and a small script. After a load, the script writes
`GetQuestXP("Abbas_Artefakt")` to the log. 600 means that the patch has an effect. 300 means the original.
