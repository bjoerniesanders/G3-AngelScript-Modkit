# Mod-Ordner und Installation

## Installation des Patches

Gothic 3 muss beendet sein.

```
powershell -NoProfile -File G:\OpenGothic3\tools\install_patch.ps1 [-Game G:\Gothic3]
```

Das Skript
- sichert `dbghelp.dll` und `og3_hooks.ini` nach `<Spiel>\og3_backup\patch_<Zeit>\`,
- kopiert den Proxy (`dbghelp.dll`) und `og3_patch.dll` in den Spielordner,
- legt `og3\scripts\` und `og3\mods\` an und setzt in `og3_hooks.ini` `[patch] enable=1`.

Rückbau: `install_patch.ps1 -Uninstall` stellt die Sicherung wieder her. Die Skripte unter `og3\` bleiben liegen.

Änderungen an `og3_patch.dll` (C++) brauchen einen Neustart des Spiels. Änderungen an Skripten nicht (Hot-Reload).

## Aufbau

```
<Gothic 3>\
  og3_patch.dll
  og3_patch.log                 Protokoll (print() und Fehlermeldungen)
  og3_hooks.ini                 Einstellungen, Abschnitt [patch]
  og3\
    scripts\main.as             Einzelskript (läuft als Mod "main", wenn vorhanden)
    mods\
      load_order.txt            optional
      mein_mod\main.as          ein Mod = ein Ordner mit main.as
      anderer_mod\main.as
      mein_mod\ScriptPatches\   optional: *.patch (Unified Diff auf Spieldateien, siehe Datei-Patches)
```

Jeder Ordner unter `og3\mods\`, der eine `main.as` oder einen Ordner `ScriptPatches` enthält, ist ein Mod. Der Ordnername ist der Modname und steht im Protokoll in eckigen Klammern (`[mein_mod] ...`).
Der Name `main` ist für das Einzelskript reserviert.

Jeder Mod ist ein **eigenes AngelScript-Modul**: globale Variablen und Funktionen sind von anderen Mods getrennt, Namen können doppelt vorkommen.
`#include "datei.as"` funktioniert relativ zur Datei, die es einbindet. So kannst du einen Mod auf mehrere Dateien verteilen.

## Ladereihenfolge: `load_order.txt`

```
# Kommentar
basis_mod
mein_mod
-kaputter_mod        # Minuszeichen davor schaltet einen Mod ab
```

- Je Zeile ein Modname. Die genannten Mods laden in dieser Reihenfolge.
- Nicht genannte Mods folgen alphabetisch.
- `-Name` schaltet den Mod ab (er wird nicht geladen, bzw. im laufenden Spiel entladen).
- Das Einzelskript `main` läuft immer zuerst.
- Jedes Ereignis geht der Reihe nach an alle Mods.

## Einstellungen: `og3_hooks.ini`

```
[patch]
enable=1                    ; 1 = Patch laden
script=og3\scripts\main.as  ; Einzelskript, relativ zum Spielordner oder absolut
hotreload=1                 ; Skripte bei Dateiänderung neu laden (Prüfung einmal je Sekunde)
mods=og3\mods               ; Mod-Ordner
hooks=1                     ; 0 = keine Spielereignisse (Hooks auf Game.dll) einhängen
filepatches=1               ; 0 = ScriptPatches (Datei-Patches) nicht anwenden
navevents=0                 ; 1 = OnEnter/OnLeave melden auch interne Navigationszonen "Nav_*"
```

## Protokoll

`<Spiel>\og3_patch.log`, während des Spiels lesbar. Es enthält:

- die Ausgaben deiner `print()`-Aufrufe mit `[Modname]` davor,
- Übersetzungsfehler mit Datei, Zeile und Spalte (der Mod wird dann nicht geladen),
- Laufzeitfehler (Funktion und Zeile), zum Beispiel Division durch 0,
- Hinweise des Patches (`Spielereignisse: 6 von 6 Hooks aktiv`, `Skript geladen: ...`).

## Hot-Reload

Speicherst du `main.as` (oder legst einen neuen Mod-Ordner an), erkennt der Patch das innerhalb einer Sekunde:

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
