# Grenzen und Stand

Stand: 2026-11-10, Skript-Schnittstelle Version 39 (Alpha). Was sich zwischen Versionen ändern darf: [Versionsregeln](api-policy.md).
Einzelne Funktionen tragen in der [API-Referenz](api-reference.md) die Vermerke **Experimentell**, **Im Spiel nicht geprüft**, **Funktioniert nicht** oder **Veraltet**.

## Im Spiel geprüft

**Grundlagen**
- Mods laden, Hot-Reload, Fehlerisolation, Mods standardmäßig aus (`mod.ini`) und per `load_order.txt` zuschaltbar; Ereignisse `OnKey`, `OnFrame`, `OnWorldActivate/Deactivate`
- Eine Spielsitzung mit 14 Beispiel-Mods ohne Fehlerzeilen, auch nach der Absicherung aller Hooks gegen Nullzeiger der Engine (2026-11-10)
- Gleitkomma-Zustand der Engine bleibt erhalten (x87-Steuerwort durchgehend `007F`)

**Ereignisse und Overrides**
- Die sechs Questsystem-Ereignisse (`OnNPCKilled`, `OnNPCDefeated`, `OnReceiveItem` mit Itemname, `OnEnter`/`OnLeave`, `OnSpellCast`)
- Schaden: `Override_Damage`/`OnDamage` für NPCs und den Helden, bei Menschen, Tieren, Zaubern und Sturz, mit Waffe/Geschoss, Verursacher und Trefferart; Menge ändern und Treffer verhindern
- `OnStatusEffect` (Krankheit beim Waranbiss), `Override_InfoAvailable`, `Override_ReceiveItem`
- Speichern: `OnBeforeSave`/`OnAfterSave` laufen genau einmal je Speichern (Menü und Schnellspeichern) im Spiel-Thread
- Spielskripte nach Namen: `CallScript`, `Override_Script`/`OnScript` für angemeldete Namen (`WatchScript`)
- Konsole (`Override_ConsoleCommand`), Quest-Status (`OnQuestStatus`), Berührung (`OnTouch`), Händlerware (`OnTradeGenerated`)

**Spielwelt**
- Attribute lesen und ändern, Inventar (zählen, entfernen, erzeugen, Stapel löschen), Item-Qualität mit eigener Zeile „Lebensraub“ in der Beschreibung
- Eigene Talente mit Kachel im Charakterbogen (`perk_lifesteal`); Abschalten mit `ENTFERNEN = true`, dann Spielstand ohne Mod ohne Absturz
- Verletzen und Töten über die Blitz-Beschwörung mit Besitzer (`beliars_klaue`), Effekte starten (`StartEffect`)
- Vorlagen klonen (`CloneTemplate`) mit fester ID; Klon-Items bleiben nach Speichern, Laden und Neustart erhalten
- NPCs erzeugen und entfernen (`SpawnNPC`, `DestroyNPC`), auch aus früheren Sitzungen; Welt-NPCs, Held und Welt-Objekte werden abgelehnt
- Fokus und Fadenkreuz (`GetFocusedEntity`, `PickCenter`/`GetPickPoint`, Kategorien), Gruppe und Teleport (`TeleportTo`, `SetPosition`, `teleport_party`)
- Zeitrate (`SetTimeScale`) und Uhrfaktor (`SetClockFactor`)
- Eigenschaften lesen (`GetProperty`, `ListProperties`, `property_inspect`)

**Quests, Dialoge, Dateien**
- Quests anlegen, starten, abschließen, scheitern lassen, Status, Titel, Beschreibung, XP, Attribut-Belohnung; Questliste aller 571 Quests
- Dialoge lesen (4593 Infos) und eigene anlegen: Optionen im Gesprächsmenü, Zeilen im Questlog, `OnInfoFinished`
- Eigene Quests und Dialoge kommen nach Speichern und Laden mit Status und Questlog zurück (`OnQuestsLoaded`/`OnInfosLoaded`); Schalter im Spielstand (`SetGameEvent`)
- Datei-Patches auf Quest-Dateien (`patch_demo`); Mod-Archive (`PackMod`) werden geladen und überschreiben vorhandene Dateien
- Zeichnen über HUD und Inventar: Text (`DrawText`) und Bilder aus den GUI-Daten (`DrawImage`), `WorldToScreen`

