# Save and load

[Deutsch](../../modkit/saving.md) | **English**

The game does not know the quests, dialogs and texts that a mod makes. To keep them through a save and a load, you must know how the game loads:

1. The game makes all quests again from its files.
2. The game makes all dialogs (infos) again.
3. Then the game reads the changes from the saved game: the status of the quests, the quest log, and the dialog options that the player
   already selected.

If you make your quests and infos **after step 1 and step 2, but before step 3**, they are part of the game. The game then restores their
progress itself. There are two events for this purpose.

## Load events

| Function | When |
|---|---|
| `void OnQuestsLoaded()` | After step 1, when the game starts and at each load. Make your quests here |
| `void OnInfosLoaded()` | After step 2. Make your infos here |

```angelscript
const bCString QUEST = "MyMod_WolfSkin";

void OnQuestsLoaded() {
    if (GetQuestCount() < 0) return;               // main menu: no quests yet
    CreateQuest(QUEST);
    SetQuestTopic(QUEST, "Wolf skin for Jens");
    SetQuestDescription(QUEST, "Jens needs a wolf skin.");
    SetQuestXP(QUEST, 100);
}

void OnInfosLoaded() {
    // make your own infos here, see Dialogs
}
```

**Result:** After a load, the quest has its status again. The quest log is complete. Dialog options that the player selected stay hidden.

**Caution:**
- Quests and infos that you make in `main()` or in `OnFrame` lose their progress when the game loads.
- In the main menu, `OnQuestsLoaded` runs 1 time without quests (`GetQuestCount()` gives -1). Do not make anything at that time. The correct
  setup comes when the player loads a saved game.

[First mod step by step](erste-mod.md) contains a complete mod that uses this pattern.

## Save events

| Function | When |
|---|---|
| `void OnBeforeSave(const bCString &in name)` (from version 31) | Immediately before the game writes a saved game |
| `void OnAfterSave(const bCString &in name, int ok)` (from version 31) | Immediately after it. `ok` 1 = success, 0 = failure |

Each event runs exactly 1 time for each save. This is also true for a quick save.

Purpose: The saved game contains the permanent changes of a mod, for example an item from a cloned template or a custom skill. If a player loads
this saved game without the mod, the entry points to nothing, and the game can crash. Thus the mod removes these changes before the save and
makes them again after the save:

```angelscript
void OnBeforeSave(const bCString &in name) { /* turn off the skill, remove the stack */ }
void OnAfterSave(const bCString &in name, int ok) { /* make the stack again, turn on the skill */ }
```

**Result:** The saved game contains nothing from the mod. It also loads without the mod. For a complete example, see
`examples/mods/perk_lifesteal`.

**Caution:**
- After a **load**, the removed items are also missing. Make them again at each load too (for example in `OnWorldActivate`).
- The game waits during the two events. Do not do long tasks in them.
- Do not keep `Entity` handles across a save. Get them again (`GetHero()`).

## Your own switches in the saved game

`SetGameEvent(name)`, `ClearGameEvent(name)` and `IsGameEventSet(name)` keep yes-or-no values in the memory of the hero. The game writes them
into each saved game. They apply to that saved game, also if the mod was off for some time.

Use them for all data that the mod must know after a load and that the game does not save itself, for example "reward already received". Give
the names a prefix (`MyMod_...`), so that they do not collide with switches of the game.

## Emergency recovery

If the player added the mod after a saved game, this saved game does not know the quest and the infos of the mod. Then use this fallback:

1. Also keep the progress in switches (`SetGameEvent("MyMod_Started")` and so on).
2. In `OnFrame`, approximately 1 time each second, do a check of `QuestExists` and `InfoExists`. If an item is missing, make it again.
3. Restore the progress from the switches.

**Caution:**
- Set the status with `SetQuestStatus(quest, QUEST_RUNNING)`. Do **not** use `RunQuest`. If you use it, the game shows "Quest angenommen"
  (quest accepted) again.
- For complete quests, set the XP and the reward to 0 first. If you do not, the game gives them again when you set the status.
- You cannot restore the quest log in this way.

`examples/mods/jens_dialog/main.as` does the two: load events in the normal case, and switches as the fallback.
