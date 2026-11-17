// PackMod demo: checks if the engine reads a mod archive that g3asm_packmod wrote (Quests.mod, Quests.m00, ...), and if it overwrites a file of the game.
// The archive contains the quest file "Agenak_ArenaDefeatDimitar" with ExperiencePoints=4242 (original: 300). To make it: make the folder G3_World_01\Agenak_ArenaDefeatDimitar_quest_G3_World_01.quest
// and run  g3asm_packmod install <folder> <game folder> Quests  (result: Data\Quests.mod or the next free generation).
// The quest is intentionally one that no saved game contains: saved quest run-time data could hide the value of the file.
// To turn off: write "-demo_packmod" in g3asm\mods\load_order.txt (delete the archive itself from Data\ to undo the test).
const bCString TEST_QUEST = "Agenak_ArenaDefeatDimitar";
bool geprueft = false;
float zeit = 0;
void OnFrame(float dt) {
    zeit += dt;
    if (geprueft || zeit < 3.0f) return;
    if (!QuestExists(TEST_QUEST)) return;
    geprueft = true;
    int xp = GetQuestXP(TEST_QUEST);
    print("PackMod: " + TEST_QUEST + " Quest-XP " + xp + (xp == 4242 ? " (Mod-Archiv ueberschreibt das Original)" : xp == 300 ? " (unveraendertes Original)" : ""));
}
