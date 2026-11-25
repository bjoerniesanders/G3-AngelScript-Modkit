# Limits

[Deutsch](../../modkit/limitations.md) | **English**

This page tells what the modkit can do, what does not work, and what nobody has checked yet. In the [API reference](api-reference.md), some
functions also have the labels **Experimental**, **Not checked in the game**, **Does not work** or **Deprecated**. The
[Version rules](api-policy.md) tell what can change between versions.

**General limit:** The game logic of Gothic 3 is in native DLLs. A mod can connect to the game only at the points that the modkit makes available.

## Checked in the game

**Basics**
- Mods load. Hot reload works. An error in one mod has no effect on the other mods. Mods can be off by default (`mod.ini`), and you can turn them on
  with `load_order.txt`.
- One game session with 14 example mods at the same time, without error lines.
- The floating-point setting of the game does not change. If it changed, the game would calculate with less accuracy, for example with blurred
  textures.

**Events and overrides**
- The six events of the quest system (`OnNPCKilled`, `OnNPCDefeated`, `OnReceiveItem`, `OnEnter` and `OnLeave`, `OnSpellCast`).
- Damage (`Override_Damage` and `OnDamage`) for NPCs and for the hero, from humans, animals, spells and falls. Change the quantity and prevent
  the hit.
- `OnStatusEffect`, `Override_InfoAvailable`, `Override_ReceiveItem`, `Override_QuestStatus` and `OnQuestStatus`, `Override_ConsoleCommand`,
  `OnTouch`, `OnTradeGenerated`, `OnTransformStart` and `OnTransformEnd`.
- Save: `OnBeforeSave` and `OnAfterSave` run exactly 1 time for each save, also for a quick save.
- Game scripts by name (`CallScript`, `Override_Script` and `OnScript`), AI states, AI functions and callbacks (`Override_AI` and `OnAI`), spoken
  sentences (`OnAIOutput`, `GetSpeechText`).
- `Override_LearnCost` (also `ALC`), `Override_MonsterDamage`.

**Game world**
- Attributes, the inventory (count, remove, make, delete stacks), item quality with a custom line "Lebensraub" (life steal) in the description.
- Custom skills with a tile in the character sheet (`perk_lifesteal`).
- Kill (`Kill`) with experience and loot. Hurt and kill through a lightning summon with an owner (`beliars_klaue`). Start effects.
- Clone templates. Items from clones stay after a save, a load and a new start.
- Make and remove NPCs, also from previous sessions.
- Focus and crosshair, group and teleport (`teleport_party`), time rate, clock factor, set the game time.
- Read properties (`GetProperty`, `ListProperties`), set references (`SetTemplateRef`, `SetEntityRef`), the schema of all properties
  (`DumpPropertySchema`).

**Quests, dialogs, files**
- Make quests and control them, with a title, XP and an attribute reward. List all 571 quests of the game. Read quest files.
- Read dialogs (4593 infos) and make your own: options in the dialog menu, lines in the quest log, `OnInfoFinished`.
- Entries in the quest log without a dialog (`AddQuestLogEntry`, from v0.47.1), also after a save, a load and a restart of the game.
- Conditions of custom infos: items with a quantity, switches in the saved game (set and not set) and skills (`SetInfoProperty`, from v0.47.2).
  The game itself does the check.
- New goods for traders (`GenerateTradeGoods`, from v0.47.2): the new delivery is added to the old goods.
- Your own quests and dialogs come back with their status and quest log after a save and a load. Switches in the saved game (`SetGameEvent`).
- File patches on quest files (`demo_patch`). The game loads mod archives (`PackMod`), and they overwrite existing files.
- Draw above the HUD and the inventory: text, images, `WorldToScreen`.

