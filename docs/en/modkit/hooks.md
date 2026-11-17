# Hooks: change game functions (Override and Super)

[Deutsch](../../modkit/hooks.md) | **English**

With an **override**, a mod replaces a function of the game. The mod defines a function `bool Override_<Name>(...)`. The game then calls this
function in place of the original. With `Super()`, the original runs after all, at the point that you select. This is the same as `override`
and `Super::` in the modkit of the Gothic 1 Remake.

```angelscript
// Offer the work of Jens only between 8:00 and 20:00 game time
bool Override_InfoAvailable(const bCString &in info, const bCString &in owner, const bCString &in quest) {
    if (info == "G3ASM_Jens_Job") {
        int h = GetGameHour();
        if (h >= 0 && (h < 8 || h >= 20)) return false;   // option hidden, the game does not get the question
    }
    return Super();                                       // in all other cases, the game decides
}
```

## Rules

| What you do | Effect |
|---|---|
| `return Super();` | The original runs without changes. Its result goes to the game |
| `bool r = Super(); return !r;` | The original runs. You change the result |
| `return true;` without `Super()` | The modkit **skips** the original. Your value goes to the game |
| More than one mod with the same override | They make a chain in the load order. `Super()` calls the next mod, and at the end the original |
| Script error in the override | The game gets its original |

- The events after the action (`OnNPCKilled`, `OnReceiveItem`, ...) run only if the original really ran.
- The return value is always `bool`. Each override tells what `true` and `false` mean.
- An override must be fast. Some overrides run very frequently (see below). If no mod defines the function, the hook has no cost.

## Change arguments

You can change number arguments (`int`, `uint`) before `Super()` calls the original:

```angelscript
bool Override_ReceiveItem(Entity giver, Entity receiver, const bCString &in name, const bCString &in id, uint count) {
    if (name == "It_Wolf_Fur") SetOverrideArg(4, count * 2);   // argument 4 = quantity
    return Super();
}
```

| Function | Meaning |
|---|---|
| `bool SetOverrideArg(int index, uint value)` | Sets the argument with the number `index` (0 = first). Gives `false` for an incorrect index, for `Entity` and text arguments, and outside of an override |
| `uint GetOverrideArg(int index)` | Reads the current value (0 if the argument does not exist) |

- The new value applies to all other mods in the chain and to the original, only for this one call.
- You cannot change `Entity` and text arguments.
- **The effect of the change depends on the hook.** For the quest events (`Override_ReceiveItem` and the other five), the quest system gets the
  data about the action only after the action. The doubled quantity counts only for quest conditions ("bring 5 skins"). The inventory still gets
  one skin. For more items, call `receiver.AddItems(name, n)` after `Super()` (example `demo_hooks`). For damage, learn costs or monster damage,
  the change has a real effect.
- Why not `int &inout`? AngelScript permits `&inout` only for object types with handles. `SetOverrideArg` is a checked interface for this purpose.

## Overview

