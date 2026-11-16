# Grenzen

Was das Modkit kann, was nicht geht und was noch niemand geprüft hat. Einzelne Funktionen tragen in der [API-Referenz](api-reference.md) zusätzlich
die Vermerke **Experimentell**, **Im Spiel nicht geprüft**, **Funktioniert nicht** oder **Veraltet**; was sich zwischen Versionen ändern darf, steht in den
[Versionsregeln](api-policy.md).

**Grundsätzlich:** Die Spiellogik von Gothic 3 liegt in nativen DLLs. Eine Mod greift nur an den Stellen ein, die das Modkit freilegt.

## Im Spiel geprüft

**Grundlagen**
- Mods laden, Hot-Reload, Fehler in einer Mod stören die anderen nicht; Mods standardmäßig aus (`mod.ini`) und per `load_order.txt` zuschaltbar.
- Eine Spielsitzung mit 14 Beispiel-Mods gleichzeitig ohne Fehlerzeilen.
- Die Gleitkomma-Einstellung des Spiels bleibt unverändert (sonst rechnet das Spiel ungenau, etwa verwaschene Texturen).

**Ereignisse und Overrides**
- Die sechs Ereignisse des Questsystems (`OnNPCKilled`, `OnNPCDefeated`, `OnReceiveItem`, `OnEnter`/`OnLeave`, `OnSpellCast`).
- Schaden (`Override_Damage`/`OnDamage`) bei NPCs und beim Helden, durch Menschen, Tiere, Zauber und Sturz; Menge ändern und Treffer verhindern.
- `OnStatusEffect`, `Override_InfoAvailable`, `Override_ReceiveItem`, `Override_QuestStatus`/`OnQuestStatus`, `Override_ConsoleCommand`, `OnTouch`,
  `OnTradeGenerated`, `OnTransformStart`/`OnTransformEnd`.
- Speichern: `OnBeforeSave`/`OnAfterSave` laufen genau einmal je Speichern, auch beim Schnellspeichern.
- Spielskripte nach Namen (`CallScript`, `Override_Script`/`OnScript`), KI-Zustände, KI-Funktionen und Callbacks (`Override_AI`/`OnAI`), gesprochene
  Sätze (`OnAIOutput`, `GetSpeechText`).
- `Override_LearnCost` (auch `ALC`), `Override_MonsterDamage`.

**Spielwelt**
- Attribute, Inventar (zählen, entfernen, erzeugen, Stapel löschen), Item-Qualität mit eigener Zeile „Lebensraub“ in der Beschreibung.
- Eigene Talente mit Kachel im Charakterbogen (`perk_lifesteal`).
- Töten (`Kill`) mit Erfahrung und Beute; Verletzen und Töten über eine Blitz-Beschwörung mit Besitzer (`beliars_klaue`); Effekte starten.
- Vorlagen klonen; Items aus Klonen bleiben nach Speichern, Laden und Neustart erhalten.
- NPCs erzeugen und entfernen, auch aus früheren Sitzungen.
- Fokus und Fadenkreuz, Gruppe und Teleport (`teleport_party`), Zeitrate, Uhrfaktor, Spielzeit setzen.
- Eigenschaften lesen (`GetProperty`, `ListProperties`), Verweise setzen (`SetTemplateRef`, `SetEntityRef`), Schema aller Eigenschaften (`DumpPropertySchema`).

**Quests, Dialoge, Dateien**
- Quests anlegen, steuern, mit Titel, Beschreibung, XP und Attribut-Belohnung; alle 571 Quests des Spiels aufzählen; Quest-Dateien lesen.
- Dialoge lesen (4593 Infos) und eigene anlegen: Optionen im Gesprächsmenü, Zeilen im Questlog, `OnInfoFinished`.
- Eigene Quests und Dialoge kommen nach Speichern und Laden mit Status und Questlog zurück; Schalter im Spielstand (`SetGameEvent`).
- Datei-Patches auf Quest-Dateien (`demo_patch`); Mod-Archive (`PackMod`) werden geladen und überschreiben vorhandene Dateien.
- Zeichnen über HUD und Inventar: Text, Bilder, `WorldToScreen`.

