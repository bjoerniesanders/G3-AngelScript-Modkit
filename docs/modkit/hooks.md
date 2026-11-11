# Hooks: Spielfunktionen ändern (Override und Super)

Mit einem **Override** ersetzt ein Mod eine Funktion des Spiels. Der Mod definiert eine Funktion `bool Override_<Name>(...)`. Das Spiel ruft dann sie statt der
ursprünglichen Funktion auf. Diese Mechanik entspricht dem `override` mit `Super::` des G1R-Modkits.

```angelscript
// Jens' Arbeit nur zwischen 8 und 20 Uhr Spielzeit anbieten
bool Override_InfoAvailable(const bCString &in info, const bCString &in owner, const bCString &in quest) {
    if (info == "OG3_Jens_Job") {
        int h = GetGameHour();
        if (h >= 0 && (h < 8 || h >= 20)) return false;   // Option verborgen, Original wird gar nicht gefragt
    }
    return Super();                                       // sonst entscheidet das Spiel
}
```

## Regeln

| Was du tust | Wirkung |
|---|---|
| `return Super();` | das Original läuft unverändert, sein Ergebnis geht ans Spiel |
| `bool r = Super(); return !r;` | das Original läuft, du änderst das Ergebnis |
| `return true;` ohne `Super()` | das Original wird **übersprungen** (abgebrochen), dein Wert geht ans Spiel |
| mehrere Mods mit demselben Override | sie bilden eine Kette in Ladereihenfolge: `Super()` ruft den nächsten Mod, am Ende das Original |
| Skriptfehler im Override | das Spiel bekommt sein Original |

Die Nachereignisse (`OnNPCKilled`, `OnReceiveItem`, ...) laufen nur, wenn das Original tatsächlich gelaufen ist.

## Verfügbare Overrides

