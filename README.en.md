# G3 AngelScript Modkit (G3ASM)

[Deutsch](README.md) | **English**

AngelScript modding for the **original Gothic 3**: `g3asm.dll` is loaded by a small replacement `dbghelp.dll` (the loader, `src/loader`), starts
AngelScript 2.38 and connects game events, quests, inventory, dialogs and file patches to mod scripts.

**Requirements:** Gothic 3 with **Community Patch 1.75** and the **Parallel Universe Patch** (PU). `tools/install_loader.ps1` checks both and stops
if one is missing (`-Force` installs anyway, untested); the modkit writes the result to its log as a line `Voraussetzungen: ...`.

The detailed documentation under `docs/` and all log messages are in German. This page is the English overview.

Contributing: house rules (hooks, floating point, style, static analysis with clang-tidy) are in [CONTRIBUTING.md](CONTRIBUTING.md) (German).

## Modding

1. **Install** (game closed), in the unpacked release folder:

       powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_loader.ps1 -Game "<game folder>" -BuildDir bin

2. **Create a mod:** one folder per mod containing `main.as`, e.g. `<game folder>\g3asm\mods\hello\main.as`:

       void main() { print("Hello from my first mod"); }
       void OnNPCKilled(Entity victim) { PrintScreen("Killed: " + victim.GetName(), 3, 255, 120, 120); }

3. **Start the game.** Output and errors (with line and column) go to `<game folder>\g3asm.log`. Save `main.as` while the game is running and the
   new version is active after about one second (hot reload).
4. **Order and switches** in `g3asm\mods\load_order.txt`: `-name` disables a mod, `+name` enables a mod that is off by default (all example mods
   ship that way). A mod's version, dependencies and settings go into its `mod.ini`.