| Override | When | Return value |
|---|---|---|
| `Override_NPCKilled(Entity victim)` | An NPC was killed | Result of the quest system |
| `Override_NPCDefeated(Entity npc)` | An NPC was defeated | Result of the quest system |
| `Override_ReceiveItem(Entity giver, Entity receiver, const bCString &in name, const bCString &in id, uint count)` | Items go to a different owner | Result of the quest system |
| `Override_Enter(Entity who, const bCString &in area)`, `Override_Leave(Entity who, const bCString &in area)` | A person goes into or out of an area | Result of the quest system |
| `Override_SpellCast(Entity caster, Entity target, const bCString &in spell)` | A person casts a spell | Result of the quest system |
| `Override_InfoAvailable(const bCString &in info, const bCString &in owner, const bCString &in quest)` | The game asks if a dialog option shows | **`true` = the player can select the option** |
| [`Override_Damage(...)`](#override_damage) (from version 25) | A hit decreases the hit points | Without `Super()`, there is no hit |
| [`Override_MonsterDamage(...)`](#override_monsterdamage) (from version 42) | A monster hits a humanoid NPC | Without `Super()`, the damage is not divided |
| [`Override_LearnCost(...)`](#override_learncost) (from version 42) | A teacher tells or takes the price for an attribute | – |
| [`Override_CombatMove(...)`](#override_combatmove) (from version 45) | **Experimental.** A combat movement starts | – |
| [`Override_QuestStatus(...)`](#override_queststatus) (from version 35) | The status of a quest changes | Without `Super()`, the old status stays |
| [`Override_ConsoleCommand(...)`](#override_consolecommand) (from version 35) | A person types a line into the console | **`true` without `Super()` = the command is done** |
| [`Override_TradeGenerate(...)`](#override_tradegenerate) (from version 37) | The game makes the goods of a trader | Without `Super()`, there are no goods |
| [`Override_Script(...)`](#override_script) (from version 32) | A registered game script runs | Result for the game |
| [`Override_AI(...)`](#override_ai) (from version 41) | A registered AI script runs | `true` = complete |

The first six overrides are connected to the quest system of the game. The game tells the quest system **after** the action. Their return value
is only the result of this message. But for `Override_InfoAvailable`, the return value is really the yes or no for the game. With it, you decide
at all times if a dialog option shows. This is also true for options of the game. The game asks for each option of each NPC when it makes the
dialog menu.

## Override_Damage

```angelscript
bool Override_Damage(Entity victim, Entity inflictor, Entity owner, int amount, int type)
```

Runs before a hit decreases the hit points. It runs for NPCs and the hero, and for weapons, animals, spells and falls.

- `amount` is the loss of hit points. For a fatal hit, it is only the remaining hit points, not the full damage.
- `SetOverrideArg(3, n)` changes the loss. Without `Super()`, there is no hit.
- `inflictor` is the weapon or the projectile (`It_1H_…`, `Arrow`, `Mis_Fireball`; for animals `Fist`). For damage over time and for a fall,
  it is empty. If an archer hits the hero, the arrow comes as the inflictor, as for NPCs.
- `owner` is the cause. If the `inflictor` is an NPC, it is the `inflictor` itself. If not, it is the current opponent of the victim. If more than
  one opponent fights against one victim, this is an approximation. For spells, `owner` is the caster. For a fall, it is empty.
- `type` is the hit type from the item of the attacker: 1 blunt (club), 2 blade, 3 projectile, 4 fire, 5 ice, 6 lightning, 7 physics
  ([Reference](reference.md#1-damage-types-type-in-override_damage-and-ondamage)). If the modkit cannot read it (no item, or a spell item in the
  hand), it is 2 for NPCs and 0 for the hero.
- A hero that is already dead does not come back to life.
- The old form `Override_Damage(Entity victim, Entity attacker, int amount, int type)` continues to run, after the new form in the same chain.
  There, `attacker` is the `inflictor`, and `SetOverrideArg(2, n)` changes the quantity.

After this, `OnDamage` reports the hit ([Events](events.md)). Examples: `demo_lifesteal`, `perk_lifesteal`, `demo_hooks`.

## Override_MonsterDamage

```angelscript
bool Override_MonsterDamage(Entity victim, Entity attacker, int damage, int divisor)
```

If a monster or an animal hits a humanoid NPC, and neither of the two is the hero, the game divides the damage by 10. For summoned attackers, it
divides by 2. This override decides about the division.

- `damage` is the damage before the division. `divisor` is 10 or 2. You can change the two (`SetOverrideArg(2, ..)`, `SetOverrideArg(3, ..)`).
  `divisor` 1 = no division.
- `Super()` divides. **Without `Super()`, the game gets `damage` without division.**
- After this, the game continues its calculation (armor and other items). `Override_Damage` shows the final loss.
- If the plugin `Script_MonsterDamage.dll` is installed, or if the game version is different, the override stays off. The log tells this.
- The case "summoned attacker" (divisor 2) did not occur in the game yet.

Example `probe_monster_damage`.

## Override_LearnCost

```angelscript
bool Override_LearnCost(const bCString &in info, const bCString &in attrib, int amount, int lp, int gold)
```

The price of a teacher for an attribute (`STR`, `DEX`, `ALC`, ...).

- `info` is the teach info. `attrib` is the attribute. `amount` is the increase. `lp` and `gold` are the price of the game.
- `SetOverrideArg(3, ..)` changes the learn points. `SetOverrideArg(4, ..)` changes the gold. This works with and without `Super()`. Negative
  values count as 0. The override does not change the increase.
- Normally, one point costs one learn point. With "Alternative Balance" of the Community Patches, the game doubles the learn points and the gold
  when STR, DEX or INT is at 250.
- **The game asks more than 1 time for each teach action** (the display in the dialog, the check, the payment). The same inputs must give the
  same result.
- Skills do not go through this override.
- If the plugin `Script_LearningThresholdTweaks.dll` is installed, or if the game version is different, the override stays off. The log tells
  this.

Example `probe_learn_cost`.

## Override_CombatMove

```angelscript
bool Override_CombatMove(Entity self, const bCString &in move, uint speedPercent)
```

**Experimental.** The start of a combat movement of an NPC or of the hero.

- `move` is the phase: `Begin`, `Raise`, `Hit`, `Recover`.
- `speedPercent` is the playback speed in percent (100 = normal; the game itself often sets other values). `SetOverrideArg(2, p)` changes it for
  this movement, with and without `Super()`. The hits and the damage do not change.
- Walk and turn movements are not included.
- For `Recover`, `self` can be the weapon and not the fighter. If you also change the recovery, do a check of this.

Example `probe_attack_speed`.

## Override_QuestStatus

```angelscript
bool Override_QuestStatus(const bCString &in name, int oldStatus, int newStatus)
```

The status of a quest will change. `SetOverrideArg(2, x)` sets a different new status. Without `Super()`, the old status stays. After this,
`OnQuestStatus` comes if the status really changed. Status values: `QUEST_*` ([Quests](quests.md)).

## Override_ConsoleCommand

```angelscript
bool Override_ConsoleCommand(const bCString &in line)
```

Each line that a person types into the console. `true` without `Super()` means: the command belongs to the mod. `SetConsoleReply(text)` sets the
answer. Send other commands through with `return Super();`. The console is available only in Marvin mode (the key above Tab). Example
`probe_events`.

## Override_TradeGenerate

```angelscript
bool Override_TradeGenerate(Entity trader)
```

The game makes the goods of a trader from the treasure sets of the trader. Without `Super()`, the game makes no goods. After this,
`OnTradeGenerated` comes if the original ran.

## Override_Script

```angelscript
bool Override_Script(const bCString &in name, Entity self, Entity other, int arg)
```

A game script of a `Script_*.dll` runs through its name (`MagicTeleport`, `OnHit`, `GetAttitude`, ...).

- **Only for names that a mod registered first with `WatchScript(name)`.** The game calls hundreds of scripts each second. Without the
  registration, each call would cost time.
- Each mod with `Override_Script` sees all registered names. Do a check of `name` first. For other names, use `return Super();`.
- `SetOverrideArg(3, ..)` changes `arg`. Without `Super()`, the game gets the value from `SetScriptResult(int)`. If you did not set a value, it
  gets 1 for `true` and 0 for `false`.
- `self` and `other` can be not valid.
- Scripts that the game calls directly in its internal code (`AssessHit`, `IsDeadlyDamage`, `CanBurn`) do not run through their names. They do
  not come here.

After this, `OnScript` comes. Examples: `teleport_party` (`MagicTeleport`), `probe_script`.

## Override_AI

```angelscript
bool Override_AI(const bCString &in name, int kind, Entity self, Entity target)
```

An AI script of an NPC runs: a state (`ZS_...`, `kind` = `AI_STATE`), an AI function (`_AI_...`, `AI_FUNCTION`), a callback (`AI_CALLBACK`) or
a routine (`AI_ROUTINE`).

- As for `Override_Script`, only for names from `WatchScript(name)`.
- `self` is the NPC. `target` is its target. The two can be not valid.
- The return value goes to the game, with and without `Super()`. States and AI functions run across many frames. `false` means "not complete
  yet", and the game calls them again in the next frame. `true` means "complete".
- Without `Super()`, the original does not run in this frame. I did not check how the game reacts to this.
- You cannot get the arguments of an AI function (for example the item that `_AI_UseInventoryItem` uses).
- Routines (`AI_ROUTINE`) never occurred in the game yet.

After this, `OnAI` comes. Example `probe_ai`.

## Damage without a hit hook

There is no general hook **before** the damage calculation. The game DLLs do not give an entry point for it. You can change the damage at these
points:

- The game script `OnHit`, through `WatchScript` and `Override_Script`.
- `Override_MonsterDamage`, at the division of the monster damage.
- `Override_Damage`, at the end, when the hit points decrease.

## Example mod

`examples/mods/demo_hooks/main.as` hides the work of Jens at night. It also writes the items that go to the hero to the log.