| Funktion | Spielfunktion | Bedeutung des Rückgabewerts |
|---|---|---|
| `Override_NPCKilled(Entity victim)` | `gCQuestManager_PS::OnNPCKilled` | Ergebnis des Quest-Managers (true = verarbeitet) |
| `Override_NPCDefeated(Entity npc)` | `OnNPCDefeated` | wie oben |
| `Override_ReceiveItem(Entity giver, Entity receiver, const bCString &in name, const bCString &in id, uint count)` | `OnReceiveItem` | wie oben |
| `Override_Enter(Entity who, const bCString &in area)` / `Override_Leave(...)` | `OnEnter` / `OnLeave` | wie oben |
| `Override_SpellCast(Entity caster, Entity target, const bCString &in spell)` | `OnSpellCast` | wie oben |
| `Override_InfoAvailable(const bCString &in info, const bCString &in owner, const bCString &in quest)` | `gCInfo_PS::AreConditionsFulfilled` | **true = Option wählbar**, false = verborgen |
| `Override_Damage(Entity victim, Entity inflictor, Entity owner, int amount, int type)` (ab Version 25) | NPCs: `gCDamageReceiver_PS::SetHitPoints` (sinkender Wert); Held: `gCPlayerMemory_PS::SetBaseValue` (Attribut `HP` sinkt) | **Eingriff wirkt:** `SetOverrideArg(3, ..)` ändert den Lebenspunkte-Verlust, ohne `Super()` entfällt der Treffer. Menge = Verlust, bei Tod auf die verbleibenden Lebenspunkte begrenzt (nicht der wahre Schaden). `inflictor` = Waffe oder Geschoss (`Arrow`, `Mis_Fireball`, `It_1H_…`; bei Tieren das Item `Fist`; bei Schaden über Zeit leer), beim Helden die vom Angreifer geführte Waffe (rechte Hand; bei einem Bogen in der linken Hand das Munitions-Item `Arrow` des Schützen, damit Fernkampf wie bei NPC-Opfern als Pfeil ankommt, ohne Munitions-Item der Bogen; das Munitions-Item ist der Pfeil im Köcher, nicht das fliegende Geschoss, hat aber denselben Namen). **`owner`** = Verursacher: ist `inflictor` selbst ein NPC, dieser; sonst der aktuelle Gegner des Opfers (`CurrentAttackerEntity`). Kann leer sein. Im Gruppenkampf eine Näherung: bei zwei Wölfen wechselte `owner` im Spieltest mit den Angreifern, ob er bei jedem Biss stimmt, ist nicht belegt. `type` = Art des Treffers, gelesen aus dem angreifenden Item (`gEDamageType`: 1 Impact, 2 Blade, 3 Missile, 4 Fire, 5 Ice, 6 Lightning, 7 Physics; gemessen: Keule 1, Schwert/Hellebarde/Speer 2, Pfeil 3, `Mis_Fireball` 4); gilt für NPCs und den Helden gleich. Ist der Wert nicht lesbar (kein `inflictor`, oder das Zauber-Item `It_Spell_*` in der Hand eines Zaubernden), gilt bei NPC-Opfern der Wert des Schadensempfängers (immer 2, **keine** Trefferart), beim Helden 0 |
| `Override_ConsoleCommand(const bCString &in line)` (ab Version 35) | `eCConsole::OnExecute` (jede eingegebene Konsolenzeile) | **true ohne `Super()` = der Befehl gehört dem Mod**; die Antwort in der Konsole setzt `SetConsoleReply(text)`. Fremde Befehle mit `return Super();` durchreichen. Die Konsole erscheint nur im Marvin-Modus (Taste über Tab). Gemessen: eigener Befehl beantwortet, `help` lief unverändert weiter |
| `Override_TradeGenerate(Entity trader)` (ab Version 37) | `gCInventory_PS::GenerateTradeInvFromTreasureSets` | Ohne `Super()` erzeugt das Spiel keine Ware (Rückgabe = Ergebnis); danach `OnTradeGenerated`, wenn das Original lief |
| `Override_QuestStatus(const bCString &in name, int oldStatus, int newStatus)` (ab Version 35) | `gCQuest_PS::SetStatus` | `SetOverrideArg(2, x)` setzt einen anderen neuen Status, ohne `Super()` bleibt der alte. Danach `OnQuestStatus`, wenn sich etwas geändert hat |
| `Override_LearnCost(const bCString &in info, const bCString &in attrib, int amount, int lp, int gold)` (ab Version 42) | Preis eines Lehrers für ein Attribut (Lehr-Info mit `TeachAttrib`, z. B. `STR`): Prüfung und Bezahlung (`gCInfo_PS::Execute`, `OnEndInfo`) und die Anzeige im Dialog (`GetDialogDescription`) | `info` ist der Name der Lehr-Info, `amount` der Zuwachs, `lp` und `gold` der Preis des Spiels. Im Spiel kostet ein Punkt einen Lernpunkt (`lp` = `amount`); mit „Alternative Balance“ der Community-Patches verdoppelt das Spiel `lp` und `gold`, sobald STR, DEX oder INT 250 erreicht haben (dann `lp` = 2 × `amount`). `SetOverrideArg(3, ..)` ändert die Lernpunkte, `SetOverrideArg(4, ..)` das Gold, mit und ohne `Super()`; negative Werte zählen als 0. Der Zuwachs selbst bleibt. **Wird mehrfach je Lehrvorgang gefragt** (Anzeige, Prüfung, Bezahlung, die Anzeige bei jedem Aufbau des Dialogs): dieselben Eingaben müssen dasselbe Ergebnis geben. Gilt für alle Attribut-Lehrer, auch `ALC`. Talente (`TeachSkill`) laufen nicht durch den Hook. Hängt an sieben festen Stellen der Game.dll, deren Originalbytes vorher verglichen werden; passt eine nicht (andere Version, oder `Script_LearningThresholdTweaks.dll` ist installiert), bleiben alle sieben aus. Beispiel `learn_cost` |
| `Override_CombatMove(Entity self, const bCString &in move, uint speedPercent)` (ab Version 45) | **Experimentell.** Start einer Kampfbewegung (Angriff, Schlag) eines NPCs oder des Helden (`gCScriptProcessingUnit::sAICombatMoveStart`) | `speedPercent` ist die Abspielgeschwindigkeit der Bewegung in Prozent (100 = normal; gemessen hat ein NPC mit 130 angegriffen). `SetOverrideArg(2, p)` setzt sie für diesen Angriff, mit und ohne `Super()`. `move` ist die Phase der Bewegung: `Begin`, `Raise`, `Hit`, `Recover`. Die Laufbewegung und das Drehen laufen nicht hierüber (`SetPlaySpeed`). Im Spiel bestätigt (2026-11-11, Faktor 2 für den Helden): Schläge sichtbar schneller, Treffer und Schaden wie sonst. Gemessene Werte des Helden (Einhänder): `Begin` 230, `Raise` 140 bis 150, `Hit` 50 bis 200; NPC Gamal `Hit` 60 und 130. Bei `Recover` kam als `self` einmal die Waffe (`It_1H_OrcSlayer_01`) statt des Kämpfers; wer die Erholung mitändern will, prüft das Item. Beispiel `attack_speed` |
| `Override_MonsterDamage(Entity victim, Entity attacker, int damage, int divisor)` (ab Version 42) | Die Schadensgrenze des Spielskripts `AssessHit` (seit Patch 1.12): Trifft ein Nicht-Humanoider (Monster, Tier) einen Humanoiden und ist keiner der beiden der Held, teilt das Spiel den Schaden durch 10, bei beschworenen Angreifern durch 2 | `damage` ist der Schaden vor der Teilung, `divisor` 10 oder 2. `Super()` teilt `damage` durch `divisor` (beide mit `SetOverrideArg(2/3, ..)` änderbar, `divisor` 1 = keine Teilung); **ohne `Super()` bekommt das Spiel `damage` ungeteilt**. Danach rechnet `AssessHit` weiter (Verdopplung, Rüstung), der Wert ist also nicht der endgültige Verlust; den zeigt `Override_Damage`. Anders als die übrigen Hooks hängt dieser an einer festen Stelle der Script_Game.dll (RVA `0x2C05F`): Die Originalbytes werden vorher verglichen; passen sie nicht (andere Version, oder die Script-DLL `Script_MonsterDamage.dll` ist installiert), bleibt der Hook aus und das Protokoll sagt es. Im Spiel gemessen (2026-11-10, Wolf gegen den NPC Jens, Faktor 0,2): `damage` 23, Original 2, an das Spiel 4, der endgültige Verlust laut `Override_Damage` 3 bzw. 6 bei Doppeltreffern. Treffer am Helden (auch verwandelt) und von oder an einem beschworenen Skelett gegen Wölfe kamen nicht durch den Hook, wie es die Bedingung erwartet; der Teiler 2 (beschworener Angreifer gegen humanoiden NPC) ist nicht ausgelöst. Beispiel `monster_damage` |
| `Override_AI(const bCString &in name, int kind, Entity self, Entity target)` (ab Version 41) | KI-Zustände (`ZS_...`), KI-Funktionen (`_AI_...`), Callbacks und Routinen der NPCs nach Namen (`gCScriptAdmin::RunScriptState`, `RunScriptFunction`, `RunScriptCallback`, `RunScriptRoutine`); `kind` ist `AI_STATE` (1), `AI_FUNCTION` (2), `AI_CALLBACK` (3) oder `AI_ROUTINE` (4) | Dieselbe Anmeldung wie bei `Override_Script`: **nur Namen aus `WatchScript(name)`**, nur im Spiel-Thread. `self` ist der NPC, `target` sein Ziel (beide können leer sein). Die Rückgabe geht ans Spiel, mit und ohne `Super()`. Zustände und KI-Funktionen laufen in Abschnitten über mehrere Bilder: `false` heißt „noch nicht fertig, nächstes Bild weiter“, `true` „fertig“; ein Zustand, der `false` liefert, wird im nächsten Bild erneut gerufen. Ohne `Super()` läuft das Original in diesem Bild nicht. Laut SDK wird eine fertige KI-Funktion vom Stapel genommen, ein Zustand dagegen beginnt von vorn; wie das Spiel auf ein ausgelassenes Original reagiert, ist nicht gemessen. Die Argumente einer KI-Funktion (z. B. welches Item `_AI_UseInventoryItem` benutzt) sind nicht zugänglich. Im Spiel gemessen (2026-11-10): Callback `OnFollowPlayer` etwa einmal je Sekunde je Begleiter, KI-Funktion `_AI_UseInventoryItem` beim Benutzen eines Items durch den Helden (Ergebnis 0, läuft über mehrere Bilder), Zustand `ZS_ObserveSuspect` bei den Zuschauern eines Arenakampfs: einmal je Zuschauer (sieben NPCs innerhalb von 0,4 s), jeweils Ergebnis 1, also nicht jedes Bild. `name` ist eine Kopie: Das Spiel übergibt einen Zeiger in den Zustandsstapel des NPCs, den der Zustand beim Laufen umbaut |
| `Override_Script(const bCString &in name, Entity self, Entity other, int arg)` (ab Version 32) | jedes Spielskript nach Namen (`gCScriptAdmin::RunScript` und `RunScriptFromScript`, z. B. `AssessHit`, `MagicTeleport`, `IsDeadlyDamage`) | **Nur für Namen, die ein Mod vorher mit `WatchScript(name)` angemeldet hat** (sonst würde jeder der vielen Skriptaufrufe Skriptzeit kosten), und nur im Spiel-Thread. Alle Mods mit `Override_Script` sehen alle angemeldeten Namen: zuerst `name` prüfen und sonst `return Super();`. `SetOverrideArg(3, ..)` ändert `arg`. Ohne `Super()` bekommt das Spiel den Wert aus `SetScriptResult(int)`, sonst 1 bei `true` und 0 bei `false`. `self`/`other` können leer sein. Gemessen: rund 860 Skriptaufrufe je Sekunde, alle im Spiel-Thread; abfangbar sind nur Skripte, die über ihren Namen laufen (z. B. `MagicTeleport`, `OnHit`, `GetAttitude`), nicht `AssessHit` |
| `Override_Damage(Entity victim, Entity attacker, int amount, int type)` (alte Form, weiter unterstützt; läuft nach der neuen Form derselben Kette) | wie oben | wie oben, `SetOverrideArg(2, ..)` ändert die Menge; `attacker` ist derselbe Wert wie `inflictor` der neuen Form |

