# Mod folder and installation

[Deutsch](../../modkit/mod-package.md) | **English**

## Installation

Before you start: Gothic 3 must have the Community Patch 1.75 and the Parallel Universe Patch. Close Gothic 3. In the unpacked release folder,
type:

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_loader.ps1 -Game "<game folder>" -BuildDir bin
```

The script does these steps:
- It does a check of the requirements. If something is missing, it stops.
- It renames the `dbghelp.dll` of the game to `dbghelp_orig.dll`. It also makes a backup in `<game>\g3asm_backup\loader_<time>\`.
- It copies the loader (as `dbghelp.dll`) and `g3asm.dll` into the game folder.
- It makes `g3asm.ini`, `g3asm\scripts\` (with a mini mod) and `g3asm\mods\`.

To uninstall: use the same command with `-Uninstall` in place of `-BuildDir bin`. This restores the original `dbghelp.dll` and removes
`g3asm.dll`. `g3asm.ini` and the folder `g3asm\` stay.

If you change `g3asm.dll` (C++), start the game again. For changes to scripts, this is not necessary (hot reload).

The loader is for modders. Players install only mod packages. See [Share mods](#share-mods).

## Share mods

Players must not have to install the modkit. `g3asm_packmod release` makes a package from a mod folder. The package has the Union Starter layout
and contains the runtime:

```
bin\g3asm\g3asm_packmod.exe release <game folder>\g3asm\mods\my_mod <target folder>
```

```
<target folder>\my_mod\
  Mod.ini                              [Main] Name, Category, Required (from title, category, requires in the mod.ini)
  ScriptFiles\scripts\Script_G3ASM_046.dll   runtime; the game loads each scripts\Script_*.dll itself
  ModFiles\G3ASM.m0x                   scripts, mod.ini, texts of the mod, in the archive below my_mod/
  ModFiles\Quests.m0x ...              game data archives of the mod (from my_mod\ModFiles\), without changes
  G3ASM-LICENSE.txt                    license of the runtime and the address of its source code
```

- **Not in the script archive:** `ModFiles\*.m0x` and `*.n0x` (your own game data archives, which are next to it), `Source\` (the sources of
  these archives), `ScriptPatches\` (see below) and all items that start with a period (`.git`). `release` gives the name of each file that it
  does not include.
- **`enabled=0`** in the `mod.ini` becomes `enabled=1` in the package, because players have no `load_order.txt`.
- **Runtime:** the `Script_G3ASM_<n>.dll` with the highest number next to `g3asm_packmod.exe` (in the release zip: `bin\g3asm\`). You can also
  give it explicitly with `--runtime <file>`. The number is the script interface.
- `release` does not overwrite a target folder that exists.
- **License:** Your mod can have any license (`LICENSE-EXCEPTIONS.md`). But the runtime in the package has the GPL. If you give a package to other
  persons, you also give them the runtime, and you must tell them where its source code is. `G3ASM-LICENSE.txt` does this. Keep this file in the
  package. Use the runtime from a published release, because the file refers to the source code of that release.

**On the computer of a player,** the Union Starter puts `ScriptFiles\*` into the game folder. It puts each `ModFiles\<name>.m0x` as the next free
generation into `Data\` (`G3ASM.mod`, `G3ASM.m00`, ...). Without the Starter, you can do the same by hand or with
`g3asm_packmod copy <file> <game folder> <name>`.

**When the game starts:**
- The game loads `scripts\Script_G3ASM_<n>.dll`. If there are more than one of these files (mods with different runtimes), only the file with the
  highest number starts. If the modkit already runs through the loader, the script DLL does nothing. The script DLL does not need `g3asm.ini`. If
  there is one, it applies (also `enable=0`).
- The runtime unpacks each mod from `Data\G3ASM.*` to `g3asm\mods\<mod>\` and puts the marker `.g3asm_packed` there. Then the mod loads as a
  loose folder. If the archive changes, the runtime unpacks it again. If the mod is not in an archive anymore (the player cleared it in the
  Starter), the runtime removes its unpacked files. Files that do not come from the archive stay.
- The runtime never overwrites a folder **without** a marker (a loose mod, for example during development). This folder has priority over the
  packed version with the same name.
- If the same mod is in two archives, the mod from the earlier archive applies (`.mod` before `.m00` before `.m01`). The log reports this.
- The game itself does not open `Data\G3ASM.*`, because it does not know the archive name.

**File patches** (`ScriptPatches`) have no effect on the computers of players. The script DLL starts only after the game has read its file
list. Thus put new or changed game files as an archive into `my_mod\ModFiles\` (see [Mod archives](archives.md)).

## Layout

```
<Gothic 3>\
  scripts\Script_G3ASM_046.dll  only for players (mod packages), in place of the loader and g3asm.dll
  Data\G3ASM.mod, G3ASM.m00 ... packed mods (mod packages), the runtime unpacks them to g3asm\mods\
  g3asm.dll
  g3asm.log                 log (print() and error messages)
  g3asm.ini                 settings, sections [g3asm] and [loader]
  g3asm_loader.log          log of the loader (crash candidates)
  dbghelp.dll               the loader; dbghelp_orig.dll is the file of the game
  g3asm\
    scripts\main.as             single script (runs as the mod "main", if it exists)
    mods\
      load_order.txt            optional
      my_mod\main.as            one mod = one folder with main.as
      other_mod\main.as
      my_mod\ScriptPatches\     optional: *.patch (unified diff on game files, see File patches)
