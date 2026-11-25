# Events

[Deutsch](../../modkit/events.md) | **English**

A mod defines the functions for the events that it wants to know about. The modkit calls a function when its event occurs. All functions are
**optional**. **The signature must be exactly correct.** Write the parameter types and `const bCString &in` exactly as shown. If you do not, the
modkit does not call the function. Each event goes to all loaded mods, one after the other.

Overrides change or cancel a game function. The page [Hooks](hooks.md) describes them.

## Start, frames, keys

| Function | When |
|---|---|
| `void main()` | 1 time when the mod starts, and after each reload (hot reload) |
| `void OnFrame(float dt)` | Each frame. `dt` is in seconds |
| `void OnKey(int vk, bool down)` | A key goes down (`down = true`) or up. The modkit does not report key repeat |
| `void OnDraw()` (from version 34) | Each frame, to draw above the HUD and the inventory |
| `void OnWorldActivate()` | The game loaded a world |
| `void OnWorldDeactivate()` | The game unloads the world. Do not use entities after this event |
| `void OnShutdown()` | Before the game stops, and before the modkit reloads or unloads the mod |
| `void OnModMessage(const bCString &in sender, const bCString &in topic, const bCString &in data)` (from version 44) | Another mod called `SendModMessage(topic, data)` |

- `OnFrame` and `OnDraw` run very frequently. Do only a small quantity of work in them. The modkit stops each call after 5 seconds.
- `OnDraw` also runs in the main menu and on menu pages. To draw only in the game, do a check of `GetHero().IsValid()` and `GetActivePage()`.
  `DrawText`, `TextWidth` and `DrawImage` work only here.
- `OnModMessage` runs immediately, during the call of the sender. `sender` is the name of the sender. The sender does not get its own message.

## Game world

| Function | When |
|---|---|
| `void OnNPCKilled(Entity victim)` | An NPC was killed |
| `void OnNPCDefeated(Entity npc)` | An NPC was defeated, but not killed (for example unconscious) |
| `void OnReceiveItem(Entity giver, Entity receiver, const bCString &in itemName, const bCString &in itemId, uint count)` | Items go to a different owner |
| `void OnEnter(Entity who, const bCString &in area)`, `void OnLeave(Entity who, const bCString &in area)` | A person goes into or out of an area (`Silden_InnerZone`, ...) |
| `void OnSpellCast(Entity caster, Entity target, const bCString &in spell)` | A person casts a spell (`Spl_Fireball`, `Spl_HealOther`, ...) |
| `void OnDamage(Entity victim, Entity inflictor, Entity owner, int amount, int type)` (from version 25) | A hit decreased the hit points |
| `void OnStatusEffect(Entity npc, int effect, int enabled)` (from version 27) | A status effect (`STATUS_*`) came on (`1`) or went off (`0`) |
| `void OnTouch(Entity self, Entity other)` (from version 37) | `other` touches `self` |
| `void OnTransformStart(Entity self, Entity target)`, `void OnTransformEnd(Entity self)` (from version 37) | A transformation starts or ends |
| `void OnTradeGenerated(Entity trader, int ok)` (from version 37) | The game made the goods of a trader (`ok` 1 = success) |
| `void OnTrade(Entity trader, const bCString &in item, int amount, int gold, bool bought)` (from version 47) | The hero bought from a trader (`bought` = `true`) or sold to a trader (`false`) |
| `void OnDamage(Entity victim, Entity attacker, int amount, int type)` | The old form without `owner`. `attacker` is the `inflictor` |

The quest system of the game reports the first five events. It reports them for **each** occurrence, not only for running quests. At that time,
the game has already done the action. Your function runs after it.

- **`OnNPCKilled`** gives only the victim. `OnDamage` (`owner`) comes before it and shows the killer.
- **`OnReceiveItem`:** `itemName` is the name of the item template (`It_Wolf_Fur`, `Arrow`). `itemId` is the ID as text. If the modkit cannot
  read the name, the two parameters contain the ID. `giver` or `receiver` can be not valid (an item that the player finds, buys or delivers). When
  the player loots a body, the dead person is `giver`. Scrolls also cause the event when the player uses them (`It_Scroll_Fireball` from the hero
  to nobody).
- **`OnEnter` and `OnLeave`:** The modkit filters out the internal navigation zones of the NPCs (`Nav_*`, approximately 600 each minute).
  `navevents=1` in the `g3asm.ini` lets them through.
