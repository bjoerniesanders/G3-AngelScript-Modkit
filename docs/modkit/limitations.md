# Grenzen

**Deutsch** | [English](../en/modkit/limitations.md)

Diese Seite sagt, was das Modkit kann, was nicht geht und was noch niemand geprüft hat. In der [API-Referenz](api-reference.md) tragen einzelne
Funktionen zusätzlich die Vermerke **Experimentell**, **Im Spiel nicht geprüft**, **Funktioniert nicht** oder **Veraltet**. Was sich zwischen Versionen
ändern darf, steht in den [Versionsregeln](api-policy.md).

**Grundsätzlich:** Die Spiellogik von Gothic 3 liegt in nativen DLLs. Eine Mod greift nur an den Stellen ein, die das Modkit freilegt.

## Im Spiel geprüft

**Grundlagen**
- Mods laden. Hot-Reload funktioniert. Ein Fehler in einer Mod stört die anderen nicht. Mods können standardmäßig aus sein (`mod.ini`), und du kannst
  sie per `load_order.txt` zuschalten.
- Eine Spielsitzung mit 14 Beispiel-Mods gleichzeitig, ohne Fehlerzeilen.
- Die Gleitkomma-Einstellung des Spiels bleibt unverändert. Sonst rechnet das Spiel ungenau, etwa mit verwaschenen Texturen.

**Ereignisse und Overrides**
- Die sechs Ereignisse des Questsystems (`OnNPCKilled`, `OnNPCDefeated`, `OnReceiveItem`, `OnEnter` und `OnLeave`, `OnSpellCast`).
- Schaden (`Override_Damage` und `OnDamage`) bei NPCs und beim Helden, durch Menschen, Tiere, Zauber und Sturz. Menge ändern und Treffer verhindern.
- `OnStatusEffect`, `Override_InfoAvailable`, `Override_ReceiveItem`, `Override_QuestStatus` und `OnQuestStatus`, `Override_ConsoleCommand`,
  `OnTouch`, `OnTradeGenerated`, `OnTransformStart` und `OnTransformEnd`.
- Speichern: `OnBeforeSave` und `OnAfterSave` laufen genau einmal je Speichern, auch beim Schnellspeichern.
- Spielskripte nach Namen (`CallScript`, `Override_Script` und `OnScript`), KI-Zustände, KI-Funktionen und Callbacks (`Override_AI` und `OnAI`),
  gesprochene Sätze (`OnAIOutput`, `GetSpeechText`).
- `Override_LearnCost` (auch `ALC`), `Override_MonsterDamage`.

**Spielwelt**
- Attribute, Inventar (zählen, entfernen, erzeugen, Stapel löschen), Item-Qualität mit eigener Zeile „Lebensraub“ in der Beschreibung.
- Eigene Talente mit Kachel im Charakterbogen (`perk_lifesteal`).
- Töten (`Kill`) mit Erfahrung und Beute. Verletzen und Töten über eine Blitz-Beschwörung mit Besitzer (`beliars_klaue`). Effekte starten.
- Vorlagen klonen. Items aus Klonen bleiben nach Speichern, Laden und Neustart erhalten.
- NPCs erzeugen und entfernen, auch aus früheren Sitzungen.
- Fokus und Fadenkreuz, Gruppe und Teleport (`teleport_party`), Zeitrate, Uhrfaktor, Spielzeit setzen.
- Eigenschaften lesen (`GetProperty`, `ListProperties`), Verweise setzen (`SetTemplateRef`, `SetEntityRef`), Schema aller Eigenschaften
  (`DumpPropertySchema`).

**Quests, Dialoge, Dateien**
- Quests anlegen und steuern, mit Titel, XP und Attribut-Belohnung. Alle 571 Quests des Spiels aufzählen. Quest-Dateien lesen.
- Dialoge lesen (4593 Infos) und eigene anlegen: Optionen im Gesprächsmenü, Zeilen im Questlog, `OnInfoFinished`.
- Questlog-Einträge ohne Gespräch (`AddQuestLogEntry`, ab v0.47.1), auch nach Speichern, Laden und Neustart des Spiels.
- Eigene Quests und Dialoge kommen nach Speichern und Laden mit Status und Questlog zurück. Schalter im Spielstand (`SetGameEvent`).
- Datei-Patches auf Quest-Dateien (`demo_patch`). Das Spiel lädt Mod-Archive (`PackMod`), und sie überschreiben vorhandene Dateien.
- Zeichnen über HUD und Inventar: Text, Bilder, `WorldToScreen`.

