# Ereignisse

**Deutsch** | [English](../en/modkit/events.md)

Eine Mod definiert die Funktionen für die Ereignisse, auf die sie reagieren will. Das Modkit ruft eine Funktion auf, wenn ihr Ereignis eintritt. Alle
Funktionen sind **optional**. **Die Signatur muss genau stimmen.** Schreibe die Parametertypen und `const bCString &in` genau so wie hier. Sonst ruft
das Modkit die Funktion nicht auf. Jedes Ereignis geht der Reihe nach an alle geladenen Mods.

Overrides ändern eine Spielfunktion oder brechen sie ab. Sie stehen auf der Seite [Hooks](hooks.md).

## Start, Bilder, Tasten

| Funktion | Wann |
|---|---|
| `void main()` | Einmal beim Start der Mod und nach jedem Neuladen (Hot-Reload) |
| `void OnFrame(float dt)` | Jedes Bild. `dt` in Sekunden |
| `void OnKey(int vk, bool down)` | Eine Taste wird gedrückt (`down = true`) oder losgelassen. Tastenwiederholung meldet das Modkit nicht |
| `void OnDraw()` (ab Version 34) | Jedes Bild, zum Zeichnen über HUD und Inventar |
| `void OnWorldActivate()` | Das Spiel hat eine Welt geladen |
| `void OnWorldDeactivate()` | Das Spiel entlädt die Welt. Verwende Entities danach nicht mehr |
| `void OnShutdown()` | Bevor das Spiel endet und bevor das Modkit die Mod neu lädt oder entlädt |
| `void OnModMessage(const bCString &in sender, const bCString &in topic, const bCString &in data)` (ab Version 44) | Eine andere Mod hat `SendModMessage(topic, data)` aufgerufen |

- `OnFrame` und `OnDraw` laufen sehr oft. Leg nur wenig Arbeit hinein. Das Modkit bricht jeden Aufruf nach 5 Sekunden ab.
- `OnDraw` läuft auch im Hauptmenü und auf Menüseiten. Willst du nur im Spiel zeichnen, prüfe `GetHero().IsValid()` und `GetActivePage()`. Nur hier
  funktionieren `DrawText`, `TextWidth` und `DrawImage`.
- `OnModMessage` läuft sofort, mitten im Aufruf des Senders. `sender` ist der Name des Senders. Der Sender bekommt seine eigene Nachricht nicht.

## Spielwelt

| Funktion | Wann |
|---|---|
| `void OnNPCKilled(Entity victim)` | Ein NPC wurde getötet |
| `void OnNPCDefeated(Entity npc)` | Ein NPC wurde besiegt, aber nicht getötet (z. B. bewusstlos) |
| `void OnReceiveItem(Entity giver, Entity receiver, const bCString &in itemName, const bCString &in itemId, uint count)` | Items wechseln den Besitzer |
| `void OnEnter(Entity who, const bCString &in area)`, `void OnLeave(Entity who, const bCString &in area)` | Jemand betritt oder verlässt einen Bereich (`Silden_InnerZone`, ...) |
| `void OnSpellCast(Entity caster, Entity target, const bCString &in spell)` | Jemand wirkt einen Zauber (`Spl_Fireball`, `Spl_HealOther`, ...) |
| `void OnDamage(Entity victim, Entity inflictor, Entity owner, int amount, int type)` (ab Version 25) | Ein Treffer hat Lebenspunkte gekostet |
| `void OnStatusEffect(Entity npc, int effect, int enabled)` (ab Version 27) | Ein Statuseffekt (`STATUS_*`) wurde an- (`1`) oder ausgeschaltet (`0`) |
| `void OnTouch(Entity self, Entity other)` (ab Version 37) | `other` berührt `self` |
| `void OnTransformStart(Entity self, Entity target)`, `void OnTransformEnd(Entity self)` (ab Version 37) | Eine Verwandlung beginnt oder endet |
| `void OnTradeGenerated(Entity trader, int ok)` (ab Version 37) | Das Spiel hat die Ware eines Händlers erzeugt (`ok` 1 = gelungen) |
| `void OnDamage(Entity victim, Entity attacker, int amount, int type)` | Alte Form ohne `owner`. `attacker` ist der `inflictor` |

Die ersten fünf Ereignisse meldet das Questsystem des Spiels, und zwar bei **jedem** Vorfall, nicht nur bei laufenden Quests. Das Spiel hat den
Vorgang dann schon ausgeführt. Deine Funktion läuft danach.

- **`OnNPCKilled`** nennt nur das Opfer. Wer getötet hat, zeigt vorher `OnDamage` (`owner`).
- **`OnReceiveItem`:** `itemName` ist der Name der Item-Vorlage (`It_Wolf_Fur`, `Arrow`). `itemId` ist die ID als Text. Kann das Modkit den Namen nicht
  lesen, steht die ID in beiden Parametern. `giver` oder `receiver` können ungültig sein (Fund, Kauf, Abgabe). Beim Plündern ist der Tote `giver`.
  Auch Schriftrollen melden sich beim Zaubern (`It_Scroll_Fireball` vom Helden an niemanden).
- **`OnEnter` und `OnLeave`:** Interne Navigationszonen der NPCs (`Nav_*`, rund 600 je Minute) filtert das Modkit heraus. `navevents=1` in der
  `g3asm.ini` lässt sie durch.
