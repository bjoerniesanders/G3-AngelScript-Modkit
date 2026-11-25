# Mitarbeiten

**Deutsch** | [English](CONTRIBUTING.en.md)

Kurze Hausregeln für Änderungen an diesem Repository. Sie beschreiben, wie der Code heute gebaut ist; wer davon abweicht, schreibt den Grund dazu.
Die Regeln für die Skript-Schnittstelle stehen in [docs/modkit/api-policy.md](docs/modkit/api-policy.md), die Doku für Modder unter [docs/modkit](docs/modkit/index.md).

## Aufbau
| Ordner | Inhalt |
|---|---|
| `src/` | DLL-Quellen (`g3asm_main`, `script_host`, `*_bindings`, `game_hooks`, `file_patches`, `packed_mods`), `dev_pipe.cpp` (Dev-Pipe, nur in `g3asm.dll`), `script_dll.cpp` (Laufzeit als `Script_G3ASM_<API>.dll`), `src/loader/` (Loader = `dbghelp.dll`) |
| `tests/` | `test_runner.cpp` (g3asm_test), `scripts/selftest.as`, `mods/` (Ordner-Test), `sb_stub/` (Ersatz-SharedBase.dll für Tests ohne Spieldateien) |
| `examples/` | Beispiel-Mods (`mods/`) und Mini-Mod (`scripts/main.as`) |
| `docs/` | `modkit/` (Modder-Doku, deutsch), `compatibility.md` (geprüfte Installationen), `en/` (englische Fassungen) |
| `tools/` | `g3pak_read.py` (Archive lesen, auch komprimierte Einträge), `reference_from_log.py` (Referenz aus dem Dump), `install_loader.ps1` (Ein-/Ausbau), `package_release.ps1` (Release-Zip), `check_no_fninit.ps1`, `run_clang_tidy.ps1`, `packmod_main.cpp` (`g3asm_packmod`), `dev_main.cpp` (`g3asm_dev`), `graphify_modding_sync.ps1` |
| (beim Bauen geladen) | AngelScript (zlib) und MinHook: `G3ASM_ANGELSCRIPT_DIR` / `G3ASM_MINHOOK_DIR` oder automatischer Download |

## Bauen (32 Bit)
    cmake -S . -B build32 -A Win32
    cmake --build build32 --config Release --target g3asm g3asm_script g3asm_test g3asm_packmod g3asm_dev g3asm_cli g3asm_loader g3asm_loader_test g3asm_sb_stub
Ausgabe: `build32/g3asm/` (g3asm.dll, Script_G3ASM_<API>.dll, Tests, g3asm_packmod, g3asm_dev, Kommandozeile g3asm.exe) und `build32/loader/dbghelp.dll` (Loader).

## Test
    g3asm_test.exe <SharedBase.dll> tests\scripts\selftest.as
    g3asm_test.exe <SharedBase.dll> tests\mods
    powershell -NoProfile -File tests\run_loader_test.ps1 -SharedBase <SharedBase.dll> -DbghelpOrig <dbghelp_orig.dll>   (Loader in einem Wegwerf-Ordner)
    python -I tests\file_patches_overlay.py g3asm_test.exe <SharedBase.dll>   (Datei-Patches in einem Wegwerf-Ordner)
    python -I tests\packed_mods.py g3asm_test.exe <SharedBase.dll> g3asm_packmod.exe   (gepackte Mods entpacken, aktuell halten, entfernen)
    python -I tests\packmod_release.py g3asm_test.exe <SharedBase.dll> g3asm_packmod.exe   (Paket für Spieler bauen, einspielen, starten)
    python -I tests\dev_pipe.py g3asm_test.exe <SharedBase.dll> g3asm_dev.exe   (Dev-Pipe: Ausgabe, Fehler, 5-s-Abbruch)
    python -I tests\keep_globals.py g3asm_test.exe <SharedBase.dll> g3asm_dev.exe   (Hot-Reload behält die Globalen)
    python -I tests\cli.py g3asm.exe <SharedBase.dll>   (Kommandozeile: new, check, pack, release, log)
