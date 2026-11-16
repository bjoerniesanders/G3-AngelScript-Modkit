# G3 AngelScript Modkit (G3ASM)

**Deutsch** | [English](README.en.md)

AngelScript-Modding für das **Original-Gothic-3**: `g3asm.dll` wird von einer schlanken Ersatz-`dbghelp.dll` (dem Loader, `src/loader`) geladen, startet
AngelScript 2.38 und bindet Spielereignisse, Quests, Inventar, Dialoge und Datei-Patches an Mod-Skripte.

**Voraussetzungen:** Gothic 3 mit **Community Patch 1.75** und **Parallel-Universe-Patch** (PU). `tools/install_loader.ps1` prüft beides und bricht
ab, wenn etwas fehlt (`-Force` baut trotzdem ein, ungetestet); das Modkit schreibt das Ergebnis als Zeile `Voraussetzungen: ...` ins Protokoll.

Wer am Code mitarbeitet: Hausregeln (Hooks, Gleitkomma, Stil, statische Prüfung mit clang-tidy) stehen in [CONTRIBUTING.md](CONTRIBUTING.md).

## Modden

1. **Einbauen** (Spiel beendet), im entpackten Release-Ordner:

       powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_loader.ps1 -Game "<Spielordner>" -BuildDir bin

2. **Mod anlegen:** ein Ordner je Mod, darin `main.as`, z. B. `<Spielordner>\g3asm\mods\hallo\main.as`:

       void main() { print("Hallo aus meiner ersten Mod"); }
       void OnNPCKilled(Entity opfer) { PrintScreen("Getötet: " + opfer.GetName(), 3, 255, 120, 120); }

3. **Spiel starten.** Ausgaben und Fehler (mit Zeile und Spalte) stehen in `<Spielordner>\g3asm.log`. Wird `main.as` im laufenden Spiel
   gespeichert, ist die neue Fassung nach etwa einer Sekunde aktiv (Hot-Reload).
4. **Reihenfolge und Schalter** in `g3asm\mods\load_order.txt`: `-name` schaltet eine Mod ab, `+name` eine standardmäßig abgeschaltete zu
   (so sind alle Beispiel-Mods ausgeliefert). Version, Abhängigkeiten und Einstellungen einer Mod stehen in ihrer `mod.ini`.