**Schaden am Helden:** Der Held hat seine Lebenspunkte im Attribut `HP`, sein Schadensempfänger steht immer auf 1. Treffer von Menschen und Tieren senken das Attribut über `gCPlayerMemory_PS::SetBaseValue` (Spielskript, nicht über `gCEntity::OnDamage`). Der Hook greift **vor** dem Schreiben: eine geänderte Menge wird direkt geschrieben, ohne `Super()` entfällt der Verlust. Bei einem tödlichen Treffer schreibt das Spiel `HP = 0`; die gemeldete Menge ist dann nur der Rest der Lebenspunkte. Der Schadenstyp kommt wie bei NPCs aus dem angreifenden Item (siehe `type` oben); ohne lesbaren Wert ist er 0. Kommt zum selben Treffer innerhalb von 150 ms noch `gCEntity::OnDamage` mit derselben Menge, wird er nicht doppelt gemeldet. Ein schon toter Held wird nicht wiederbelebt.

**Fallschaden** läuft ebenfalls über `SetBaseValue` (gemessen 2026-11-06) und kommt durch dieselbe Kette. Er hat keinen Angreifer: `owner` und `inflictor` sind leer. Erkannt wird das daran, dass die Schadensroutine des Spielskripts bei Kampfschaden (Biss, Schlag, Zauber) direkt von der Engine aufgerufen wird, beim Sturz aber von einer anderen Skriptroutine; das ist eine Heuristik und gilt wahrscheinlich auch für anderen Umgebungsschaden (Gift u. ä., nicht geprüft). Zauber werden dagegen richtig zugeordnet (`owner` der Zaubernde, `inflictor` der Zauber, z. B. `It_Spell_Fireball`). Ältere Hinweise, ein Sturz bleibe ohne Meldung, gelten nicht mehr; `hooks_demo` zeigt trotzdem, wie ein Skript jedes Bild (`OnFrame`) das Attribut `HP` vergleicht und Verluste ohne Meldung ausgleicht.