**Installationen:** Entwicklerinstallation und eine stark gemoddete Steam-Kopie, siehe `docs/compatibility.md` im Repository.

## Nicht oder nur teilweise geprüft

- **Verwandlung** (`OnTransformStart`/`OnTransformEnd`): eingehängt, im Spiel noch nie ausgelöst.
- **`SetGameTime`:** ohne Spiel getestet, im Spiel nicht bestätigt.
- **Spielstand ohne Mod nach automatischem Entfernen:** `perk_lifesteal` nimmt Talent und Stapel in `OnBeforeSave` heraus (beim Laden war der Stapel 0). Der
  vollständige Prüfablauf (mit der Mod speichern, Mod aus, laden, Charakterbogen öffnen, Waffe ausrüsten) steht noch aus. Ob der Verweis `ActivateSkill2` an der
  Waffe im Spielstand hängt, ist nicht geprüft.
- **Datei-Patches auf `.info`-Dateien:** offen, weil das Spiel Infos aus der kompilierten `Infos.cpt` lädt; ebenso komprimierte Archiveinträge (`Strings.pak`).
- **`GenerateTradeGoods`:** liefert `true`, ob dabei andere Items entstehen, ist nicht geprüft.
- **`owner` im Gruppenkampf** ist eine Näherung (der aktuelle Gegner des Opfers, nicht zwingend der Schläger).
- **Attribut `"XP"`** las im Test 0. Quest-XP zahlt das Spiel selbst aus.
- **Funktioniert nicht:** `DumpQuest`/`LoadQuestFile` (immer `false`), `ExecuteInfo` (kein `OnInfoFinished`), `GetPickPosition` (immer Nullvektor).

## Bekannte Risiken

- **Dauerhafte Zustände im Spielstand.** Was ein Mod ins Spiel schreibt (Item-Stapel aus Klon-Vorlagen, Talente, Uhrfaktor), steht im Spielstand. Fehlt der
  Mod beim Laden, zeigen solche Einträge ins Leere; ein Spielstand mit dem Talent-Stapel ohne `perk_lifesteal` **stürzte beim Öffnen des Charakterbogens ab**
  (gemessen). Mods müssen solche Zustände in `OnBeforeSave` entfernen und in `OnAfterSave` wiederherstellen, siehe [Speichern und Laden](saving.md).
- **Feste Adressen.** Die Lebensraub-Zeile in der Item-Beschreibung hängt an zwei festen Adressen in `Game.dll`. Weichen ihre Bytes ab (z. B. in der
  Steam-Fassung), wird dieser eine Hook ausgelassen und das Protokoll sagt es; alle anderen Hooks hängen an exportierten Namen.
- **Andere Script-Plugins** mit eigenen Hooks (z. B. CombatFix) sind nicht zusammen mit dem Patch geprüft.

## Fehlt noch