**Mod-Pakete für Spieler**
- Die Laufzeit startet als Script-DLL ohne Loader, ohne `g3asm.ini` und ohne Ordner `g3asm\`. Alle Hooks sind aktiv. Die Laufzeit entpackt und lädt
  gepackte Mods. Speichern und Beenden laufen normal.
- Über den Union Starter auf einer Installation mit vielen anderen Starter-Mods und 20 weiteren Script-Plugins: Das Paket installiert sich. Dialoge und
  Quest aus dem mitgelieferten Archiv funktionieren.
- Spielstand ohne die Mod: Ein mit `perk_lifesteal` gespeicherter Spielstand lädt ohne die Mod und ohne Absturz, weil die Mod ihre Zustände vor dem
  Speichern entfernt (siehe [Bekannte Risiken](#bekannte-risiken)).

**Installationen:** Das Spiel braucht den Community Patch 1.75 und den Parallel-Universe-Patch. Welche Installationen ich geprüft habe, steht in der
[Kompatibilität](../compatibility.md).

## Geht nicht

Diese Punkte habe ich im Spiel gemessen oder im Spielcode untersucht:

- **Einfarbige Flächen zeichnen:** Die Rechteck-Funktionen des Spiels zeichnen nichts. Es gehen nur Text und Bilder.
- **Talente lernen, die erst spät geklont werden:** `LearnSkill` lernt einen Klon nur, wenn die Mod ihn in `OnQuestsLoaded` klont. Die Wirkung eines
  eigenen Talents muss die Mod selbst herstellen.
- **Verweise per `SetProperty` setzen:** Es liefert `true`, wirkt aber nicht. Dafür gibt es `SetTemplateRef` und `SetEntityRef`.
- **Ein Treffer-Hook vor der Schadensberechnung:** Die Spiel-DLLs bieten dafür keinen Einstieg. Eingreifen lässt sich über das Spielskript `OnHit`,
  `Override_MonsterDamage` und `Override_Damage` ([Hooks](hooks.md#schaden-ohne-treffer-hook)).
- **Spielskripte, die das Spiel intern direkt aufruft** (`AssessHit`, `IsDeadlyDamage`, `CanBurn`): Sie lassen sich nicht nach Namen abfangen.
- **Datei-Patches bei Spielern:** Die Laufzeit startet als Script-DLL erst, nachdem das Spiel seine Dateiliste gelesen hat. Mod-Pakete liefern
  geänderte Spieldateien deshalb als Archiv. Datei-Patches wirken nur mit dem Loader.
- **Quest-Dateien schreiben.**
- **Die Beschreibung einer Quest im Questlog:** Das Log zeigt den Text von `SetQuestDescription` nicht. Einträge ohne Gespräch gehen ab v0.47.1
  mit `AddQuestLogEntry` ([Quests](quests.md#einträge-ohne-gespräch-ab-v0471)).
- **Bedingungen eigener Infos** (Items, Reputation, Fähigkeiten) lassen sich nicht setzen. Das Skript prüft sie selbst in `Override_InfoAvailable`.
- **Text für Umgebungsgerede:** `GetSpeechText` findet für `SVM_..._Smalltalk_NN` keinen Text.
- **Attribut `"XP"`** liest 0. Die Erfahrung steht in `GetProperty(GetHero(), "PlayerMemory", "XP")`. Quest-XP zahlt das Spiel selbst aus.
- **Attribute von NPCs:** `GetAttribute` und `SetAttribute` wirken nur beim Helden. NPCs geben `-1`. Lesen geht über Spielskripte, zum Beispiel
  `CallScript("GetHitPointsMax", npc, Entity())` (auch `GetHitPoints`, `GetStaminaPoints`, `GetManaPoints`, `GetStrength`). Die aktuellen
  Lebenspunkte ändern `CallScript("AddHitPoints", npc, Entity(), menge)` und `HurtHitPoints`. Höchstwerte und Stärke von NPCs ließen sich so
  nicht ändern. `GetProperty(npc, "DamageReceiver", "HitPoints")` liefert bei NPCs außerhalb der Verarbeitung 1.

## Gibt es nicht

Diese Punkte habe ich nicht gebaut oder noch nicht untersucht. Sie sind also nicht als unmöglich erwiesen:

- Animationen: abspielen, abfangen, eigene einbinden.
- Die Welt bearbeiten (Objekte setzen), eigene Modelle.
- Eigene Bedienelemente mit Eingabe, ein Einstellungsmenü im Spiel (Einstellungen gehen über die `mod.ini`).
- Datei-Patches auf komprimierte Archiveinträge (`Strings.pak`) und auf Binärdateien. Als Grundlage lesen Datei-Patches immer die Fassung aus dem
  `.pak`, auch wenn ein Mod-Archiv dieselbe Datei ersetzt.
- Die Bedeutung aller Werte von `GetActivePage()`.

## Experimentell

Diese Funktionen laufen, aber ihre Wirkung habe ich nicht vollständig geprüft. Name und Verhalten dürfen sich noch ändern:

- `HurtHitPoints`: senkt Lebenspunkte von NPCs, tötet aber nicht.
- `SetProperty`: schreibt Texte, Zahlen, Bool-Werte und Enums. Ob das Spiel den Wert beachtet, hängt von der Eigenschaft ab.
- `GenerateTradeGoods`: liefert `true`, aber die Ware bleibt bisher gleich.
- `Override_CombatMove`: Schläge werden sichtbar schneller. Bei `Recover` kann die Waffe statt des Kämpfers kommen.

## Nicht getestet

- **GOG-, CD- und DVD-Version.** Ohne DXVK nur auf einer Installation. Installationen ohne Community Patch 1.75 oder ohne Parallel-Universe-Patch
  unterstützt G3ASM nicht.
- **Zusammen mit anderen Script-Plugins**, die eigene Hooks setzen (z. B. CombatFix). `Script_MonsterDamage.dll` und
  `Script_LearningThresholdTweaks.dll` ändern dieselben Stellen wie das Modkit. Ist `Script_MonsterDamage.dll` installiert, bleibt
  `Override_MonsterDamage` aus. Ist `Script_LearningThresholdTweaks.dll` installiert, bleibt `Override_LearnCost` aus.
- **Im Spiel nie aufgetreten:** KI-Routinen (`AI_ROUTINE`), der Teiler 2 bei `Override_MonsterDamage` (beschworene Angreifer) und die verdoppelten
  Lernkosten mit „Alternative Balance“.
- **Abhängigkeiten in der `mod.ini`** (`version`, `api`, `requires`): nur ohne Spiel getestet.
- **Datei-Patches auf `.info`-Dateien:** Das Spiel lädt Dialoge aus der kompilierten `Infos.cpt`. Ob Patches dort ankommen, ist offen.
- **`owner` bei `OnDamage`**, wenn mehrere gegen einen kämpfen, ist eine Näherung. Es ist der aktuelle Gegner des Opfers, nicht sicher der Schläger.
- **Zwei Mods mit verschiedenen Laufzeit-Versionen** gleichzeitig beim Spieler.
- **Ein Spielstand mit `perk_lifesteal` auf einer Installation mit vielen fremden Mods** stürzte beim Öffnen des Charakterbogens ab. Das passierte
  auch ohne die Mod. Die Ursache ist offen. Auf einer sauberen Installation tritt das nicht auf.

## Bekannte Risiken

- **Dauerhafte Zustände im Spielstand.** Was eine Mod ins Spiel schreibt (Items aus geklonten Vorlagen, Talente, Uhrfaktor), steht im Spielstand. Fehlt
  die Mod beim Laden, zeigen solche Einträge ins Leere. Ein Spielstand mit dem Talent von `perk_lifesteal` **stürzte ohne die Mod beim Öffnen des
  Charakterbogens ab**. Mods entfernen solche Zustände deshalb in `OnBeforeSave` und stellen sie in `OnAfterSave` wieder her
  ([Speichern und Laden](saving.md)). Mit diesem Muster lädt der Spielstand auch ohne die Mod.
- **Feste Stellen im Spielcode.** Drei Funktionen hängen nicht an benannten Funktionen des Spiels, sondern an festen Stellen: die Zeile „Lebensraub“ in
  der Item-Beschreibung, `Override_LearnCost` und `Override_MonsterDamage`. Weicht die Spielversion dort ab, schaltet das Modkit nur diese Funktion ab
  und meldet es im Protokoll. Alles andere läuft weiter.
- **Union Starter:** Starte das Spiel erst, wenn der Starter die Installation als fertig gemeldet hat. Während er Archive neu einspielt, ist `Data\`
  unvollständig.

## Vergleich mit dem Modkit des Gothic-1-Remakes

Im Gothic-1-Remake ist die Spiellogik selbst in AngelScript geschrieben, und das Modkit liefert die Spielskripte mit. Eine Mod kann jede Skriptklasse
ableiten oder per Patch ändern. In Gothic 3 liegt die Logik in nativen DLLs. Eine Mod greift nur an den Stellen ein, die das Modkit freilegt.

| Gothic-1-Remake | G3ASM |
|---|---|
| Skripte in AngelScript | Ja, AngelScript 2.38 |
| Mod-Pakete mit Reihenfolge | Ja, `mods\<Name>\main.as`, `mod.ini` und `load_order.txt` |
| Abhängigkeiten mit Versionsangaben | Ja, `version`, `api` und `requires` in der `mod.ini`. Keine Angabe für Unverträglichkeiten |
| Einstellungen je Mod | Ja, Vorgaben in der `mod.ini`, Benutzerwerte in `g3asm\settings\<Mod>.ini`. Kein Einstellungsmenü im Spiel |
| Schnittstellen zwischen Mods | Teilweise: Funktionsaufruf mit Text (`CallMod`) und Nachrichten (`SendModMessage`). Keine gemeinsamen Typen |
| Script Hooking (Vererbung, `Super::`) | Teilweise: `Override_...` mit `Super()` an ausgewählten Spielfunktionen, dazu Ereignisse |
| Script Patches (Unified Diff auf Spielskripte) | Ja, auf die Textdateien des Spiels ([Datei-Patches](patches.md)) |
| Konversationen | Ja, als Infos mit Skriptzeilen und `OnInfoFinished` |
| Quests | Ja, über das Questsystem des Spiels |
| Eigene Inhalte über den Unreal-Editor | Nein. `PackMod` packt fertige Dateien in Archive. Die Werkzeuge dafür kommen von außen |