**Mod-Pakete für Spieler**
- Die Laufzeit startet als Script-DLL ohne Loader, ohne `g3asm.ini` und ohne `g3asm\`-Ordner: alle Hooks aktiv, gepackte Mods werden entpackt und
  geladen, Speichern und Beenden laufen normal.
- Über den Union Starter auf einer Installation mit vielen anderen Starter-Mods und 20 weiteren Script-Plugins: Paket installiert, Dialoge und
  Quest aus dem mitgelieferten Archiv funktionieren.
- Spielstand ohne die Mod: Ein mit `perk_lifesteal` gespeicherter Spielstand lädt ohne die Mod ohne Absturz, weil die Mod ihre Zustände vor dem
  Speichern entfernt (siehe [Bekannte Risiken](#bekannte-risiken)).

**Installationen:** Voraussetzung sind Community Patch 1.75 und Parallel-Universe-Patch. Welche Installationen geprüft sind, steht in der
[Kompatibilität](../compatibility.md).

## Geht nicht

Im Spiel gemessen oder im Spielcode untersucht:

- **Einfarbige Flächen zeichnen:** Die Rechteck-Funktionen des Spiels zeichnen nichts; es gehen nur Text und Bilder.
- **Eigene Talente vom Spiel lernen lassen:** `LearnSkill` liefert für ein geklontes Talent `false`; die Wirkung stellt die Mod selbst her.
- **Verweise per `SetProperty` setzen:** liefert `true` ohne Wirkung; dafür gibt es `SetTemplateRef` und `SetEntityRef`.
- **Ein Treffer-Hook vor der Schadensberechnung:** Die Spiel-DLLs bieten dafür keinen Einstieg. Eingreifen lässt sich über das Spielskript `OnHit`,
  `Override_MonsterDamage` und `Override_Damage` ([Hooks](hooks.md#schaden-ohne-treffer-hook)).
- **Spielskripte, die das Spiel intern direkt aufruft** (`AssessHit`, `IsDeadlyDamage`, `CanBurn`), lassen sich nicht nach Namen abfangen.
- **Datei-Patches bei Spielern:** Die Laufzeit startet als Script-DLL erst, nachdem das Spiel seine Dateien erfasst hat. Mod-Pakete liefern geänderte
  Spieldateien deshalb als Archiv; Datei-Patches wirken nur mit dem Loader.
- **Quest-Dateien schreiben.**
- **Das Questlog direkt beschreiben:** Einträge entstehen über Dialogzeilen (`SetInfoQuest`).
- **Bedingungen eigener Infos** (Items, Reputation, Fähigkeiten) lassen sich nicht setzen; das Skript prüft sie selbst in `Override_InfoAvailable`.
- **Text für Umgebungsgerede:** `GetSpeechText` findet für `SVM_..._Smalltalk_NN` keinen Text.
- **Attribut `"XP"`** liest 0. Quest-XP zahlt das Spiel selbst aus.

## Gibt es nicht

Nicht gebaut oder noch nicht untersucht, also nicht als unmöglich erwiesen:

- Animationen: abspielen, abfangen, eigene einbinden.
- Die Welt bearbeiten (Objekte setzen), eigene Modelle.
- Eigene Bedienelemente mit Eingabe, ein Einstellungsmenü im Spiel (Einstellungen gehen über die `mod.ini`).
- Datei-Patches auf komprimierte Archiveinträge (`Strings.pak`) und auf Binärdateien. Datei-Patches lesen als Grundlage immer die Fassung aus dem
  `.pak`, auch wenn ein Mod-Archiv dieselbe Datei ersetzt.
- Die Bedeutung aller Werte von `GetActivePage()`.
- Die ausführliche Doku auf Englisch (es gibt die [englische README](../../README.en.md)).

## Experimentell

Läuft, die Wirkung ist aber nicht vollständig geprüft; Name und Verhalten dürfen sich noch ändern:

- `HurtHitPoints`: senkt Lebenspunkte, tötet nicht.
- `SetProperty`: schreibt nur Textwerte.
- `GenerateTradeGoods`: liefert `true`, die Ware bleibt bisher gleich.
- `Override_CombatMove`: Schläge werden sichtbar schneller; bei `Recover` kann die Waffe statt des Kämpfers kommen.

## Nicht getestet

- **GOG- und CD/DVD-Version.** Ohne DXVK nur auf einer Installation. Ohne Community Patch 1.75 oder Parallel-Universe-Patch nicht unterstützt.
- **Zusammen mit anderen Script-Plugins**, die eigene Hooks setzen (z. B. CombatFix). `Script_MonsterDamage.dll` und
  `Script_LearningThresholdTweaks.dll` ändern dieselben Stellen wie das Modkit; sind sie installiert, bleiben `Override_MonsterDamage` bzw.
  `Override_LearnCost` aus.
- **Im Spiel nie ausgelöst:** KI-Routinen (`AI_ROUTINE`), der Teiler 2 bei `Override_MonsterDamage` (beschworene Angreifer) und die verdoppelten
  Lernkosten mit „Alternative Balance“.
- **Abhängigkeiten in der `mod.ini`** (`version`, `api`, `requires`): nur ohne Spiel getestet.
- **Datei-Patches auf `.info`-Dateien:** Das Spiel lädt Dialoge aus der kompilierten `Infos.cpt`; ob Patches dort ankommen, ist offen.
- **`owner` bei `OnDamage`** im Kampf mehrerer gegen einen ist eine Näherung (der aktuelle Gegner des Opfers, nicht sicher der Schläger).
- **Zwei Mods mit verschiedenen Laufzeit-Versionen** nebeneinander beim Spieler.
- **Ein Spielstand mit `perk_lifesteal` auf einer Installation mit vielen fremden Mods** stürzte beim Öffnen des Charakterbogens ab, auch ohne die
  Mod; die Ursache ist offen. Auf einer sauberen Installation tritt das nicht auf.

## Bekannte Risiken

- **Dauerhafte Zustände im Spielstand.** Was eine Mod ins Spiel schreibt (Items aus geklonten Vorlagen, Talente, Uhrfaktor), steht im Spielstand.
  Fehlt die Mod beim Laden, zeigen solche Einträge ins Leere; ein Spielstand mit dem Talent von `perk_lifesteal` **stürzte ohne die Mod beim Öffnen
  des Charakterbogens ab**. Mods entfernen solche Zustände deshalb in `OnBeforeSave` und stellen sie in `OnAfterSave` wieder her
  ([Speichern und Laden](saving.md)). Mit diesem Muster lädt der Spielstand auch ohne die Mod.
- **Feste Stellen im Spielcode.** Drei Funktionen hängen nicht an benannten Funktionen des Spiels, sondern an festen Stellen: die Zeile „Lebensraub“ in
  der Item-Beschreibung, `Override_LearnCost` und `Override_MonsterDamage`. Weicht die Spielversion dort ab, lässt das Modkit nur diese Funktion aus
  und meldet es im Protokoll; alles andere läuft weiter.
- **Union Starter:** Das Spiel erst starten, wenn der Starter die Installation gemeldet hat; während er Archive neu einspielt, ist `Data\` unvollständig.

## Vergleich mit dem Modkit des Gothic-1-Remakes

Im Gothic-1-Remake ist die Spiellogik selbst in AngelScript geschrieben, und das Modkit liefert die Spielskripte mit; eine Mod kann jede Skriptklasse
ableiten oder per Patch ändern. In Gothic 3 liegt die Logik in nativen DLLs; eine Mod greift nur an den Stellen ein, die das Modkit freilegt.

| Gothic-1-Remake | G3ASM |
|---|---|
| Skripte in AngelScript | ja, AngelScript 2.38 |
| Mod-Pakete mit Reihenfolge | ja, `mods\<Name>\main.as`, `mod.ini` und `load_order.txt` |
| Abhängigkeiten mit Versionsangaben | ja, `version`, `api` und `requires` in der `mod.ini`; keine Angabe für Unverträglichkeiten |
| Einstellungen je Mod | ja, Vorgaben in der `mod.ini`, Benutzerwerte in `g3asm\settings\<Mod>.ini`; kein Einstellungsmenü im Spiel |
| Schnittstellen zwischen Mods | teilweise: Funktionsaufruf mit Text (`CallMod`) und Nachrichten (`SendModMessage`); keine gemeinsamen Typen |
| Script Hooking (Vererbung, `Super::`) | teilweise: `Override_...` mit `Super()` an ausgewählten Spielfunktionen, dazu Ereignisse |
| Script Patches (Unified Diff auf Spielskripte) | ja, auf die Textdateien des Spiels ([Datei-Patches](patches.md)) |
| Konversationen | ja, als Infos mit Skriptzeilen und `OnInfoFinished` |
| Quests | ja, über das Questsystem des Spiels |
| Eigene Inhalte über den Unreal-Editor | nein; `PackMod` packt fertige Dateien in Archive, die Werkzeuge dafür kommen von außen |