```

Each folder below `g3asm\mods\` that contains a `main.as`, a folder `ScriptPatches` or a `stringtable.ini` (see below) is a mod. The name of the
folder is the name of the mod. The log shows it in square brackets (`[my_mod] ...`). The name `main` is reserved for the single script.

Each mod is a **separate AngelScript module**. Its global variables and functions are separate from other mods. The same names can occur in more
than one mod. `#include "file.as"` works relative to the file that contains the include. In this way, you can put a mod into more than one file.

## Load order: `load_order.txt`

```
# comment
base_mod
my_mod
-broken_mod          # a minus sign before the name turns a mod off
+demo_lifesteal      # a plus sign turns on a mod that is off by default
```

- Write one mod name on each line. The mods in the list load in this sequence.
- Mods that are not in the list come after them, in alphabetical sequence.
- `-Name` turns the mod off. The mod does not load, or it unloads while the game runs.
- A mod with `[mod] enabled=0` in the `mod.ini` in its own folder is **off by default**. All example mods come in this condition. The mod loads
  only if `+Name` is in `load_order.txt`. `+Name` also sets its position in the sequence. Mods without a `mod.ini` load as before.
- The single script `main` always runs first.
- Each event goes to all mods, one after the other.
- If a mod needs another mod (`requires` in `mod.ini`, see below), the needed mod loads first, also if `load_order.txt` gives a different
  sequence. The log gives each of these changes.

## Version and dependencies: `mod.ini` (from version 40)

```
[mod]
enabled=1                              ; 0 = off by default (see above)
version=1.2.0                          ; version of this mod
api=40                                 ; minimum necessary script interface (GetApiVersion)
requires=base_mod >= 1.1, other_mod    ; needed mods, separated by commas, minimum version optional
title=My Mod                           ; only for release: name in the Union Starter (default: folder name)
category=Combat                        ; only for release: category in the Union Starter (default: G3ASM)
keepglobals=0                          ; from version 47: do not keep the global variables during a hot reload (default: 1)
```

All entries are optional. If the conditions of a mod are not met, the modkit **does not load the mod**: not its script, not its file patches and
not its string table. The log gives the reason 1 time, for example:

```
[my_mod] wird nicht geladen: braucht die Mod 'base_mod' ab Version 1.1, vorhanden ist Version 1.0
[my_mod] wird nicht geladen: braucht die Mod 'other_mod', die abgeschaltet ist (in load_order.txt mit +other_mod zuschalten)
[my_mod] wird nicht geladen: braucht Skript-Schnittstelle 41, vorhanden ist 40 (G3ASM aktualisieren)
```

