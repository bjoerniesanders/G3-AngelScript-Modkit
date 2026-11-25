# Kommandozeile `g3asm` (ab Version 47)

**Deutsch** | [English](../en/modkit/cli.md)

`bin\g3asm\g3asm.exe` erledigt die häufigen Arbeiten an einer Mod ohne das Spiel: eine neue Mod anlegen, das Skript prüfen, packen und das
Protokoll lesen. Jeder Befehl schreibt mit `--json` ein JSON-Objekt. So können Skripte und Werkzeuge das Ergebnis auswerten.

| Befehl | Wirkung |
|---|---|
| `g3asm new <Mod-Ordner>` | Legt den Ordner mit `main.as` und `mod.ini` an |
| `g3asm check <Mod-Ordner\|Datei.as>` | Übersetzt das Skript ohne das Spiel und nennt Fehler mit Zeile und Spalte |
| `g3asm pack <Ordner> <Archiv>` | Schreibt einen Ordner als Archiv, wie `g3asm_packmod pack` ([Mod-Archive](archives.md)) |
| `g3asm release <Mod-Ordner> <Ziel-Ordner>` | Baut ein Paket für Spieler, wie `g3asm_packmod release` ([Mods weitergeben](mod-package.md#mods-weitergeben)) |
| `g3asm log [<Spielordner>] [-n <Zahl>] [--all]` | Zeigt die letzten Fehler und Warnungen aus `g3asm.log` |

Rückgabewert: `0` = in Ordnung, `1` = Fehler in der Mod (oder `pack`/`release` fehlgeschlagen), `2` = falscher Aufruf oder das Werkzeug kann
nicht arbeiten (zum Beispiel fehlt eine Datei).

## Eine neue Mod anlegen

```
bin\g3asm\g3asm.exe new "<Spielordner>\g3asm\mods\meine_mod"
```

Ergebnis:

```
Mod angelegt: C:\Spiele\Gothic 3\g3asm\mods\meine_mod
  main.as
  mod.ini (api=47)
```

- Der Ordnername ist der Name der Mod. Erlaubt sind Buchstaben `a`-`z`, Ziffern, `_` und `-`.
- `mod.ini` enthält `version=1.0.0`, die aktuelle Skript-Schnittstelle als `api` und den Namen als `title`.
- `new` überschreibt keinen vorhandenen Ordner.

## Ein Skript prüfen

```
bin\g3asm\g3asm.exe check "<Spielordner>\g3asm\mods\meine_mod"
```

Ergebnis bei einem Fehler (Zeile 2 ist `int x = ;`):

```
C:/Spiele/Gothic 3/g3asm/mods/meine_mod/main.as (1, 1): Info: Compiling void main()
C:/Spiele/Gothic 3/g3asm/mods/meine_mod/main.as (2, 13): FEHLER: Expected expression value
C:/Spiele/Gothic 3/g3asm/mods/meine_mod/main.as (2, 13): FEHLER: Instead found ';'
meine_mod: 2 Fehler, 0 Warnung(en)
```

Ohne Fehler:

```
OK: meine_mod ohne Fehler (Skript-Schnittstelle 47)
```

- `check` übersetzt `main.as` mit denselben Typen und Funktionen wie im Spiel. Es führt keine Funktion aus, auch nicht `main()`.
- `check` prüft auch den Eintrag `api` der `mod.ini`. Verlangt die Mod eine neuere Skript-Schnittstelle als die des Modkits, ist das ein Fehler.
- `check` braucht keine Spieldateien. Es benutzt die Ersatz-`SharedBase.dll` aus `bin\g3asm\sbstub\`. Mit `--sharedbase <Datei>` gibst du eine
  andere an.

**Achtung:** `check` findet nur Fehler, die schon beim Übersetzen auffallen: Tippfehler, falsche Typen, unbekannte Funktionen. Fehler zur
Laufzeit (zum Beispiel `Null pointer access`) zeigt erst das Spiel im Protokoll.

Mit `--json`:

```json
{"ok":false,"mod":"meine_mod","file":"...\\main.as","api":47,"errors":2,"warnings":0,
 "messages":[{"type":"info","file":".../main.as","line":1,"col":1,"text":"Compiling void main()"},
             {"type":"error","file":".../main.as","line":2,"col":13,"text":"Expected expression value"},
             {"type":"error","file":".../main.as","line":2,"col":13,"text":"Instead found ';'"}]}
```

`type` ist `error`, `warning` oder `info`. AngelScript schreibt vor die Fehler einer Funktion eine `info`-Zeile mit ihrem Namen.

## Das Protokoll lesen

```
bin\g3asm\g3asm.exe log "<Spielordner>"
```

- Ohne Spielordner liest `log` die Datei `g3asm.log` im aktuellen Ordner.
- `log` zeigt die letzten 20 Zeilen mit Fehlern, Warnungen, Skript-Ausnahmen und Abbrüchen. `-n <Zahl>` ändert die Anzahl. `--all` zeigt alle
  Zeilen, nicht nur die Fehler.
- Das Modkit schreibt `g3asm.log` bei jedem Spielstart neu. `log` zeigt also nur die laufende oder die letzte Sitzung.
- `log` geht auch, während das Spiel läuft.

Mit `--json` hat jede Zeile die Felder `time`, `mod` und `text`:

```json
{"ok":true,"file":"...\\g3asm.log","count":1,
 "lines":[{"time":"10:00:00.004","mod":"meine_mod","text":"Skript-Ausnahme in void main() (Zeile 4): Null pointer access"}]}
```

## Packen und weitergeben

`g3asm pack` und `g3asm release` geben alle Argumente an `g3asm_packmod.exe` weiter. Die beiden Werkzeuge müssen im selben Ordner liegen. Mit
`--json` steht die Ausgabe von `g3asm_packmod` im Feld `out`, der Rückgabewert im Feld `exit`.

```
bin\g3asm\g3asm.exe release "<Spielordner>\g3asm\mods\meine_mod" "<Zielordner>"
```
