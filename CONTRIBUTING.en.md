# Contributing

[Deutsch](CONTRIBUTING.md) | **English**

Short house rules for changes to this repository. They describe how the code is made today. If you do something different, write the reason.
The rules for the script interface are in [docs/en/modkit/api-policy.md](docs/en/modkit/api-policy.md). The documentation for modders is in
[docs/en/modkit](docs/en/modkit/index.md).

## Layout
| Folder | Contents |
|---|---|
| `src/` | DLL sources (`g3asm_main`, `script_host`, `*_bindings`, `game_hooks`, `file_patches`, `packed_mods`), `script_dll.cpp` (runtime as `Script_G3ASM_<API>.dll`), `src/loader/` (loader = `dbghelp.dll`) |
| `tests/` | `test_runner.cpp` (g3asm_test), `scripts/selftest.as`, `mods/` (folder test), `sb_stub/` (replacement SharedBase.dll for tests without game files) |
| `examples/` | Example mods (`mods/`) and mini mod (`scripts/main.as`) |
| `docs/` | `modkit/` (modder documentation, German), `compatibility.md` (checked installations), `en/` (English versions) |
| `tools/` | `g3pak_read.py` (read archives, also compressed entries), `reference_from_log.py` (reference from the dump), `install_loader.ps1` (install and uninstall), `package_release.ps1` (release zip), `check_no_fninit.ps1`, `run_clang_tidy.ps1`, `packmod_main.cpp` (`g3asm_packmod`), `graphify_modding_sync.ps1` |
| (downloaded during the build) | AngelScript (zlib) and MinHook: `G3ASM_ANGELSCRIPT_DIR` and `G3ASM_MINHOOK_DIR`, or automatic download |

## Build (32 bit)
    cmake -S . -B build32 -A Win32
    cmake --build build32 --config Release --target g3asm g3asm_script g3asm_test g3asm_packmod g3asm_loader g3asm_loader_test g3asm_sb_stub
Output: `build32/g3asm/` (g3asm.dll, Script_G3ASM_<API>.dll, tests, g3asm_packmod) and `build32/loader/dbghelp.dll` (loader).

## Test
    g3asm_test.exe <SharedBase.dll> tests\scripts\selftest.as
    g3asm_test.exe <SharedBase.dll> tests\mods
    powershell -NoProfile -File tests\run_loader_test.ps1 -SharedBase <SharedBase.dll> -DbghelpOrig <dbghelp_orig.dll>   (loader in a temporary folder)
    python -I tests\file_patches_overlay.py g3asm_test.exe <SharedBase.dll>   (file patches in a temporary folder)
    python -I tests\packed_mods.py g3asm_test.exe <SharedBase.dll> g3asm_packmod.exe   (unpack packed mods, keep them current, remove them)
    python -I tests\packmod_release.py g3asm_test.exe <SharedBase.dll> g3asm_packmod.exe   (make, install and start a package for players)
Without game files: as `<SharedBase.dll>`, use the replacement DLL `build32\sbstub\SharedBase.dll` (target `g3asm_sb_stub`). As
`<dbghelp_orig.dll>`, use the system DLL `%WINDIR%\SysWOW64\dbghelp.dll`. The replacement DLL has only the exports that the modkit needs for
bCString and the math types. It does not calculate bit for bit as the original does. Measure the behavior of the original only with the real
SharedBase.dll.

## CI and release (GitHub Actions)
- `.github/workflows/ci.yml` (each push to `main`, pull requests, manual): builds 32 bit with MSVC. It makes sure that `g3asm.dll` contains no
  `fninit` (`tools/check_no_fninit.ps1`, protection for the FPU fix). It runs the self-test, the test mods, the compile check of the example mods
  and the loader test with the replacement SharedBase, and also `tests/packmod_generations.py`, `tests/file_patches_overlay.py`,
  `tests/packed_mods.py` and `tests/packmod_release.py`. It packs a test zip (artifact, 7 days).
- `.github/workflows/release.yml`: when you push a tag `v*`, the same steps, and a GitHub release with the zip:
      git tag -a v0.46.0-beta.1 -m "G3 AngelScript Modkit v0.46.0-beta.1"
      git push origin v0.46.0-beta.1
  Tags with a hyphen (`-alpha.N`, `-beta.N`) are published as pre-release versions. For the name pattern, see
  [Version rules](docs/en/modkit/api-policy.md). The workflow sets the title ("G3 AngelScript Modkit <tag>"). Write the release notes by hand on
  GitHub after this. The project status goes there.