| Funktion | Zustand |
|---|---|
| **Animationen** | Abspielen, Namen der Engine abfangen, eigene Animationen aus Blender: geplant, noch nichts gebaut |
| **Abhängigkeiten zwischen Mods** | `mod.ini` kennt nur an/aus; keine Angabe „braucht Mod X ab Version Y“ oder „braucht Skript-Schnittstelle N“ |
| **Schaden mit fester Menge, der tötet** | `HurtHitPoints` tötet nicht, `DoDamage` ist verdreht und veraltet; tödlicher Schaden nur über die Blitz-Beschwörung |
| **Verweis-Eigenschaften schreiben** | `SetProperty` schreibt Textwerte, Verweise auf andere Vorlagen nicht; dafür gibt es `SetTemplateRef`/`SetEntityRef` (experimentell) |
| **Bedingungen von Infos** | Items, Reputation, Fähigkeiten als Bedingung einer eigenen Info: die Listen (`CondItems`, ...) sind nicht schreibbar. Bedingungen prüfst du im Skript (`Override_InfoAvailable`) |
| **Questlog selbst schreiben** | Einträge entstehen über Dialogzeilen (`SetInfoQuest`); direkt ins Log schreiben geht nicht |
| **Talente vom Spiel lernen lassen** | `LearnSkill` am Klon liefert `false` (Voraussetzungen des Originals); die Wirkung eines eigenen Talents stellt die Mod selbst her |
| **Spielseiten** | `GetActivePage()` liefert Zahlen (-1, 4, 5, 6, 17), die Bedeutung ist nur teilweise bekannt |
| **Einfarbige Flächen zeichnen** | Die Rechteck-Funktionen von GUI und Renderer sind im Spiel leer; Ersatz ist ein Bild aus den GUI-Daten |
| **Selbsttest in der CI** | Die CI baut und packt; Selbsttest und Kompilierprüfung der Beispiele brauchen die `SharedBase.dll` des Spiels und laufen nur lokal |
| **Englische Dokumentation** | Die Doku ist deutsch |

Ideen aus dem Editor AdanosEdit, nichts davon verbindlich: Truhen und Schlösser (Inhalt, Schlosshärte, Schlüssel, auch für Türen); Tagesabläufe von NPCs
(Routinen, per Dialog umschaltbar); eigene Zonen in der Navigationskarte (nötig, damit `OnEnter`/`OnLeave` eigene Bereiche erkennen; Mods mit eigener
Navigationskarte dürfen nur einzeln integriert werden); NPC-Aussehen und -Ausstattung (Kopf, Haare, Kleidung, Gruppe, Waren, Beute); Quest-Vorlagen
(bringen, ausrichten, töten, besiegen, begleiten, Arenakampf, Ort erreichen).

## Vergleich mit dem G1R-Modkit

Der Kernunterschied: Im Gothic-1-Remake ist die Spiellogik selbst in AngelScript geschrieben, und das Modkit liefert die Spielskripte mit. Ein Mod kann dort jede
Skriptklasse ableiten oder per Patch ändern. In Gothic 3 liegt die Logik in nativen DLLs; ein Mod kann nur an den Stellen eingreifen, die der Patch freilegt.

| G1R-Modkit | OG3 |
|---|---|
| Skripte in AngelScript | ja, AngelScript 2.38 |
| Mod-Pakete mit Reihenfolge | ja, `mods\<Name>\main.as`, `mod.ini` und `load_order.txt` |
| Abhängigkeiten mit Versionsangaben | nein, siehe „Fehlt noch“ |
| Script Hooking (Vererbung, `ReplaceParentClass`, `Super::`) | teilweise: `Override_...` mit `Super()` an ausgewählten Spielfunktionen (Questsystem, Schaden, Dialog, Quest-Status, Konsole, Händler, Spielskripte nach Namen) und Ereignisse |
| Script Patches (Unified-Diff auf Spielskripte) | ja, als Unified-Diff auf die Textdateien des Spiels (`ScriptPatches\*.patch`), siehe [Datei-Patches](patches.md) |
| Konversationen (`UConversationTopic`) | ja, als Infos mit Skriptzeilen und `OnInfoFinished` |
| Quests (`UQuest`) | ja, als Funktionen auf dem Questsystem des Spiels |
| Eigene Inhalte (Modelle, Levels) über den Unreal-Editor | nein; `PackMod` packt fertige Dateien in Archive, Werkzeuge dafür kommen von außen |
| Verteilung über den Steam Workshop | nein; Mods werden von Hand in den Mod-Ordner gelegt |
| Blueprint-Varianten | nicht übertragbar (Gothic 3 hat keine Blueprints) |
