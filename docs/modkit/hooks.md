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
| `Override_Damage(Entity victim, Entity attacker, int amount, int type)` | NPCs: `gCDamageReceiver_PS::SetHitPoints` (sinkender Wert); Held: `gCEntity::OnDamage` | **Eingriff wirkt:** `SetOverrideArg(2, ..)` ändert den Lebenspunkte-Verlust, ohne `Super()` entfällt der Treffer. Menge = tatsächlicher Verlust, `type` = Schadenstyp des Spiels (gemessen: 2). `attacker` ist meist die Waffe oder das Geschoss (`Arrow`, `Mis_Fireball`), bei Schaden über Zeit und beim Helden leer |

**Schaden am Helden:** Der Held hat seine Lebenspunkte im Attribut `HP`, sein Schadensempfänger steht immer auf 1. Beim Treffer sind die Lebenspunkte schon abgezogen, wenn der Hook
läuft; eine geänderte Menge wird deshalb nachträglich ausgeglichen (die Differenz geht an das Attribut zurück bzw. wird zusätzlich abgezogen, `Schaden am Helden: 34 -> 17` im Protokoll).
Ein schon toter Held wird nicht wiederbelebt. **Fallschaden** läuft nicht über diesen Weg (gemessen: Sturz 176 → 145 ohne `OnDamage`, und auch nicht über `gCPlayerMemory_PS`). Er lässt sich nur erkennen, indem ein Skript jedes Bild (`OnFrame`) das Attribut `HP` vergleicht; `hooks_demo` zeigt das und gleicht den Verlust aus. Ein tödlicher Sturz ist dafür zu spät.

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
- Kein Schaden- oder Treffer-Hook: in der Game.dll fehlt dafür ein exportierter Einstiegspunkt.
- Ein Override sollte schnell sein. `Override_InfoAvailable` wird für jede Option jedes NPCs beim Aufbau des Menüs aufgerufen.
  Der Patch ruft sie nur auf, wenn irgendein Mod die Funktion definiert, sonst kostet es nichts.

## Beispiel-Mod

`examples/mods/hooks_demo/main.as`: verbirgt Jens' Mod-Arbeit nachts und schreibt Item-Übergaben an den Helden mit.