Ohne Spieldateien: als `<SharedBase.dll>` die Ersatz-DLL `build32\sbstub\SharedBase.dll` nehmen (Ziel `g3asm_sb_stub`), als `<dbghelp_orig.dll>`
die System-DLL `%WINDIR%\SysWOW64\dbghelp.dll`. Die Ersatz-DLL hat nur die Exporte, die das Modkit für bCString und die Mathe-Typen braucht,
und rechnet nicht bitgenau wie das Original; Messungen am Verhalten des Originals nur mit der echten SharedBase.dll.

## CI und Release (GitHub Actions)
- `.github/workflows/ci.yml` (jeder Push auf `main`, Pull-Requests, von Hand): baut 32 Bit mit MSVC, prüft, dass `g3asm.dll` kein `fninit` enthält
  (`tools/check_no_fninit.ps1`, Schutz für den FPU-Fix), führt Selbsttest, Test-Mods, Kompilierprüfung der Beispiel-Mods und Loader-Test mit der Ersatz-SharedBase aus, dazu `tests/packmod_generations.py`, `tests/file_patches_overlay.py`, `tests/packed_mods.py`, `tests/packmod_release.py`, `tests/dev_pipe.py`, `tests/keep_globals.py` und `tests/cli.py`, und packt probeweise ein Zip (Artefakt, 7 Tage).
- `.github/workflows/release.yml`: beim Push eines Tags `v*` dasselbe plus GitHub-Release mit dem Zip:
      git tag -a v0.46.0-beta.1 -m "G3 AngelScript Modkit v0.46.0-beta.1"
      git push origin v0.46.0-beta.1
  Tags mit Bindestrich (`-alpha.N`, `-beta.N`) werden als Vorabversion veröffentlicht; Namensschema siehe [Versionsregeln](docs/modkit/api-policy.md). Den Titel setzt der Workflow („G3 AngelScript Modkit <Tag>“); die Release-Notizen danach auf GitHub von Hand schreiben, dort gehört der Projektstand hin.