- Local package: `powershell -NoProfile -File tools\package_release.ps1 -BuildDir build32 -Version v0.46.0-beta.1` (result in `dist\`). Contents:
  loader, `g3asm.dll`, `Script_G3ASM_<API>.dll` (runtime for mod packages), `g3asm_packmod.exe`, `tools\install_loader.ps1`, `tools\g3pak_read.py`,
  example mods (off by default), documentation, `INSTALL.txt`. No game files.
- Not in the CI: all items that need the real game (measurements with the original `SharedBase.dll`, game tests). Game files must not go into the
  repository.

## Install from your own build
    powershell -NoProfile -File tools\install_loader.ps1 -Game <game folder> [-BuildDir build32]
    powershell -NoProfile -File tools\install_loader.ps1 -Game <game folder> -Uninstall
The installer renames the existing `dbghelp.dll` of the game to `dbghelp_orig.dll`. The loader sends `MiniDumpWriteDump` to it, so the crash
reports of the game continue to work. The installer puts the loader as `dbghelp.dll` and copies `g3asm.dll`, a `g3asm.ini` and a mini mod. Backup
in `<game>\g3asm_backup\loader_<time>\`. Logs: `g3asm_loader.log` (loader, crash candidates) and `g3asm.log` (modkit). Settings in `g3asm.ini`
(`[g3asm] enable/script/hotreload/mods`, `[loader] crashlog`).

## Before each commit
1. [Build](#build-32-bit) and do the [test runs](#test): self-test, `tests\mods`, compile check of `examples\mods` (there, exactly one error
   "Override-Kette über mehrere Mods" is expected), loader test, `tests\file_patches_overlay.py`, `tests\packed_mods.py` and
   `tests\packmod_release.py`.
2. `powershell -NoProfile -File tools\check_no_fninit.ps1 -Dll <build>\g3asm\g3asm.dll`, and the same for `Script_G3ASM_<API>.dll` (see Floating
   point).
3. `powershell -NoProfile -File tools\run_clang_tidy.ps1 -BuildDir <build> -Changed -Strict` (see Static analysis).
4. If a change has an effect on the behavior in the game, check it in the game and read the log (`g3asm.log`). Without game files, only the part
   with the replacement SharedBase (`tests/sb_stub`) is possible.

## Language
- **German:** the documentation in `docs/` (without `docs/en/`), `README.md`, `CONTRIBUTING.md`, log messages, texts in the example mods. The
  documentation with umlauts. Log messages in ASCII (ae, oe, ue, ss), so that they stay readable in each console and each editor.
- **English:** `docs/en/`, `README.en.md`, this file, comments in the code, commit messages, `.github/workflows`, scripts for CI and release
  (`tools/package_release.ps1`, `tools/check_no_fninit.ps1`, `tools/run_clang_tidy.ps1`) and the generated `INSTALL.txt`. Write the English
  documentation and the comments in simple English: short sentences, active voice, one term for one thing.
- If you change a German page, change its English version in `docs/en/` too (and the other way around). This also applies to README and
  CONTRIBUTING.
- Commit messages with the pattern `feat: ...`, `fix: ...`, `docs: ...`, `ci: ...`, `refactor: ...`. Use `feat!:` if the script interface
  changes in an incompatible way.

## Write documentation
- Write for modders: what a function does, what it gives, and what you must be careful about. How something was measured, dates, addresses and
  classes of the engine go into commit messages, release notes or code comments, not into `docs/`.
- No project status in the documentation (no "Status:" lines, no lists of open tasks). Limits and items that are not tested go into
  `docs/modkit/limitations.md`, factual and without dates. The README gives only a summary.
- Reference tables: for each row, the signature and one or two sentences. All other data goes into a list below the table or into a separate
  section (pattern: `docs/modkit/hooks.md`).
- Short sentences, active voice, "you" for the reader. Use the same name for the same thing: numeric keypad, template, info (dialog option),
  saved game.
- Code examples must compile. Mark parts of code as parts.

## Style
- `.clang-format` in the root folder (LLVM base, 4 spaces, 140 columns). Not all existing code has this format: format **only new and changed
  lines**, not full files (if you do, each change becomes unreadable).
- Sources are UTF-8 **without BOM** (the modkit reads files with BOM, but the repository stays consistent). PowerShell 5.1 writes a BOM with
  `Set-Content -Encoding utf8`. To write files, use `[IO.File]::WriteAllText(..., (New-Object Text.UTF8Encoding($false)))`. Intentional
  exception: `tests/mods/load_order.txt` and `tests/mods/alpha/mod.ini` start with a BOM (test).
- Comments explain why, and where a value comes from (measured on ..., export ..., RVA ...), not what.
- Do not copy code from the Gothic 3 SDK or from GPL examples. Use only findings (which function, which layout). See `LICENSE-EXCEPTIONS.md`.
- No game files in the repository (DLLs, archives, saved games).

## Static analysis (clang-tidy)
- Selection in `.clang-tidy`: sources of errors (`bugprone-*`, `clang-analyzer-*`), expensive code (`performance-*`) and a small number of other
  checks. No style rules or rename rules. The Core Guidelines rules against `reinterpret_cast` and pointer arithmetic are off, because hook code
  must break them.
- The existing code has no findings. Correct new findings, or give the reason at the line: `// NOLINT(<check>): <reason>`. A `NOLINT` without a
  check and a reason is not permitted.
- Turn off a full check only in `.clang-tidy`, with a line that tells why it gives only false alarms here.
- Call: `tools\run_clang_tidy.ps1 -BuildDir <build>` (all), `-Files a.cpp,b.cpp`, `-Changed` (only files changed against HEAD), `-Strict` (exit
  code 1 if there are findings). Exit code 2 means: a file could not be examined. With the clang-tidy from Visual Studio, use the 64-bit version
  (the script does this itself). The 32-bit version of LLVM 15 crashes on these sources.

## Hooks and access to the game
The game is code of other persons, and it does not change. Each error here is a crash on the computer of a player.

- **Exports before addresses.** Hooks connect to exported names. Install new hooks through `HookExport(...)` (`script_host.h`). If the export is
  missing, only this function is not available. The log tells this. (Older code in `dialog_bindings.cpp` and `quest_bindings.cpp` still calls
  MinHook directly. Change it at the next change there.)
- **Fixed addresses only with a byte check.** Before you install the hook, compare the expected bytes at the location. If they are different, do
  not install the hook, and report it. If more than one location belongs together (example learn costs: seven), install all or none. The checked
  installations are in `docs/compatibility.md`.
- **Exact signature.** Check the calling convention, the number of arguments and the stack cleanup in the assembler code
  (`dumpbin /disasm`). Do not guess. Functions with a special convention get a `__declspec(naked)` frame.
- **Check pointers.** Data from the game can be empty: `Valid(...)`, `ReadableMem(...)`, texts through `SafeStrText(...)`. Report an unexpected
  null pointer 1 time with `NoteNull("<location>")`. Do not ignore it, and do not report it in each frame.
- **Keep SEH small.** Use `__try` only in functions without C++ objects with a destructor (also no local `std::string`), and not nested. Copy texts
  into buffers first.
- **Thread.** Scripts run only in the game thread. `HasHandler`, `CallEvent`, `CallSimple` and `CallOverride` ignore calls from other threads and
  report the first ones in the log. A hook on a frequent path still does its own check with `OnGameThread()` before it prepares the arguments.
  `CallKey` (window procedure) does not have this protection.
- **Do not keep data that belongs to the game.** Do not keep entity pointers across a world change. Copy names that point to game memory.

## Floating point
The engine calculates with the x87 control word `007F` (24 bit). One `fninit` sets `037F`, and the game then calculates incorrectly for the rest
of the session (blurred building textures, measured 2026-11-06).

- No `fninit`, `fldcw`, `_controlfp` in the modkit. Thus AngelScript is built with `CLEAR_FPU_STACK=emms`. `tools/check_no_fninit.ps1` checks the
  DLL.
- Check each new third-party library for this first.
- Script calls go through `ScriptHost::Execute`. Only there, the modkit saves and restores the floating-point state.

## Run time
- Hooks on frequent paths (each frame, each touch, each script call) call scripts only if a mod has the function. They do not allocate memory
  there if this is possible.
- Log lines from these hooks are limited (1 time for each location, the first N lines, or a summary for each time window). A log that fills in
  seconds hides the error that you look for.
- Measurement points and diagnostics are connected to an ini switch that is off by default.

## Script interface
- Each change to the interface increases `G3ASM_SCRIPT_API_VERSION` by 1. `docs/modkit/api-policy.md` tells what is permitted.
- **First measure, then document.** A new function goes into the reference without a label only when its effect in the game is measured. Until
  then, it has the label *Experimental* or *Not checked in the game*.
- Each new function needs: an entry in `docs/modkit/api-reference.md` (or `events.md` or `hooks.md`) and in its English version, a check in the
  self-test (`tests/scripts/selftest.as`) if it runs without the game, and if necessary a small measurement mod in `examples/mods`
  (`enabled=0`).
- Example mods are off by default (`mod.ini`: `enabled=0`) and have the license CC0. Names: `demo_<topic>` for learning examples, `probe_<topic>`
  for measurement mods (English, lower case, with underscores). Mods with game content can have their own name.
- The saved game contains the permanent data that a mod writes into the game. Examples that do this clean up in `OnBeforeSave` and restore the
  condition in `OnAfterSave` and after each load (pattern: `examples/mods/perk_lifesteal`).

## License
Contributions have the license GPL-3.0-or-later with the additional permissions from `LICENSE-EXCEPTIONS.md`. New source files in `src/` and
`tools/` get the SPDX header as the existing files do.
