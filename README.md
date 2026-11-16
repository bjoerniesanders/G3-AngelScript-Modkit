# G3 AngelScript Modkit (G3ASM)

**Deutsch** | [English](README.en.md)

AngelScript-Modding fuer das **Original-Gothic-3**: `g3asm.dll` wird von einer schlanken Ersatz-`dbghelp.dll` (dem Loader, `src/loader`) geladen, startet
AngelScript 2.38 und bindet Spielereignisse, Quests, Inventar, Dialoge und Datei-Patches an Mod-Skripte.

**Voraussetzungen:** Gothic 3 mit **Community Patch 1.75** und **Parallel-Universe-Patch** (PU). `tools/install_loader.ps1` prueft beides und bricht
ab, wenn etwas fehlt (`-Force` baut trotzdem ein, ungetestet); das Modkit schreibt das Ergebnis als Zeile `Voraussetzungen: ...` ins Protokoll.

Wer am Code mitarbeitet: Hausregeln (Hooks, Gleitkomma, Stil, statische Pruefung mit clang-tidy) stehen in [CONTRIBUTING.md](CONTRIBUTING.md).

## Modden

1. **Einbauen** (Spiel beendet), im entpackten Release-Ordner:

       powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_loader.ps1 -Game "<Spielordner>" -BuildDir bin

2. **Mod anlegen:** ein Ordner je Mod, darin `main.as`, z. B. `<Spielordner>\g3asm\mods\hallo\main.as`:

       void main() { print("Hallo aus meiner ersten Mod"); }
       void OnNPCKilled(Entity opfer) { PrintScreen("Getoetet: " + opfer.GetName(), 3, 255, 120, 120); }

3. **Spiel starten.** Ausgaben und Fehler (mit Zeile und Spalte) stehen in `<Spielordner>\g3asm.log`. Wird `main.as` im laufenden Spiel
   gespeichert, ist die neue Fassung nach etwa einer Sekunde aktiv (Hot-Reload).
4. **Reihenfolge und Schalter** in `g3asm\mods\load_order.txt`: `-name` schaltet eine Mod ab, `+name` eine standardmaessig abgeschaltete zu
   (so sind alle Beispiel-Mods ausgeliefert). Version, Abhaengigkeiten und Einstellungen einer Mod stehen in ihrer `mod.ini`.
