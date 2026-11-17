# Compatibility

[Deutsch](../compatibility.md) | **English**

I tested G3ASM on a small number of installations. This page gives the checks for each installation. If you try it on a different
installation, you can report the result (for the logs, see below).

## Requirements

G3ASM needs the **Community Patch 1.75** and the **Parallel Universe Patch** (PU). Without one of the two, it is not tested and not supported.

- `tools/install_loader.ps1` does a check of the two patches during the installation. If a patch is missing, it stops. With `-Force`, it installs
  G3ASM, but this is not tested.
- At the start, the modkit writes the line `Voraussetzungen: Community Patch 1.75 ja/NEIN, Parallel-Universe-Patch ja/NEIN` (requirements:
  Community Patch 1.75 yes/NO, Parallel Universe Patch yes/NO) to the log. For a `NEIN` (no), it also writes a `WARNUNG` (warning). It continues
  to run. Hooks at fixed addresses do a check of their bytes themselves.
- Detection: the Community Patch 1.75 by the PE time stamp `4F959F4F` of `Gothic3.exe`. `Game.dll` is not usable for this, because mods patch it
  in binary form. The PU patch by `paru_s1.dll` and `scripts\Script_paru.dll` in the game folder.

## Test matrix

| Installation | Community Patch | PU patch | DXVK | Result | Status |
|---|---|---|---|---|---|
| Development installation (copy, Windows 11; `FileSystem.dll` and `Dialogs.dll` are separate test versions there) | 1.75 | Yes | Yes | All checks passed. All measurements come from this installation | API 46, 2026-11 |
| Steam (heavily modded, copy) | 1.75 (mods changed the game DLLs in binary form) | Yes | Yes (different DXVK version) | Start, load a saved game, world change, close: no errors (only the mini mod). All 8 game hooks and 2 status effect hooks active. x87 control word always 007F. Tooltip hook not installed, because `Game.dll+0xA48B0` is different there (expected). 2026-11-08: black screen in the intro because of a shared `Shader.Cache` (separated: repaired). A saved game with `perk_lifesteal` crashed after a new start and a load when the player opened the character sheet. This also occurred without mods (cause not known) | 2026-11 |
| Steam (clean, copy; only the Community Patch and PU) | 1.75 | Yes | No | Game DLLs are the same byte for byte as the development installation (but not `FileSystem.dll` and `Dialogs.dll`). Without the patch (loader installed, `enable=0`) and with the patch (only the mini mod): start, load a saved game, character sheet, save into a new slot, load again, close: no errors. Requirements detected. All hooks active (also the tooltip and the fixed points monster damage and learn costs). x87 control word always 007F. Mod test with `perk_lifesteal` and `beliars_klaue` with the check procedure below: life steal heals, the tile is there after each load and save, lightning with owner and visuals. Turn off with `ENTFERNEN = true`, then mods off, and at the end the patch uninstalled: load a saved game, character sheet, unequip and equip the weapon, each time without a crash | API 46, 2026-11-13 |
| GOG | 1.75 necessary | Necessary | Not done | Not tested | |
| CD or DVD version | 1.75 necessary | Necessary | Not done | Not tested | |

I do not check installations without the Community Patch 1.75 or without the PU patch (see Requirements). Not done yet: with and without DXVK.

## Known differences between installations

The same file size and the same PE time stamp do **not** mean the same contents. The `Game.dll` of a Steam installation and the `Game.dll` of the
development installation both have 4,640,768 bytes and the time stamp `4F959E2D`. But 15,701 bytes are different (427 areas, mostly jumps into
code that was added later). This is also true for `Engine.dll` and `Script.dll` (1,860 and 461 bytes). Thus the modkit also writes a CRC32
checksum for each module to the log. It installs hooks at fixed addresses only if the bytes agree. If they are different, only the affected
function is missing. All other functions continue to run.

| File | Development installation (CRC32) | Steam installation (CRC32) |
|---|---|---|
| Game.dll | AA8B3F6D | 148CEEE1 |
| Engine.dll | 37DB85F7 | 2D240B9E |
| Script.dll | 906D2266 | 6DC635DE |
| Gothic3.exe | 9A6E7A58 | 9A6E7A58 |
| SharedBase.dll | 022D320C | 022D320C |

## Check procedure

**Always work on a copy of the installation, never on the original.** Copy the game folder (with `Gothic3.exe`) and change only the copy.

**Saved games:** All copies use the same folder `Documents\gothic3`. Make a backup of the folder first. Save only into **new, free slots**.
Delete the test saves after the test.

1. **Install** (close the game first): from the release zip, in the unpacked folder:
   `powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_loader.ps1 -Game "<copy>" -BuildDir bin`
2. **Start**, load a saved game, play for some minutes, close. Then look at `<copy>\g3asm.log`:
   - The line `Voraussetzungen: Community Patch 1.75 ja, Parallel-Universe-Patch ja` (requirements: yes, yes).
   - Seven lines `Modul ...: <Bytes> Bytes, PE-Zeitstempel ..., CRC32 ..., Spielordner ja/nein` (module, PE time stamp, CRC32, in the game folder
     yes or no) for Gothic3.exe, Game, Engine, Script, Script_Game, SharedBase, d3d9.
   - All hook lines (`Spielereignisse: 8 von 8 Hooks aktiv`, game events: 8 of 8 hooks active, and similar lines;
     `Feste Stellen: 2 von 2 aktiv`, fixed points: 2 of 2 active), and also `Qualitaets-Tooltip (Lebensraub): eingehaengt` (quality tooltip,
     life steal: installed).
   - The x87 control word is `007F` in each line `FPU-Zustand ...` (FPU state).
   - No line with `nicht eingehaengt` (not installed), `FEHLER` (error) or `WARNUNG` (warning). If the log shows `nicht eingehaengt`, the game
     version is different from the expected version. The modkit does not use this function and continues to run. This is not a crash, but please
     report it.
3. **Turn on mods:** add the lines `+perk_lifesteal` and `+beliars_klaue` to `<copy>\g3asm\mods\load_order.txt`. The example mods are in the zip
   in `examples\mods`. Copy them into the folder `<copy>\g3asm\mods\`. Start the game, fight, open the character sheet.
4. **Save and load** with the mods on, **into a new slot**.
5. **Uninstall:** `... install_loader.ps1 -Game "<copy>" -Uninstall`. Then start the game **without the modkit**.

**Caution for step 5:** `perk_lifesteal` makes an item stack in the inventory of the hero. If you load a saved game with this stack **without** the
mod and open the character sheet, the game crashes (measured, see [Limits](modkit/limitations.md)). Since version 31, the mod removes the stack
before each save and makes it again after the save and after each load. Thus saved games of this version do not contain it. Older saved games can
contain it. In this case, set `ENTFERNEN = true` (remove) in `perk_lifesteal`, load 1 time, save, and only then turn off the mod.

## Data that helps in a report
- The installation (Steam, GOG, CD), DXVK yes or no, and the line `Voraussetzungen: ...` from the log.
- `g3asm.log` and `g3asm_loader.log` from the game folder. The loader writes the path to the settings file in the first line. Before you send the
  file, hide the user name in the path if necessary.
- If the game crashes: the last action in the game.
