# Quests

[Deutsch](../../modkit/quests.md) | **English**

The game has 571 quests. They are text files in the archive `Data\Quests.pak` (`G3_World_01/<Name>_quest_G3_World_01.quest`). Your mod makes its
own quests when the game loads. The game then uses them as its own quests.

## Status

| Constant | Value | Meaning |
|---|---|---|
| `QUEST_OPEN` | 0 | Made, not started |
| `QUEST_RUNNING` | 1 | Running |
| `QUEST_SUCCESS` | 2 | Completed with success |
| `QUEST_FAILED` | 3 | Failed |
| `QUEST_OBSOLETE` | 4 | Obsolete |
| `QUEST_CANCELLED` | 5 | Cancelled |
| `QUEST_LOST` | 6 | Lost |
| `QUEST_WON` | 7 | Won |

`GetQuestStatus(name)` gives `-1` if the quest does not exist.

## The sequence of your own quest

```angelscript
const bCString QUEST = "MyMod_WolfSkin";

void Begin() {
    CreateQuest(QUEST);                                        // any name, but it must be unique
    SetQuestTopic(QUEST, "Wolf skin for Jens");                // title in the quest log
    SetQuestDescription(QUEST, "Jens needs a wolf skin.");
    RunQuest(QUEST);                                           // status 1
}

void Complete() {
    SetQuestXP(QUEST, 100);                                    // set this before you complete the quest
    SucceedQuest(QUEST);                                       // status 2
}
```

Notes:

- `CreateQuest` causes no problem if the quest already exists. It gives `true`.
- The modkit refuses `SucceedQuest` on a quest that already has success, and `FailQuest` on a quest that is complete. Both give `false`.
- **XP:** The game itself gives the quest XP when the quest is complete, with a message "Quest abgeschlossen" (quest complete). Use `SetQuestXP`
  before `SucceedQuest`. In a test, a value of 100 gave 500 XP. This is probably a factor. A value of 0 gives no XP.
- **Quests in the saved game:** If the mod makes a quest in `OnQuestsLoaded`, the game restores its status and its quest log after a load. I
  checked this in the game. If the mod makes the quest later (for example in `OnFrame`), the progress is lost. See [Save and load](saving.md).

## Texts

The names in the quest log are text IDs. The game finds the text for each ID in its string table. An unknown ID gives an empty text.
The modkit intercepts the functions that find the texts:

- `RegisterText(id, text)` keeps a text for an ID. Your text shows at all places where the game shows this ID.
- `SetQuestTopic(name, text)` and `SetQuestDescription(name, text)` make the IDs `G3ASM_QTOPIC_<Quest>` and `G3ASM_QDESC_<Quest>` for this
  purpose. Then they write the IDs into the quest.

The title shows in the quest log. Umlauts work if you save the `.as` file as ANSI (Windows-1252) ([Scripting guide](scripting-guide.md#strings-bcstring)).

## Quest log and dialogs

Below the title, the quest log of Gothic 3 shows the **dialog lines** that the player heard for this quest: each line with the name of the
speaker in front of it. Thus the log shows only the title of a quest that a script makes at first. Entries come in two ways:

- from your own dialog lines that you connect to the quest (`SetInfoQuest`, see [Dialogs](dialogs.md)),
- from v0.47.1, without a dialog, with `AddQuestLogEntry` (see below).

**Caution:** The quest log does not show the description from `SetQuestDescription`, also not if it changes later. For progress in the log,
use `AddQuestLogEntry` or dialog lines.

### Entries without a dialog (from v0.47.1)

`AddQuestLogEntry(quest, heading, text)` adds an entry to the log of the quest. `heading` shows in front of it, as the name of a speaker. If
`heading` is empty, nothing shows in front of it.

```angelscript
float timer = 0;
void OnFrame(float dt) {
    timer += dt;
    if (timer < 1.0f) return;
    timer = 0;
    // killed, found, bought or received: it is important only that the hero has a skin
    if (GetQuestStatus(QUEST) != QUEST_RUNNING || IsGameEventSet("MyMod_FurLog") || GetHero().GetItemCount("It_Wolf_Fur") < 1) return;
    if (AddQuestLogEntry(QUEST, "", "I have a wolf skin. Now I must only bring it to Jens.") > 0)
        SetGameEvent("MyMod_FurLog");   // only one time
}
```

Result in the quest log: the new line shows below "Wolf skin for Jens". The return value is the number of entries of the quest, `-1` if there
is an error (unknown quest, no game).

- The game saves the entries with the quest. They come back after a save, a load and a restart of the game. The texts are in the entry
  itself. The mod does not have to make them again.
- The quest must still exist after a load. Thus the mod must make it in `OnQuestsLoaded` (see [Save and load](saving.md)). A quest that the mod
  makes only at run time is missing after a load, together with its entries.
- Each call adds a new entry. Use a switch in the saved game (`SetGameEvent`) to remember which entries are already written.
- An entry stays as it was written, also if the mod uses a different text later.
- Umlauts work if you save the `.as` file as ANSI (Windows-1252).
- Example: `jens_dialog` (the hero has a wolf skin).

## All quests of the game

```angelscript
int n = GetQuestCount();                       // for example 571
for (int i = 0; i < n; i++) {
    bCString name = GetQuestNameAt(i);
    print(name + ": " + GetQuestStatus(name));
}
```

Known limit: I did not fully check this list. In a game test, the status query reported no running quests for the names in the list, but one
quest was running. The mod `demo_quest` (Page Up key) writes the details to the log.