5. **Weitergeben:** `bin\g3asm\g3asm_packmod.exe release <Spielordner>\g3asm\mods\meine_mod <Zielordner>` baut ein Paket fuer Spieler
   (Aufbau des Union Starters, Laufzeit eingeschlossen). Einzelheiten: [Mods weitergeben](docs/modkit/mod-package.md#mods-weitergeben).

## Mods nutzen (fuer Spieler)

Spieler brauchen das Modkit nicht. Jedes Mod-Paket bringt die Laufzeit (`Script_G3ASM_<Version>.dll`) mit; das Spiel laedt sie selbst.

- **Mit dem Union Starter:** den Paketordner nach `<Spielordner>\MDS\Mods\` legen, im Starter anhaken, installieren. Abwaehlen und erneut
  installieren baut die Mod wieder aus.
- **Ohne Starter:** `ScriptFiles\scripts\*.dll` nach `<Spielordner>\scripts\` kopieren und jede Datei aus `ModFiles\` als naechste freie
  Generation nach `<Spielordner>\Data\` (aus `G3ASM.m0x` wird `G3ASM.mod`, bei einer zweiten Mod `G3ASM.m00` usw.; ebenso fuer andere
  Archive wie `Quests.m0x`).

Beim Start entpackt die Laufzeit jede Mod aus `Data\G3ASM.*` nach `g3asm\mods\<Mod>\`. Ausgaben und Fehler stehen in `g3asm.log` im Spielordner.
Voraussetzung wie oben: Community Patch 1.75 und Parallel-Universe-Patch.

Was eine Mod kann:
- auf Spielereignisse reagieren (Kills, Items, Zauber, Schaden, Bereiche, Quest-Status, Speichern/Laden, gesprochene Saetze, Tasten, jedes Bild);
- ausgewaehlte Spielfunktionen mit `Override_...` und `Super()` aendern oder abbrechen (Schaden, Dialogoptionen, Lernkosten, Monsterschaden,
  Spielskripte und KI-Zustaende nach Namen, Konsole, Haendlerware);
- Attribute, Inventar, Item-Qualitaet, Talente, Eigenschaften, Quests und Dialoge lesen und aendern, Vorlagen klonen, NPCs erzeugen;
- ueber dem HUD Text und Bilder zeichnen;
- Texte des Spiels per `stringtable.ini` ersetzen, Quest- und Info-Dateien per Unified-Diff patchen (`ScriptPatches`) und eigene Dateien als
  Mod-Archiv packen (`PackMod`);
- mit anderen Mods zusammenarbeiten (`IsModActive`, `CallMod`, `SendModMessage`).

Anleitung und Referenz: [docs/modkit/index.md](docs/modkit/index.md) (Einstieg, Schnellstart), [Scripting-Guide](docs/modkit/scripting-guide.md),
[Ereignisse](docs/modkit/events.md), [API-Referenz](docs/modkit/api-reference.md), [Beispiele](docs/modkit/examples.md). Die Beispiel-Mods unter
`examples/mods` heissen `demo_*` (Lern-Beispiele), `probe_*` (Messmods) oder tragen einen eigenen Namen (kleine Mods mit Spielinhalt).

## Grenzen

Einzelheiten in [Grenzen und Stand](docs/modkit/limitations.md).

**Grundsaetzlich:** Die Spiellogik von Gothic 3 liegt in nativen DLLs. Eine Mod greift nur an den Stellen ein, die das Modkit freilegt.

**Geht nicht** (im Spiel gemessen oder im Spielcode untersucht):
- Einfarbige Flaechen zeichnen: Die Rechteck-Funktionen von GUI und Renderer zeichnen im Spiel nichts; es gehen nur Text und Bilder.
- Eigene Talente vom Spiel lernen lassen: `LearnSkill` liefert fuer ein geklontes Talent `false`; die Wirkung stellt die Mod selbst her.
- Verweise auf andere Vorlagen per `SetProperty` als Text setzen (elf Schreibweisen probiert); dafuer gibt es `SetTemplateRef`/`SetEntityRef`.
- Ein allgemeiner Treffer-Hook vor der Schadensberechnung: Die Game.dll exportiert dafuer keinen Einstiegspunkt.
- Datei-Patches (`ScriptPatches`) bei Spielern: Die Laufzeit startet als Script-DLL erst, nachdem das Spiel seine Dateien erfasst hat. Mod-Pakete
  liefern Spieldateien deshalb als Archiv (`ModFiles\<Volume>.m0x`); Datei-Patches wirken nur mit dem Loader.
- Spielstaende ohne die Mod, wenn sie dauerhafte Zustaende enthalten (Items aus geklonten Vorlagen, Talente): Ein solcher Spielstand stuerzte ohne
  die Mod beim Oeffnen des Charakterbogens ab. Mods raeumen das deshalb in `OnBeforeSave` weg und stellen es danach wieder her (Muster: `perk_lifesteal`).

**Gibt es nicht** (nicht gebaut oder noch nicht untersucht, nicht als unmoeglich erwiesen):
- Animationen: abspielen, abfangen, eigene einbinden.
- Welt bearbeiten (Objekte setzen), eigene Modelle.
- Eigene Bedienelemente mit Eingabe, ein Einstellungsmenue im Spiel.
- Das Questlog direkt beschreiben (Eintraege entstehen ueber Dialogzeilen) und die Bedingungslisten eigener Infos (Items, Reputation) setzen;
  Bedingungen prueft das Skript selbst.
- Datei-Patches auf komprimierte Archiveintraege (`Strings.pak`) und Binaerdateien; Datei-Patches patchen immer die Fassung aus dem `.pak`, auch
  wenn ein anderes Mod-Archiv dieselbe Datei ersetzt.
- Die ausfuehrliche Doku auf Englisch (es gibt nur die [englische README](README.en.md)).

**Experimentell** (laeuft, Wirkung nicht vollstaendig gemessen, darf sich noch aendern): `HurtHitPoints` (senkt Lebenspunkte, toetet nicht),
`SetProperty` (nur Textwerte), `GenerateTradeGoods` (die Ware aendert sich im Test nicht), `Override_CombatMove`.

**Nicht getestet** (Einzelheiten in [Kompatibilitaet](docs/compatibility.md)):
- GOG- und CD/DVD-Version; ohne DXVK nur auf einer Installation. Ohne Community Patch 1.75 oder Parallel-Universe-Patch nicht unterstuetzt.
- Zusammen mit anderen Script-Plugins, die eigene Hooks setzen (z. B. CombatFix). `Script_MonsterDamage.dll` und
  `Script_LearningThresholdTweaks.dll` aendern dieselben Stellen wie das Modkit; sind sie installiert, bleiben `Override_MonsterDamage` bzw.
  `Override_LearnCost` aus.
- Auf einer Steam-Installation mit vielen Fremd-Mods stuerzte ein Spielstand mit `perk_lifesteal` beim Oeffnen des Charakterbogens ab, auch ohne
  Mods; die Ursache ist offen. Auf einer sauberen Installation tritt das nicht auf.
- Im Spiel nie ausgeloest: KI-Routinen (`AI_ROUTINE`), der Teiler 2 bei `Override_MonsterDamage` (beschworene Angreifer) und die verdoppelten
  Lernkosten mit "Alternative Balance". Abhaengigkeiten in der `mod.ini` sind nur ohne Spiel getestet; Datei-Patches auf `.info`-Dateien
  sind ungeprueft.
- Mod-Pakete ueber den Union Starter: im Spiel geprueft; zwei Mods mit verschiedenen Laufzeit-Versionen noch nicht.

## Aufbau
| Ordner | Inhalt |
|---|---|
| `src/` | DLL-Quellen (`g3asm_main`, `script_host`, `*_bindings`, `game_hooks`, `file_patches`, `packed_mods`), `script_dll.cpp` (Laufzeit als `Script_G3ASM_<API>.dll`), `src/loader/` (Loader = `dbghelp.dll`) |
| `tests/` | `test_runner.cpp` (g3asm_test), `scripts/selftest.as`, `mods/` (Ordner-Test), `sb_stub/` (Ersatz-SharedBase.dll fuer Tests ohne Spieldateien) |
| `examples/` | Beispiel-Mods (`mods/`) und Mini-Mod (`scripts/main.as`) |
| `docs/` | `modkit/` (Modder-Doku), `compatibility.md` (gepruefte Installationen) |
| `tools/` | `g3pak_read.py` (Archive lesen, auch komprimierte Eintraege), `reference_from_log.py` (Referenz aus dem Dump), `install_loader.ps1` (Ein-/Ausbau), `package_release.ps1` (Release-Zip), `check_no_fninit.ps1`, `run_clang_tidy.ps1`, `packmod_main.cpp` (`g3asm_packmod`), `graphify_modding_sync.ps1` |
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
    python -I tests\packmod_release.py g3asm_test.exe <SharedBase.dll> g3asm_packmod.exe   (Paket fuer Spieler bauen, einspielen, starten)
Ohne Spieldateien: als `<SharedBase.dll>` die Ersatz-DLL `build32\sbstub\SharedBase.dll` nehmen (Ziel `g3asm_sb_stub`), als `<dbghelp_orig.dll>`
die System-DLL `%WINDIR%\SysWOW64\dbghelp.dll`. Die Ersatz-DLL hat nur die Exporte, die das Modkit fuer bCString und die Mathe-Typen braucht,
und rechnet nicht bitgenau wie das Original; Messungen am Verhalten des Originals nur mit der echten SharedBase.dll.

## CI und Release (GitHub Actions)
- `.github/workflows/ci.yml` (jeder Push auf `main`, Pull-Requests, von Hand): baut 32 Bit mit MSVC, prueft, dass `g3asm.dll` kein `fninit` enthaelt
  (`tools/check_no_fninit.ps1`, Schutz fuer den FPU-Fix), fuehrt Selbsttest, Test-Mods, Kompilierpruefung der Beispiel-Mods und Loader-Test mit der Ersatz-SharedBase aus, dazu `tests/packmod_generations.py`, `tests/file_patches_overlay.py`, `tests/packed_mods.py` und `tests/packmod_release.py`, und packt probeweise ein Zip (Artefakt, 7 Tage).
- `.github/workflows/release.yml`: beim Push eines Tags `v*` dasselbe plus GitHub-Release mit dem Zip:
      git tag v0.46.1-beta.1
      git push origin v0.46.1-beta.1
  Tags mit Bindestrich (`-alpha.N`, `-beta.N`) werden als Vorabversion veroeffentlicht; Namensschema siehe [Versionsregeln](docs/modkit/api-policy.md).
- Paket lokal: `powershell -NoProfile -File tools\package_release.ps1 -BuildDir build32 -Version v0.46.1` (Ergebnis in `dist\`). Inhalt: Loader, `g3asm.dll`,
  `g3asm_packmod.exe`, `tools\install_loader.ps1`, Beispiel-Mods (standardmaessig aus), Doku, `INSTALL.txt`; keine Spieldateien.
- Nicht in der CI: alles, was das echte Spiel braucht (Messungen mit der Original-`SharedBase.dll`, Spieltests). Spieldateien gehoeren nicht ins Repo.

## Einbau (Loader, eigenstaendig)
    powershell -NoProfile -File tools\install_loader.ps1 -Game <Spielordner> [-BuildDir build32]
    powershell -NoProfile -File tools\install_loader.ps1 -Game <Spielordner> -Uninstall
Der Installer legt die vorhandene `dbghelp.dll` des Spiels als `dbghelp_orig.dll` beiseite (der Loader leitet `MiniDumpWriteDump` dorthin weiter, die Absturzberichte
des Spiels funktionieren weiter), stellt den Loader als `dbghelp.dll` hin und kopiert `g3asm.dll`, eine `g3asm.ini` und eine Mini-Mod. Sicherung unter
`<Spiel>\g3asm_backup\loader_<Zeit>\`. Protokolle: `g3asm_loader.log` (Loader, Absturzkandidaten) und `g3asm.log` (Modkit). Einstellungen in `g3asm.ini`
(`[g3asm] enable/script/hotreload/mods`, `[loader] crashlog`).

**Frueherer Name:** Bis API 46 hiess das Projekt OG3-Patch (`og3_patch.dll`, `og3_patch.ini`, Ordner `og3\`, ini-Abschnitt `[patch]`). Der Installer zieht eine solche Installation beim
Einbau um (Ordner und ini werden umbenannt, die alte DLL entfernt) und findet beim Ausbau auch die alten Sicherungen.

## Lizenz
GPL-3.0-or-later, siehe `LICENSE`, mit zwei Zusatzerlaubnissen nach Abschnitt 7 in `LICENSE-EXCEPTIONS.md`: Das Modkit darf mit den Programmdateien
von Gothic 3 zusammen weitergegeben werden, und Mods, die nur die Skript-Schnittstelle nutzen, duerfen unter jeder Lizenz stehen. Die Beispiel-Mods in
`examples/` sind gemeinfrei (CC0 1.0, `examples/LICENSE`). Bis zum Wechsel (vor dem ersten Release) stand das Repo unter MIT.

AngelScript (zlib-Lizenz) und MinHook (BSD-2-Clause) werden beim Bauen geladen und liegen nicht im Repo. Das Projekt enthaelt keine Spieldaten. Speicher-Layouts
der Engine sind dem [Gothic 3 SDK](https://github.com/georgeto/gothic3sdk) (Georgeto, GPL-3.0-or-later) entnommen; Code des SDK enthaelt das Repo nicht.

## Danksagung

An die Autoren des Gothic-3-SDK (gothic3sdk), dessen Header die Klassen, Signaturen und Eigenschaften der Engine erschlossen haben, an das Team des
Community Patch, an die Macher des Gothic 3 Mod Starters, deren Arbeit das Format der Mod-Archive offengelegt hat, und an die Gothic-Community.
Das Modkit nutzt AngelScript (Andreas Joensson, AngelCode, zlib-Lizenz) und MinHook (Tsuda Kageyu, BSD-2-Lizenz). Vorbild fuer den Aufbau war das Modkit
des Gothic-1-Remakes.

Gothic 3 ist eine Marke der jeweiligen Rechteinhaber. G3ASM ist ein inoffizielles Fan-Projekt und enthaelt keine Dateien des Spiels.
