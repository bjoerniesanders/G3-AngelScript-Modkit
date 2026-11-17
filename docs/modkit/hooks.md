# Hooks: Spielfunktionen ändern (Override und Super)

**Deutsch** | [English](../en/modkit/hooks.md)

Mit einem **Override** ersetzt eine Mod eine Funktion des Spiels. Die Mod definiert eine Funktion `bool Override_<Name>(...)`. Das Spiel ruft dann
diese Funktion statt des Originals auf. Mit `Super()` läuft das Original doch noch, an der Stelle, die du bestimmst. Das entspricht `override` und
`Super::` im Modkit des Gothic-1-Remakes.

```angelscript
// Jens' Arbeit nur zwischen 8 und 20 Uhr Spielzeit anbieten
bool Override_InfoAvailable(const bCString &in info, const bCString &in owner, const bCString &in quest) {
    if (info == "G3ASM_Jens_Job") {
        int h = GetGameHour();
        if (h >= 0 && (h < 8 || h >= 20)) return false;   // Option verborgen, das Spiel wird gar nicht gefragt
    }
    return Super();                                       // sonst entscheidet das Spiel
}
```

## Regeln

| Was du tust | Wirkung |
|---|---|
| `return Super();` | Das Original läuft unverändert. Sein Ergebnis geht ans Spiel |
| `bool r = Super(); return !r;` | Das Original läuft. Du änderst das Ergebnis |
| `return true;` ohne `Super()` | Das Modkit **überspringt** das Original. Dein Wert geht ans Spiel |
| Mehrere Mods mit demselben Override | Sie bilden eine Kette in Ladereihenfolge. `Super()` ruft die nächste Mod, am Ende das Original |
| Skriptfehler im Override | Das Spiel bekommt sein Original |

- Die Nachereignisse (`OnNPCKilled`, `OnReceiveItem`, ...) laufen nur, wenn das Original tatsächlich gelaufen ist.
- Die Rückgabe ist immer `bool`. Was `true` und `false` bedeuten, steht beim jeweiligen Override.
- Ein Override sollte schnell sein. Manche laufen sehr oft (siehe unten). Definiert keine Mod die Funktion, kostet der Hook nichts.

## Argumente ändern

Zahlenargumente (`int`, `uint`) lassen sich ändern, bevor `Super()` das Original aufruft:

```angelscript
bool Override_ReceiveItem(Entity giver, Entity receiver, const bCString &in name, const bCString &in id, uint count) {
    if (name == "It_Wolf_Fur") SetOverrideArg(4, count * 2);   // Argument 4 = Menge
    return Super();
}
```

| Funktion | Bedeutung |
|---|---|
| `bool SetOverrideArg(int index, uint wert)` | Setzt das Argument Nummer `index` (0 = erstes). `false` bei falschem Index, bei `Entity`- und Text-Argumenten und außerhalb eines Overrides |
| `uint GetOverrideArg(int index)` | Liest den aktuellen Wert (0, wenn es das Argument nicht gibt) |

- Der neue Wert gilt für alle weiteren Mods der Kette und für das Original, nur für diesen einen Aufruf.
- `Entity`- und Text-Argumente kannst du nicht ändern.
- **Was die Änderung bewirkt, hängt vom Hook ab.** Bei den Quest-Ereignissen (`Override_ReceiveItem` und die anderen fünf) erfährt das Questsystem
  vom Vorgang erst danach. Die verdoppelte Menge zählt nur für Quest-Bedingungen („bring 5 Felle“). Im Inventar landet weiterhin ein Fell. Für mehr
  Items rufe nach `Super()` `receiver.AddItems(name, n)` auf (Beispiel `demo_hooks`). Bei Schaden, Lernkosten oder Monsterschaden wirkt die Änderung
  wirklich.
- Warum nicht `int &inout`? AngelScript erlaubt `&inout` nur bei Objekttypen mit Handles. `SetOverrideArg` ist dafür eine geprüfte Schnittstelle.

## Übersicht