- **`OnDamage`** runs after the hit. [`Override_Damage`](hooks.md#override_damage) gives the details about `inflictor`, `owner`, `amount` and
  `type`.
- **`OnStatusEffect`** comes 1 time for each changed effect, only for a real change. It comes for NPCs and for the hero, also if a mod sets the
  effect.
- **`OnTouch`** comes very frequently: thousands of times in a few seconds after a load, usually for NPCs at navigation zones. Filter first, for
  example `if (self != GetHero() && other != GetHero()) return;`. Then do the work.
- **`OnTransformEnd`:** `self` is the transformed creature (for example the snapper), not the hero.
- **`OnTrade`** comes after the player confirms on the trade page, one time for each item (name of the item template, for example `It_Apple`).
  `amount` is the quantity of this item. `gold` is the gold that went in the other direction in the same trade: for a purchase, the gold that the
  hero pays. For a sale, the gold that the trader pays. For more than one item in one direction, `gold` is the total. **Caution:** If the hero
  buys and sells in the same trade, the game offsets the two. Only the side with the higher value gets gold. The other direction gives `gold` = 0.
  Example: two weapons sold and one bottle of booze bought give `gold` = 295 for the sale and `gold` = 0 for the purchase. `OnTrade` does not report
  gold as an item. Looting, gifts and dialogs do not cause `OnTrade`. `OnReceiveItem` does not report a trade.

## Quests, dialogs, load and save

| Function | When |
|---|---|
| `void OnQuestsLoaded()` | The game made its quests again (game start, load). Make your own quests here |
| `void OnInfosLoaded()` | The game made its dialogs again. Make your own infos here |
| `void OnQuestStatus(const bCString &in name, int oldStatus, int newStatus)` (from version 35) | The status of a quest changed (`QUEST_*`) |
| `void OnInfoFinished(const bCString &in info, const bCString &in owner, const bCString &in quest)` | The speakers said all lines of a dialog (info) |
| `void OnBeforeSave(const bCString &in name)` (from version 31) | The game will write a saved game. Remove permanent conditions here |
| `void OnAfterSave(const bCString &in name, int ok)` (from version 31) | The game wrote the saved game (`ok` 1 = success). Restore the conditions here |

- [Save and load](saving.md) tells why your quests and infos must go into the load events. [Dialogs](dialogs.md) describes dialogs.
- `OnBeforeSave` and `OnAfterSave` run exactly 1 time for each save, also for a quick save. `name` is the saved game (`G3_World_01_0029`,
  `QuickSave`). `OnAfterSave` also comes if the save failed.
- `OnQuestStatus` comes only for a real change. For example, when the player accepts a task, it reports 0 → 1.

## Game scripts and AI

| Function | When |
|---|---|
| `void OnScript(const bCString &in name, Entity self, Entity other, int arg, int result)` (from version 32) | A game script that you registered with `WatchScript(name)` ran. `result` is its result |
| `void OnAI(const bCString &in name, int kind, Entity self, Entity target, int result)` (from version 41) | A registered AI script ran. `result` 1 = complete, 0 = it continues in the next frame |
| `void OnAIOutput(Entity speaker, const bCString &in id)` (from version 41) | An NPC or the hero starts to say a sentence |

- `OnScript` and `OnAI` come only if the original ran (see [`Override_Script`](hooks.md#override_script) and [`Override_AI`](hooks.md#override_ai)).
- `OnAIOutput` comes 1 time for each sentence, in dialogs and in ambient talk. `id` is the name of the speech file without `.wav`, for example
  `INFO_Hum_Elite_Hard_Bufford_SKALVERAM3638_German`. `GetSpeechText(id)` gives the text. For subtitles as an example, see `probe_ai`.

## Example

```angelscript
void OnNPCKilled(Entity victim) {
    print("killed: " + victim.GetName());
    PrintScreen("Killed: " + victim.GetName(), 3, 255, 120, 120);
}

void OnReceiveItem(Entity from, Entity to, const bCString &in item, const bCString &in guid, uint count) {
    if (to.IsValid() && to == GetHero())
        PrintScreen("+ " + item + " x" + count, 3, 140, 255, 140);
}
```

## Sequence and nesting

- The modkit calls the mods in the load order ([Mod folder](mod-package.md#load-order-load_ordertxt)).
- One action can cause more than one event in a short time. A kill with a spell reports `OnSpellCast`, `OnReceiveItem` and `OnNPCKilled`.
- If your script itself causes an event (for example because it removes an item), this event runs nested, before your call continues.
- Scripts run only in the game thread. This is the thread that also calls `OnFrame`. If an event comes from a different thread, the scripts do not
  get it, and for overrides the original runs. The log gives the first cases (`Aufruf aus fremdem Thread, nicht an Skripte gemeldet: ...`, call
  from a different thread, not reported to scripts).
