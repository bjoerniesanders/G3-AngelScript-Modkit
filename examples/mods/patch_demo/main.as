// Patch-Demo: ScriptPatches\abbas_xp.patch aendert die Quest-Datei "Abbas_Artefakt" des Spiels (ExperiencePoints 300 -> 600), bevor das Spiel sie liest.
// Dieser Mod prueft nach dem Laden, ob die Aenderung angekommen ist. Abschalten: in g3asm\\mods\\load_order.txt "-patch_demo" eintragen.
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
