# Grenzen und Stand

Stand: 2026-11-02, Skript-Schnittstelle Version 28.

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
| **Talente (Perks)** | Eigenes Talent mit Kachel im Charakterbogen und Bonustalent an einer Waffe geht (Beispiel `perk_lifesteal`): Talent-Vorlage klonen (`CloneTemplate` + Tag, Name über `FO_It_Perk_<Tag>`), Lern-Item `It_Perk_<Name>` klonen und dessen `Skill`-Verweis mit `SetTemplateRef` auf das neue Talent setzen, den Item-Stapel dem Helden geben (`AddItems`; der Bogen listet die Lern-Item-Stapel im Inventar, ausgegraut wenn nicht gelernt) und `ActivateSkill2` der Waffe auf das Talent setzen (die Engine zählt es dann beim Ausrüsten mit). Zustand lesen: `HasLearnedSkill`, `IsSkillActive`, `GetSkillActivationCount`. **Zu beachten:** Der Stapel steht im Spielstand; ohne die Mod zeigt er auf eine fehlende Vorlage, und das Spiel **stürzt beim Öffnen des Charakterbogens ab** (gemessen). Zum Abschalten die Mod einmal mit `ENTFERNEN = true` laufen lassen (nimmt Talent und Stapel mit `DeleteItemStack` heraus; `RemoveItems` genügt nicht, weil Lern-Items `Permanent` sind und ein leerer Stapel bleibt), speichern, dann die Mod abschalten. Getestet: Spielstand danach ohne Mod laden und den Charakterbogen öffnen läuft ohne Absturz. Es gibt noch kein Ereignis vor dem Speichern, mit dem sich das automatisch machen ließe. `LearnSkill` am Klon liefert `false` (Voraussetzungen des Originals). Die Wirkung eines eigenen Talents muss die Mod selbst herstellen (z. B. `OnDamage`) |
| **Weitere Einstiegspunkte** | Override gibt es für die sieben Quest-/Dialog-Funktionen ([Hooks](hooks.md)); Zahlenargumente lassen sich mit `SetOverrideArg` ändern (ohne Spiel getestet, im Spiel zu bestätigen mit `hooks_demo`). Schaden, Treffer, Statuseffekte und Item-Qualität sind inzwischen vorhanden ([Hooks](hooks.md), [Referenz](reference.md)). |
| **Entities erzeugen / NPCs spawnen** | `SpawnNPC` ist gebaut (Version 23), im Spiel aber noch nicht bestätigt. Offen: Tagesablauf/Routine, Speichern der erzeugten NPCs, Aufräumen. |
| **Bedingungen von Infos** | Items, Reputation, Fähigkeiten als Bedingung einer eigenen Info: die Listen (`CondItems`, ...) sind noch nicht schreibbar. Bedingungen prüfst du in `OnInfoFinished` im Skript. |
| **Mod-Archive (`PackMod`)** | Schreiben ist gebaut und gegen echte Community-Archive geprüft ([Mod-Archive](archives.md)); das Laden und das Überschreiben vorhandener Dateien sind im Spiel bestätigt (mit der Einschränkung zu den vom Spiel angelegten Kopien, siehe Archive). Offen: Komprimierung, komprimierte Einträge lesen, Datei-Patches auf die Schichten `.mod`/`.m00` ausdehnen. |
| **Questlog selbst schreiben** | Einträge entstehen über Dialogzeilen (`SetInfoQuest`); direkt in das Log schreiben geht nicht. |
| **Truhen und Schlösser** | Inhalt (zufällig über Schatzsets oder fest), Schlosshärte (offen bis schwer, nur mit Schlüssel), Schlüssel; auch für Türen. Idee aus dem Editor AdanosEdit, nichts verbindlich. |
| **Tagesabläufe (Routinen) von NPCs** | Arbeit, Freizeit, Schlaf mit Routinepunkten; mehrere Abläufe pro NPC, per Dialog umschaltbar (damit ein NPC nach einer Quest bleibt). Idee aus AdanosEdit. |
| **Eigene Zonen und die Navigationskarte** | Nötig, damit `OnEnter`/`OnLeave` auch eigene Bereiche erkennen: Gothic 3 erkennt Zonen nur, wenn sie in der Navigationskarte der Mod stehen. Mods mit eigener Navigationskarte dürfen nur einzeln integriert werden. Idee aus AdanosEdit. |
| **NPC-Aussehen und -Ausstattung** | Kopf, Haare, Bart, Kleidung, Gruppe und Enklave, Händlerwaren, Waffen, Beute, Taschendiebstahl. Idee aus AdanosEdit. |
| **Quest-Vorlagen** | Ein Katalog gängiger Quest-Arten mit automatisch erzeugten Dialogen: Gegenstände bringen, etwas ausrichten, töten, besiegen (Zweikampf), jemanden zu einer Person bringen, jemandem folgen, vertreiben, Arenakampf, Ort erreichen, Truhe ausrauben. Eher ein Baustein für spätere Mods als für das Kit selbst. Idee aus AdanosEdit. |
| **Eigenschaften schreiben** | Lesen geht (`GetProperty`, `ListProperties`), Schreiben nicht. Damit ließe sich zum Beispiel das „Bonustalent“ (`ActivateSkill2` ist bei Waffen frei) für eigene Eigenschaften nutzen: mit einer geklonten Talent-Vorlage (`CloneTemplate` von `Perk_OrcSlayer`) und einem Schreibzugriff auf `ActivateSkill`. Unklar, ob das für Item-Instanzen speichert und stapelt; nicht untersucht. |
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
