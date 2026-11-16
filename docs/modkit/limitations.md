# Grenzen und Stand

Stand: Skript-Schnittstelle Version 46. Was sich zwischen Versionen ändern darf: [Versionsregeln](api-policy.md).
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
- Spielstand ohne Mod (2026-11-10): mit `perk_lifesteal` gespeichert (Talent und Stapel in `OnBeforeSave` entnommen), Mod abgeschaltet, Neustart, geladen,
  Charakterbogen geöffnet, Waffe ab- und wieder ausgerüstet: kein Absturz, keine Fehlerzeilen. Der Verweis `ActivateSkill2` an der Waffe, der beim Speichern
  noch auf das Talent zeigt, stört ohne die Mod nicht. Wiederholt 2026-11-13 auf einer sauberen Steam-Kopie (Community Patch 1.75 + PU), dort zusätzlich ganz ohne Patch.
  Dabei gefunden und behoben: Nach dem **Laden** legte die Mod den Stapel nicht neu an (Kachel und Talent fehlten bis zum nächsten Speichern); jetzt bei
  jeder Weltaktivierung
- Verletzen und Töten über die Blitz-Beschwörung mit Besitzer (`beliars_klaue`), Effekte starten (`StartEffect`)
- Vorlagen klonen (`CloneTemplate`) mit fester ID; Klon-Items bleiben nach Speichern, Laden und Neustart erhalten
- NPCs erzeugen und entfernen (`SpawnNPC`, `DestroyNPC`), auch aus früheren Sitzungen; Welt-NPCs, Held und Welt-Objekte werden abgelehnt
- Fokus und Fadenkreuz (`GetFocusedEntity`, `PickCenter`/`GetPickPoint`, Kategorien), Gruppe und Teleport (`TeleportTo`, `SetPosition`, `teleport_party`)
- Zeitrate (`SetTimeScale`) und Uhrfaktor (`SetClockFactor`)
- Eigenschaften lesen (`GetProperty`, `ListProperties`, `demo_properties`)

**Quests, Dialoge, Dateien**
- Quests anlegen, starten, abschließen, scheitern lassen, Status, Titel, Beschreibung, XP, Attribut-Belohnung; Questliste aller 571 Quests
- Dialoge lesen (4593 Infos) und eigene anlegen: Optionen im Gesprächsmenü, Zeilen im Questlog, `OnInfoFinished`
- Eigene Quests und Dialoge kommen nach Speichern und Laden mit Status und Questlog zurück (`OnQuestsLoaded`/`OnInfosLoaded`); Schalter im Spielstand (`SetGameEvent`)
- Datei-Patches auf Quest-Dateien (`demo_patch`); Mod-Archive (`PackMod`) werden geladen und überschreiben vorhandene Dateien
- Zeichnen über HUD und Inventar: Text (`DrawText`) und Bilder aus den GUI-Daten (`DrawImage`), `WorldToScreen`