- Paket lokal: `powershell -NoProfile -File tools\package_release.ps1 -BuildDir build32 -Version v0.46.0-beta.1` (Ergebnis in `dist\`). Inhalt: Loader, `g3asm.dll`,
  `Script_G3ASM_<API>.dll` (Laufzeit für Mod-Pakete), `g3asm_packmod.exe`, `g3asm_dev.exe`, `tools\install_loader.ps1`, `tools\g3pak_read.py`, Beispiel-Mods (standardmäßig aus), Doku, `INSTALL.txt`; keine Spieldateien.
- Nicht in der CI: alles, was das echte Spiel braucht (Messungen mit der Original-`SharedBase.dll`, Spieltests). Spieldateien gehören nicht ins Repo.

## Einbau aus dem eigenen Build
    powershell -NoProfile -File tools\install_loader.ps1 -Game <Spielordner> [-BuildDir build32]
    powershell -NoProfile -File tools\install_loader.ps1 -Game <Spielordner> -Uninstall
Der Installer legt die vorhandene `dbghelp.dll` des Spiels als `dbghelp_orig.dll` beiseite (der Loader leitet `MiniDumpWriteDump` dorthin weiter, die Absturzberichte
des Spiels funktionieren weiter), stellt den Loader als `dbghelp.dll` hin und kopiert `g3asm.dll`, eine `g3asm.ini` und eine Mini-Mod. Sicherung unter
`<Spiel>\g3asm_backup\loader_<Zeit>\`. Protokolle: `g3asm_loader.log` (Loader, Absturzkandidaten) und `g3asm.log` (Modkit). Einstellungen in `g3asm.ini`
(`[g3asm] enable/script/hotreload/mods`, `[loader] crashlog`, `[dev] pipe`).

## Dev-Pipe (Code im laufenden Spiel ausführen)
Mit der Dev-Pipe führst du AngelScript im laufenden Spiel aus, ohne Mess-Mod und ohne Neustart. Sie ist für Entwickler des Modkits und für
Werkzeuge gedacht, nicht für Mods.

1. In `<Spielordner>\g3asm.ini` diesen Abschnitt ergänzen:

       [dev]
       pipe=1

2. Das Spiel starten. In `g3asm.log` steht `Dev-Pipe bereit: \.\pipe\g3asm_dev`.
3. Code schicken:

       g3asm_dev "print(GetHero().GetName());"
       g3asm_dev -m meine_mod "print(\"Zaehler: \" + zaehler);"
       g3asm_dev -f probe.as

   Ergebnis (eine Zeile JSON):

       {"ok":true,"out":"[dev] Held
","ms":0.31}

| Schalter | Bedeutung |
|---|---|
| `-m <Mod>` | Code im Modul dieser Mod ausführen; ihre globalen Variablen und Funktionen sind sichtbar |
| `-f <Datei>` | Code aus einer Datei (UTF-8) |
| `-w <Sekunden>` | so lange auf die Pipe warten (Standard 5) |

Exit-Code: 0 = ok, 1 = Fehler im Skript (Meldung in `out`), 2 = keine Verbindung.

Der Code ist der Rumpf einer Funktion `void dev()`. Er läuft im nächsten Bild auf dem Spiel-Thread, mit demselben 5-Sekunden-Abbruch wie
jedes Skript. `out` enthält alle Protokollzeilen des Aufrufs (`print`, Übersetzungsfehler mit Zeile und Spalte, Skript-Ausnahmen).

**Achtung:** Wer die Pipe öffnen kann, führt Code im Spiel aus. Die Pipe nimmt deshalb nur lokale Verbindungen des Benutzers an, unter dem
das Spiel läuft, und ist standardmäßig aus. Die Script-DLL für Spieler (`Script_G3ASM_<API>.dll`) enthält die Pipe nicht.

## Vor jedem Commit
1. [Bauen](#bauen-32-bit) und die [Testläufe](#test): Selbsttest, `tests\mods`, Kompilierprüfung `examples\mods`
   (dort ist genau der eine Fehler "Override-Kette über mehrere Mods" erwartet), Loader-Test, `tests\file_patches_overlay.py`, `tests\packed_mods.py`, `tests\packmod_release.py`, `tests\dev_pipe.py`, `tests\keep_globals.py` und `tests\cli.py`.
2. `powershell -NoProfile -File tools\check_no_fninit.ps1 -Dll <Build>\g3asm\g3asm.dll` und dasselbe für `Script_G3ASM_<API>.dll` (siehe Gleitkomma).
3. `powershell -NoProfile -File tools\run_clang_tidy.ps1 -BuildDir <Build> -Changed -Strict` (siehe Statische Prüfung).
4. Was das Verhalten im Spiel ändert, im Spiel prüfen und das Protokoll (`g3asm.log`) lesen. Ohne Spieldateien geht nur der Teil mit der
   Ersatz-SharedBase (`tests/sb_stub`).

## Sprache
- **Deutsch:** Doku (`docs/` ohne `docs/en/`), `README.md`, diese Datei, Protokollmeldungen, Texte in den Beispiel-Mods. Doku, README und
  diese Datei mit Umlauten; Protokollmeldungen in ASCII (ae, oe, ue, ss), damit sie in jeder Konsole und jedem Editor lesbar bleiben.
- **Englisch:** `docs/en/`, `README.en.md`, `CONTRIBUTING.en.md`, Kommentare im Code, Commit-Texte, `.github/workflows`, Skripte für CI und
  Release (`tools/package_release.ps1`, `tools/check_no_fninit.ps1`, `tools/run_clang_tidy.ps1`) und die erzeugte `INSTALL.txt`. Englische Doku
  und Kommentare in einfachem Englisch: kurze Sätze, Aktiv, ein Begriff für eine Sache.
- Wer eine deutsche Seite ändert, ändert ihre englische Fassung unter `docs/en/` mit (und umgekehrt); ebenso README und CONTRIBUTING.
- Commit-Texte nach dem Muster `feat: ...`, `fix: ...`, `docs: ...`, `ci: ...`, `refactor: ...`; `feat!:` wenn sich die Skript-Schnittstelle unverträglich ändert.

## Doku schreiben
- Für Modder schreiben: was eine Funktion tut, was sie liefert, worauf man achten muss. Wie etwas gemessen wurde, Datum, Adressen und Klassen der
  Engine gehören in Commit-Texte, Release-Notizen oder Code-Kommentare, nicht in `docs/`.
- Kein Projektstand in der Doku (keine „Stand:“-Zeilen, keine Listen offener Aufgaben). Grenzen und Ungetestetes gehören hinein, sachlich und ohne
  Datum, in `docs/modkit/limitations.md`; die README fasst sie nur zusammen.
- Tabellen zum Nachschlagen: je Zeile Signatur und ein bis zwei Sätze. Alles Weitere als Aufzählung unter der Tabelle oder als eigener Abschnitt
  (Muster: `docs/modkit/hooks.md`).
- Kurze Sätze, aktive Form, „du“ für den Leser. Gleiche Dinge gleich nennen: Ziffernblock, Vorlage, Info (Dialogoption), Spielstand.
- Code-Beispiele müssen übersetzen; Ausschnitte als solche kennzeichnen.

## Stil
- `.clang-format` im Wurzelordner (LLVM-Basis, 4 Leerzeichen, 140 Spalten). Der Bestand ist nicht durchgehend danach formatiert: **nur neue und
  geänderte Zeilen** formatieren, keine ganzen Dateien (sonst wird jede Änderung unlesbar).
- Quellen sind UTF-8 **ohne BOM** (das Modkit liest Dateien mit BOM, im Repo bleibt es einheitlich). PowerShell 5.1 schreibt mit
  `Set-Content -Encoding utf8` ein BOM; zum Schreiben `[IO.File]::WriteAllText(..., (New-Object Text.UTF8Encoding($false)))` nehmen.
  Ausnahme mit Absicht: `tests/mods/load_order.txt` und `tests/mods/alpha/mod.ini` beginnen mit BOM (Test).
- Kommentare erklären das Warum und woher ein Wert stammt (gemessen am ..., Export ..., RVA ...), nicht das Was.
- Kein Code aus dem Gothic-3-SDK oder aus GPL-Beispielen übernehmen; nur Erkenntnisse (welche Funktion, welches Layout). Siehe `LICENSE-EXCEPTIONS.md`.
- Keine Spieldateien ins Repository (DLLs, Archive, Spielstände).

## Statische Prüfung (clang-tidy)
- Auswahl in `.clang-tidy`: Fehlerquellen (`bugprone-*`, `clang-analyzer-*`), teure Stellen (`performance-*`) und wenige weitere. Keine Stil- oder
  Umbenennungsregeln; die Core-Guidelines-Regeln gegen `reinterpret_cast` und Zeigerarithmetik sind aus, weil Hook-Code sie zwangsläufig bricht.
- Der Bestand ist frei von Meldungen. Neue Meldungen werden behoben oder an der Zeile begründet:
  `// NOLINT(<Pruefung>): <Grund>`. Ein `NOLINT` ohne Prüfung und Grund gibt es nicht.
- Eine Prüfung ganz abschalten nur in `.clang-tidy`, mit einer Zeile, warum sie hier nur Fehlalarme liefert.
- Aufruf: `tools\run_clang_tidy.ps1 -BuildDir <Build>` (alles), `-Files a.cpp,b.cpp`, `-Changed` (nur gegen HEAD geänderte Dateien), `-Strict`
  (Rückgabewert 1 bei Meldungen). Rückgabewert 2 heißt: eine Datei ließ sich nicht untersuchen. Mit dem clang-tidy aus Visual Studio die
  64-Bit-Fassung nehmen (das Skript tut das selbst); die 32-Bit-Fassung von LLVM 15 stürzt an diesen Quellen ab.

## Hooks und Zugriffe auf das Spiel
Das Spiel ist fremder, nicht geänderter Code. Jeder Fehler hier ist ein Absturz beim Spieler.

- **Exporte vor Adressen.** Hooks hängen an exportierten Namen; neue Hooks werden über `HookExport(...)` (`script_host.h`) eingehängt. Fehlt der
  Export, fällt nur diese Funktion aus; das Protokoll sagt es. (Ältere Stellen in `dialog_bindings.cpp` und `quest_bindings.cpp` rufen MinHook noch
  direkt auf; bei der nächsten Änderung dort umstellen.)
- **Feste Adressen nur mit Byteprüfung.** Vor dem Einhängen die erwarteten Bytes der Stelle vergleichen; bei Abweichung nicht einhängen und
  melden. Gehören mehrere Stellen zusammen (Beispiel Lernkosten: sieben), gilt alle oder keine. Die geprüften Installationen stehen in
  `docs/compatibility.md`.
- **Signatur exakt.** Aufrufkonvention, Zahl der Argumente und Stapelbereinigung am Assembler prüfen (`dumpbin /disasm`), nicht raten. Funktionen
  mit eigener Konvention bekommen einen `__declspec(naked)`-Rahmen.
- **Zeiger prüfen.** Was vom Spiel kommt, kann leer sein: `Valid(...)`, `ReadableMem(...)`, Texte über `SafeStrText(...)`; ein unerwarteter
  Nullzeiger wird mit `NoteNull("<Stelle>")` einmal gemeldet, nicht verschluckt und nicht jedes Bild.
- **SEH klein halten.** `__try` nur in Funktionen ohne C++-Objekte mit Destruktor (auch kein lokales `std::string`), nicht verschachtelt;
  Texte vorher in Puffer kopieren.
- **Thread.** Skripte laufen nur im Spiel-Thread. `HasHandler`, `CallEvent`, `CallSimple` und `CallOverride` verwerfen Aufrufe aus anderen Threads und melden die
  ersten im Protokoll; ein Hook auf einem häufigen Weg prüft trotzdem selbst mit `OnGameThread()`, bevor er Argumente aufbereitet. `CallKey`
  (Fensterprozedur) hat diesen Schutz nicht.
- **Nichts aufbewahren, was dem Spiel gehört.** Entity-Zeiger nicht über einen Weltwechsel halten; Namen, die auf Spielspeicher zeigen, kopieren.

## Gleitkomma
Die Engine rechnet mit dem x87-Steuerwort `007F` (24 Bit). Ein einziges `fninit` stellt `037F` ein, und das Spiel rechnet den Rest der Sitzung
falsch (verwaschene Gebäude-Texturen, gemessen 2026-11-06).

- Kein `fninit`, `fldcw`, `_controlfp` im Modkit. AngelScript wird deshalb mit `CLEAR_FPU_STACK=emms` gebaut; `tools/check_no_fninit.ps1` prüft die DLL.
- Jede neue Fremdbibliothek zuerst darauf prüfen.
- Skriptaufrufe laufen über `ScriptHost::Execute`; nur dort wird der Gleitkomma-Zustand gesichert und wiederhergestellt.

## Laufzeit
- Hooks auf häufigen Wegen (jedes Bild, jede Berührung, jeder Skriptaufruf) rufen Skripte nur, wenn eine Mod die Funktion hat, und reservieren
  dort keinen Speicher, wenn es sich vermeiden lässt.
- Protokollzeilen aus solchen Hooks sind begrenzt (einmal je Stelle, erste N Zeilen oder eine Zusammenfassung je Zeitfenster). Ein Protokoll, das
  in Sekunden vollläuft, verdeckt den Fehler, den man sucht.
- Messpunkte und Diagnose hängen an einem ini-Schalter, der standardmäßig aus ist.

## Skript-Schnittstelle
- Jede Änderung an der Schnittstelle erhöht `G3ASM_SCRIPT_API_VERSION` um 1; was erlaubt ist, steht in `docs/modkit/api-policy.md`.
- **Erst messen, dann dokumentieren.** Eine neue Funktion steht erst ohne Vermerk in der Referenz, wenn ihre Wirkung im Spiel gemessen ist; bis
  dahin trägt sie den Vermerk *Experimentell* oder *Im Spiel nicht geprüft*.
- Zu jeder neuen Funktion gehören: Eintrag in `docs/modkit/api-reference.md` (oder `events.md`/`hooks.md`) und in der englischen Fassung, eine Prüfung im Selbsttest
  (`tests/scripts/selftest.as`), soweit sie ohne Spiel läuft, und bei Bedarf eine schlanke Mess-Mod unter `examples/mods` (`enabled=0`).
- Beispiel-Mods sind standardmäßig aus (`mod.ini`: `enabled=0`) und stehen unter CC0. Namen: `demo_<thema>` für Lern-Beispiele,
  `probe_<thema>` für Mess-Mods (englisch, klein, mit Unterstrich); Mods mit Spielinhalt dürfen einen eigenen Namen tragen.
- Was eine Mod dauerhaft ins Spiel schreibt, steht im Spielstand. Beispiele, die das tun, räumen in `OnBeforeSave` auf und stellen den Zustand in
  `OnAfterSave` und nach jedem Laden wieder her (Muster: `examples/mods/perk_lifesteal`).

## Lizenz
Beiträge stehen unter GPL-3.0-or-later mit den Zusatzerlaubnissen aus `LICENSE-EXCEPTIONS.md`; neue Quelldateien in `src/` und `tools/` bekommen
den SPDX-Kopf wie die vorhandenen.
