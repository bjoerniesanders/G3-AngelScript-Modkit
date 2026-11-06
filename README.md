# OG3-Patch

AngelScript-Modding fuer das **Original-Gothic-3**: `og3_patch.dll` wird von einer schlanken Ersatz-`dbghelp.dll` (dem Loader, `src/loader`) geladen, startet
AngelScript 2.38 und bindet Spielereignisse, Quests, Inventar, Dialoge und Datei-Patches an Mod-Skripte.

## Aufbau
| Ordner | Inhalt |
|---|---|
| `src/` | DLL-Quellen (`patch_main`, `script_host`, `*_bindings`, `game_hooks`, `file_patches`), `src/loader/` (Loader = `dbghelp.dll`) |
| `tests/` | `test_runner.cpp` (og3_patch_test), `scripts/selftest.as`, `mods/` (Ordner-Test) |
| `examples/` | Beispiel-Mods (`mods/`) und Mini-Mod (`scripts/main.as`) |
| `docs/` | `og3_patch.md` (Stand/Technik), `modkit/` (Modder-Doku) |
| `tools/` | `g3pak_read.py` (Archive lesen, auch komprimierte Eintraege), `reference_from_log.py` (Referenz aus dem Dump), `install_loader.ps1` (Ein-/Ausbau mit dem Loader), `install_patch.ps1` (nur mit dem Proxy des Hauptrepos), `graphify_modding_sync.ps1` |
| (beim Bauen geladen) | AngelScript (zlib) und MinHook: `OG3P_ANGELSCRIPT_DIR` / `OG3P_MINHOOK_DIR` oder automatischer Download |

## Bauen (32 Bit)
    cmake -S . -B build32 -A Win32
    cmake --build build32 --config Release --target og3_patch og3_patch_test og3_loader og3_loader_test
Ausgabe: `build32/patch/` (og3_patch.dll, Tests, og3_packmod) und `build32/loader/dbghelp.dll` (Loader). Im Hauptrepo `OpenGothic3` wird der Ordner per `add_subdirectory(og3-patch)` eingebunden.

## Test
    og3_patch_test.exe <SharedBase.dll> tests\scripts\selftest.as
    og3_patch_test.exe <SharedBase.dll> tests\mods
    powershell -NoProfile -File tests\run_loader_test.ps1 -SharedBase <SharedBase.dll> -DbghelpOrig <dbghelp_orig.dll>   (Loader in einem Wegwerf-Ordner)

## Einbau (Loader, eigenstaendig)
    powershell -NoProfile -File tools\install_loader.ps1 -Game <Spielordner> [-BuildDir build32]
    powershell -NoProfile -File tools\install_loader.ps1 -Game <Spielordner> -Uninstall
Der Installer legt die vorhandene `dbghelp.dll` des Spiels als `dbghelp_orig.dll` beiseite (der Loader leitet `MiniDumpWriteDump` dorthin weiter, die Absturzberichte
des Spiels funktionieren weiter), stellt den Loader als `dbghelp.dll` hin und kopiert `og3_patch.dll`, eine `og3_patch.ini` und einen Mini-Mod. Sicherung unter
`<Spiel>\og3_backup\loader_<Zeit>\`. Protokolle: `og3_loader.log` (Loader, Absturzkandidaten) und `og3_patch.log` (Patch). Einstellungen in `og3_patch.ini`
(`[patch] enable/script/hotreload/mods`, `[loader] crashlog`); fehlt sie, liest der Loader die `og3_hooks.ini` des Hauptrepos.

`tools/install_patch.ps1` ist der ältere Weg mit dem Proxy aus dem Hauptrepo `OpenGothic3` (`build32\hook\dbghelp.dll`, enthaelt zusaetzlich die Nachbau-Hooks).
Beides gleichzeitig geht nicht, beide ersetzen dieselbe `dbghelp.dll`.

## Abhaengigkeiten zum Hauptrepo
Keine. Zur Laufzeit braucht der Patch die `SharedBase.dll` des Spiels (Original; die Tests laufen auch mit dem Nachbau) und den Loader aus diesem Repo. Quellcode und Build sind eigenstaendig.

## Lizenz
MIT, siehe `LICENSE`. AngelScript (zlib-Lizenz) und MinHook (BSD-2-Clause) werden beim Bauen geladen und liegen nicht im Repo. Das Projekt enthaelt keine Spieldaten.
