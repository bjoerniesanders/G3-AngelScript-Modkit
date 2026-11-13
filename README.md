# OG3-Patch

AngelScript-Modding fuer das **Original-Gothic-3**: `og3_patch.dll` wird von einer schlanken Ersatz-`dbghelp.dll` (dem Loader, `src/loader`) geladen, startet
AngelScript 2.38 und bindet Spielereignisse, Quests, Inventar, Dialoge und Datei-Patches an Mod-Skripte.

**Voraussetzungen:** Gothic 3 mit **Community Patch 1.75** und **Parallel-Universe-Patch** (PU). `tools/install_loader.ps1` prueft beides und bricht
ab, wenn etwas fehlt (`-Force` baut trotzdem ein, ungetestet); der Patch schreibt das Ergebnis als Zeile `Voraussetzungen: ...` ins Protokoll.

Wer am Code mitarbeitet: Hausregeln (Hooks, Gleitkomma, Stil, statische Pruefung mit clang-tidy) stehen in [CONTRIBUTING.md](CONTRIBUTING.md).

## Aufbau
| Ordner | Inhalt |
|---|---|
| `src/` | DLL-Quellen (`patch_main`, `script_host`, `*_bindings`, `game_hooks`, `file_patches`), `src/loader/` (Loader = `dbghelp.dll`) |
| `tests/` | `test_runner.cpp` (og3_patch_test), `scripts/selftest.as`, `mods/` (Ordner-Test), `sb_stub/` (Ersatz-SharedBase.dll fuer Tests ohne Spieldateien) |
| `examples/` | Beispiel-Mods (`mods/`) und Mini-Mod (`scripts/main.as`) |
| `docs/` | `og3_patch.md` (Stand/Technik), `modkit/` (Modder-Doku) |
| `tools/` | `g3pak_read.py` (Archive lesen, auch komprimierte Eintraege), `reference_from_log.py` (Referenz aus dem Dump), `install_loader.ps1` (Ein-/Ausbau mit dem Loader), `install_patch.ps1` (nur mit dem Proxy des Hauptrepos), `graphify_modding_sync.ps1` |
| (beim Bauen geladen) | AngelScript (zlib) und MinHook: `OG3P_ANGELSCRIPT_DIR` / `OG3P_MINHOOK_DIR` oder automatischer Download |

## Bauen (32 Bit)
    cmake -S . -B build32 -A Win32
    cmake --build build32 --config Release --target og3_patch og3_patch_test og3_loader og3_loader_test og3_sb_stub
Ausgabe: `build32/patch/` (og3_patch.dll, Tests, og3_packmod) und `build32/loader/dbghelp.dll` (Loader). Im Hauptrepo `OpenGothic3` wird der Ordner per `add_subdirectory(og3-patch)` eingebunden.

## Test
    og3_patch_test.exe <SharedBase.dll> tests\scripts\selftest.as
    og3_patch_test.exe <SharedBase.dll> tests\mods
    powershell -NoProfile -File tests\run_loader_test.ps1 -SharedBase <SharedBase.dll> -DbghelpOrig <dbghelp_orig.dll>   (Loader in einem Wegwerf-Ordner)
Ohne Spieldateien: als `<SharedBase.dll>` die Ersatz-DLL `build32\sbstub\SharedBase.dll` nehmen (Ziel `og3_sb_stub`), als `<dbghelp_orig.dll>`
die System-DLL `%WINDIR%\SysWOW64\dbghelp.dll`. Die Ersatz-DLL hat nur die Exporte, die der Patch fuer bCString und die Mathe-Typen braucht,
und rechnet nicht bitgenau wie das Original; Messungen am Verhalten des Originals nur mit der echten SharedBase.dll.

## CI und Release (GitHub Actions)
- `.github/workflows/ci.yml` (jeder Push auf `main`, Pull-Requests, von Hand): baut 32 Bit mit MSVC, prueft, dass `og3_patch.dll` kein `fninit` enthaelt
  (`tools/check_no_fninit.ps1`, Schutz fuer den FPU-Fix), fuehrt Selbsttest, Test-Mods, Kompilierpruefung der Beispiel-Mods und Loader-Test mit der Ersatz-SharedBase aus, dazu `tests/packmod_generations.py`, und packt probeweise ein Zip (Artefakt, 7 Tage).
