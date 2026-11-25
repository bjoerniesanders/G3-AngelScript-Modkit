# Dialogs (infos)

[Deutsch](../../modkit/dialogs.md) | **English**

Dialogs in Gothic 3 consist of **infos**. An info is an option in the dialog menu of an NPC, together with the lines that the speakers say after
it. The modkit can read infos and make new infos. [First mod](erste-mod.md#5-a-dialog-with-jens) gives step-by-step instructions.
The game has 4593 infos. Each info belongs to an NPC and runs as a small script of command lines.

## The parts of an info

| Part | Meaning |
|---|---|
| Name | A unique identifier (`SKALVERAM31`; for your own infos, for example `G3ASM_Jens_Job`) |
| Owner | The NPC that shows the option (`Jens`) |
| Parent | The parent info. It makes the info a subsequent question in the dialog tree |
| Quest | Connects the info to a quest. The dialog lines of the info go automatically into the quest log of this quest |
| Condition type | When the option shows (see below) |
| Type | `INFO_TYPE_INFO` (3, normal option), `INFO_TYPE_NEWS` (2), `INFO_TYPE_IMPORTANT` (1), `INFO_TYPE_COMMENT` (5), ... |
| Script lines | Commands that run when the player selects the option |

Condition types (`INFO_COND_*`):

| Type | Value | When the option shows |
|---|---|---|
| `GENERAL` | 3 | Always |
| `OPEN` | 5 | The quest is not started yet |
| `ACTIVATOR` | 6 | The NPC offers the quest |
| `RUNNING` | 7 | While the quest runs |
| `DELIVERY`, `PARTDELIVERY` | 8, 9 | The player delivers items |
| `SUCCESS` | 10 | The quest has success |
| `FAILED` | 12 | The quest failed |

There are also `CRIME`, `FIGHT`, `HELLO`, `OVERTIME` and `DOCANCEL`.

The first `Say` line of the player is the text in the dialog menu.

## Commands in script lines

`AddInfoLine(info, command, entity1, entity2, id1, id2, text)` adds a line. Frequent commands in the game (number of uses):

| Command | Parameters | Effect |
|---|---|---|
| `Say` (12553) | entity1 = `player` or `npc`, id1 = voice mood (for example `SD_Thoughtful`), text | Says a line |
| `SaySVM` (467) | entity1, id1 = standard sentence (`LetMePass`) | Standard sentence of the NPC |
| `Description` (623) | entity1 = `player`, text | Label of an answer (for example `ENDE`) |
| `End` (271) | | Ends the dialog |
| `SetGameEvent`, `ClearGameEvent` (1354, 147) | id1 = event name | Sets or clears a switch in the saved game |
| `RunQuest` (57), `SucceedQuest` (20), `CloseQuest` (76) | id1 = quest name | Controls a quest |
| `AddQuestLog` (2) | id1 = quest, id2 = text ID | Writes an entry into the quest log |
| `Give` (584) | entity1 = `npc`, entity2 = `player`, id1 = item, id2 = quantity | The NPC gives an item |
| `Erase` (4) | entity1 = `player`, id1 = item, id2 = quantity | Removes an item |
| `GiveXP` (346) | id1 = quantity | Experience points |
| `SetTradeEnabled`, `SetTeachEnabled` (139, 143) | entity1 = `npc`, id1 = `true` or `false` | Turns the trade or the teacher on or off |
| `SetRoutine` (247) | entity1 = NPC, id1 = daily routine (`Work`, `Free`) | Changes the daily routine |
| `Attack` (83) | entity1 = `npc`, entity2 = `player` | The NPC attacks |
| `InfoWait` (51) | id1 = milliseconds | Pause |
| `ShowGameMessage` (2) | id2 = type, text | Message on the screen |

`DescribeCommands()` lists all 60 commands of the game, each with an example.

## Functions

| Function | Description |
|---|---|
| `int GetInfoCount()`, `GetInfoName(i)`, `GetInfoOwner(i)` | Lists the infos |
| `bCString DescribeInfo(name)` | All data of an info as text: owner, quest, condition, all lines |
| `bCString DescribeCommands()` | The command vocabulary of all infos |
| `bool InfoExists(name)` | Does the info exist? |
| `bool CreateInfo(name, owner)` | Makes an info (type `INFO`, condition `GENERAL`). See below |
| `bool SetInfoQuest(name, quest)`, `SetInfoParent(name, parent)`, `SetInfoType(name, type)`, `SetInfoCondition(name, condType)` | Set properties |
| `bool AddInfoLine(info, command, entity1, entity2, id1, id2, text)` | Adds a script line. If `text` is not empty, the modkit registers it as a new text ID |
| `bool RebuildInfos()` | Makes the owner list of the game again. **Call it after you make infos.** If you do not, the options do not show |
| `bool RemoveInfo(name)` | Removes an info |
| `bool SetInfoGiven(name, bool)`, `bool IsInfoGiven(name)` | "The info was already given". This hides the option |
| `bCString GetInfoProperty(name, prop)`, `bool SetInfoProperty(name, prop, value)` (from v0.47.2) | Reads or writes a property of the info as text, for example its conditions (see below) |
| `bCString ListInfoProperties(name)` (from v0.47.2) | All properties of the info as `"Name=Value;..."` |

**`CreateInfo`, read this carefully:**
- If the info already exists, `CreateInfo` changes nothing and gives `true`. A subsequent `AddInfoLine` then adds the lines a second time, and
  the NPC says everything two times. Thus, do a check with `InfoExists` first. This is very important if you make infos in `OnInfosLoaded` and
  then again later.
- `false` means: the game is not ready yet, or the game still uses the name internally and would rename the new info. The function then makes
  nothing and reports this 1 time in the log (`CreateInfo('...'): Name im Info-Manager noch belegt`, name still in use in the info manager).
- After you make infos, call `RebuildInfos()`. If you do not, the options do not show.

Event: `void OnInfoFinished(const bCString &in info, const bCString &in owner, const bCString &in quest)` comes when the speakers have said all
lines of a dialog. Use it for results that the game cannot do itself: a check of the inventory, the end of a quest, rewards.

## Conditions: items and switches (from v0.47.2)

An option can depend on items in the inventory and on switches in the saved game. The game itself does a check of the conditions before it
shows the option. You do not need `Override_InfoAvailable` for this.

```angelscript
SetInfoCondition(INFO_DELIVERY, INFO_COND_RUNNING);                       // while the quest runs
SetInfoProperty(INFO_DELIVERY, "ConditionItemContainer", "PC_Hero");      // whose inventory counts
SetInfoProperty(INFO_DELIVERY, "CondItems", "[It_Wolf_Fur]");              // which item
SetInfoProperty(INFO_DELIVERY, "CondItemAmounts", "[2]");                  // how many
SetInfoProperty(INFO_DELIVERY, "ConditionPlayerKnows", "[MyMod_MetJens]");  // switch from SetGameEvent
RebuildInfos();
```

Result: the option shows only when the hero has two wolf skins **and** the switch `MyMod_MetJens` is set.

| Property | Value | Effect |
|---|---|---|
| `ConditionItemContainer` | `PC_Hero` (or the name of an NPC) | The inventory in which the game looks for the items |
| `CondItems` | `[It_Wolf_Fur]` | The necessary item |
| `CondItemAmounts` | `[2]` | The necessary quantity. Without an entry, one item is sufficient |
| `ConditionPlayerKnows` | `[MyMod_Switch]` | A switch that the mod sets with `SetGameEvent`. The game uses the same list for its own switches (`[Chris_DeerDead]`) |

- Lists are in square brackets. The value of `CondItemAmounts` that you read is empty or `[]`, also if the quantity has an effect.
- Set the conditions when you make the info, thus in `OnInfosLoaded`, and then call `RebuildInfos()`. As the info itself, they do not come back
  from the saved game.
- The info also has other conditions (`ListInfoProperties` shows them): `ConditionPlayerKnowsNot`, `CondWearsItem`, `CondHasSkill`,
  `CondReputGroup`, `CondPlayerPartyMember` and others. I did not measure their effect.
- Example: in `jens_dialog`, "Ich habe das Wolfsfell." shows only when the hero has a wolf skin.

## Example: a quest through a dialog

```angelscript
const bCString NPC = "Jens", QUEST = "MyMod_WolfSkin", INFO_JOB = "MyMod_Job";

bool Make() {
    if (!CreateInfo(INFO_JOB, NPC)) return false;
    SetInfoQuest(INFO_JOB, QUEST);                         // the lines go into the quest log
    SetInfoCondition(INFO_JOB, INFO_COND_GENERAL);
    AddInfoLine(INFO_JOB, "Say", "player", "", "", "", "Do you have work for me?");
    AddInfoLine(INFO_JOB, "Say", "npc", "", "", "", "Yes, I need a wolf skin.");
    RebuildInfos();
    return true;
}

void OnInfosLoaded() { Make(); }                           // see "Save and load"

void OnInfoFinished(const bCString &in info, const bCString &in owner, const bCString &in quest) {
    if (info == INFO_JOB) RunQuest(QUEST);
}
```

The complete mod is in `examples/mods/jens_dialog/main.as`.

## Your own texts

Texts in `AddInfoLine` and in quests are text IDs of the string table of the game. The modkit intercepts the text search. It registers a text that
you give under a new ID (`G3ASM_INFO_<Info>_<n>`, `G3ASM_QTOPIC_<Quest>`). The text then shows at all places where the game resolves this ID.
Umlauts work if you save the `.as` file as ANSI (Windows-1252) ([Scripting guide](scripting-guide.md#strings-bcstring)).
