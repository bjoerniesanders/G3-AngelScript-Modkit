# OG3-Patch

AngelScript-Modding fuer Gothic 3: `og3_patch.dll` wird vom dbghelp-Proxy (`[patch]` in `og3_hooks.ini`) geladen, startet AngelScript 2.38 und
bindet Spielereignisse, Quests, Inventar, Dialoge und Datei-Patches an Mod-Skripte.

## Aufbau
| Ordner | Inhalt |
|---|---|
| `src/` | DLL-Quellen (`patch_main`, `script_host`, `*_bindings`, `game_hooks`, `file_patches`) |
| `tests/` | `test_runner.cpp` (og3_patch_test), `scripts/selftest.as`, `mods/` (Ordner-Test) |
| `examples/` | Beispiel-Mods (`mods/`) und Mini-Mod (`scripts/main.as`) |
| `docs/` | `og3_patch.md` (Stand/Technik), `modkit/` (Modder-Doku) |
| `tools/` | `install_patch.ps1` (Ein-/Ausbau in eine Spielkopie), `graphify_modding_sync.ps1` |
| (beim Bauen geladen) | AngelScript (zlib) und MinHook: `OG3P_ANGELSCRIPT_DIR` / `OG3P_MINHOOK_DIR` oder automatischer Download |

## Bauen (32 Bit)
    cmake -S . -B build32 -A Win32
    cmake --build build32 --config Release --target og3_patch og3_patch_test
Ausgabe: `build32/patch/`. Im Hauptrepo `OpenGothic3` wird der Ordner per `add_subdirectory(og3-patch)` eingebunden.

## Test
    og3_patch_test.exe <SharedBase.dll> tests\scripts\selftest.as
    og3_patch_test.exe <SharedBase.dll> tests\mods

## Einbau
    powershell -NoProfile -File tools\install_patch.ps1 -Game G:\Gothic3 [-BuildDir build32] [-ProxyDll <dbghelp.dll>]
Der Proxy (dbghelp.dll mit `[patch]`-Abschnitt) kommt aus dem Hauptrepo (`build32\hook\dbghelp.dll`); ausserhalb davon `-ProxyDll` angeben.

## Abhaengigkeiten zum Hauptrepo
Nur zur Laufzeit/Test: SharedBase.dll (Original oder Nachbau) und der Proxy. Quellcode und Build sind eigenstaendig.

## Lizenz
MIT, siehe `LICENSE`. AngelScript (zlib-Lizenz) und MinHook (BSD-2-Clause) werden beim Bauen geladen und liegen nicht im Repo. Das Projekt enthaelt keine Spieldaten.
