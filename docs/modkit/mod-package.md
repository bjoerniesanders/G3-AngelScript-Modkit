# Mod-Ordner und Installation

## Installation

Voraussetzung: Gothic 3 mit Community Patch 1.75 und Parallel-Universe-Patch. Gothic 3 muss beendet sein. Im entpackten Release-Ordner:

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_loader.ps1 -Game "<Spielordner>" -BuildDir bin
```

Das Skript
- prüft die Voraussetzungen und bricht ab, wenn etwas fehlt,
- legt die `dbghelp.dll` des Spiels als `dbghelp_orig.dll` beiseite und sichert sie nach `<Spiel>\g3asm_backup\loader_<Zeit>\`,
- kopiert den Loader (als `dbghelp.dll`) und `g3asm.dll` in den Spielordner,
- legt `g3asm.ini`, `g3asm\scripts\` (mit einem Mini-Mod) und `g3asm\mods\` an,
- zieht eine ältere Installation mit den früheren Namen (`og3_patch.dll`, `og3_patch.ini`, Ordner `og3\`, ini-Abschnitt `[patch]`) auf die neuen Namen um.

Rückbau: dasselbe mit `-Uninstall` statt `-BuildDir bin` stellt die originale `dbghelp.dll` wieder her und entfernt `g3asm.dll`. `g3asm.ini` und der Ordner
`g3asm\` bleiben liegen.

Änderungen an `g3asm.dll` (C++) brauchen einen Neustart des Spiels. Änderungen an Skripten nicht (Hot-Reload).

## Aufbau

```
<Gothic 3>\
  g3asm.dll
  g3asm.log                 Protokoll (print() und Fehlermeldungen)
  g3asm.ini                 Einstellungen, Abschnitte [g3asm] und [loader]
  g3asm_loader.log          Protokoll des Loaders (Absturzkandidaten)
  dbghelp.dll               der Loader; dbghelp_orig.dll ist die des Spiels
  g3asm\
    scripts\main.as             Einzelskript (läuft als Mod "main", wenn vorhanden)
    mods\
      load_order.txt            optional
      mein_mod\main.as          ein Mod = ein Ordner mit main.as
      anderer_mod\main.as
      mein_mod\ScriptPatches\   optional: *.patch (Unified Diff auf Spieldateien, siehe Datei-Patches)
```

Jeder Ordner unter `g3asm\mods\`, der eine `main.as` oder einen Ordner `ScriptPatches` enthält, ist ein Mod. Der Ordnername ist der Modname und steht im Protokoll in eckigen Klammern (`[mein_mod] ...`).
Der Name `main` ist für das Einzelskript reserviert.

Jeder Mod ist ein **eigenes AngelScript-Modul**: globale Variablen und Funktionen sind von anderen Mods getrennt, Namen können doppelt vorkommen.
`#include "datei.as"` funktioniert relativ zur Datei, die es einbindet. So kannst du einen Mod auf mehrere Dateien verteilen.

## Ladereihenfolge: `load_order.txt`

```
# Kommentar
basis_mod
mein_mod
-kaputter_mod        # Minuszeichen davor schaltet einen Mod ab
+lifesteal_demo      # Pluszeichen schaltet einen standardmaessig abgeschalteten Mod zu
```

- Je Zeile ein Modname. Die genannten Mods laden in dieser Reihenfolge.
- Nicht genannte Mods folgen alphabetisch.
- `-Name` schaltet den Mod ab (er wird nicht geladen, bzw. im laufenden Spiel entladen).
- Ein Mod mit `mod.ini` und `[mod] enabled=0` im eigenen Ordner ist **standardmäßig aus** (so sind alle Beispiel-Mods ausgeliefert). Er lädt nur, wenn `+Name` in `load_order.txt` steht; `+Name` legt zugleich seine Stelle in der Reihenfolge fest. Mods ohne `mod.ini` laden wie bisher.
- Das Einzelskript `main` läuft immer zuerst.
- Jedes Ereignis geht der Reihe nach an alle Mods.
- Braucht ein Mod einen anderen (`requires` in `mod.ini`, siehe unten), lädt der benötigte zuerst, auch wenn `load_order.txt` es anders sagt. Das Protokoll nennt jede solche Umstellung.

## Version und Abhängigkeiten: `mod.ini` (ab Version 40)

```
[mod]
enabled=1                              ; 0 = standardmäßig aus (siehe oben)
version=1.2.0                          ; Version dieses Mods
api=40                                 ; mindestens nötige Skript-Schnittstelle (GetApiVersion)
requires=basis_mod >= 1.1, andere_mod  ; benötigte Mods, Komma-getrennt, Mindestversion optional
```