These lines mean: the mod is not loaded, because it needs `base_mod` from version 1.1 but version 1.0 is available; because it needs `other_mod`,
which is off (turn it on with `+other_mod` in `load_order.txt`); because it needs script interface 41 but 40 is available (update G3ASM).

- If a needed mod is missing, is off, or does not load itself, the mod that needs it also does not load.
- The modkit compares versions part by part as numbers. `1.10` is newer than `1.9`. `1.2` is the same as `1.2.0`. Text after a number does not
  count (`1.0-beta` = `1.0`). If the needed mod has no `version`, it does not meet a minimum version.
- `api` replaces the check with `GetApiVersion()` in the script. If a function is missing, the compilation fails first, and the player sees only a
  compile error. With `api`, the player sees that the modkit is too old. Versions before version 40 do not know `api` and `requires` and ignore
  them.
- If two mods need each other, the log reports the circle. The two mods load in the sequence of `load_order.txt`.
- Encoding: `mod.ini`, `load_order.txt` and `g3asm.ini` can be ANSI or UTF-8, with or without BOM (editor setting "UTF-8 with BOM",
  PowerShell 5 `-Encoding utf8`). For `g3asm.ini` with BOM, the log gives a note.
- If `mod.ini` or `load_order.txt` changes while the game runs, the change applies at the next scan (1 time each second, with `hotreload=1`).
- In the script: use `IsModActive("name")` and `GetModVersion("name")` for optional cooperation with other mods. See
  [API reference](api-reference.md).

## Settings of a mod: `[settings]` (from version 44)

The default values are in the `mod.ini` of the mod, in the section `[settings]`:

```
[mod]
version=1.0
api=44

[settings]
Factor=2.5
Display=yes
```

The script reads them with `GetModSetting("Factor")`, `GetModSettingFloat(...)` and similar functions. It can save its own values with
`SetModSetting`. These values go into `<game>\g3asm\settings\<mod>.ini`, never into the `mod.ini`. Thus an update of the mod keeps them. To change
a setting by hand, write it there (section `[settings]`). It applies after a reload of the mod (hot reload) or after a new start. Each mod sees
only its own values.

## Settings: `g3asm.ini`

```
[g3asm]
enable=1                    ; 1 = load the modkit
script=g3asm\scripts\main.as  ; single script, relative to the game folder or absolute
hotreload=1                 ; load scripts again when a file changes (check 1 time each second)
mods=g3asm\mods               ; mod folder
hooks=1                     ; 0 = do not install game events (hooks on Game.dll)
filepatches=1               ; 0 = do not apply ScriptPatches (file patches)
packedmods=1                ; 0 = do not unpack packed mods from Data\G3ASM.* (see Share mods)
navevents=0                 ; 1 = OnEnter/OnLeave also report internal navigation zones "Nav_*"
scriptlog=0                 ; 1 = each 60 s, log the most frequent game scripts and the time of the hooks
aitrace=0                   ; 1 = AI tracer: log changes of AI state, AI task, AI routine, AI mode and action of the NPCs
aitracefilter=              ; only NPCs with a name that contains this text (empty = all)
aitracemax=2000             ; maximum number of AI lines for each game start
anispeedlog=0               ; 1 = measurement: log which code sets or reads the playback speed of animations (SetPlaySpeed/GetPlaySpeed)
tradelog=0                  ; 1 = measurement: log purchases and sales on the trade page (items, quantities, gold)
printgame=1                 ; 0 = print() writes only to the modkit log, not also to the log of the game
; only for troubleshooting:
asynclog=0                  ; 1 = write log lines through a queue (separate thread)
logwrite=1                  ; 0 = after the start, write no more log lines
loadpause=0                 ; for this number of seconds after a load, the modkit does not call OnFrame of the scripts (0 = off)
memlog=0                    ; 1 = each 30 s, log the free memory and the address space
[loader]
crashlog=1                  ; 0 = at a crash, the loader writes no line to g3asm_loader.log
[dev]
pipe=0                      ; 1 = dev pipe: run code in the running game (only for developers, see CONTRIBUTING.en.md)
```

