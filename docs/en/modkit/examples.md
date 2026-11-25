# Examples

[Deutsch](../../modkit/examples.md) | **English**

The complete mods are in `examples/mods/` (in the release zip: `examples\mods\`). If you did not write a mod before, start with
[First mod step by step](erste-mod.md).

## How to use an example

1. Copy the folder of the mod to `<game folder>\g3asm\mods\`. Example: copy `examples\mods\demo_events` to `<game folder>\g3asm\mods\demo_events`.
2. All examples are **off by default**. To turn the mod on, add a line `+demo_events` to `<game folder>\g3asm\mods\load_order.txt`.
3. Start the game. `g3asm.log` shows what the mod does. Frequently, the screen also shows it.

**Caution:** Some mods write permanent data into the saved game (`perk_lifesteal`, `probe_time`). Read the section of the mod before you save with
it. Do not turn on mods that use the same key at the same time (see [Key assignments](#key-assignments)).

The names tell the type of a mod:

| Name | Type |
|---|---|
| `demo_*` | Learning example: shows one function of the modkit. It is small, and you can copy it |
| `probe_*` | Measurement mod: measures or logs a part of the game, for the development of the modkit |
| Other | Small complete mods with game content: `jens_quest`, `jens_dialog`, `perk_lifesteal`, `beliars_klaue`, `teleport_party` |

## Small scripts to copy

Three parts without their own mod folder. Each part fits into one `main.as`.

**Log events** (the full version is `demo_events`). This is useful to find the names of NPCs, items and spells:

```angelscript
bool IsHero(Entity e) { return e.IsValid() && e == GetHero(); }

void OnNPCKilled(Entity victim) {
    print("killed: " + victim.GetName());
    PrintScreen("Killed: " + victim.GetName(), 3, 255, 120, 120);
}

void OnReceiveItem(Entity from, Entity to, const bCString &in item, const bCString &in guid, uint count) {
    print("Item " + item + " x" + count + " from " + from.GetName() + " to " + to.GetName());
    if (IsHero(to)) PrintScreen("+ " + item + " x" + count, 3, 140, 255, 140);
}

void OnSpellCast(Entity caster, Entity target, const bCString &in spell) {
    print(caster.GetName() + " casts " + spell + " on " + target.GetName());
}
```

**Keep a position and go back to it** (numeric keypad 1 keeps the position, numeric keypad 2 goes back):

```angelscript
bCVector mark;
bool marked = false;

void OnKey(int vk, bool down) {
    if (!down) return;
    Entity hero = GetHero();
    if (vk == 0x61 && hero.IsValid()) {            // numeric keypad 1: keep the position
        mark = hero.GetPosition();
        marked = true;
        PrintScreen("Position kept: " + mark.ToString());
    } else if (vk == 0x62 && marked) {             // numeric keypad 2: go back
        hero.SetPosition(mark);
    }
}
```

**Attributes and time of day** (numeric keypad 3: +1 strength, numeric keypad 4: noon):

```angelscript
void OnKey(int vk, bool down) {
    if (!down) return;
    Entity hero = GetHero();
    if (vk == 0x63) {                              // numeric keypad 3: +1 strength
        hero.AddAttributeBase("STR", 1);
        PrintScreen("Strength: " + hero.GetAttribute("STR"));
    } else if (vk == 0x64) {                       // numeric keypad 4: noon
        SetGameTime(-1, 12, 0);
    }
}
```

## Mods with game content

### `jens_quest`: a quest without a dialog

**Shows:** a quest that consists only of events. When the hero comes near Jens in Kap Dun, Jens asks for a wolf skin, and the quest starts. To
deliver the skin, push the Scroll Lock key near Jens, or give the skin to him in the game (`OnReceiveItem`). If Jens dies, the quest fails.

```angelscript
// Part of the script
void OnKey(int vk, bool down) {
    if (!down || vk != 0x91) return;               // Scroll Lock key
    Entity hero = GetHero();
    if (hero.GetItemCount(ITEM) < 1) return;
    if (hero.RemoveItems(ITEM, 1) < 1) return;
    SetQuestXP(QUEST, 100);
    SucceedQuest(QUEST);
}
```

**Caution:** Do not turn it on together with `jens_dialog`. The two mods use the same quest.

### `jens_dialog`: a quest with dialogs

**Shows:** the full version of [First mod step by step](erste-mod.md). Jens gets two dialog options ("Hast du Arbeit für mich?", do you have work
for me?, and "Ich habe das Wolfsfell.", I have the wolf skin). The quest shows in the quest log with the dialog lines. The reward is a bow, arrows
and one point of dexterity. The mod also does two things that the guide does not include:
- The quest comes as a real quest file from a mod archive (`ModFiles\Quests.m0x`, see [Mod archives](archives.md)). If the archive is not
  installed, the script makes the quest itself.
- A fallback for saved games from before the mod was turned on: the progress is also in switches in the saved game (`SetGameEvent`). See
  [Save and load](saving.md#emergency-recovery).

### `perk_lifesteal`: a custom skill with a tile

**Shows:** a bonus skill "Lebensraub" (life steal) for the Orkschlächter (orc slayer), with a tile in the character sheet. When the hero uses the
Orkschlächter, each hit heals the hero. For this, the mod clones a skill template and a learn item. It connects the two with `SetTemplateRef`. It
gives the learn item to the hero, and the weapon turns on the skill. The healing comes from `OnDamage`.

**Caution:** The skill is in the saved game. Thus the mod removes it before each save and makes it again after the save. If you want to turn off
the mod and you have older saved games with the skill, do these steps first: set `ENTFERNEN = true` (remove) in `main.as`, load, save, and only
then turn the mod off. If you do not do this, the game crashes when you open the character sheet.

### `beliars_klaue`: lightning at strong hits

**Shows:** damage with a cause through a summon. When the hero hits an opponent strongly with the Orkschlächter (55 damage or more), lightning
strikes with a chance of 30 %. For this, the mod makes the summon `Smn_Lightning` at the victim. It sets the hero as the owner with
`SetEntityRef` and starts the lightning effect.

- Opponents are NPCs that are hostile to the hero, predators and angry animals.
- There are a minimum of 4 seconds between two lightning strikes. The lightning causes between 17 and more than 500 damage. It can hit persons
  near the victim.
- You can adjust values at the top of `main.as` (`MIN_TREFFER`, minimum hit, and `CHANCE_PROZENT`, chance in percent).

### `teleport_party`: companions teleport with the hero

**Shows:** how to intercept a game script. When the hero teleports, the mod brings the companions, summons and slaves of the hero. It puts them
in a circle 1.5 m next to the hero. For this, it intercepts the game script `MagicTeleport` (`WatchScript` and `Override_Script`).

- Companions that the hero told "Wait here" stay (`WARTENDE_MIT`, waiting ones too, in `main.as`).
- If a companion is more than 15 m away after 1.5 seconds, the mod brings the companion again.
- **Ctrl + numeric keypad ,** writes the group to the log.

## Learning examples

| Mod | Shows | Controls |
|---|---|---|
| `demo_events` | All events of the quest system and trade (`OnTrade`) in the log and on the screen | – |
| `demo_hooks` | Change game functions with `Override_...` and `Super()` ([Hooks](hooks.md)) | – |
| `demo_quest` | All quest functions on a demo quest | Numeric keypad 1–6: status, make and start, success, failure, remove, XP. Page Up: list the quests. Page Down: show infos. Insert: load the quest files again |
| `demo_properties` | Read the properties of an item | Ctrl + numeric keypad /: write the item in the hand (if there is none, the item below the crosshair) to the log |
| `demo_lifesteal` | Set the quality of a weapon, a custom text in the description, healing at hits | Numeric keypad \*: turn "Lebensraub" on the weapon on or off |
| `demo_clone` | Clone an item template, with a custom name and text | Numeric keypad 0: put a cloned skin into the inventory |
| `demo_spawn` | Make and remove NPCs | Numeric keypad +: search templates. `,`: make an NPC at the crosshair. `-`: remove the last one. `*`: remove all of this session. `/`: remove all Diegos that the mod made |
| `demo_focus` | Read the focus and the crosshair | Home: report the target. Delete: remove a spawned NPC below the crosshair |
| `demo_text` | Custom texts only through `stringtable.ini`, without a script ([Mod folder](mod-package.md#your-own-texts-stringtableini)) | – |
| `demo_patch` | A file patch on a quest file ([File patches](patches.md)) | – |
| `demo_packmod` | Write a mod archive and do a check if the game reads it ([Mod archives](archives.md)) | – |
| `demo_coroutine` | Coroutines: countdown with `Wait`, stop with `StopCoroutine`, a clock in the log ([Coroutines](scripting-guide.md#coroutines-from-version-47)) | F10: start the countdown. F11: stop it |

**`demo_lifesteal`:** "Lebensraub" (life steal) is a property of the weapon, as "geschärft" (sharpened) is. It shows as a line
`(20 %) Lebensraub` in the description of the weapon. When the hero hits with the weapon, the hero gets 20 % of the damage back as hit points.
For a fatal hit, only the remaining hit points of the victim count.

## Measurement mods

These mods are for the development of the modkit. They write mainly to the log. They are useful for learning if you look for a specific function.

| Mod | Measures | Controls |
|---|---|---|
| `probe_draw` | Draw above the HUD and the inventory: text, images, a turning compass, `WorldToScreen` | – |
| `probe_script` | Game scripts by name: `ScriptExists`, `Override_Script`, `CallScript` | Ctrl + numeric keypad +: call read-only scripts for the hero and the target |
| `probe_time` | Time rate and game clock | Shift + numeric keypad + or -: time rate 2 times or half. Shift + numeric keypad \*: clock ×10 |
| `probe_ai` | AI scripts (`Override_AI`) and spoken sentences as subtitles (`OnAIOutput`) | – |
| `probe_monster_damage` | `Override_MonsterDamage`: monsters cause 2 times the damage to NPCs | – |
| `probe_learn_cost` | `Override_LearnCost`: teachers ask for half the price | – |
| `probe_schema` | `DumpPropertySchema`: writes `g3asm\property_schema_1.json` and `_2.json` | – |
| `probe_attack_speed` | `Override_CombatMove`: faster hits of the hero (factor in the `mod.ini`) | – |
| `probe_freeze` | Functions that came with version 46 | Ctrl + Shift + numeric keypad \*: `Kill`. -: `GenerateTradeGoods`. +: crosshair and clock |
| `probe_events` | Console, quest status, trader goods | Ctrl + numeric keypad \*: open the console. Shift + numeric keypad /: make the goods of the trader again |
| `probe_lightning` | Lightning on a target | Ctrl + numeric keypad -: lightning on the NPC in the crosshair |
| `probe_reference` | Lists of checked names for the log ([Reference](reference.md)) | Numeric keypad /: write the list |

**Caution with `probe_time`:** The clock factor is in the saved game. The mod sets it back before the save and sets it again after the save.

## Key assignments

Free keys for your own mods: the numeric keypad (but not 7, 8, 9 and Ctrl + 1 to 6), Home, Page Up, Page Down, Insert, Delete and Scroll Lock
(without Ctrl).

The game uses these keys itself:
- Numeric keypad 7, 8, 9 and Ctrl + numeric keypad 1 to 6.
- F1 to F4, F6 to F9 (slow motion, reset the player, quick load) and F12.

Do not use: End and Ctrl + End do not get to the script. Many keyboards do not have a Pause key.

These examples use the same keys. Do not turn them on at the same time:
- `demo_spawn` and `demo_lifesteal` (numeric keypad \*).
- `demo_spawn` and `probe_reference` (numeric keypad /).
- `jens_quest` and the mini mod from the [Quick start](index.md#quick-start) (Scroll Lock).

The mini mod `examples/scripts/main.as`, which the installer makes, uses F10 and F11.

## Tips

- First find names with `print()`. Then write them into the mod.
- Start small, and add one event after the other. With hot reload, this means: save, wait 1 second, try.
- To turn off a mod, write `-Name` in `g3asm\mods\load_order.txt`. To turn on an example, write `+Name`.