| Override | Wann | Rückgabe |
|---|---|---|
| `Override_NPCKilled(Entity victim)` | Ein NPC wurde getötet | Ergebnis des Questsystems |
| `Override_NPCDefeated(Entity npc)` | Ein NPC wurde besiegt | Ergebnis des Questsystems |
| `Override_ReceiveItem(Entity giver, Entity receiver, const bCString &in name, const bCString &in id, uint count)` | Items wechseln den Besitzer | Ergebnis des Questsystems |
| `Override_Enter(Entity who, const bCString &in area)`, `Override_Leave(Entity who, const bCString &in area)` | Jemand betritt oder verlässt einen Bereich | Ergebnis des Questsystems |
| `Override_SpellCast(Entity caster, Entity target, const bCString &in spell)` | Jemand wirkt einen Zauber | Ergebnis des Questsystems |
| `Override_InfoAvailable(const bCString &in info, const bCString &in owner, const bCString &in quest)` | Das Spiel fragt, ob eine Dialogoption erscheint | **`true` = Option wählbar** |
| [`Override_Damage(...)`](#override_damage) (ab Version 25) | Ein Treffer kostet Lebenspunkte | Ohne `Super()` entfällt der Treffer |
| [`Override_MonsterDamage(...)`](#override_monsterdamage) (ab Version 42) | Ein Monster trifft einen humanoiden NPC | Ohne `Super()` ungeteilter Schaden |
| [`Override_LearnCost(...)`](#override_learncost) (ab Version 42) | Ein Lehrer nennt oder kassiert den Preis für ein Attribut | – |
| [`Override_CombatMove(...)`](#override_combatmove) (ab Version 45) | **Experimentell.** Eine Kampfbewegung beginnt | – |
| [`Override_QuestStatus(...)`](#override_queststatus) (ab Version 35) | Der Status einer Quest ändert sich | Ohne `Super()` bleibt der alte |
| [`Override_ConsoleCommand(...)`](#override_consolecommand) (ab Version 35) | Jemand gibt eine Zeile in die Konsole ein | **`true` ohne `Super()` = Befehl erledigt** |
| [`Override_TradeGenerate(...)`](#override_tradegenerate) (ab Version 37) | Das Spiel erzeugt die Ware eines Händlers | Ohne `Super()` keine Ware |
| [`Override_Script(...)`](#override_script) (ab Version 32) | Ein angemeldetes Spielskript läuft | Ergebnis fürs Spiel |
| [`Override_AI(...)`](#override_ai) (ab Version 41) | Ein angemeldetes KI-Skript läuft | `true` = fertig |

Die ersten sechs Overrides hängen am Questsystem des Spiels. Das Spiel benachrichtigt es **nach** dem eigentlichen Vorgang. Ihr Rückgabewert ist nur
das Ergebnis dieser Benachrichtigung. Bei `Override_InfoAvailable` dagegen ist er wirklich das Ja oder Nein für das Spiel. Damit entscheidest du
jederzeit, ob eine Dialogoption erscheint, auch eine des Spiels. Das Spiel fragt für jede Option jedes NPCs, sobald es das Gesprächsmenü aufbaut.

## Override_Damage

```angelscript
bool Override_Damage(Entity victim, Entity inflictor, Entity owner, int amount, int type)
```

Läuft, bevor ein Treffer Lebenspunkte abzieht. Es läuft bei NPCs und beim Helden, für Waffen, Tiere, Zauber und Stürze.

- `amount` ist der Verlust an Lebenspunkten. Bei einem tödlichen Treffer ist es nur der Rest der Lebenspunkte, nicht der volle Schaden.
- `SetOverrideArg(3, n)` ändert den Verlust. Ohne `Super()` entfällt der Treffer.
- `inflictor` ist die Waffe oder das Geschoss (`It_1H_…`, `Arrow`, `Mis_Fireball`; bei Tieren `Fist`). Bei Schaden über Zeit und beim Sturz ist er
  leer. Trifft ein Bogenschütze den Helden, kommt wie bei NPCs der Pfeil an.
- `owner` ist der Verursacher. Ist der `inflictor` ein NPC, ist es der `inflictor` selbst. Sonst ist es der aktuelle Gegner des Opfers. Kämpfen
  mehrere gegen einen, ist das eine Näherung. Bei Zaubern ist `owner` der Zaubernde, beim Sturz leer.
- `type` ist die Trefferart aus dem Item des Angreifers: 1 Schlag (Keule), 2 Klinge, 3 Geschoss, 4 Feuer, 5 Eis, 6 Blitz, 7 Physik
  ([Referenz](reference.md#1-schadensarten-type-in-override_damage-und-ondamage)). Kann das Modkit sie nicht lesen (kein Item, oder ein Zauber-Item
  in der Hand), ist sie bei NPCs 2 und beim Helden 0.
- Ein schon toter Held wird nicht wiederbelebt.
- Die alte Form `Override_Damage(Entity victim, Entity attacker, int amount, int type)` läuft weiter, nach der neuen in derselben Kette. Dort ist
  `attacker` der `inflictor`, und `SetOverrideArg(2, n)` ändert die Menge.

Danach meldet `OnDamage` den Treffer ([Ereignisse](events.md)). Beispiele: `demo_lifesteal`, `perk_lifesteal`, `demo_hooks`.

## Override_MonsterDamage

```angelscript
bool Override_MonsterDamage(Entity victim, Entity attacker, int damage, int divisor)
```

Trifft ein Monster oder Tier einen humanoiden NPC und ist keiner der beiden der Held, teilt das Spiel den Schaden durch 10. Bei beschworenen
Angreifern teilt es durch 2. Dieser Override entscheidet über die Teilung.

- `damage` ist der Schaden vor der Teilung. `divisor` ist 10 oder 2. Beide lassen sich ändern (`SetOverrideArg(2, ..)`, `SetOverrideArg(3, ..)`).
  `divisor` 1 = keine Teilung.
- `Super()` teilt. **Ohne `Super()` bekommt das Spiel `damage` ungeteilt.**
- Danach rechnet das Spiel weiter (Rüstung und anderes). Den endgültigen Verlust zeigt `Override_Damage`.
- Ist das Plugin `Script_MonsterDamage.dll` installiert oder weicht die Spielversion ab, bleibt der Override aus. Das Protokoll sagt es.
- Der Fall „beschworener Angreifer“ (Teiler 2) ist im Spiel noch nicht aufgetreten.

Beispiel `probe_monster_damage`.

## Override_LearnCost

```angelscript
bool Override_LearnCost(const bCString &in info, const bCString &in attrib, int amount, int lp, int gold)
```

Der Preis eines Lehrers für ein Attribut (`STR`, `DEX`, `ALC`, ...).

- `info` ist die Lehr-Info. `attrib` ist das Attribut. `amount` ist der Zuwachs. `lp` und `gold` sind der Preis des Spiels.
- `SetOverrideArg(3, ..)` ändert die Lernpunkte. `SetOverrideArg(4, ..)` ändert das Gold. Das geht mit und ohne `Super()`. Negative Werte zählen als
  0. Den Zuwachs ändert der Override nicht.
- Normal kostet ein Punkt einen Lernpunkt. Mit „Alternative Balance“ der Community Patches verdoppelt das Spiel Lernpunkte und Gold, sobald STR, DEX
  oder INT 250 erreicht haben.
- **Das Spiel fragt je Lehrvorgang mehrfach** (Anzeige im Dialog, Prüfung, Bezahlung). Gleiche Eingaben müssen dasselbe Ergebnis liefern.
- Talente laufen nicht über diesen Override.
- Ist das Plugin `Script_LearningThresholdTweaks.dll` installiert oder weicht die Spielversion ab, bleibt der Override aus. Das Protokoll sagt es.

Beispiel `probe_learn_cost`.

## Override_CombatMove

```angelscript
bool Override_CombatMove(Entity self, const bCString &in move, uint speedPercent)
```

**Experimentell.** Der Beginn einer Kampfbewegung eines NPCs oder des Helden.

- `move` ist die Phase: `Begin`, `Raise`, `Hit`, `Recover`.
- `speedPercent` ist die Abspielgeschwindigkeit in Prozent (100 = normal; das Spiel selbst setzt oft andere Werte). `SetOverrideArg(2, p)` ändert sie
  für diese Bewegung, mit und ohne `Super()`. Treffer und Schaden bleiben gleich.
- Laufen und Drehen gehören nicht dazu.
- Bei `Recover` kann `self` die Waffe statt des Kämpfers sein. Änderst du auch die Erholung, prüfe das.

Beispiel `probe_attack_speed`.

## Override_QuestStatus

```angelscript
bool Override_QuestStatus(const bCString &in name, int oldStatus, int newStatus)
```

Der Status einer Quest soll sich ändern. `SetOverrideArg(2, x)` setzt einen anderen neuen Status. Ohne `Super()` bleibt der alte. Danach kommt
`OnQuestStatus`, wenn sich wirklich etwas geändert hat. Statuswerte: `QUEST_*` ([Quests](quests.md)).

## Override_ConsoleCommand

```angelscript
bool Override_ConsoleCommand(const bCString &in line)
```

Jede Zeile, die jemand in die Konsole eingibt. `true` ohne `Super()` heißt: Der Befehl gehört der Mod. Die Antwort setzt `SetConsoleReply(text)`.
Reiche fremde Befehle mit `return Super();` durch. Die Konsole gibt es nur im Marvin-Modus (die Taste über Tab). Beispiel `probe_events`.

## Override_TradeGenerate

```angelscript
bool Override_TradeGenerate(Entity trader)
```

Das Spiel erzeugt die Ware eines Händlers aus seinen Schatz-Sets. Ohne `Super()` erzeugt es keine Ware. Danach kommt `OnTradeGenerated`, wenn das
Original lief.

## Override_Script

```angelscript
bool Override_Script(const bCString &in name, Entity self, Entity other, int arg)
```

Ein Spielskript einer `Script_*.dll` läuft über seinen Namen (`MagicTeleport`, `OnHit`, `GetAttitude`, ...).

- **Nur für Namen, die eine Mod vorher mit `WatchScript(name)` angemeldet hat.** Das Spiel ruft hunderte Skripte je Sekunde. Ohne Anmeldung würde
  jeder Aufruf Zeit kosten.
- Jede Mod mit `Override_Script` sieht alle angemeldeten Namen. Prüfe zuerst `name`. Für andere Namen nimm `return Super();`.
- `SetOverrideArg(3, ..)` ändert `arg`. Ohne `Super()` bekommt das Spiel den Wert aus `SetScriptResult(int)`. Hast du keinen gesetzt, bekommt es 1
  bei `true` und 0 bei `false`.
- `self` und `other` können ungültig sein.
- Skripte, die das Spiel intern direkt aufruft (`AssessHit`, `IsDeadlyDamage`, `CanBurn`), laufen nicht über ihren Namen und kommen hier nicht an.

Danach kommt `OnScript`. Beispiele: `teleport_party` (`MagicTeleport`), `probe_script`.

## Override_AI

```angelscript
bool Override_AI(const bCString &in name, int kind, Entity self, Entity target)
```

Ein KI-Skript eines NPCs läuft: ein Zustand (`ZS_...`, `kind` = `AI_STATE`), eine KI-Funktion (`_AI_...`, `AI_FUNCTION`), ein Callback
(`AI_CALLBACK`) oder eine Routine (`AI_ROUTINE`).

- Wie bei `Override_Script` nur für Namen aus `WatchScript(name)`.
- `self` ist der NPC. `target` ist sein Ziel. Beide können ungültig sein.
- Die Rückgabe geht ans Spiel, mit und ohne `Super()`. Zustände und KI-Funktionen laufen über mehrere Bilder. `false` heißt „noch nicht fertig“, und
  das Spiel ruft sie im nächsten Bild wieder. `true` heißt „fertig“.
- Ohne `Super()` läuft das Original in diesem Bild nicht. Wie das Spiel darauf reagiert, habe ich nicht geprüft.
- Die Argumente einer KI-Funktion (etwa welches Item `_AI_UseInventoryItem` benutzt) sind nicht zugänglich.
- Routinen (`AI_ROUTINE`) sind im Spiel noch nie aufgetreten.

Danach kommt `OnAI`. Beispiel `probe_ai`.

## Schaden ohne Treffer-Hook

Einen allgemeinen Hook **vor** der Schadensberechnung gibt es nicht. Die Spiel-DLLs bieten dafür keinen Einstieg. Eingreifen lässt sich an diesen
Stellen:

- Das Spielskript `OnHit`, über `WatchScript` und `Override_Script`.
- `Override_MonsterDamage`, an der Teilung des Monsterschadens.
- `Override_Damage`, am Ende, wenn die Lebenspunkte sinken.

## Beispiel-Mod

`examples/mods/demo_hooks/main.as` verbirgt Jens' Arbeit nachts. Außerdem schreibt sie die Items mit, die der Held bekommt.
