# OG3-Patch: license and additional permissions

OG3-Patch is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at your option) any later version. See `LICENSE`.

Copyright (C) 2026 OG3-Patch authors

The example mods and the template script in `examples/` are dedicated to the public domain under CC0 1.0 (see `examples/LICENSE`).
You may copy them into your own mods without any conditions.

## Additional permissions under GNU GPL version 3 section 7

The following additional permissions apply to the parts of OG3-Patch whose copyright is held by the OG3-Patch authors. They do not
extend to third-party code that may be included in the future under the plain GPL (for example code taken from the Gothic 3 SDK).
As stated in section 7, anyone who conveys a modified version may remove these permissions from their version.

### 1. Linking with the game

If you modify this Program, or any covered work, by linking or combining it with the executables and libraries of the game Gothic 3
(including `Gothic3.exe`, `Engine.dll`, `Game.dll`, `Script.dll`, `SharedBase.dll`, the other libraries and script DLLs shipped with the
game or with its patches, and modified versions of these), containing parts covered by the terms of their own licenses, the licensors of
this Program grant you additional permission to convey the resulting work. Corresponding Source for a non-source form of such a
combination does not need to include the source code of those game files.

### 2. Mods using the script interface

A "Mod" is a set of files that is loaded by this Program at run time and interacts with it only through the script interface documented
in `docs/modkit` (the functions, types, constants, events and overrides that the Program registers with its script engine). This
includes AngelScript sources, `mod.ini` and `load_order.txt` entries, string tables, file patches, mod archives and other game data.

A Mod is not regarded as a work based on this Program merely because it uses that script interface, is loaded by the Program or is
distributed together with it. You may license and convey Mods under terms of your choice. This permission does not cover modified
versions of the Program itself (its C++ sources and the binaries built from them); those remain under the GPL.

---

## Zusammenfassung (Deutsch, nicht rechtsverbindlich)

- Der Patch (C++-Quellen, `og3_patch.dll`, Loader, Werkzeuge) steht unter **GPL-3.0-or-later**. Wer ihn verändert und weitergibt, muss
  den Quellcode der Änderung ebenfalls unter der GPL offenlegen.
- **Erlaubnis 1:** Der Patch darf mit den Programmdateien von Gothic 3 zusammen laufen und weitergegeben werden, obwohl das Spiel nicht frei ist.
- **Erlaubnis 2:** Mods, die den Patch nur über die Skript-Schnittstelle nutzen, darfst du unter jeder Lizenz veröffentlichen, auch
  geschlossen. Die GPL gilt für sie nicht.
- Die Beispiel-Mods unter `examples/` sind gemeinfrei (CC0): beliebig kopieren und ändern.
