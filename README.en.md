# G3 AngelScript Modkit (G3ASM)

[Deutsch](README.md) | **English**

G3ASM lets you write mods for the **original Gothic 3** in AngelScript. A small replacement `dbghelp.dll` (the loader, `src/loader`) loads
`g3asm.dll`. `g3asm.dll` starts AngelScript 2.38. It connects the mod scripts to game events, quests, the inventory, dialogs and file patches.

**Before you start:** Gothic 3 must have the **Community Patch 1.75** and the **Parallel Universe Patch** (PU). `tools/install_loader.ps1` does a
check for the two patches. If a patch is missing, the script stops. With `-Force`, the script installs G3ASM, but I did not test this
configuration. The modkit writes the result of the check to its log, in a line that starts with `Voraussetzungen: ...`.

The detailed English documentation is in [docs/en/modkit](docs/en/modkit/index.md). The log messages of the modkit are in German.

For contributors: [CONTRIBUTING.en.md](CONTRIBUTING.en.md) gives the layout, the build procedure, the tests, the continuous integration (CI),
the release procedure and the house rules.

## Make a mod

1. **Install G3ASM.** Close the game. In the unpacked release folder, type this command:

       powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_loader.ps1 -Game "<game folder>" -BuildDir bin

2. **Make a mod.** Make one folder for each mod. Put a file `main.as` in the folder. Example: `<game folder>\g3asm\mods\hello\main.as`:

       void main() { print("Hello from my first mod"); }
       void OnNPCKilled(Entity victim) { PrintScreen("Killed: " + victim.GetName(), 3, 255, 120, 120); }

3. **Start the game.** The modkit writes output and errors to `<game folder>\g3asm.log`. Each error shows the line and the column. If you save
   `main.as` while the game runs, the new version is active after approximately 1 second (hot reload).

4. **Set the sequence and the switches** in `g3asm\mods\load_order.txt`:
   - `-name` turns a mod off.
   - `+name` turns on a mod that is off by default. All example mods are off by default.

   The `mod.ini` of a mod contains its version, its dependencies and its settings.

