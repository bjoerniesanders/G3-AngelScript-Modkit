# Grenzen und Stand

Stand: 2026-11-02, Skript-Schnittstelle Version 25.

## Im Spiel geprüft

- Mods laden, Hot-Reload, Fehlerisolation; Ereignisse `OnKey`, `OnFrame`, `OnWorldActivate/Deactivate`
- Alle sechs Spielereignisse (`OnNPCKilled`, `OnNPCDefeated`, `OnReceiveItem` mit Itemname, `OnEnter`/`OnLeave`, `OnSpellCast`)
- Attribute lesen und ändern, `PrintScreen`
- Quests anlegen, starten, abschließen, scheitern lassen, Status lesen, Titel, Beschreibung, XP, Attribut-Belohnung (Erfolgsmeldung zeigt sie)
- Inventar: Items zählen, entfernen und erzeugen (`AddItems`)
- Dialoge lesen (4593 Infos) und eigene anlegen: Optionen erscheinen im Gesprächsmenü, Zeilen kommen ins Questlog, `OnInfoFinished`
- Speichern und Laden: eigene Quests und Dialoge kommen mit Status und Questlog zurück (über `OnQuestsLoaded`/`OnInfosLoaded`), auch nach erneutem Speichern
- Schalter im Spielstand (`SetGameEvent` und Co.)
- Vorlagen klonen (`CloneTemplate`) mit fester ID, eigenem Namen/Text (`RegisterText`): Klon-Items bleiben nach Speichern, Laden und Neustart im Inventar (ein Neustart-Test, Stapel mit neuen Items funktioniert)
- NPCs erzeugen und entfernen (`SpawnNPC`, `DestroyNPC`, `LookAt`, `IsNPC`), auch NPCs aus früheren Sitzungen nach dem Laden (Systemsektor + NPC-Prüfung); Welt-NPCs, Held und Welt-Objekte werden abgelehnt
- Fokus und Fadenkreuz: `GetFocusedEntity` (anvisierter NPC/Objekt), `PickCenter`/`GetPickPoint` (Entity unter dem Fadenkreuz mit exaktem Weltpunkt, Held ausgeschlossen), Kategorien `NPC`/`Building`/`Object`/`Landscape`/`Border`; Spawn am Zielpunkt bestätigt
- Override-Hooks mit `Super()`: `Override_ReceiveItem` (Protokoll) und `Override_InfoAvailable` (Dialogoption nachts verborgen)

## Nicht oder nur teilweise geprüft

- **Datei-Patches** ([Datei-Patches](patches.md)): im Spiel bestätigt für Quest-Dateien (`patch_demo`: `Abbas_Artefakt` XP 300 → 600, Skript liest 600). Offen: ob `.info`-Patches ankommen, weil das Spiel Infos aus
  der kompilierten `Infos.cpt` lädt, und komprimierte Archiveinträge (`Strings.pak`).
- **Teleport (`SetPosition`) und Spielzeit (`SetGameTime`)** sind implementiert und ohne Spiel getestet, im Spiel aber nicht bestätigt.
- **Attribut `"XP"`** las im Test 0. Quest-XP zahlt das Spiel selbst aus.
- **`DumpQuest` / `LoadQuestFile`** gaben im Spiel `false`; das Format der `.quest`-Dateien ist unbekannt.
- **Questliste:** `GetQuestCount()`/`GetQuestNameAt(i)` liefern im Spiel die echten Namen, Status und XP aller 571 Quests (Element der Liste = Zeiger auf ein `bTPOPureSmartPtr`, das Quest-Objekt liegt bei +8).
- **`ExecuteInfo`:** Skript einer Info ohne Gesprächsfenster ausführen. Läuft im Spiel nicht durch (kein `OnInfoFinished`), für Mods nicht zu empfehlen.

## Fehlt noch

| Funktion | Zustand |
|---|---|
| **Weitere Einstiegspunkte** | Override gibt es für die sieben Quest-/Dialog-Funktionen ([Hooks](hooks.md)); Zahlenargumente lassen sich mit `SetOverrideArg` ändern (ohne Spiel getestet, im Spiel zu bestätigen mit `hooks_demo`). Schaden/Treffer gibt es noch nicht. |
| **Entities erzeugen / NPCs spawnen** | `SpawnNPC` ist gebaut (Version 23), im Spiel aber noch nicht bestätigt. Offen: Tagesablauf/Routine, Speichern der erzeugten NPCs, Aufräumen. |
| **Bedingungen von Infos** | Items, Reputation, Fähigkeiten als Bedingung einer eigenen Info: die Listen (`CondItems`, ...) sind noch nicht schreibbar. Bedingungen prüfst du in `OnInfoFinished` im Skript. |
| **Mod-Archive (`PackMod`)** | Schreiben ist gebaut und gegen echte Community-Archive geprüft ([Mod-Archive](archives.md)); das Laden im Spiel ist erst mit `packmod_demo` zu bestätigen. Offen: Komprimierung, komprimierte Einträge lesen, Datei-Patches auf die Schichten `.mod`/`.m00` ausdehnen. |
| **Questlog selbst schreiben** | Einträge entstehen über Dialogzeilen (`SetInfoQuest`); direkt in das Log schreiben geht nicht. |
| **Spielseiten** | `GetActivePage()` liefert Zahlen (-1, 4, 5, 6, 17), die Bedeutung ist nur teilweise bekannt. |

## Vergleich mit dem G1R-Modkit

| G1R-Modkit | OG3 |
|---|---|
| Skripte in AngelScript | ja, AngelScript 2.38 |
| Mod-Pakete mit Reihenfolge | ja, `mods\<Name>\main.as` und `load_order.txt` |
| Script Hooking (Vererbung, `ReplaceParentClass`, `Super::`) | teilweise: die Spiellogik liegt in nativen DLLs, nicht in Skripten. Stattdessen `Override_...` mit `Super()` auf ausgewählten Game.dll-Funktionen und Spielereignisse |
| Script Patches (UnifiedDiff auf Spielskripte) | ja, als Unified-Diff auf die Textdateien des Spiels (`ScriptPatches\*.patch`), siehe [Datei-Patches](patches.md) |
| Konversationen (`UConversationTopic`) | ja, als Infos mit Skriptzeilen und `OnInfoFinished` |
| Quests (`UQuest`) | ja, als Funktionen auf dem Questsystem des Spiels |
| Blueprint-Varianten | nicht übertragbar (Gothic 3 hat keine Blueprints) |