Alle Einträge sind freiwillig. Ein Mod, dessen Bedingungen nicht erfüllt sind, **wird nicht geladen**: weder sein Skript noch seine Datei-Patches oder seine
Stringtabelle. Das Protokoll nennt den Grund einmal, zum Beispiel:

```
[mein_mod] wird nicht geladen: braucht die Mod 'basis_mod' ab Version 1.1, vorhanden ist Version 1.0
[mein_mod] wird nicht geladen: braucht die Mod 'andere_mod', die abgeschaltet ist (in load_order.txt mit +andere_mod zuschalten)
[mein_mod] wird nicht geladen: braucht Skript-Schnittstelle 41, vorhanden ist 40 (G3ASM aktualisieren)
```

- Fehlt ein benötigter Mod, ist er abgeschaltet oder wird er selbst nicht geladen, wird auch der Mod nicht geladen, der ihn braucht.
- Versionen werden Teil für Teil als Zahlen verglichen: `1.10` ist neuer als `1.9`, `1.2` gleich `1.2.0`. Text hinter einer Zahl zählt nicht (`1.0-beta` = `1.0`).
  Hat der benötigte Mod keine `version`, erfüllt er keine Mindestversion.
- `api` ersetzt die Prüfung mit `GetApiVersion()` im Skript: Fehlt eine Funktion, scheitert sonst schon das Übersetzen, und der Spieler sieht nur einen
  Übersetzungsfehler. Mit `api` sieht er, dass das Modkit zu alt ist. Fassungen vor Version 40 kennen `api` und `requires` nicht und ignorieren sie.
- Brauchen sich zwei Mods gegenseitig, meldet das Protokoll den Kreis; beide laden in der Reihenfolge von `load_order.txt`.
- Kodierung: `mod.ini`, `load_order.txt` und `g3asm.ini` dürfen ANSI oder UTF-8 sein, mit oder ohne BOM (Editor-Einstellung „UTF-8 mit BOM“, PowerShell 5 `-Encoding utf8`). Bei `g3asm.ini` mit BOM vermerkt das Protokoll einen Hinweis.
- Ändert sich `mod.ini` oder `load_order.txt` im laufenden Spiel, gilt das beim nächsten Scan (einmal je Sekunde, mit `hotreload=1`).
- Im Skript: `IsModActive("name")` und `GetModVersion("name")` für optionale Zusammenarbeit mit anderen Mods, siehe [API-Referenz](api-reference.md).

## Einstellungen eines Mods: `[settings]` (ab Version 44)

Vorgaben stehen in der `mod.ini` des Mods, Abschnitt `[settings]`:

```
[mod]
version=1.0
api=44

[settings]
Faktor=2.5
Anzeige=ja
```

Das Skript liest sie mit `GetModSetting("Faktor")`, `GetModSettingFloat(...)` usw. und kann eigene Werte mit `SetModSetting` speichern. Die landen in
`<Spiel>\g3asm\settings\<Mod>.ini`, nie in der `mod.ini`; ein Update des Mods behält sie also. Wer eine Einstellung von Hand ändert, trägt sie dort ein
(Abschnitt `[settings]`); sie gilt nach dem Neuladen des Mods (Hot-Reload) oder einem Neustart. Jeder Mod sieht nur seine eigenen Werte.

## Einstellungen: `g3asm.ini`