- `.github/workflows/release.yml`: beim Push eines Tags `v*` dasselbe plus GitHub-Release mit dem Zip:
      git tag v0.30.0
      git push origin v0.30.0
- Paket lokal: `powershell -NoProfile -File tools\package_release.ps1 -BuildDir build32 -Version v0.42.0` (Ergebnis in `dist\`). Inhalt: Loader, `og3_patch.dll`,
  `og3_packmod.exe`, `tools\install_loader.ps1`, Beispiel-Mods (standardmaessig aus), Doku, `INSTALL.txt`; keine Spieldateien.
- Nicht in der CI: Selbsttest, Mod-Tests, Kompilierpruefung der Beispiele, Loader-Test und PackMod-Rundlauf. Sie brauchen die `SharedBase.dll` bzw. Archive des
  Spiels, die nicht ins Repo gehoeren, und laufen lokal (Abschnitt Test).

## Einbau (Loader, eigenstaendig)
    powershell -NoProfile -File tools\install_loader.ps1 -Game <Spielordner> [-BuildDir build32]
    powershell -NoProfile -File tools\install_loader.ps1 -Game <Spielordner> -Uninstall
Der Installer legt die vorhandene `dbghelp.dll` des Spiels als `dbghelp_orig.dll` beiseite (der Loader leitet `MiniDumpWriteDump` dorthin weiter, die Absturzberichte
des Spiels funktionieren weiter), stellt den Loader als `dbghelp.dll` hin und kopiert `og3_patch.dll`, eine `og3_patch.ini` und einen Mini-Mod. Sicherung unter
`<Spiel>\og3_backup\loader_<Zeit>\`. Protokolle: `og3_loader.log` (Loader, Absturzkandidaten) und `og3_patch.log` (Patch). Einstellungen in `og3_patch.ini`
(`[patch] enable/script/hotreload/mods`, `[loader] crashlog`); fehlt sie, liest der Loader die `og3_hooks.ini` des Hauptrepos.

`tools/install_patch.ps1` ist der aeltere Weg mit dem Proxy aus dem Hauptrepo `OpenGothic3` (`build32\hook\dbghelp.dll`, enthaelt zusaetzlich die Nachbau-Hooks).
Beides gleichzeitig geht nicht, beide ersetzen dieselbe `dbghelp.dll`.

## Abhaengigkeiten zum Hauptrepo
Keine. Zur Laufzeit braucht der Patch die `SharedBase.dll` des Spiels (Original; die Tests laufen auch mit dem Nachbau) und den Loader aus diesem Repo. Quellcode und Build sind eigenstaendig.

## Lizenz
GPL-3.0-or-later, siehe `LICENSE`, mit zwei Zusatzerlaubnissen nach Abschnitt 7 in `LICENSE-EXCEPTIONS.md`: Der Patch darf mit den Programmdateien
von Gothic 3 zusammen weitergegeben werden, und Mods, die nur die Skript-Schnittstelle nutzen, duerfen unter jeder Lizenz stehen. Die Beispiel-Mods in
`examples/` sind gemeinfrei (CC0 1.0, `examples/LICENSE`). Bis zum Wechsel (vor dem ersten Release) stand das Repo unter MIT.

AngelScript (zlib-Lizenz) und MinHook (BSD-2-Clause) werden beim Bauen geladen und liegen nicht im Repo. Das Projekt enthaelt keine Spieldaten. Speicher-Layouts
der Engine sind dem [Gothic 3 SDK](https://github.com/georgeto/gothic3sdk) (Georgeto, GPL-3.0-or-later) entnommen; Code des SDK enthaelt das Repo nicht.

## Danksagung

An die Autoren des Gothic-3-SDK (gothic3sdk), dessen Header die Klassen, Signaturen und Eigenschaften der Engine erschlossen haben, an das Team des
Community Patch, an die Macher des Gothic 3 Mod Starters, deren Arbeit das Format der Mod-Archive offengelegt hat, und an die Gothic-Community.
Das Modkit nutzt AngelScript (Andreas Joensson, AngelCode, zlib-Lizenz) und MinHook (Tsuda Kageyu, BSD-2-Lizenz). Vorbild fuer den Aufbau war das Modkit
des Gothic-1-Remakes.

Gothic 3 ist eine Marke der jeweiligen Rechteinhaber. OG3-Patch ist ein inoffizielles Fan-Projekt und enthaelt keine Dateien des Spiels.
