# Mitarbeiten

Kurze Hausregeln fuer Aenderungen an diesem Repository. Sie beschreiben, wie der Code heute gebaut ist; wer davon abweicht, schreibt den Grund dazu.
Bauen, Testen und Einbau stehen in der [README](README.md), die Regeln fuer die Skript-Schnittstelle in
[docs/modkit/api-policy.md](docs/modkit/api-policy.md).

## Vor jedem Commit
1. Bauen (32 Bit) und die Testlaeufe aus der README: Selbsttest, `tests\mods`, Kompilierpruefung `examples\mods`
   (dort ist genau der eine Fehler "Override-Kette ueber mehrere Mods" erwartet), Loader-Test, `tests\file_patches_overlay.py`, `tests\packed_mods.py` und `tests\packmod_release.py`.
2. `powershell -NoProfile -File tools\check_no_fninit.ps1 -Dll <Build>\g3asm\g3asm.dll` und dasselbe fuer `Script_G3ASM_<API>.dll` (siehe Gleitkomma).
3. `powershell -NoProfile -File tools\run_clang_tidy.ps1 -BuildDir <Build> -Changed -Strict` (siehe Statische Pruefung).
4. Was das Verhalten im Spiel aendert, im Spiel pruefen und das Protokoll (`g3asm.log`) lesen. Ohne Spieldateien geht nur der Teil mit der
   Ersatz-SharedBase (`tests/sb_stub`).

## Sprache
- **Deutsch:** Doku (`docs/`), Kommentare im Code, Protokollmeldungen, Beispiel-Mods. README und diese Datei in reinem ASCII (ae, oe, ue, ss).
- **Englisch:** `README.en.md` (die englische Fassung der README; bei Aenderungen an der README beide anpassen), Commit-Texte, `.github/workflows`, Skripte fuer CI und Release (`tools/package_release.ps1`, `tools/check_no_fninit.ps1`,
  `tools/run_clang_tidy.ps1`) und die erzeugte `INSTALL.txt`.
- Commit-Texte nach dem Muster `feat: ...`, `fix: ...`, `docs: ...`, `ci: ...`, `refactor: ...`; `feat!:` wenn sich die Skript-Schnittstelle unvertraeglich aendert.

## Stil
- `.clang-format` im Wurzelordner (LLVM-Basis, 4 Leerzeichen, 140 Spalten). Der Bestand ist nicht durchgehend danach formatiert: **nur neue und
  geaenderte Zeilen** formatieren, keine ganzen Dateien (sonst wird jede Aenderung unlesbar).
- Quellen sind UTF-8 **ohne BOM** (das Modkit liest Dateien mit BOM, im Repo bleibt es einheitlich). PowerShell 5.1 schreibt mit
  `Set-Content -Encoding utf8` ein BOM; zum Schreiben `[IO.File]::WriteAllText(..., (New-Object Text.UTF8Encoding($false)))` nehmen.
  Ausnahme mit Absicht: `tests/mods/load_order.txt` und `tests/mods/alpha/mod.ini` beginnen mit BOM (Test).
- Kommentare erklaeren das Warum und woher ein Wert stammt (gemessen am ..., Export ..., RVA ...), nicht das Was.
- Kein Code aus dem Gothic-3-SDK oder aus GPL-Beispielen uebernehmen; nur Erkenntnisse (welche Funktion, welches Layout). Siehe `LICENSE-EXCEPTIONS.md`.
- Keine Spieldateien ins Repository (DLLs, Archive, Spielstaende).

## Statische Pruefung (clang-tidy)
- Auswahl in `.clang-tidy`: Fehlerquellen (`bugprone-*`, `clang-analyzer-*`), teure Stellen (`performance-*`) und wenige weitere. Keine Stil- oder
  Umbenennungsregeln; die Core-Guidelines-Regeln gegen `reinterpret_cast` und Zeigerarithmetik sind aus, weil Hook-Code sie zwangslaeufig bricht.
- Der Bestand ist frei von Meldungen. Neue Meldungen werden behoben oder an der Zeile begruendet:
  `// NOLINT(<Pruefung>): <Grund>`. Ein `NOLINT` ohne Pruefung und Grund gibt es nicht.
- Eine Pruefung ganz abschalten nur in `.clang-tidy`, mit einer Zeile, warum sie hier nur Fehlalarme liefert.
- Aufruf: `tools\run_clang_tidy.ps1 -BuildDir <Build>` (alles), `-Files a.cpp,b.cpp`, `-Changed` (nur gegen HEAD geaenderte Dateien), `-Strict`
  (Rueckgabewert 1 bei Meldungen). Rueckgabewert 2 heisst: eine Datei liess sich nicht untersuchen. Mit dem clang-tidy aus Visual Studio die
  64-Bit-Fassung nehmen (das Skript tut das selbst); die 32-Bit-Fassung von LLVM 15 stuerzt an diesen Quellen ab.

## Hooks und Zugriffe auf das Spiel
Das Spiel ist fremder, nicht geaenderter Code. Jeder Fehler hier ist ein Absturz beim Spieler.

