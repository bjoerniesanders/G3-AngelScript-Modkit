# First mod step by step

[Deutsch](../../modkit/erste-mod.md) | **English**

In this guide, you make a small, complete mod. Jens in Kap Dun gets a new dialog option and gives you a task: bring a wolf skin. The quest shows
in the quest log. When you deliver the skin, you get a reward. Each chapter continues from the previous chapter. At the end, you find the full
mod in one part.

## 1. Before you start (read this)

1. Install the modkit (see [Mod folder and installation](mod-package.md#installation)). The game must have the Community Patch 1.75 and the
   Parallel Universe Patch.
2. **Work on a copy of the game folder.** Do not work in your main installation.
3. **Make a backup of your saved games** (`Documents\gothic3`). All copies of the game use the same folder. Save tests only into new, free slots.
4. Keep the log open: `<game folder>\g3asm.log`. It shows your output and each error with the line and the column. An editor that loads changes
   automatically is useful here.
5. If you turned on the example mods `jens_quest` and `jens_dialog`, turn them off. They also use Jens.
6. **Save your `.as` files as ANSI (Windows-1252)** if you use umlauts or other special characters in texts. The game reads texts in this
   encoding. If you save the file as UTF-8, characters such as "ü" and "ß" show as incorrect characters. This guide uses only English texts, so
   this is not a problem here.

## 2. Make the mod

1. Make the folder `<game folder>\g3asm\mods\wolfskin\`. The name of the folder is the name of your mod.
2. Make the file `main.as` in this folder:

   ```angelscript
   void main() {
       print("Wolf skin mod loaded");
   }
   ```

3. Start the game.

**Result:** The log shows `[wolfskin] Wolf skin mod loaded`.

`main()` runs 1 time at the start. Change the text and save the file while the game runs. After approximately 1 second, the log shows the new
line. This is **hot reload**. From now on, you can try each change in this way. You do not have to start the game again.

**Caution:** If your script has an error, the mod does not load. The log then shows `Skript enthaelt Fehler` (script contains errors). The lines
before it give the line, the column and the reason.

## 3. React to the game

Usually, a mod does nothing by itself. It reacts to **events**. Events are functions with fixed names. The modkit calls them when something
occurs in the game. Add this:

```angelscript
void OnNPCKilled(Entity victim) {
    print("killed: " + victim.GetName());
}
```

**Result:** When you kill an opponent, the log shows its name, for example `[wolfskin] killed: Wolf`.

In this way, you find the names of things in the game. You need these names soon. [Events](events.md) gives all events.

**Caution:** The name and the parameters must be exactly correct. `void OnNPCKilled(Entity victim)` works. The modkit never calls
`void OnNpcKilled(Entity victim)`, and you get no error message.

## 4. Make a quest

The game makes its quests again at each game start and at each load. Your quest must be part of this, so that its progress goes into the saved
game. Thus you make it at exactly this time, in the event `OnQuestsLoaded`.

1. At the top of the file, write the names that the mod uses:

   ```angelscript
   const bCString NPC   = "Jens";
   const bCString QUEST = "WolfSkin_Quest";
   const bCString SKIN  = "It_Wolf_Fur";
   ```

2. Add this:

   ```angelscript
   void OnQuestsLoaded() {
       if (GetQuestCount() < 0) return;               // main menu: no quests yet
       CreateQuest(QUEST);
       SetQuestTopic(QUEST, "A skin for Jens");        // title in the quest log
       SetQuestDescription(QUEST, "Jens needs a wolf skin.");
       SetQuestXP(QUEST, 100);                         // experience at success
   }
   ```

3. Save, and load a saved game.

**Result:** The quest exists now, but it is not started yet. It shows in the quest log only when it runs (chapter 5).

**Caution:** Never make quests in `OnFrame` or in `main`. If you do, they are not part of the saved game, and the progress is lost after a load.
[Save and load](saving.md) gives the background.

## 5. A dialog with Jens

In Gothic 3, dialogs are **infos**. An info belongs to an NPC and shows as an option in the dialog menu. It consists of lines that the speakers
say one after the other. As with quests, the game makes infos again when it loads. Thus you make your infos in `OnInfosLoaded`.

1. Add this to the names at the top:

   ```angelscript
   const bCString INFO_TASK = "WolfSkin_Task";
   ```

2. Add this:

   ```angelscript
   void OnInfosLoaded() {
       if (InfoExists(INFO_TASK)) return;
       CreateInfo(INFO_TASK, NPC);
       SetInfoQuest(INFO_TASK, QUEST);                   // the lines go into the quest log of this quest
       AddInfoLine(INFO_TASK, "Say", "player", "", "", "", "Do you have work for me?");
       AddInfoLine(INFO_TASK, "Say", "npc", "", "", "", "Bring me a wolf skin, and I will pay you.");
       RebuildInfos();
   }

   void OnInfoFinished(const bCString &in info, const bCString &in owner, const bCString &in quest) {
       if (info == INFO_TASK) RunQuest(QUEST);
   }
   ```

3. Load a saved game and speak to Jens.

**Result:** Jens has the option "Do you have work for me?". After the dialog, the game shows "Quest angenommen" (quest accepted). The quest shows
in the quest log with the two lines.

- The first line of the player (`"player"`) is the text of the option in the menu.
- `OnInfoFinished` comes when the speakers said all lines of a dialog. Put here all actions that must occur after the dialog.
- After the player selected the option 1 time, the game hides it.

**Caution:** The `InfoExists` check at the start is important. Without the check, each new setup adds the same lines again, and Jens says
everything two times. Also, do not forget `RebuildInfos()`. Without it, the option does not show.

## 6. Deliver the skin

A second info shows only while the quest runs. If the player has the skin, the player delivers it and gets the reward.

1. Add this to the names at the top:

   ```angelscript
   const bCString INFO_DELIVER = "WolfSkin_Deliver";
   ```

2. Add this to `OnInfosLoaded`, before `RebuildInfos();`:

   ```angelscript
       CreateInfo(INFO_DELIVER, NPC);
       SetInfoQuest(INFO_DELIVER, QUEST);
       SetInfoCondition(INFO_DELIVER, INFO_COND_RUNNING);  // only while the quest runs
       AddInfoLine(INFO_DELIVER, "Say", "player", "", "", "", "I have the wolf skin.");
       AddInfoLine(INFO_DELIVER, "Say", "npc", "", "", "", "Good work. Here, take these arrows.");
   ```

3. Add this to `OnInfoFinished`:

   ```angelscript
       if (info == INFO_DELIVER) {
           Entity hero = GetHero();
           if (hero.RemoveItems(SKIN, 1) < 1) {
               PrintScreen("You do not have a wolf skin.");
               return;
           }
           SucceedQuest(QUEST);          // the game gives the quest XP
           hero.AddItems("Arrow", 15);   // your own reward
       }
   ```

**Result:** With a wolf skin in the inventory, select "I have the wolf skin.". The skin goes away, the quest has success, and you get experience
and 15 arrows.

**Caution:** Here the script does the check for the skin, not the game. You cannot set conditions such as "has an item" for your own infos. Thus
the option also shows without a skin. To hide it in this case, use [`Override_InfoAvailable`](hooks.md).

## 7. If Jens dies

```angelscript
void OnNPCKilled(Entity victim) {
    if (victim.GetName() == NPC && GetQuestStatus(QUEST) == QUEST_RUNNING)
        FailQuest(QUEST);
}
```

This function replaces the function from chapter 3. Each event function can exist only 1 time.

**Result:** If Jens dies while the quest runs, the quest fails.

## 8. Try save and load

1. Accept the task, save into a free slot, and close the game.
2. Start the game again and load the saved game.

**Result:** The quest continues. The quest log is complete. Jens offers only the delivery. The game restores this itself, because the quest and
the infos come from the load events.

For data that the game does not save itself, for example a number in your script, use switches in the saved game (`SetGameEvent`). See
[Save and load](saving.md#your-own-switches-in-the-saved-game).

## 9. Version and sharing

1. Make a `mod.ini` next to `main.as`:

   ```ini
   [mod]
   version=1.0
   api=46
   title=A skin for Jens
   ```

   With `api=46`, a modkit that is too old does not load the mod and tells this in the log. It does not stop with compile errors.

2. Make the package for players (in the folder of the modkit):

   ```
   bin\g3asm\g3asm_packmod.exe release <game folder>\g3asm\mods\wolfskin <target folder>
   ```

**Result:** The target folder contains the folder `wolfskin` in the Union Starter layout. Players put it into `<game folder>\MDS\Mods\` and
install it in the Starter. They do not need the modkit. For more data, see [Share mods](mod-package.md#share-mods).

## The full mod

```angelscript
const bCString NPC          = "Jens";
const bCString QUEST        = "WolfSkin_Quest";
const bCString SKIN         = "It_Wolf_Fur";
const bCString INFO_TASK    = "WolfSkin_Task";
const bCString INFO_DELIVER = "WolfSkin_Deliver";

void main() {
    print("Wolf skin mod loaded");
}

void OnQuestsLoaded() {
    if (GetQuestCount() < 0) return;               // main menu: no quests yet
    CreateQuest(QUEST);
    SetQuestTopic(QUEST, "A skin for Jens");
    SetQuestDescription(QUEST, "Jens needs a wolf skin.");
    SetQuestXP(QUEST, 100);
}

void OnInfosLoaded() {
    if (InfoExists(INFO_TASK)) return;
    CreateInfo(INFO_TASK, NPC);
    SetInfoQuest(INFO_TASK, QUEST);
    AddInfoLine(INFO_TASK, "Say", "player", "", "", "", "Do you have work for me?");
    AddInfoLine(INFO_TASK, "Say", "npc", "", "", "", "Bring me a wolf skin, and I will pay you.");

    CreateInfo(INFO_DELIVER, NPC);
    SetInfoQuest(INFO_DELIVER, QUEST);
    SetInfoCondition(INFO_DELIVER, INFO_COND_RUNNING);
    AddInfoLine(INFO_DELIVER, "Say", "player", "", "", "", "I have the wolf skin.");
    AddInfoLine(INFO_DELIVER, "Say", "npc", "", "", "", "Good work. Here, take these arrows.");
    RebuildInfos();
}

void OnInfoFinished(const bCString &in info, const bCString &in owner, const bCString &in quest) {
    if (info == INFO_TASK) RunQuest(QUEST);
    if (info == INFO_DELIVER) {
        Entity hero = GetHero();
        if (hero.RemoveItems(SKIN, 1) < 1) {
            PrintScreen("You do not have a wolf skin.");
            return;
        }
        SucceedQuest(QUEST);
        hero.AddItems("Arrow", 15);
    }
}

void OnNPCKilled(Entity victim) {
    if (victim.GetName() == NPC && GetQuestStatus(QUEST) == QUEST_RUNNING)
        FailQuest(QUEST);
}
```

## What to read next

- [Examples](examples.md): complete mods to learn from. One of them is `jens_dialog`, the full version of this mod (with a quest from a mod
  archive and a fallback for old saved games).
- [Hooks](hooks.md): change game functions, for example damage, or if a dialog option shows.
- [Scripting guide](scripting-guide.md): special points of the language, and troubleshooting.
- [API reference](api-reference.md): all functions for reference.