5. **Give the mod to other persons.** This command makes a package for players:

       bin\g3asm\g3asm_packmod.exe release <game folder>\g3asm\mods\my_mod <target folder>

   The package has the Union Starter layout and contains the runtime. It also contains `G3ASM-LICENSE.txt` for the runtime, because the runtime
   has the GNU General Public License (GPL). For more data, see [Share mods](docs/en/modkit/mod-package.md#share-mods).

## Use mods (for players)

Players do not need the modkit. Each mod package contains the runtime (`Script_G3ASM_<version>.dll`). The game loads the runtime automatically.

- **With the Union Starter:** Put the package folder into `<game folder>\MDS\Mods\`. Select the mod in the Starter, then install. To remove the
  mod, clear the selection and install again.
- **Without the Starter:**
  1. Copy `ScriptFiles\scripts\*.dll` to `<game folder>\scripts\`.
  2. Copy all files from `ModFiles\` to `<game folder>\Data\`. Give each file the next free generation. Example: `G3ASM.m0x` becomes
     `G3ASM.mod`. For a second mod, it becomes `G3ASM.m00`, and so on. Do the same for other archives, for example `Quests.m0x`.

When the game starts, the runtime unpacks each mod from `Data\G3ASM.*` to `g3asm\mods\<mod>\`. The runtime writes output and errors to
`g3asm.log` in the game folder. Players also must have the Community Patch 1.75 and the Parallel Universe Patch.

A mod can do these tasks:
- Run script functions when game events occur: kills, items, spells, damage, areas, quest status, save and load, spoken lines, keys, and each
  frame.
- Change or cancel some game functions with `Override_...` and `Super()`. These functions include damage, dialog options, the cost to learn a
  skill, monster damage, game scripts and artificial intelligence (AI) states by name, the console and trader goods.
- Read and change attributes, the inventory, item quality, perks, properties, quests and dialogs.
- Clone templates and spawn non-player characters (NPCs).
- Show text and images above the head-up display (HUD).
- Replace game texts with a `stringtable.ini`.
- Change quest files and info files with unified diffs (`ScriptPatches`).
- Put its own files into a mod archive (`PackMod`).
- Work together with other mods (`IsModActive`, `CallMod`, `SendModMessage`).

Guide and reference: [First mod step by step](docs/en/modkit/erste-mod.md), [docs/en/modkit/index.md](docs/en/modkit/index.md) (overview),
[Scripting guide](docs/en/modkit/scripting-guide.md), [Events](docs/en/modkit/events.md), [API reference](docs/en/modkit/api-reference.md),
[Examples](docs/en/modkit/examples.md).

The example mods in `examples/mods` have these names:
- `demo_*`: examples to learn from.
- `probe_*`: mods that measure the game.
- Mods with their own name: small mods with game content.

## Limits

**General limit:** The game logic of Gothic 3 is in native DLLs. A mod can connect to the game only at the points that the modkit makes
available. The full list is in [Limits](docs/en/modkit/limitations.md).

**These functions do not work.** I measured them in the game or examined the game code:
- A mod cannot draw rectangles with only one color. A mod can draw only text and images.
- The game cannot teach custom skills. The mod must make the effect of the skill itself.
- There is no hit hook before the game calculates the damage. To change a hit, use the game script `OnHit`, `Override_MonsterDamage` or
  `Override_Damage`.
- A mod cannot write directly to the quest log. The entries come from dialog lines.
- A mod cannot set the conditions of custom infos. The script must do a check of the conditions itself.
- Players cannot use file patches (`ScriptPatches`). Mod packages contain changed game files as archives (`ModFiles\<volume>.m0x`).

**These functions are not available.** I did not make or examine them yet:
- Animations, changes to the world, custom models.
- Custom user interface (UI) controls with input, a settings menu in the game.
- File patches for compressed archive entries (`Strings.pak`) and for binary files.

**Experimental:** `HurtHitPoints`, `SetProperty`, `GenerateTradeGoods`, `Override_CombatMove`.

**Not tested.** For more data, see [Compatibility](docs/en/compatibility.md):
- GOG versions, CD versions and DVD versions. G3ASM does not support installations without the Community Patch 1.75 or without the Parallel
  Universe Patch.
- Use with other script plugins that set their own hooks, for example CombatFix.
- If `Script_MonsterDamage.dll` is installed, `Override_MonsterDamage` stays off. If `Script_LearningThresholdTweaks.dll` is installed,
  `Override_LearnCost` stays off.
- Two mods with different runtime versions on the computer of one player.

**CAUTION:** If your mod makes permanent changes, remove the changes in `OnBeforeSave` and restore them after the save. Permanent changes are
items from cloned templates and skills. A save with such changes crashed when the mod was not installed. For an example, see `perk_lifesteal`.

## License

G3ASM has the license GPL-3.0-or-later. See `LICENSE`. `LICENSE-EXCEPTIONS.md` gives two more permissions under section 7:
- You can distribute the modkit together with the program files of Gothic 3.
- Mods that use only the script interface can have any license.

The example mods in `examples/` are public domain (CC0 1.0, `examples/LICENSE`). Before the first release, the repository had the MIT license.

The build procedure gets AngelScript (zlib license) and MinHook (BSD-2-Clause). They are not part of the repository. The project contains no
game data. The memory layouts of the engine come from the [Gothic 3 SDK](https://github.com/georgeto/gothic3sdk) (georgeto,
GPL-3.0-or-later). SDK means software development kit. The repository contains no SDK code.

## Acknowledgements

I thank these persons and groups:
- The authors of the Gothic 3 SDK (gothic3sdk). Their headers made the classes, signatures and properties of the engine available.
- The Community Patch team.
- The makers of the Gothic 3 Mod Starter. Their work documented the format of the mod archives.
- The Gothic community.

The modkit uses AngelScript (Andreas Joensson, AngelCode, zlib license) and MinHook (Tsuda Kageyu, BSD-2 license). The modkit of the Gothic 1
Remake was the model for the structure of G3ASM.

Gothic 3 is a trademark of its owners. G3ASM is an unofficial fan project. It contains no game files.