**Mod-Pakete für Spieler** ([Mods weitergeben](mod-package.md#mods-weitergeben))
- Laufzeit als Script-DLL (`scripts\Script_G3ASM_046.dll`) ohne Loader, ohne `g3asm.ini` und ohne `g3asm\`-Ordner: alle Hooks aktiv, gepackte Mods aus
  `Data\G3ASM.mod`/`.m00` entpackt und geladen, Stringtabellen, Lebensraub, Speichern, sauberes Ende (saubere Steam-Kopie, 2026-11-16)
- Archive mit dem Namen `G3ASM.*` öffnet das Spiel nicht; Start, Laden und Speichern laufen normal
- Die Script-DLL startet nach `OnPostInitializeModules`; `main()` der Mods läuft beim ersten Bild. **Datei-Patches kommen auf diesem Weg zu spät**
  (gemessen: Quest-XP blieb beim Original); Spieldateien gehören in Mod-Pakete als Archiv

**Installationen:** Voraussetzung sind Community Patch 1.75 und Parallel-Universe-Patch. Geprüft: Entwicklerinstallation, eine stark gemoddete und eine saubere Steam-Kopie, siehe `docs/compatibility.md` im Repository.

## Nicht oder nur teilweise geprüft

- **Schema-Dump** (`DumpPropertySchema`, Version 43): im Spiel bestätigt (2026-11-11, 273 Klassen, Hauptmenü und nach dem Laden gleich); drei Eigenschaften ohne Namen. Die Getter `bCPOTypeListItem::GetBaseClassName` und `bCPropertyObjectTypeBase::GetPropertyType(int)` blieben im Test hängen (vermutlich ein Assert-Dialog der Original-DLL); die Liste wird deshalb direkt aus dem Speicher gelesen (SDK-Aufbau).
- **KI-Tracer** (`aitrace=1` in `g3asm.ini`, nur Protokoll): sechs Hooks auf `gCScriptRoutine_PS` (Zustand, Aufgabe, Routine, KI-Modus, Aktion) und `gCNavigation_PS::SetRoutine`. Im Spiel gemessen (2026-11-11, 3,5 min, 40 NPCs, 574 Zeilen, kein Ruckeln): Es kommen nur **Aufgabe**, **KI-Modus** und **Aktion**; Zustand, Routine und Navigation-Routine wurden nie gesetzt. Die Aufgabe trägt die Zustandsnamen (`ZS_Ambient`, `ZS_HangAround`, `ZS_Guard`, `ZS_WatchFight`, `ZS_Sleep`, beim Helden `PS_Normal`, `ZS_Talk`, `PS_Interact`); Zustandswechsel sieht man also über `SetCurrentTask`.
- **Angriffsgeschwindigkeit** (`Override_CombatMove`, experimentell): Messpunkt gebaut (`anispeedlog=1`): `eCWrapper_emfx2Actor::SetPlaySpeed`/`GetPlaySpeed` der Engine.dll, die Game.dll importiert. Erste Messung (2026-11-11): fast alle Aufrufe von `Game.dll+0xDC5AA` mit fließenden Werten 0,34 bis 0,98 für die Bewegungsarten 0 bis 3 (vermutlich Überblendung der Laufbewegung), einmal `Game.dll+0xDA21A` mit 1,778. Die Messung lief nur 2 s (Zeilenzahl erschöpft) und erfasste keinen Kampf. Zweite Messung mit Kampf (2026-11-11) und Assembler: `SetPlaySpeed` steuert nur die **Laufbewegung** (`+0xDC5AA`, Schleife über vier Bewegungsschichten, Wert aus dem Betrag der Bewegungsgeschwindigkeit, daher Werte bis über 10000) und das **Drehen auf der Stelle** (`+0xDA031`/`+0xDA21A`, Wert = Winkel × Länge / 90). Die Kampfschleife `gCScriptProcessingUnit::sAICombatMoveItlLoop` liest die Geschwindigkeit nur, um eine Pause (0) zu erkennen. Angriffe starten über `eCVisualAnimation_PS::PlayMotion(Art, eSMotionDesc*)`; der Aufbau von `eSMotionDesc` ist im SDK leer; gemessen: +8 Geschwindigkeit, +0x10 Gewicht. Angriffe kommen aus `gCScriptProcessingUnit::sAICombatMoveStart`, das die Geschwindigkeit aus seinen Argumenten (+0x10) übernimmt (ein NPC mit 1,3). Daraus `Override_CombatMove` (Version 45, experimentell), im Spiel bestätigt (2026-11-11: Faktor 2 sichtbar schneller, Schaden normal; `Recover` meldet einmal die Waffe als `self`).
- **Lernkosten** (`Override_LearnCost`, Version 42): im Spiel bestätigt (Messmod `probe_learn_cost`, 2026-11-11): STR +5 bei Bufford zeigte und kostete 3 LP und 125 Gold statt 5 LP und 250 Gold, der Held bekam +5 STR; die Bezahlung der Lernpunkte lief genau einmal, auch `ALC` geht durch den Hook. Sieben feste Stellen; Bytes und Stapelaufbau stimmen in der Entwicklerinstallation und der Steam-Game.dll überein. Nur Attribute, keine Talente. Nicht ausgelöst: die Verdopplung mit „Alternative Balance“ ab 250 (nur am Code gelesen; im Test war der Wert darunter oder der Schalter aus).
- **Monsterschaden** (`Override_MonsterDamage`, Version 42): im Spiel bestätigt (Wolf gegen NPC, 2026-11-10); der Teiler 2 für beschworene Angreifer ist nicht ausgelöst. Hängt wie `Override_LearnCost` an einer festen Stelle statt an einem Export; die Bytes der Stelle und des Codes davor stimmen in der Entwicklerinstallation, im Steam-Original und in der Steam-Kopie überein, obwohl sich die Script_Game.dll sonst unterscheidet. Nicht zusammen mit `Script_MonsterDamage.dll`, die dieselbe Stelle ändert.
- **KI-Zustände** (`Override_AI`/`OnAI` mit `AI_STATE`, Version 41): Zustand, Callback und KI-Funktion sind im Spiel bestätigt (Zustand nach der Korrektur mit kopiertem Namen, 2026-11-10). Routinen (`AI_ROUTINE`) sind nie ausgelöst worden.
- **Satztexte:** `GetSpeechText` liefert Dialogzeilen und Befehle des Helden; das Umgebungsgerede (`SVM_..._Smalltalk_NN`) hat keinen gefundenen Text.
- **Abhängigkeiten in `mod.ini`** (`version`, `api`, `requires`, Version 40): ohne Spiel getestet (Testmods in `tests/mods`), im Spiel noch nicht.
- **Mod-Pakete mit dem Union Starter:** im Spiel geprüft (2026-11-16, Steam-Kopie mit vielen Starter-Mods und 20 weiteren Script-Plugins wie
  `Script_CombatFix.dll` und `Script_G3Fixes.dll`): Paket aus `release` installiert, Laufzeit startet als Script-DLL, Mod wird aus `Data\G3ASM.mod`
  entpackt, Quest aus dem mitgelieferten Archiv, Dialoge und Quest funktionieren. Zwei Mods mit verschiedenen Laufzeit-Versionen sind noch nicht geprüft.
  Das Spiel erst starten, wenn der Starter die Installation gemeldet hat: Während er die Archive neu einspielt, ist `Data\` unvollständig.
  Der Starter legt beim Installieren keine neuen Ordner unter `ScriptFiles\` an (nur vorhandene wie `scripts\`); Pakete aus `release` brauchen nur `scripts\`.
- **Datei-Patches auf `.info`-Dateien:** offen, weil das Spiel Infos aus der kompilierten `Infos.cpt` lädt; ebenso komprimierte Archiveinträge (`Strings.pak`).
- **`owner` im Gruppenkampf** ist eine Näherung (der aktuelle Gegner des Opfers, nicht zwingend der Schläger).
- **Attribut `"XP"`** las im Test 0. Quest-XP zahlt das Spiel selbst aus.
- **Entfernt mit Version 46:** `DoDamage` (verdreht; Ersatz `Kill`), `ExecuteInfo` (kein `OnInfoFinished`), `GetPickPosition` (immer Nullvektor), `DumpQuest` (schrieb mit keinem Pfad eine Datei).
- **Quest-Dateien:** `LoadQuestFile` liest mit dem virtuellen Pfad `Data/Quests/<Welt>/...` (`GetQuestFilePath`), im Spiel bestätigt; Schreiben geht nicht.
- **Händlerware neu erzeugen** (`GenerateTradeGoods`, experimentell): liefert `true`, die Ware bleibt aber gleich (zweimal gemessen).

## Bekannte Risiken

- **Dauerhafte Zustände im Spielstand.** Was eine Mod ins Spiel schreibt (Item-Stapel aus Klon-Vorlagen, Talente, Uhrfaktor), steht im Spielstand. Fehlt der
  Mod beim Laden, zeigen solche Einträge ins Leere; ein Spielstand mit dem Talent-Stapel ohne `perk_lifesteal` **stürzte beim Öffnen des Charakterbogens ab**
  (gemessen). Mods müssen solche Zustände in `OnBeforeSave` entfernen und in `OnAfterSave` wiederherstellen, siehe [Speichern und Laden](saving.md). Mit diesem Muster lädt der Spielstand auch ohne die Mod (geprüft mit `perk_lifesteal`).
- **Feste Adressen.** Drei Funktionen hängen an festen Stellen statt an exportierten Namen: die Lebensraub-Zeile in der Item-Beschreibung (`Game.dll`),
  `Override_LearnCost` (sieben Stellen in `Game.dll`) und `Override_MonsterDamage` (`Script_Game.dll`). Weichen die Bytes ab (z. B. in einer binär
  veränderten Steam-Fassung), wird nur die betroffene Funktion ausgelassen und das Protokoll sagt es; alle anderen Hooks hängen an exportierten Namen.
- **Andere Script-Plugins** mit eigenen Hooks (z. B. CombatFix) sind nicht zusammen mit dem Modkit geprüft.

## Fehlt noch

| Funktion | Zustand |
|---|---|
| **Animationen** | Abspielen, Namen der Engine abfangen, eigene Animationen aus Blender: geplant, noch nichts gebaut |
| **Schaden mit fester Menge, der tötet** | `HurtHitPoints` tötet nicht; töten ohne Schaden mit `Entity::Kill` (ab Version 46, mit Erfahrung und Beute), tödlicher Schaden nur über die Blitz-Beschwörung |
| **Verweis-Eigenschaften schreiben** | `SetProperty` schreibt Textwerte, Verweise auf andere Vorlagen nicht; dafür gibt es `SetTemplateRef`/`SetEntityRef` (experimentell) |
| **Bedingungen von Infos** | Items, Reputation, Fähigkeiten als Bedingung einer eigenen Info: die Listen (`CondItems`, ...) sind nicht schreibbar. Bedingungen prüfst du im Skript (`Override_InfoAvailable`) |
| **Questlog selbst schreiben** | Einträge entstehen über Dialogzeilen (`SetInfoQuest`); direkt ins Log schreiben geht nicht |
| **Talente vom Spiel lernen lassen** | `LearnSkill` am Klon liefert `false` (Voraussetzungen des Originals); die Wirkung eines eigenen Talents stellt die Mod selbst her |
| **Spielseiten** | `GetActivePage()` liefert Zahlen (-1, 4, 5, 6, 17), die Bedeutung ist nur teilweise bekannt |
| **Einfarbige Flächen zeichnen** | Die Rechteck-Funktionen von GUI und Renderer sind im Spiel leer; Ersatz ist ein Bild aus den GUI-Daten |
| **Messungen in der CI** | Die CI testet mit einer Ersatz-`SharedBase.dll`; was das Verhalten des Originals misst, läuft nur lokal mit den Spieldateien |
| **Englische Dokumentation** | Die Doku ist deutsch |

Ideen aus dem Editor AdanosEdit, nichts davon verbindlich: Truhen und Schlösser (Inhalt, Schlosshärte, Schlüssel, auch für Türen); Tagesabläufe von NPCs
(Routinen, per Dialog umschaltbar); eigene Zonen in der Navigationskarte (nötig, damit `OnEnter`/`OnLeave` eigene Bereiche erkennen; Mods mit eigener
Navigationskarte dürfen nur einzeln integriert werden); NPC-Aussehen und -Ausstattung (Kopf, Haare, Kleidung, Gruppe, Waren, Beute); Quest-Vorlagen
(bringen, ausrichten, töten, besiegen, begleiten, Arenakampf, Ort erreichen).

## Vergleich mit dem G1R-Modkit

Der Kernunterschied: Im Gothic-1-Remake ist die Spiellogik selbst in AngelScript geschrieben, und das Modkit liefert die Spielskripte mit. Eine Mod kann dort jede
Skriptklasse ableiten oder per Patch ändern. In Gothic 3 liegt die Logik in nativen DLLs; eine Mod kann nur an den Stellen eingreifen, die das Modkit freilegt.

| G1R-Modkit | G3ASM |
|---|---|
| Skripte in AngelScript | ja, AngelScript 2.38 |
| Mod-Pakete mit Reihenfolge | ja, `mods\<Name>\main.as`, `mod.ini` und `load_order.txt` |
| Abhängigkeiten mit Versionsangaben | ja, `version`, `api` und `requires` in der `mod.ini` (ab Version 40); keine Angabe für Unverträglichkeiten |
| Einstellungen je Mod | ja, Vorgaben in `mod.ini [settings]`, Benutzerwerte in `g3asm\settings\<Mod>.ini` (ab Version 44); kein Einstellungsmenü im Spiel |
| Schnittstellen zwischen Mods (Klassen, Interfaces) | teilweise: Funktionsaufruf mit Text (`CallMod`) und Nachrichten (`SendModMessage`/`OnModMessage`, ab Version 44); keine gemeinsamen Klassen oder Typen |
| Script Hooking (Vererbung, `ReplaceParentClass`, `Super::`) | teilweise: `Override_...` mit `Super()` an ausgewählten Spielfunktionen (Questsystem, Schaden, Dialog, Quest-Status, Konsole, Händler, Spielskripte nach Namen) und Ereignisse |
| Script Patches (Unified-Diff auf Spielskripte) | ja, als Unified-Diff auf die Textdateien des Spiels (`ScriptPatches\*.patch`), siehe [Datei-Patches](patches.md) |
| Konversationen (`UConversationTopic`) | ja, als Infos mit Skriptzeilen und `OnInfoFinished` |
| Quests (`UQuest`) | ja, als Funktionen auf dem Questsystem des Spiels |
| Eigene Inhalte (Modelle, Levels) über den Unreal-Editor | nein; `PackMod` packt fertige Dateien in Archive, Werkzeuge dafür kommen von außen |
| Verteilung über den Steam Workshop | nein; Mods werden von Hand in den Mod-Ordner gelegt |
| Blueprint-Varianten | nicht übertragbar (Gothic 3 hat keine Blueprints) |