## Log

`<game>\g3asm.log`. You can read it while the game runs. It contains:

- The output of your `print()` calls, with `[mod name]` before it.
- Compile errors with the file, the line and the column. The modkit then does not load the mod.
- Run-time errors (function and line), for example a division by 0.
- Notes from the modkit (`Spielereignisse: 8 von 8 Hooks aktiv`, game events: 8 of 8 hooks active; `Skript geladen: ...`, script loaded).

## Hot reload

If you save `main.as` (or make a new mod folder), the modkit sees this in 1 second or less:

1. When a mod reloads, its `OnShutdown()` runs. Its coroutines stop.
2. The modkit compiles the new script. If there are errors, the mod stays unloaded until you change the file again.
3. From version 47, the global variables get their values from before again (see below).
4. Then `main()` runs again.

### Keep the global variables (from version 47)

The global variables of a mod keep their values during a hot reload. Thus counters, lists and tables do not start at zero when you change the
script. The modkit keeps the values after `OnShutdown()`. Thus a value that you reset there stays reset.

| Kept | Not kept |
|---|---|
| Numbers, `bool`, `bCString`, `string` | `const` variables: the new value in the code applies |
| `Entity`, `bCVector` and the other math types | Function handles (for example `Coroutine@`): they are `null` after the reload |
| `array<T>`, objects of your classes and handles to them | Handles and objects of your classes as values in a `dictionary` |
| `dictionary` with numbers, texts and math types | Variables with a changed type |

The log shows:

```
[my_mod] Hot-Reload: 12 globale Variablen uebernommen
```

**Caution:** If you change only the start value of a variable (`int max = 5;` to `int max = 10;`), the old value 5 stays after the hot reload.
For fixed values, use `const int max = 10;`. If a mod must always start fresh, write `keepglobals=0` in its `mod.ini`.

If there is a syntax error, the kept values stay until the script has no errors again. When the game starts, all variables start with their
start values.

If you remove the folder or write `-Name`, the modkit unloads the mod (with `OnShutdown()`).

## Error isolation

An error in a mod (syntax error, run-time error, endless loop) has no effect on the other mods and on the game.
The modkit stops a call that runs for more than **5 seconds**.

## Your own texts: `stringtable.ini`

A mod can overwrite or add texts of the game (item names, descriptions, quest texts and dialog texts) with its own string table. File:
`<mod>\stringtable.ini`, or, as for the Union Starter, `<mod>\ModFiles\stringtable.ini`. If the two files exist, the file in the mod folder
applies. The mod does not need a script for this. [PackMod](archives.md) packs your own game files (quests, templates, ...) into a mod archive.

```
Languages=English;Italian;French;German;Spanish;Czech;Hungarian;Polish;Russian;TRC
FO_It_Wolf_Fur=Wolf skin (Mod);;Pelle di lupo (Mod);;Peau de loup (Mod);;Wolfsfell (Mod);;...
```

- The format is the same as the `stringtable.ini` of the game: `key=text;;text;;...`. With the line `Languages=`, the columns are in this
  sequence. The modkit uses the column of the current game language. If this entry is empty or missing, it uses the first column. Without
  `Languages=`, the full value is one text.
- Keys: `FO_<template>` (item name), `ITEMDESC_<template>` (description). `Strings.pak\stringtable.ini` contains more keys.
- Encoding: UTF-16 (with BOM), UTF-8 (with BOM) or ANSI (cp1252). Comment lines start with `;`, `#` or `[`.
- Later mods overwrite earlier mods. The script equivalent is `RegisterText(id, text)`.
- Changes to the file have an effect after approximately 1 second (hot reload). The log shows
  `Stringtabellen angewendet: N Eintraege, Sprache ...` (string tables applied: N entries, language ...).
- Example: `examples/mods/demo_text`.
