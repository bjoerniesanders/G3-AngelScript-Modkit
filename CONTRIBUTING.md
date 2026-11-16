# Mitarbeiten

Kurze Hausregeln für Änderungen an diesem Repository. Sie beschreiben, wie der Code heute gebaut ist; wer davon abweicht, schreibt den Grund dazu.
Bauen, Testen und Einbau stehen in der [README](README.md), die Regeln für die Skript-Schnittstelle in
[docs/modkit/api-policy.md](docs/modkit/api-policy.md).

## Vor jedem Commit
1. Bauen (32 Bit) und die Testläufe aus der README: Selbsttest, `tests\mods`, Kompilierprüfung `examples\mods`
   (dort ist genau der eine Fehler "Override-Kette über mehrere Mods" erwartet), Loader-Test, `tests\file_patches_overlay.py`, `tests\packed_mods.py` und `tests\packmod_release.py`.
2. `powershell -NoProfile -File tools\check_no_fninit.ps1 -Dll <Build>\g3asm\g3asm.dll` und dasselbe für `Script_G3ASM_<API>.dll` (siehe Gleitkomma).
3. `powershell -NoProfile -File tools\run_clang_tidy.ps1 -BuildDir <Build> -Changed -Strict` (siehe Statische Prüfung).
4. Was das Verhalten im Spiel ändert, im Spiel prüfen und das Protokoll (`g3asm.log`) lesen. Ohne Spieldateien geht nur der Teil mit der
   Ersatz-SharedBase (`tests/sb_stub`).

## Sprache
- **Deutsch:** Doku (`docs/`), README, diese Datei, Kommentare im Code, Protokollmeldungen, Beispiel-Mods. Doku, README und diese Datei mit
  Umlauten; Kommentare im Code und Protokollmeldungen in ASCII (ae, oe, ue, ss), damit sie in jeder Konsole und jedem Editor lesbar bleiben.
- **Englisch:** `README.en.md` (die englische Fassung der README; bei Änderungen an der README beide anpassen), Commit-Texte, `.github/workflows`, Skripte für CI und Release (`tools/package_release.ps1`, `tools/check_no_fninit.ps1`,
  `tools/run_clang_tidy.ps1`) und die erzeugte `INSTALL.txt`.
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
- Zu jeder neuen Funktion gehören: Eintrag in `docs/modkit/api-reference.md` (oder `events.md`/`hooks.md`), eine Prüfung im Selbsttest
  (`tests/scripts/selftest.as`), soweit sie ohne Spiel läuft, und bei Bedarf eine schlanke Mess-Mod unter `examples/mods` (`enabled=0`).
- Beispiel-Mods sind standardmäßig aus (`mod.ini`: `enabled=0`) und stehen unter CC0. Namen: `demo_<thema>` für Lern-Beispiele,
  `probe_<thema>` für Mess-Mods (englisch, klein, mit Unterstrich); Mods mit Spielinhalt dürfen einen eigenen Namen tragen.
- Was eine Mod dauerhaft ins Spiel schreibt, steht im Spielstand. Beispiele, die das tun, räumen in `OnBeforeSave` auf und stellen den Zustand in
  `OnAfterSave` und nach jedem Laden wieder her (Muster: `examples/mods/perk_lifesteal`).

## Lizenz
Beiträge stehen unter GPL-3.0-or-later mit den Zusatzerlaubnissen aus `LICENSE-EXCEPTIONS.md`; neue Quelldateien in `src/` und `tools/` bekommen
den SPDX-Kopf wie die vorhandenen.
