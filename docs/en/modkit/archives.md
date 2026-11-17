# Pack mod archives (PackMod)

[Deutsch](../../modkit/archives.md) | **English**

The game keeps its data (quests, infos, templates, textures, sounds) in archives (`Data\Quests.pak`, `Data\Templates.pak`, ...). A mod gives its
own files as a **mod archive**. `PackMod` writes this archive from a folder. The engine reads the result as a `.pak`. The result is the same as
the output of `G3PakDir` (Union Starter). Thus you can use one tool in place of the other.

## How the engine loads mod archives

The same rule applies to all makers: the Community Patch, the Union Starter, and G3ASM.

| Sequence | Extensions | Meaning |
|---|---|---|
| 1 | `.pak`, `.p00`, `.p01`, ... | The game and the official patches |
| 2 | `.cpt`, `.c00`, ... | Only when "Alternative Balancing" is active |
| 3 | **`.mod`, `.m00`, `.m01`, ...** | Mods. The engine always loads them |
| 4 | **`.nod`, `.n00`, ...** | Mods only when "Alternative Balancing" is active |

- Later archives **overwrite** earlier archives. This also applies to files of the original game. The manual of the Community Patch tells this. I
  saw it in the game, but only while there is no physical copy from the game in `Data\<volume>\` (see "Limits").
- The numbers must have **no gaps**. The engine stops at the first missing number. If there are `.m00` and `.m02` without `.m01`, the engine loads
  only `.m00`.
- The **archive name** sets the volume. `Quests.mod` contains files such as `G3_World_01/Abbas_Artefakt_quest_G3_World_01.quest`.
  `Templates.mod` contains files such as `CMB/BodyParts/Male/HERO/CMB_MaleBody.lrtpl`. Paths in the archive are relative to the volume.
- Thus, if more than one mod has files for the same volume, each mod makes **its own generation**. The mods do not overwrite one shared
  `Quests.mod`.

## Tool: `g3asm_packmod.exe`

```
g3asm_packmod pack    <folder> <archive>                           write a folder as an archive
g3asm_packmod install <folder> <game folder> <volume> [m|n]        put it as the next free generation
g3asm_packmod next    <Data folder> <volume> [m|n]                 show the next free file name
g3asm_packmod list    <archive>                                    show the contents
g3asm_packmod copy    <archive> <game folder> <volume> [m|n]       install a complete archive as the next generation
g3asm_packmod release <mod folder> <target folder>                 package for players, see Share mods
```

[Share mods](mod-package.md#share-mods) describes `release`.

`install` selects the first free file in the sequence `Volume.mod`, `Volume.m00`, `Volume.m01`, ... (with `n`: `.nod`, `.n00`, ...). It fills a
gap first. Example:

```
mods\my_mod\ModFiles\Quests\G3_World_01\My_quest_G3_World_01.quest
g3asm_packmod install mods\my_mod\ModFiles\Quests G:\Gothic3 Quests      ->  G:\Gothic3\Data\Quests.mod
```

Options:
- `--comments` keeps the source path of each file, as `G3PakDir` does. This shows local paths, thus it is off by default.
- `--no-deleted` packs `.deleted` files as normal files.

`copy` installs a **complete** archive as the next generation (`g3asm_packmod copy ModFiles\Quests.m0x G:\Gothic3 Quests`). The Union Starter
does this step when it installs a mod: it gives `ModFiles\<volume>.m0x` and `.n0x` the numbers `.m00`, `.m01`, ... itself. Before this, `copy`
makes sure that the file is a valid archive.

## A mod folder in the Union Starter layout

A mod folder can have the same layout as for the Union Starter. G3ASM ignores the parts that it does not need. The Union Starter ignores
`main.as`:

```
my_mod\
  Mod.ini                 [Main] Name=..., Category=...   (Union Starter)
  main.as                 script (G3ASM)
  ModFiles\
    Quests.m0x            complete archive (the Union Starter gives it the number .m00, .m01, ...)
    stringtable.ini       mod texts (G3ASM also reads them here; the mod folder and ModFiles have the same effect)
  Source\Quests\...       source of the archive: g3asm_packmod pack Source\Quests ModFiles\Quests.m0x