5. **Weitergeben:** `bin\g3asm\g3asm_packmod.exe release <Spielordner>\g3asm\mods\meine_mod <Zielordner>` baut ein Paket für Spieler
   (Aufbau des Union Starters, Laufzeit eingeschlossen, mit `G3ASM-LICENSE.txt` für die GPL-Laufzeit). Einzelheiten: [Mods weitergeben](docs/modkit/mod-package.md#mods-weitergeben).

## Mods nutzen (für Spieler)

Spieler brauchen das Modkit nicht. Jedes Mod-Paket bringt die Laufzeit (`Script_G3ASM_<Version>.dll`) mit; das Spiel lädt sie selbst.

- **Mit dem Union Starter:** den Paketordner nach `<Spielordner>\MDS\Mods\` legen, im Starter anhaken, installieren. Abwählen und erneut
  installieren baut die Mod wieder aus.
- **Ohne Starter:** `ScriptFiles\scripts\*.dll` nach `<Spielordner>\scripts\` kopieren und jede Datei aus `ModFiles\` als nächste freie
  Generation nach `<Spielordner>\Data\` (aus `G3ASM.m0x` wird `G3ASM.mod`, bei einer zweiten Mod `G3ASM.m00` usw.; ebenso für andere
  Archive wie `Quests.m0x`).

Beim Start entpackt die Laufzeit jede Mod aus `Data\G3ASM.*` nach `g3asm\mods\<Mod>\`. Ausgaben und Fehler stehen in `g3asm.log` im Spielordner.
Voraussetzung wie oben: Community Patch 1.75 und Parallel-Universe-Patch.

Was eine Mod kann:
- auf Spielereignisse reagieren (Kills, Items, Zauber, Schaden, Bereiche, Quest-Status, Speichern/Laden, gesprochene Sätze, Tasten, jedes Bild);
- ausgewählte Spielfunktionen mit `Override_...` und `Super()` ändern oder abbrechen (Schaden, Dialogoptionen, Lernkosten, Monsterschaden,
  Spielskripte und KI-Zustände nach Namen, Konsole, Händlerware);
- Attribute, Inventar, Item-Qualität, Talente, Eigenschaften, Quests und Dialoge lesen und ändern, Vorlagen klonen, NPCs erzeugen;
- über dem HUD Text und Bilder zeichnen;
- Texte des Spiels per `stringtable.ini` ersetzen, Quest- und Info-Dateien per Unified-Diff patchen (`ScriptPatches`) und eigene Dateien als
  Mod-Archiv packen (`PackMod`);
- mit anderen Mods zusammenarbeiten (`IsModActive`, `CallMod`, `SendModMessage`).

Anleitung und Referenz: [Erste Mod Schritt für Schritt](docs/modkit/erste-mod.md), [docs/modkit/index.md](docs/modkit/index.md) (Überblick), [Scripting-Guide](docs/modkit/scripting-guide.md),
[Ereignisse](docs/modkit/events.md), [API-Referenz](docs/modkit/api-reference.md), [Beispiele](docs/modkit/examples.md). Die Beispiel-Mods unter
`examples/mods` heißen `demo_*` (Lern-Beispiele), `probe_*` (Messmods) oder tragen einen eigenen Namen (kleine Mods mit Spielinhalt).

## Grenzen

**Grundsätzlich:** Die Spiellogik von Gothic 3 liegt in nativen DLLs. Eine Mod greift nur an den Stellen ein, die das Modkit freilegt.
Die vollständige Liste steht unter [Grenzen](docs/modkit/limitations.md).

**Geht nicht** (im Spiel gemessen oder im Spielcode untersucht):
- Einfarbige Flächen zeichnen; es gehen nur Text und Bilder.
- Eigene Talente vom Spiel lernen lassen; die Wirkung stellt die Mod selbst her.
- Ein Treffer-Hook vor der Schadensberechnung; eingreifen lässt sich über das Spielskript `OnHit`, `Override_MonsterDamage` und `Override_Damage`.
- Das Questlog direkt beschreiben und Bedingungen eigener Infos setzen; Einträge entstehen über Dialogzeilen, Bedingungen prüft das Skript.
- Datei-Patches (`ScriptPatches`) bei Spielern: Mod-Pakete liefern geänderte Spieldateien als Archiv (`ModFiles\<Volume>.m0x`).

**Gibt es nicht** (nicht gebaut oder nicht untersucht):
- Animationen, die Welt bearbeiten, eigene Modelle.
- Eigene Bedienelemente mit Eingabe, ein Einstellungsmenü im Spiel.
- Datei-Patches auf komprimierte Archiveinträge (`Strings.pak`) und Binärdateien.
- Die ausführliche Doku auf Englisch (es gibt die [englische README](README.en.md)).

**Experimentell:** `HurtHitPoints`, `SetProperty`, `GenerateTradeGoods`, `Override_CombatMove`.

**Nicht getestet** (Einzelheiten in [Kompatibilität](docs/compatibility.md)):
- GOG- und CD/DVD-Version. Ohne Community Patch 1.75 oder Parallel-Universe-Patch nicht unterstützt.
- Zusammen mit anderen Script-Plugins, die eigene Hooks setzen (z. B. CombatFix). Sind `Script_MonsterDamage.dll` oder
  `Script_LearningThresholdTweaks.dll` installiert, bleiben `Override_MonsterDamage` bzw. `Override_LearnCost` aus.
- Zwei Mods mit verschiedenen Laufzeit-Versionen beim Spieler.

**Wichtig für Mods mit dauerhaften Änderungen** (Items aus geklonten Vorlagen, Talente): Ein solcher Spielstand stürzte ohne die Mod ab. Mods
räumen das deshalb in `OnBeforeSave` weg und stellen es danach wieder her (Muster: `perk_lifesteal`).

## Aufbau
| Ordner | Inhalt |
|---|---|
| `src/` | DLL-Quellen (`g3asm_main`, `script_host`, `*_bindings`, `game_hooks`, `file_patches`, `packed_mods`), `script_dll.cpp` (Laufzeit als `Script_G3ASM_<API>.dll`), `src/loader/` (Loader = `dbghelp.dll`) |
| `tests/` | `test_runner.cpp` (g3asm_test), `scripts/selftest.as`, `mods/` (Ordner-Test), `sb_stub/` (Ersatz-SharedBase.dll für Tests ohne Spieldateien) |
| `examples/` | Beispiel-Mods (`mods/`) und Mini-Mod (`scripts/main.as`) |
| `docs/` | `modkit/` (Modder-Doku), `compatibility.md` (geprüfte Installationen) |
| `tools/` | `g3pak_read.py` (Archive lesen, auch komprimierte Einträge), `reference_from_log.py` (Referenz aus dem Dump), `install_loader.ps1` (Ein-/Ausbau), `package_release.ps1` (Release-Zip), `check_no_fninit.ps1`, `run_clang_tidy.ps1`, `packmod_main.cpp` (`g3asm_packmod`), `graphify_modding_sync.ps1` |
| (beim Bauen geladen) | AngelScript (zlib) und MinHook: `G3ASM_ANGELSCRIPT_DIR` / `G3ASM_MINHOOK_DIR` oder automatischer Download |

## Bauen (32 Bit)
    cmake -S . -B build32 -A Win32
    cmake --build build32 --config Release --target g3asm g3asm_script g3asm_test g3asm_packmod g3asm_loader g3asm_loader_test g3asm_sb_stub
Ausgabe: `build32/g3asm/` (g3asm.dll, Script_G3ASM_<API>.dll, Tests, g3asm_packmod) und `build32/loader/dbghelp.dll` (Loader).

## Test
    g3asm_test.exe <SharedBase.dll> tests\scripts\selftest.as
    g3asm_test.exe <SharedBase.dll> tests\mods
    powershell -NoProfile -File tests\run_loader_test.ps1 -SharedBase <SharedBase.dll> -DbghelpOrig <dbghelp_orig.dll>   (Loader in einem Wegwerf-Ordner)
    python -I tests\file_patches_overlay.py g3asm_test.exe <SharedBase.dll>   (Datei-Patches in einem Wegwerf-Ordner)
    python -I tests\packed_mods.py g3asm_test.exe <SharedBase.dll> g3asm_packmod.exe   (gepackte Mods entpacken, aktuell halten, entfernen)
    python -I tests\packmod_release.py g3asm_test.exe <SharedBase.dll> g3asm_packmod.exe   (Paket für Spieler bauen, einspielen, starten)
Ohne Spieldateien: als `<SharedBase.dll>` die Ersatz-DLL `build32\sbstub\SharedBase.dll` nehmen (Ziel `g3asm_sb_stub`), als `<dbghelp_orig.dll>`
die System-DLL `%WINDIR%\SysWOW64\dbghelp.dll`. Die Ersatz-DLL hat nur die Exporte, die das Modkit für bCString und die Mathe-Typen braucht,
und rechnet nicht bitgenau wie das Original; Messungen am Verhalten des Originals nur mit der echten SharedBase.dll.

## CI und Release (GitHub Actions)
- `.github/workflows/ci.yml` (jeder Push auf `main`, Pull-Requests, von Hand): baut 32 Bit mit MSVC, prüft, dass `g3asm.dll` kein `fninit` enthält
  (`tools/check_no_fninit.ps1`, Schutz für den FPU-Fix), führt Selbsttest, Test-Mods, Kompilierprüfung der Beispiel-Mods und Loader-Test mit der Ersatz-SharedBase aus, dazu `tests/packmod_generations.py`, `tests/file_patches_overlay.py`, `tests/packed_mods.py` und `tests/packmod_release.py`, und packt probeweise ein Zip (Artefakt, 7 Tage).
- `.github/workflows/release.yml`: beim Push eines Tags `v*` dasselbe plus GitHub-Release mit dem Zip:
      git tag v0.46.1-beta.1
      git push origin v0.46.1-beta.1
  Tags mit Bindestrich (`-alpha.N`, `-beta.N`) werden als Vorabversion veröffentlicht; Namensschema siehe [Versionsregeln](docs/modkit/api-policy.md).
- Paket lokal: `powershell -NoProfile -File tools\package_release.ps1 -BuildDir build32 -Version v0.46.1` (Ergebnis in `dist\`). Inhalt: Loader, `g3asm.dll`,
  `Script_G3ASM_<API>.dll` (Laufzeit für Mod-Pakete), `g3asm_packmod.exe`, `tools\install_loader.ps1`, `tools\g3pak_read.py`, Beispiel-Mods (standardmäßig aus), Doku, `INSTALL.txt`; keine Spieldateien.
- Nicht in der CI: alles, was das echte Spiel braucht (Messungen mit der Original-`SharedBase.dll`, Spieltests). Spieldateien gehören nicht ins Repo.

## Einbau (Loader, eigenständig)
    powershell -NoProfile -File tools\install_loader.ps1 -Game <Spielordner> [-BuildDir build32]
    powershell -NoProfile -File tools\install_loader.ps1 -Game <Spielordner> -Uninstall
Der Installer legt die vorhandene `dbghelp.dll` des Spiels als `dbghelp_orig.dll` beiseite (der Loader leitet `MiniDumpWriteDump` dorthin weiter, die Absturzberichte
des Spiels funktionieren weiter), stellt den Loader als `dbghelp.dll` hin und kopiert `g3asm.dll`, eine `g3asm.ini` und eine Mini-Mod. Sicherung unter
`<Spiel>\g3asm_backup\loader_<Zeit>\`. Protokolle: `g3asm_loader.log` (Loader, Absturzkandidaten) und `g3asm.log` (Modkit). Einstellungen in `g3asm.ini`
(`[g3asm] enable/script/hotreload/mods`, `[loader] crashlog`).

## Lizenz
GPL-3.0-or-later, siehe `LICENSE`, mit zwei Zusatzerlaubnissen nach Abschnitt 7 in `LICENSE-EXCEPTIONS.md`: Das Modkit darf mit den Programmdateien
von Gothic 3 zusammen weitergegeben werden, und Mods, die nur die Skript-Schnittstelle nutzen, dürfen unter jeder Lizenz stehen. Die Beispiel-Mods in
`examples/` sind gemeinfrei (CC0 1.0, `examples/LICENSE`). Bis zum Wechsel (vor dem ersten Release) stand das Repo unter MIT.

AngelScript (zlib-Lizenz) und MinHook (BSD-2-Clause) werden beim Bauen geladen und liegen nicht im Repo. Das Projekt enthält keine Spieldaten. Speicher-Layouts
der Engine sind dem [Gothic 3 SDK](https://github.com/georgeto/gothic3sdk) (Georgeto, GPL-3.0-or-later) entnommen; Code des SDK enthält das Repo nicht.

## Danksagung

An die Autoren des Gothic-3-SDK (gothic3sdk), dessen Header die Klassen, Signaturen und Eigenschaften der Engine erschlossen haben, an das Team des
Community Patch, an die Macher des Gothic 3 Mod Starters, deren Arbeit das Format der Mod-Archive offengelegt hat, und an die Gothic-Community.
Das Modkit nutzt AngelScript (Andreas Jönsson, AngelCode, zlib-Lizenz) und MinHook (Tsuda Kageyu, BSD-2-Lizenz). Vorbild für den Aufbau war das Modkit
des Gothic-1-Remakes.

Gothic 3 ist eine Marke der jeweiligen Rechteinhaber. G3ASM ist ein inoffizielles Fan-Projekt und enthält keine Dateien des Spiels.