`Override_InfoAvailable` ist der nützlichste Hook: du entscheidest damit zu jeder Zeit, ob eine Dialogoption (auch die des Spiels selbst) im Menü erscheint.
Dort ist der Rückgabewert auch wirklich ein Ja/Nein für das Spiel. Bei den Quest-Ereignissen ist er nur das Ergebnis der Verarbeitung.

## Argumente ändern

Zahlenargumente (Ganzzahlen, `uint`) lassen sich verändern, bevor `Super()` das Original aufruft:

```angelscript
bool Override_ReceiveItem(Entity giver, Entity receiver, const bCString &in name, const bCString &in id, uint count) {
    if (name == "It_Wolf_Fur") SetOverrideArg(4, count * 2);   // Argument 4 = Menge, doppelt so viele Felle
    return Super();                                            // das Original bekommt die doppelte Menge
}
```

| Funktion | Bedeutung |
|---|---|
| `bool SetOverrideArg(int index, uint wert)` | Setzt das Argument mit diesem Index (0 = erstes) der laufenden Kette. `false` (Hinweis im Protokoll) bei falschem Index, bei Entity-/Text-Argumenten oder außerhalb eines Overrides |
| `uint GetOverrideArg(int index)` | Liest den aktuellen Wert (0, wenn es das Argument nicht gibt) |

