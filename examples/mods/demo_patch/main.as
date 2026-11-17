// Patch demo: ScriptPatches\abbas_xp.patch changes the quest file "Abbas_Artefakt" of the game (ExperiencePoints 300 -> 600) before the game reads it.
// After a load, this mod checks if the change has an effect. To turn off: write "-demo_patch" in g3asm\mods\load_order.txt.
bool geprueft = false;
float zeit = 0;
void OnFrame(float dt) {
    zeit += dt;
    if (geprueft || zeit < 3.0f) return;
    if (!QuestExists("Abbas_Artefakt")) return;
    geprueft = true;
    int xp = GetQuestXP("Abbas_Artefakt");
    print("Abbas_Artefakt: Quest-XP " + xp + (xp == 600 ? " (Datei-Patch wirkt)" : xp == 300 ? " (unveraendertes Original)" : ""));
}
