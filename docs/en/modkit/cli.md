# Command line `g3asm` (from version 47)

[Deutsch](../../modkit/cli.md) | **English**

`bin\g3asm\g3asm.exe` does the usual work on a mod without the game: it makes a new mod, checks the script, packs the mod and reads the log.
With `--json`, each command writes a JSON object. Thus scripts and tools can read the result.

| Command | Result |
|---|---|
| `g3asm new <mod folder>` | Makes the folder with `main.as` and `mod.ini` |
| `g3asm check <mod folder\|file.as>` | Compiles the script without the game and shows the errors with line and column |
| `g3asm pack <folder> <archive>` | Writes a folder as an archive, as `g3asm_packmod pack` ([Mod archives](archives.md)) |
| `g3asm release <mod folder> <target folder>` | Makes a package for players, as `g3asm_packmod release` ([Share mods](mod-package.md#share-mods)) |
| `g3asm log [<game folder>] [-n <count>] [--all]` | Shows the last errors and warnings from `g3asm.log` |

Exit code: `0` = correct, `1` = errors in the mod (or `pack`/`release` failed), `2` = incorrect call or the tool cannot do the work (for
example, a file is missing).

## Make a new mod

```
bin\g3asm\g3asm.exe new "<game folder>\g3asm\mods\my_mod"
```

Result:

```
Mod angelegt: C:\Games\Gothic 3\g3asm\mods\my_mod
  main.as
  mod.ini (api=47)
```

- The folder name is the name of the mod. You can use the letters `a`-`z`, digits, `_` and `-`.
- `mod.ini` contains `version=1.0.0`, the current script interface as `api` and the name as `title`.
- `new` does not overwrite a folder that exists.

## Check a script

```
bin\g3asm\g3asm.exe check "<game folder>\g3asm\mods\my_mod"
```

Result if there is an error (line 2 is `int x = ;`):

```
C:/Games/Gothic 3/g3asm/mods/my_mod/main.as (1, 1): Info: Compiling void main()
C:/Games/Gothic 3/g3asm/mods/my_mod/main.as (2, 13): FEHLER: Expected expression value
C:/Games/Gothic 3/g3asm/mods/my_mod/main.as (2, 13): FEHLER: Instead found ';'
my_mod: 2 Fehler, 0 Warnung(en)
```

Result if there are no errors:

```
OK: my_mod ohne Fehler (Skript-Schnittstelle 47)
```

- `check` compiles `main.as` with the same types and functions as in the game. It does not run a function, also not `main()`.
- `check` also examines the `api` entry in `mod.ini`. If the mod needs a newer script interface than the modkit has, this is an error.
- `check` does not need game files. It uses the replacement `SharedBase.dll` in `bin\g3asm\sbstub\`. To use a different file, add
  `--sharedbase <file>`.

**Caution:** `check` finds only the errors that the compiler finds: typing errors, incorrect types, unknown functions. Errors at run time (for
example `Null pointer access`) show only in the log of the game.

With `--json`:

```json
{"ok":false,"mod":"my_mod","file":"...\\main.as","api":47,"errors":2,"warnings":0,
 "messages":[{"type":"info","file":".../main.as","line":1,"col":1,"text":"Compiling void main()"},
             {"type":"error","file":".../main.as","line":2,"col":13,"text":"Expected expression value"},
             {"type":"error","file":".../main.as","line":2,"col":13,"text":"Instead found ';'"}]}
```

`type` is `error`, `warning` or `info`. Before the errors of a function, AngelScript writes an `info` line with the name of the function.

## Read the log

```
bin\g3asm\g3asm.exe log "<game folder>"
```

- If you do not give a game folder, `log` reads the file `g3asm.log` in the current folder.
- `log` shows the last 20 lines with errors, warnings, script exceptions and stops. To change the number, use `-n <count>`. To show all lines
  (not only the errors), use `--all`.
- The modkit writes a new `g3asm.log` at each start of the game. Thus `log` shows only the current or the last session.
- You can use `log` while the game runs.

With `--json`, each line has the fields `time`, `mod` and `text`:

```json
{"ok":true,"file":"...\\g3asm.log","count":1,
 "lines":[{"time":"10:00:00.004","mod":"my_mod","text":"Skript-Ausnahme in void main() (Zeile 4): Null pointer access"}]}
```

## Pack and share

`g3asm pack` and `g3asm release` give all arguments to `g3asm_packmod.exe`. The two tools must be in the same folder. With `--json`, the
field `out` contains the output of `g3asm_packmod`, and the field `exit` contains its exit code.

```
bin\g3asm\g3asm.exe release "<game folder>\g3asm\mods\my_mod" "<target folder>"
```
