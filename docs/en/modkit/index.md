# G3 AngelScript Modkit (G3ASM): Mods for Gothic 3

[Deutsch](../../modkit/index.md) | **English**

With **G3ASM**, you write mods for Gothic 3 in a script language. You do not compile the game again, and you do not change game files.
The scripts are **AngelScript**. The syntax is similar to C++: `int`, `float`, `bCString`, classes, arrays. The modkit loads the scripts when the
game starts. It runs them when game events occur. When you save a script file while the game runs, the modkit loads the script again.

**For whom:** Modders with basic knowledge of programming.

Current script interface: **version 47** (`GetApiVersion()`).

The log messages of the modkit are in German. This documentation quotes them in German and gives the English meaning where necessary.

## Contents

| Page | Contents |
|---|---|
| [First mod step by step](erste-mod.md) | **Start here:** a small mod with a quest and a dialog, chapter by chapter |
| [Mod folder and installation](mod-package.md) | The location of a mod, the load order, ini settings, the log, hot reload |
| [Scripting guide](scripting-guide.md) | The language, special points, traps, troubleshooting |
| [Events](events.md) | All functions that a mod can define (engine events and game events) |
| [API reference](api-reference.md) | All types and functions that a script can call, with labels (experimental, deprecated, ...) |
| [Version rules](api-policy.md) | The changes that are permitted between two versions of the script interface |
| [Quests](quests.md) | Make quests, control them, give them texts and a reward |
| [Hooks](hooks.md) | Change or cancel game functions with Override and Super() |
| [File patches](patches.md) | Unified diff patches for the quest files and info files of the game |
| [Pack mod archives](archives.md) | `PackMod`: write a folder as a `.mod` or `.m00` archive, compatible with G3PakDir and the Union Starter |
| [Command line](cli.md) | `g3asm.exe`: make a new mod, check a script without the game, pack, read the log, each also as JSON |
| [Dialogs](dialogs.md) | Read dialogs (infos) and make your own, commands, the connection to the quest log |
| [Save and load](saving.md) | Load events, switches in the saved game, restore |
| [Examples](examples.md) | Complete mods to learn from and to copy |
| [Reference](reference.md) | Checked values, names and constants: damage types, status effects, qualities, slots, AI modes, attributes, names from the game |
| [Limits](limitations.md) | What works, what does not work and what is not tested |

## Quick start

1. Install the modkit. Close the game first. The game must have the Community Patch 1.75 and the Parallel Universe Patch. In the unpacked
   release folder, type:
   `powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_loader.ps1 -Game "<game folder>" -BuildDir bin`.
   For more data, see [Mod folder](mod-package.md).
2. Make the folder `<game folder>\g3asm\mods\hello\`. Put a file `main.as` in it:
   ```angelscript
   void main() {
       print("Hello from my first mod");
   }

   void OnKey(int vk, bool down) {
       if (down && vk == 0x91)                       // Scroll Lock key
           PrintScreen("Hello!", 3, 255, 255, 255);  // text, seconds, R, G, B
   }
   ```
3. Start the game. Push the Scroll Lock key. The text shows on the screen. `<game folder>\g3asm.log` contains `[hello] Hello from my first mod`.
4. Change the file and save it. After 1 second, the new version is active. You do not have to start the game again.

Then continue with [First mod step by step](erste-mod.md).

## How it works (short version)

- The loader is a replacement `dbghelp.dll` in the game folder. It loads the modkit (`g3asm.dll`). The modkit connects to the game through the
  import table of `Gothic3.exe`. It does not change code, so it is compatible with the Parallel Universe Patch.
- For game events (kills, items, spells, ...), the modkit intercepts some functions of `Game.dll` in memory (MinHook). The files on the disk do not
  change.
- Scripts call the same engine functions as the game itself, for example for attributes, the inventory and quests. Thus the behavior is the
  original behavior.

## The license of your mod

The modkit has the license GPL-3.0-or-later. An additional permission (`LICENSE-EXCEPTIONS.md` in the main folder) makes this clear: if a mod uses
the modkit only through the script interface, you can publish the mod under any license. The script interface includes scripts, `mod.ini`,
string tables, file patches and archives. The example mods are public domain (CC0). You can copy them into your own mods without conditions.