**Mod packages for players**
- The runtime starts as a script DLL without the loader, without `g3asm.ini` and without the `g3asm\` folder. All hooks are active. The runtime
  unpacks and loads packed mods. Save and close work normally.
- Through the Union Starter, on an installation with many other Starter mods and 20 other script plugins: the package installs. Dialogs and the
  quest from the archive in the package work.
- A saved game without the mod: a game saved with `perk_lifesteal` loads without the mod and without a crash, because the mod removes its
  conditions before the save (see [Known risks](#known-risks)).

**Installations:** The game must have the Community Patch 1.75 and the Parallel Universe Patch. [Compatibility](../compatibility.md) tells which
installations I checked.

## Does not work

I measured these items in the game or examined them in the game code:

- **Draw rectangles with only one color:** The rectangle functions of the game draw nothing. Only text and images work.
- **Learn skills that the mod clones late:** `LearnSkill` learns a clone only if the mod clones it in `OnQuestsLoaded`. The mod must make the
  effect of a custom skill itself.
- **Set references with `SetProperty`:** it gives `true`, but has no effect. Use `SetTemplateRef` and `SetEntityRef` for this.
- **A hit hook before the damage calculation:** The game DLLs do not give an entry point for this. You can change the damage through the game
  script `OnHit`, `Override_MonsterDamage` and `Override_Damage` ([Hooks](hooks.md#damage-without-a-hit-hook)).
- **Game scripts that the game calls directly in its internal code** (`AssessHit`, `IsDeadlyDamage`, `CanBurn`): you cannot intercept them by
  name.
- **File patches for players:** The runtime starts as a script DLL only after the game has read its file list. Thus mod packages give changed game
  files as an archive. File patches work only with the loader.
- **Write quest files.**
- **The description of a quest in the quest log:** The log does not show the text of `SetQuestDescription`. From v0.47.1, entries without a
  dialog are possible with `AddQuestLogEntry` ([Quests](quests.md#entries-without-a-dialog-from-v0471)).
- **Text for ambient talk:** `GetSpeechText` finds no text for `SVM_..._Smalltalk_NN`.
- **Attribute `"XP"`** reads 0. The experience is in `GetProperty(GetHero(), "PlayerMemory", "XP")`. The game itself gives the quest XP.
- **Attributes of NPCs:** `GetAttribute` and `SetAttribute` have an effect only on the hero. NPCs give `-1`. To read the values, use game
  scripts, for example `CallScript("GetHitPointsMax", npc, Entity())` (also `GetHitPoints`, `GetStaminaPoints`, `GetManaPoints`, `GetStrength`).
  To change the current hit points, use `CallScript("AddHitPoints", npc, Entity(), amount)` or `HurtHitPoints`. It was not possible to change
  the maximum values and the strength of NPCs in this way. For NPCs out of processing range, `GetProperty(npc, "DamageReceiver", "HitPoints")`
  gives 1.

## Not available

I did not make these items or did not examine them yet. Thus they are not shown to be impossible:

- Animations: play them, intercept them, add your own.
- Change the world (put objects), custom models.
- Custom user interface controls with input, a settings menu in the game (settings go through the `mod.ini`).
- File patches on compressed archive entries (`Strings.pak`) and on binary files. As the base, file patches always read the version from the
  `.pak`, also if a mod archive replaces the same file.
- The meaning of all values of `GetActivePage()`.

## Experimental

These functions run, but I did not fully check their effect. Their names and their behavior can still change:

- `HurtHitPoints`: decreases the hit points of NPCs, but does not kill.
- `SetProperty`: writes texts, numbers, Boolean values and enums. It depends on the property if the game uses the value.
- `Override_CombatMove`: hits are visibly faster. For `Recover`, the weapon can come in place of the fighter.

## Not tested

- **GOG versions, CD versions and DVD versions.** Without DXVK, only on one installation. G3ASM does not support installations without the
  Community Patch 1.75 or without the Parallel Universe Patch.
- **Together with other script plugins** that set their own hooks (for example CombatFix). `Script_MonsterDamage.dll` and
  `Script_LearningThresholdTweaks.dll` change the same points as the modkit. If `Script_MonsterDamage.dll` is installed, `Override_MonsterDamage`
  stays off. If `Script_LearningThresholdTweaks.dll` is installed, `Override_LearnCost` stays off.
- **Never occurred in the game:** AI routines (`AI_ROUTINE`), the divisor 2 for `Override_MonsterDamage` (summoned attackers), and the doubled
  learn costs with "Alternative Balance".
- **Other conditions of custom infos:** reputation, party and others (`ListInfoProperties` shows them). `CondWearsItem` had no effect with the
  condition type `GENERAL`.
- **Dependencies in the `mod.ini`** (`version`, `api`, `requires`): tested only without the game.
- **File patches on `.info` files:** The game loads dialogs from the compiled `Infos.cpt`. It is not known if patches have an effect there.
- **`owner` in `OnDamage`**, when more than one fighter fights against one victim, is an approximation. It is the current opponent of the victim,
  and not always the fighter that hit.
- **Two mods with different runtime versions** at the same time on the computer of a player.
- **A saved game with `perk_lifesteal` on an installation with many other mods** crashed when the player opened the character sheet. This also
  occurred without the mod. The cause is not known. On a clean installation, this does not occur.

## Known risks

- **Permanent conditions in the saved game.** The data that a mod writes into the game (items from cloned templates, skills, clock factor) is in
  the saved game. If the mod is missing at a load, these entries point to nothing. A saved game with the skill of `perk_lifesteal` **crashed
  without the mod when the player opened the character sheet**. Thus mods remove these conditions in `OnBeforeSave` and restore them in
  `OnAfterSave` ([Save and load](saving.md)). With this pattern, the saved game also loads without the mod.
- **Fixed points in the game code.** Three functions do not connect to named functions of the game, but to fixed points: the line "Lebensraub" in
  the item description, `Override_LearnCost` and `Override_MonsterDamage`. If the game version is different at these points, the modkit turns off
  only this function and reports it in the log. All other functions continue to run.
- **Union Starter:** Start the game only after the Starter reported that the installation is complete. While the Starter installs archives again,
  `Data\` is not complete.

## Comparison with the modkit of the Gothic 1 Remake

In the Gothic 1 Remake, the game logic itself is written in AngelScript, and the modkit includes the game scripts. A mod can derive from each
script class or change it with a patch. In Gothic 3, the logic is in native DLLs. A mod can connect to the game only at the points that the modkit
makes available.

| Gothic 1 Remake | G3ASM |
|---|---|
| Scripts in AngelScript | Yes, AngelScript 2.38 |
| Mod packages with a sequence | Yes, `mods\<name>\main.as`, `mod.ini` and `load_order.txt` |
| Dependencies with versions | Yes, `version`, `api` and `requires` in the `mod.ini`. No entry for incompatibilities |
| Settings for each mod | Yes, default values in the `mod.ini`, user values in `g3asm\settings\<mod>.ini`. No settings menu in the game |
| Interfaces between mods | Partially: function calls with text (`CallMod`) and messages (`SendModMessage`). No shared types |
| Script hooking (inheritance, `Super::`) | Partially: `Override_...` with `Super()` at selected game functions, and events |
| Script patches (unified diff on game scripts) | Yes, on the text files of the game ([File patches](patches.md)) |
| Conversations | Yes, as infos with script lines and `OnInfoFinished` |
| Quests | Yes, through the quest system of the game |
| Custom content through the Unreal Editor | No. `PackMod` packs complete files into archives. The tools to make these files come from other sources |