- **`OnDamage`** läuft nach dem Treffer. Einzelheiten zu `inflictor`, `owner`, `amount` und `type` stehen bei
  [`Override_Damage`](hooks.md#override_damage).
- **`OnStatusEffect`** kommt einmal je geändertem Effekt, nur bei echter Änderung. Es kommt für NPCs und den Helden, auch wenn eine Mod den Effekt
  setzt.
- **`OnTouch`** kommt sehr oft: nach dem Laden tausende Male in wenigen Sekunden, meist für NPCs an Navigationszonen. Filtere zuerst, zum Beispiel
  `if (self != GetHero() && other != GetHero()) return;`. Arbeite erst danach.
- **`OnTransformEnd`:** `self` ist das verwandelte Wesen (z. B. der Snapper), nicht der Held.

## Quests, Dialoge, Laden und Speichern

| Funktion | Wann |
|---|---|
| `void OnQuestsLoaded()` | Das Spiel hat seine Quests neu aufgebaut (Spielstart, Laden). Lege hier eigene Quests an |
| `void OnInfosLoaded()` | Das Spiel hat seine Dialoge neu aufgebaut. Lege hier eigene Infos an |
| `void OnQuestStatus(const bCString &in name, int oldStatus, int newStatus)` (ab Version 35) | Der Status einer Quest hat sich geändert (`QUEST_*`) |
| `void OnInfoFinished(const bCString &in info, const bCString &in owner, const bCString &in quest)` | Ein Gespräch (Info) ist zu Ende gesprochen |
| `void OnBeforeSave(const bCString &in name)` (ab Version 31) | Das Spiel schreibt gleich einen Spielstand. Entferne hier dauerhafte Zustände |
| `void OnAfterSave(const bCString &in name, int ok)` (ab Version 31) | Das Spiel hat den Spielstand geschrieben (`ok` 1 = gelungen). Stelle hier die Zustände wieder her |

- Warum eigene Quests und Infos in die Lade-Ereignisse gehören, steht unter [Speichern und Laden](saving.md). Dialoge stehen unter
  [Dialoge](dialogs.md).
- `OnBeforeSave` und `OnAfterSave` laufen genau einmal je Speichern, auch beim Schnellspeichern. `name` ist der Spielstand (`G3_World_01_0029`,
  `QuickSave`). `OnAfterSave` kommt auch, wenn das Speichern fehlschlug.
- `OnQuestStatus` kommt nur bei echter Änderung. Nimmt der Spieler zum Beispiel einen Auftrag an, meldet es 0 → 1.

## Spielskripte und KI

| Funktion | Wann |
|---|---|
| `void OnScript(const bCString &in name, Entity self, Entity other, int arg, int result)` (ab Version 32) | Ein mit `WatchScript(name)` angemeldetes Spielskript ist gelaufen. `result` ist sein Ergebnis |
| `void OnAI(const bCString &in name, int kind, Entity self, Entity target, int result)` (ab Version 41) | Ein angemeldetes KI-Skript ist gelaufen. `result` 1 = fertig, 0 = läuft im nächsten Bild weiter |
| `void OnAIOutput(Entity speaker, const bCString &in id)` (ab Version 41) | Ein NPC oder der Held beginnt einen gesprochenen Satz |

- `OnScript` und `OnAI` kommen nur, wenn das Original lief (siehe [`Override_Script`](hooks.md#override_script) und
  [`Override_AI`](hooks.md#override_ai)).
- `OnAIOutput` kommt einmal je Satz, im Gespräch wie beim Umgebungsgerede. `id` ist der Name der Sprachdatei ohne `.wav`, zum Beispiel
  `INFO_Hum_Elite_Hard_Bufford_SKALVERAM3638_German`. Den Text liefert `GetSpeechText(id)`. Untertitel als Beispiel: `probe_ai`.

## Beispiel

```angelscript
void OnNPCKilled(Entity opfer) {
    print("getötet: " + opfer.GetName());
    PrintScreen("Getötet: " + opfer.GetName(), 3, 255, 120, 120);
}

void OnReceiveItem(Entity von, Entity an, const bCString &in item, const bCString &in guid, uint anzahl) {
    if (an.IsValid() && an == GetHero())
        PrintScreen("+ " + item + " x" + anzahl, 3, 140, 255, 140);
}
```

## Reihenfolge und Verschachtelung

- Das Modkit ruft die Mods in Ladereihenfolge auf ([Mod-Ordner](mod-package.md#ladereihenfolge-load_ordertxt)).
- Ein Vorgang kann mehrere Ereignisse kurz nacheinander auslösen. Ein Kill mit Zauber meldet `OnSpellCast`, `OnReceiveItem` und `OnNPCKilled`.
- Löst dein Skript selbst ein Ereignis aus (zum Beispiel, weil es ein Item entfernt), läuft dieses verschachtelt, bevor dein Aufruf weitergeht.
- Skripte laufen nur im Spiel-Thread. Das ist der Thread, der auch `OnFrame` ruft. Kommt ein Ereignis aus einem anderen Thread, bekommen die Skripte
  es nicht, und bei Overrides läuft das Original. Das Protokoll nennt die ersten Fälle
  (`Aufruf aus fremdem Thread, nicht an Skripte gemeldet: ...`).