5. **Share it:** `bin\g3asm\g3asm_packmod.exe release <game folder>\g3asm\mods\my_mod <target folder>` builds a package for players (Union Starter
   layout, runtime included). Details (German): [sharing mods](docs/modkit/mod-package.md#mods-weitergeben).

## Using mods (for players)

Players do not need the modkit. Every mod package brings the runtime (`Script_G3ASM_<version>.dll`); the game loads it by itself.

- **With the Union Starter:** put the package folder into `<game folder>\MDS\Mods\`, tick it in the Starter, install. Untick and install again to
  remove the mod.
- **Without the Starter:** copy `ScriptFiles\scripts\*.dll` to `<game folder>\scripts\` and every file from `ModFiles\` to `<game folder>\Data\` as
  the next free generation (`G3ASM.m0x` becomes `G3ASM.mod`, for a second mod `G3ASM.m00` and so on; the same for other archives such as
  `Quests.m0x`).

At startup the runtime unpacks every mod from `Data\G3ASM.*` to `g3asm\mods\<mod>\`. Output and errors go to `g3asm.log` in the game folder.
Requirements as above: Community Patch 1.75 and Parallel Universe Patch.

What a mod can do:
- react to game events (kills, items, spells, damage, areas, quest status, saving/loading, spoken lines, keys, every frame);
- change or cancel selected game functions with `Override_...` and `Super()` (damage, dialog options, learning costs, monster damage, game scripts
  and AI states by name, console, trader goods);
- read and change attributes, inventory, item quality, perks, properties, quests and dialogs, clone templates, spawn NPCs;
- draw text and images above the HUD;
- replace game texts with a `stringtable.ini`, patch quest and info files with unified diffs (`ScriptPatches`) and pack own files into a mod
  archive (`PackMod`);
- work together with other mods (`IsModActive`, `CallMod`, `SendModMessage`).

Guide and reference (German): [docs/modkit/index.md](docs/modkit/index.md) (start, quick start), [scripting guide](docs/modkit/scripting-guide.md),
[events](docs/modkit/events.md), [API reference](docs/modkit/api-reference.md), [examples](docs/modkit/examples.md). The example mods in
`examples/mods` are named `demo_*` (learning examples), `probe_*` (measurement mods) or have their own name (small mods with game content).

## Limits

Details in [limitations](docs/modkit/limitations.md) (German).

**In principle:** Gothic 3's game logic lives in native DLLs. A mod can only hook in where the modkit exposes a place.

**Does not work** (measured in the game or examined in the game code):
- Drawing plain colored rectangles: the rectangle functions of the GUI and the renderer draw nothing in the game; only text and images work.
- Letting the game teach own perks: `LearnSkill` returns `false` for a cloned perk; the mod provides the effect itself.
- Setting references to other templates as text with `SetProperty` (eleven spellings tried); use `SetTemplateRef`/`SetEntityRef` instead.
- A general hit hook before the damage calculation: Game.dll exports no entry point for it.
- File patches (`ScriptPatches`) for players: as a script DLL the runtime starts only after the game has scanned its files. Mod packages therefore
  ship game files as archives (`ModFiles\<volume>.m0x`); file patches work only with the loader.
- Loading saves without the mod when they contain permanent state (items from cloned templates, perks): such a save crashed without the mod when
  opening the character sheet. Mods therefore remove that state in `OnBeforeSave` and restore it afterwards (pattern: `perk_lifesteal`).

**Not available** (not built or not examined yet, not proven impossible):
- Animations: playing, intercepting, adding own ones.
- World editing (placing objects), own models.
- Own input controls, a settings menu in the game.
- Writing the quest log directly (entries come from dialog lines) and setting the condition lists of own infos (items, reputation); the script
  checks conditions itself.
- File patches on compressed archive entries (`Strings.pak`) and binary files; file patches always patch the version from the `.pak`, even if
  another mod archive replaces the same file.
- The detailed documentation in English (only this README is English).

**Experimental** (works, effect not fully measured, may still change): `HurtHitPoints` (lowers hit points, does not kill), `SetProperty` (text
values only), `GenerateTradeGoods` (goods did not change in tests), `Override_CombatMove`.

**Not tested** (details in [compatibility](docs/compatibility.md)):
- GOG and CD/DVD versions; without DXVK only on one installation. Not supported without Community Patch 1.75 or Parallel Universe Patch.
- Together with other script plugins that set their own hooks (e.g. CombatFix). `Script_MonsterDamage.dll` and `Script_LearningThresholdTweaks.dll`
  change the same code as the modkit; if they are installed, `Override_MonsterDamage` or `Override_LearnCost` stays off.
- On a Steam installation with many third-party mods, a save with `perk_lifesteal` crashed when opening the character sheet, even without mods;
  the cause is unknown. It does not happen on a clean installation.
- Never triggered in the game: AI routines (`AI_ROUTINE`), divisor 2 in `Override_MonsterDamage` (summoned attackers) and the doubled learning
  costs with "Alternative Balance". Dependencies in `mod.ini` are tested without the game only; file patches on `.info` files are untested.
- Mod packages through the Union Starter: tested in the game; two mods with different runtime versions are not tested yet.

## Layout
| Folder | Content |
|---|---|
| `src/` | DLL sources (`g3asm_main`, `script_host`, `*_bindings`, `game_hooks`, `file_patches`, `packed_mods`), `script_dll.cpp` (runtime as `Script_G3ASM_<API>.dll`), `src/loader/` (loader = `dbghelp.dll`) |
| `tests/` | `test_runner.cpp` (g3asm_test), `scripts/selftest.as`, `mods/` (folder test), `sb_stub/` (replacement SharedBase.dll for tests without game files) |
| `examples/` | example mods (`mods/`) and mini mod (`scripts/main.as`) |
| `docs/` | `modkit/` (modder documentation), `compatibility.md` (tested installations) |
| `tools/` | `g3pak_read.py` (read archives, including compressed entries), `reference_from_log.py` (reference from the dump), `install_loader.ps1` (install/uninstall), `package_release.ps1` (release zip), `check_no_fninit.ps1`, `run_clang_tidy.ps1`, `packmod_main.cpp` (`g3asm_packmod`), `graphify_modding_sync.ps1` |
| (fetched when building) | AngelScript (zlib) and MinHook: `G3ASM_ANGELSCRIPT_DIR` / `G3ASM_MINHOOK_DIR` or automatic download |

## Building (32 bit)
    cmake -S . -B build32 -A Win32
    cmake --build build32 --config Release --target g3asm g3asm_script g3asm_test g3asm_packmod g3asm_loader g3asm_loader_test g3asm_sb_stub
Output: `build32/g3asm/` (g3asm.dll, Script_G3ASM_<API>.dll, tests, g3asm_packmod) and `build32/loader/dbghelp.dll` (loader).

## Tests
    g3asm_test.exe <SharedBase.dll> tests\scripts\selftest.as
    g3asm_test.exe <SharedBase.dll> tests\mods
    powershell -NoProfile -File tests\run_loader_test.ps1 -SharedBase <SharedBase.dll> -DbghelpOrig <dbghelp_orig.dll>   (loader in a throwaway folder)
    python -I tests\file_patches_overlay.py g3asm_test.exe <SharedBase.dll>   (file patches in a throwaway folder)
    python -I tests\packed_mods.py g3asm_test.exe <SharedBase.dll> g3asm_packmod.exe   (packed mods: unpack, keep up to date, remove)
    python -I tests\packmod_release.py g3asm_test.exe <SharedBase.dll> g3asm_packmod.exe   (build a player package, install, start)
Without game files: use the replacement `build32\sbstub\SharedBase.dll` (target `g3asm_sb_stub`) as `<SharedBase.dll>` and the system DLL
`%WINDIR%\SysWOW64\dbghelp.dll` as `<dbghelp_orig.dll>`. The replacement only has the exports the modkit needs for bCString and the math types
and does not compute bit-exactly like the original; measure the original's behavior only with the real SharedBase.dll.

## CI and release (GitHub Actions)
- `.github/workflows/ci.yml` (every push to `main`, pull requests, manual): builds 32 bit with MSVC, checks that `g3asm.dll` contains no `fninit`
  (`tools/check_no_fninit.ps1`, guards the FPU fix), runs self test, test mods, compile check of the example mods and loader test with the
  replacement SharedBase, plus `tests/packmod_generations.py`, `tests/file_patches_overlay.py`, `tests/packed_mods.py` and `tests/packmod_release.py`, and packs a trial zip (artifact, 7 days).
- `.github/workflows/release.yml`: on pushing a tag `v*` the same plus a GitHub release with the zip:
      git tag v0.46.1-beta.1
      git push origin v0.46.1-beta.1
  Tags with a hyphen (`-alpha.N`, `-beta.N`) are published as pre-releases; naming scheme in the [versioning rules](docs/modkit/api-policy.md).
- Local package: `powershell -NoProfile -File tools\package_release.ps1 -BuildDir build32 -Version v0.46.1` (result in `dist\`). Content: loader,
  `g3asm.dll`, `g3asm_packmod.exe`, `tools\install_loader.ps1`, example mods (off by default), documentation, `INSTALL.txt`; no game files.
- Not in CI: everything that needs the real game (measurements with the original `SharedBase.dll`, game tests). Game files do not belong in the repository.

## Installing the loader
    powershell -NoProfile -File tools\install_loader.ps1 -Game <game folder> [-BuildDir build32]
    powershell -NoProfile -File tools\install_loader.ps1 -Game <game folder> -Uninstall
The installer moves the game's `dbghelp.dll` aside as `dbghelp_orig.dll` (the loader forwards `MiniDumpWriteDump` to it, so the game's crash
reports keep working), puts the loader in place as `dbghelp.dll` and copies `g3asm.dll`, a `g3asm.ini` and a mini mod. Backup in
`<game>\g3asm_backup\loader_<time>\`. Logs: `g3asm_loader.log` (loader, crash candidates) and `g3asm.log` (modkit). Settings in `g3asm.ini`
(`[g3asm] enable/script/hotreload/mods`, `[loader] crashlog`).

**Former name:** Up to API 46 the project was called OG3-Patch (`og3_patch.dll`, `og3_patch.ini`, folder `og3\`, ini section `[patch]`). The
installer migrates such an installation (folders and ini are renamed, the old DLL is removed) and also finds the old backups when uninstalling.

## License
GPL-3.0-or-later, see `LICENSE`, with two additional permissions under section 7 in `LICENSE-EXCEPTIONS.md`: the modkit may be distributed together
with the program files of Gothic 3, and mods that only use the script interface may be under any license. The example mods in `examples/` are
public domain (CC0 1.0, `examples/LICENSE`). Before the switch (before the first release) the repository was under MIT.

AngelScript (zlib license) and MinHook (BSD-2-Clause) are fetched when building and are not part of the repository. The project contains no game
data. Memory layouts of the engine are taken from the [Gothic 3 SDK](https://github.com/georgeto/gothic3sdk) (georgeto, GPL-3.0-or-later); the
repository contains no SDK code.

## Acknowledgements

To the authors of the Gothic 3 SDK (gothic3sdk), whose headers opened up the engine's classes, signatures and properties, to the Community Patch
team, to the makers of the Gothic 3 Mod Starter, whose work documented the mod archive format, and to the Gothic community. The modkit uses
AngelScript (Andreas Joensson, AngelCode, zlib license) and MinHook (Tsuda Kageyu, BSD-2 license). The Gothic 1 Remake modkit served as the model
for its structure.

Gothic 3 is a trademark of its respective owners. G3ASM is an unofficial fan project and contains no game files.