```
[g3asm]
enable=1                    ; 1 = Modkit laden
script=g3asm\scripts\main.as  ; Einzelskript, relativ zum Spielordner oder absolut
hotreload=1                 ; Skripte bei Dateiänderung neu laden (Prüfung einmal je Sekunde)
mods=g3asm\mods               ; Mod-Ordner
hooks=1                     ; 0 = keine Spielereignisse (Hooks auf Game.dll) einhängen
filepatches=1               ; 0 = ScriptPatches (Datei-Patches) nicht anwenden
navevents=0                 ; 1 = OnEnter/OnLeave melden auch interne Navigationszonen "Nav_*"
scriptlog=0                 ; 1 = alle 60 s die haeufigsten Spielskripte und den Zeitaufwand der Hooks protokollieren
aitrace=0                   ; 1 = KI-Tracer: Wechsel von KI-Zustand, -Aufgabe, -Routine, KI-Modus und Aktion der NPCs protokollieren
aitracefilter=              ; nur NPCs, deren Name diesen Text enthaelt (leer = alle)
aitracemax=2000             ; hoechstens so viele KI-Zeilen je Spielstart
anispeedlog=0               ; 1 = Messung: wer die Abspielgeschwindigkeit von Animationen setzt/liest (SetPlaySpeed/GetPlaySpeed)
printgame=1                 ; 0 = print() schreibt nur ins eigene Protokoll, nicht zusätzlich ins Protokoll des Spiels
; nur zur Fehlersuche:
asynclog=0                  ; 1 = Protokollzeilen über eine Warteschlange schreiben (eigener Thread)
logwrite=1                  ; 0 = nach dem Start keine Protokollzeilen mehr schreiben
loadpause=0                 ; so viele Sekunden nach einem Ladevorgang wird OnFrame der Skripte nicht aufgerufen (0 = aus)
memlog=0                    ; 1 = alle 30 s freien Speicher und Adressraum protokollieren
[loader]
crashlog=1                  ; 0 = der Loader schreibt bei einem Absturz keine Zeile nach g3asm_loader.log
```

Der Abschnitt hieß bis Version 46 `[patch]`; eine ini mit dem alten Namen wird weiter gelesen, der Installer benennt ihn um.

## Protokoll

`<Spiel>\g3asm.log`, während des Spiels lesbar. Es enthält:

- die Ausgaben deiner `print()`-Aufrufe mit `[Modname]` davor,
- Übersetzungsfehler mit Datei, Zeile und Spalte (der Mod wird dann nicht geladen),
- Laufzeitfehler (Funktion und Zeile), zum Beispiel Division durch 0,
- Hinweise des Modkits (`Spielereignisse: 6 von 6 Hooks aktiv`, `Skript geladen: ...`).

## Hot-Reload

Speicherst du `main.as` (oder legst einen neuen Mod-Ordner an), erkennt das Modkit das innerhalb einer Sekunde:

1. Beim Neuladen eines Mods läuft sein `OnShutdown()`.
2. Das neue Skript wird übersetzt. Bei Fehlern bleibt der Mod entladen, bis du die Datei wieder änderst.
3. Danach läuft `main()` erneut.

Ordner entfernen oder `-Name` eintragen entlädt den Mod (mit `OnShutdown()`).

## Fehlerisolation

Ein Fehler in einem Mod (Syntaxfehler, Laufzeitfehler, Endlosschleife) beeinträchtigt die anderen Mods und das Spiel nicht.
Ein Aufruf, der länger als **5 Sekunden** läuft, wird abgebrochen.

## Eigene Texte: `stringtable.ini`

Ein Mod kann Texte des Spiels (Itemnamen, Beschreibungen, Quest- und Dialogtexte) mit einer eigenen Stringtabelle überschreiben oder ergänzen. Datei: `<Mod>\stringtable.ini` oder, wie beim Union Starter, `<Mod>\ModFiles\stringtable.ini` (liegen beide da, gilt die im Mod-Ordner).
Der Mod braucht dafür kein Skript. Eigene Dateien des Spiels (Quests, Vorlagen, ...) packt [PackMod](archives.md) in ein Mod-Archiv.

```
Languages=English;Italian;French;German;Spanish;Czech;Hungarian;Polish;Russian;TRC
FO_It_Wolf_Fur=Wolf skin (Mod);;Pelle di lupo (Mod);;Peau de loup (Mod);;Wolfsfell (Mod);;...
```

- Format wie die `stringtable.ini` des Spiels: `Schlüssel=Text;;Text;;...`. Mit der Zeile `Languages=` sind die Spalten in dieser Reihenfolge; genommen wird die Spalte der aktuellen
  Spielsprache, bei leerem oder fehlendem Eintrag die erste. Ohne `Languages=` gilt der ganze Wert als ein Text.
- Schlüssel: `FO_<Vorlage>` (Itemname), `ITEMDESC_<Vorlage>` (Beschreibung); weitere Schlüssel stehen in `Strings.pak\stringtable.ini`.
- Kodierung: UTF-16 (mit BOM), UTF-8 (mit BOM) oder ANSI (cp1252). Kommentarzeilen beginnen mit `;`, `#` oder `[`.
- Spätere Mods überschreiben frühere. Das Skript-Gegenstück ist `RegisterText(id, text)`.
- Änderungen an der Datei wirken per Hot-Reload nach etwa einer Sekunde; das Protokoll zeigt `Stringtabellen angewendet: N Eintraege, Sprache ...`.
- Beispiel: `examples/mods/text_demo`.