- Der neue Wert gilt für alle weiteren Mods der Kette und am Ende für das Original. Er wirkt nur für diesen einen Aufruf.
- **Warum nicht `int &inout`?** AngelScript erlaubt `&inout` nur für Objekttypen mit Handles (`uint &inout` und `Entity &inout` werden mit „Only object types that support object handles can use &inout“ abgelehnt). Ginge es mit der Einstellung für unsichere Referenzen, hätten alle Mods ungeprüfte Referenzen. `SetOverrideArg` ist stattdessen eine ausdrückliche, geprüfte Schnittstelle.
- **Was die Änderung bewirkt, hängt vom Hook ab.** Die Overrides der Quest-Ereignisse (`Override_ReceiveItem` und die anderen fünf) hängen am Questsystem (`gCQuestManager_PS`), das **nach** dem eigentlichen Vorgang benachrichtigt wird. Im Spiel geprüft: `SetOverrideArg(4, count * 2)` kommt dort an (Meldung „Menge verdoppelt: 1 -> 2“), das Inventar bleibt aber unverändert, der Held erhält nur ein Stück. Die geänderte Menge zählt nur für Quest-Bedingungen („bring 5 Felle“). Um wirklich mehr Gegenstände zu geben, `receiver.AddItems(name, n)` nach `Super()` benutzen (Beispiel `hooks_demo`). Bei einem Hook auf die eigentliche Spielfunktion (Schaden, Inventar) hat dieselbe Schnittstelle echte Wirkung.
- **Entities und Texte** (`Entity`, `const bCString &in`) sind nicht änderbar.

## Grenzen

- **Rückgabe** ist immer `bool`.
- Kein allgemeiner Treffer-Hook vor der Schadensberechnung: in der Game.dll fehlt dafür ein exportierter Einstiegspunkt. `Override_Script("AssessHit")` ersetzt die ganze Berechnung, `Override_MonsterDamage` greift an einer Stelle mittendrin.
- Ein Override sollte schnell sein. `Override_InfoAvailable` wird für jede Option jedes NPCs beim Aufbau des Menüs aufgerufen.
  Der Patch ruft sie nur auf, wenn irgendein Mod die Funktion definiert, sonst kostet es nichts.

## Beispiel-Mod

`examples/mods/hooks_demo/main.as`: verbirgt Jens' Mod-Arbeit nachts und schreibt Item-Übergaben an den Helden mit.