```

**Example `jens_dialog`:** The quest `G3ASM_Jens_Wolfsfell` is a real quest file
(`Source\Quests\G3_World_01\G3ASM_Jens_Wolfsfell_quest_G3_World_01.quest`). It has XP 100 and the reward `DEX` +1. The title and the
description are the text IDs `G3ASM_JENS_WOLFSFELL` and `G3ASM_JENS_WOLFSFELL_TEXT` from `ModFiles\stringtable.ini`. The dialogs and the logic
stay in the script. If the archive is not installed, the script makes the quest at run time, as before. The log tells the source of the quest:
`OnQuestsLoaded: Quest vorbereitet (aus dem Mod-Archiv, XP 100)` (quest prepared, from the mod archive) or `(zur Laufzeit angelegt)` (made at
run time).

### Remove files of the game

A file `<name>.deleted` in the folder becomes an entry "file deleted". The contents of the file have no effect. With this entry, the archive
removes the file `<name>` from the layers below it. `G3PakDir` uses the same notation.

## Script interface (from version 24)

| Function | Meaning |
|---|---|
| `bCString PackMod(const bCString &in folder, const bCString &in volume, bool alternativeBalancing = false)` | Puts the folder as the next free generation into `<game>\Data`. Return value: the path of the file, or empty if there is an error |
| `bool PackFolder(const bCString &in folder, const bCString &in archiveFile)` | Writes an archive to any location |
| `bCString NextModFile(const bCString &in volume, bool alternativeBalancing = false)` | The file that `PackMod` would make next. Empty if the name is not valid |
| `bCString GetPackError()` | The error text of the last `PackMod` or `PackFolder`. Empty if there was no error |

The game reads a new archive only **at the next start**.

## Compatibility

I did a test with 8 archives from the community: `Quests.mod`, `Infos.m00`, `Infos.mod`, `Quests.m05`, `Templates.m02`, `Templates.mod`,
`Library.mod`, `Materials.mod`. Together they contain approximately 1600 files. The test unpacks each archive with an independent Python reader,
packs it again with `g3asm_packmod`, and compares the two. Names, directories, contents, attributes and the header are the same
(`tests/packmod_roundtrip.py`).

The format is the same as the format of `G3PakDir`:
- A header with `G3V0` and `G3PD`.
- Relative paths with `/`.
- Directories with `/` at the end.
- The entry attribute "Packed" (`0x20000`).
- `.dds`, `.ogg` and `.wav` as "Stream".

## Limits

- **No compression:** The tool stores the files. The archive is larger than a compressed archive (template archives up to approximately 3 times
  larger). The engine reads it correctly. The Union Starter also packs without compression when it packs again ("Neu verpacken").
- **Read compressed entries:** `list` shows them, but not their contents. This is necessary for `Strings.pak\stringtable.ini`. See
  [Patches](patches.md).
- **The file patches of G3ASM do not know the mod layers yet.** As the base, they read only the `.pak`. If another mod archive (`.mod`, `.m00`,
  ...) replaced the same file, the diff patches the version from the `.pak`.
- **New files** from an archive of `g3asm_packmod`: the game reads them. I checked this with the quest `G3ASM_Jens_Wolfsfell` in `Quests.m00`.
- **Overwrite existing files:** this also works, while there is **no loose copy** of the file in `Data\<volume>\`. See the next item and the
  example `demo_packmod`.
- **Caution: loose copies hide archives.** If a file is loose in the folder `Data\<volume>\` (for example `Data\Quests\G3_World_01\…quest`), the
  game uses this file and not the file from an archive. The game makes these copies itself when it loads, for all 571 quest files. Thus a copy
  from a previous run hides each mod archive that you install later. To correct this:
  1. Close the game.
  2. Delete the folder `Data\<volume>\` (for example `Data\Quests\`). If your own file patches made files there, the modkit makes them again at
     the next start.
  3. Start the game and load a saved game. The game makes the folder again from the archives. The folder now contains the files of your mod.

  This also applies if you change a mod archive that is already installed.
- **Main menu:** At the first `OnQuestsLoaded` in the main menu, there are no quests yet (`GetQuestCount()` gives -1). This is also true for the
  quests of the game. Do not make anything at that time (`if (GetQuestCount() < 0) return;`). The correct setup comes when the player loads a
  saved game.