- **Exporte vor Adressen.** Hooks haengen an exportierten Namen; neue Hooks werden ueber `HookExport(...)` (`script_host.h`) eingehaengt. Fehlt der
  Export, faellt nur diese Funktion aus; das Protokoll sagt es. (Aeltere Stellen in `dialog_bindings.cpp` und `quest_bindings.cpp` rufen MinHook noch
  direkt auf; bei der naechsten Aenderung dort umstellen.)
- **Feste Adressen nur mit Bytepruefung.** Vor dem Einhaengen die erwarteten Bytes der Stelle vergleichen; bei Abweichung nicht einhaengen und
  melden. Gehoeren mehrere Stellen zusammen (Beispiel Lernkosten: sieben), gilt alle oder keine. Die geprueften Installationen stehen in
  `docs/compatibility.md`.
- **Signatur exakt.** Aufrufkonvention, Zahl der Argumente und Stapelbereinigung am Assembler pruefen (`dumpbin /disasm`), nicht raten. Funktionen
  mit eigener Konvention bekommen einen `__declspec(naked)`-Rahmen.
- **Zeiger pruefen.** Was vom Spiel kommt, kann leer sein: `Valid(...)`, `ReadableMem(...)`, Texte ueber `SafeStrText(...)`; ein unerwarteter
  Nullzeiger wird mit `NoteNull("<Stelle>")` einmal gemeldet, nicht verschluckt und nicht jedes Bild.
- **SEH klein halten.** `__try` nur in Funktionen ohne C++-Objekte mit Destruktor (auch kein lokales `std::string`), nicht verschachtelt;
  Texte vorher in Puffer kopieren.
- **Thread.** Skripte laufen nur im Spiel-Thread. `HasHandler`, `CallEvent`, `CallSimple` und `CallOverride` verwerfen Aufrufe aus anderen Threads und melden die
  ersten im Protokoll; ein Hook auf einem haeufigen Weg prueft trotzdem selbst mit `OnGameThread()`, bevor er Argumente aufbereitet. `CallKey`
  (Fensterprozedur) hat diesen Schutz nicht.
- **Nichts aufbewahren, was dem Spiel gehoert.** Entity-Zeiger nicht ueber einen Weltwechsel halten; Namen, die auf Spielspeicher zeigen, kopieren.

## Gleitkomma
Die Engine rechnet mit dem x87-Steuerwort `007F` (24 Bit). Ein einziges `fninit` stellt `037F` ein, und das Spiel rechnet den Rest der Sitzung
falsch (verwaschene Gebaeude-Texturen, gemessen 2026-11-06).

- Kein `fninit`, `fldcw`, `_controlfp` im Modkit. AngelScript wird deshalb mit `CLEAR_FPU_STACK=emms` gebaut; `tools/check_no_fninit.ps1` prueft die DLL.
- Jede neue Fremdbibliothek zuerst darauf pruefen.
- Skriptaufrufe laufen ueber `ScriptHost::Execute`; nur dort wird der Gleitkomma-Zustand gesichert und wiederhergestellt.

## Laufzeit
- Hooks auf haeufigen Wegen (jedes Bild, jede Beruehrung, jeder Skriptaufruf) rufen Skripte nur, wenn eine Mod die Funktion hat, und reservieren
  dort keinen Speicher, wenn es sich vermeiden laesst.
- Protokollzeilen aus solchen Hooks sind begrenzt (einmal je Stelle, erste N Zeilen oder eine Zusammenfassung je Zeitfenster). Ein Protokoll, das
  in Sekunden vollaeuft, verdeckt den Fehler, den man sucht.
- Messpunkte und Diagnose haengen an einem ini-Schalter, der standardmaessig aus ist.

## Skript-Schnittstelle
- Jede Aenderung an der Schnittstelle erhoeht `G3ASM_SCRIPT_API_VERSION` um 1; was erlaubt ist, steht in `docs/modkit/api-policy.md`.
- **Erst messen, dann dokumentieren.** Eine neue Funktion steht erst ohne Vermerk in der Referenz, wenn ihre Wirkung im Spiel gemessen ist; bis
  dahin traegt sie den Vermerk *Experimentell* oder *Im Spiel nicht geprueft*.
- Zu jeder neuen Funktion gehoeren: Eintrag in `docs/modkit/api-reference.md` (oder `events.md`/`hooks.md`), eine Pruefung im Selbsttest
  (`tests/scripts/selftest.as`), soweit sie ohne Spiel laeuft, und bei Bedarf eine schlanke Mess-Mod unter `examples/mods` (`enabled=0`).
- Beispiel-Mods sind standardmaessig aus (`mod.ini`: `enabled=0`) und stehen unter CC0. Namen: `demo_<thema>` fuer Lern-Beispiele,
  `probe_<thema>` fuer Mess-Mods (englisch, klein, mit Unterstrich); Mods mit Spielinhalt duerfen einen eigenen Namen tragen.
- Was eine Mod dauerhaft ins Spiel schreibt, steht im Spielstand. Beispiele, die das tun, raeumen in `OnBeforeSave` auf und stellen den Zustand in
  `OnAfterSave` und nach jedem Laden wieder her (Muster: `examples/mods/perk_lifesteal`).

## Lizenz
Beitraege stehen unter GPL-3.0-or-later mit den Zusatzerlaubnissen aus `LICENSE-EXCEPTIONS.md`; neue Quelldateien in `src/` und `tools/` bekommen
den SPDX-Kopf wie die vorhandenen.
