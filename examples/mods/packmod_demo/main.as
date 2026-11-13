// PackMod-Demo: prueft, ob die Engine ein von g3asm_packmod geschriebenes Mod-Archiv (Quests.mod, Quests.m00, ...) liest und ob es eine Datei des Spiels ueberschreibt.
// Das Archiv enthaelt die Quest-Datei "Agenak_ArenaDefeatDimitar" mit ExperiencePoints=4242 (Original: 300). Erzeugen: Ordner G3_World_01\Agenak_ArenaDefeatDimitar_quest_G3_World_01.quest
// anlegen und  g3asm_packmod install <Ordner> <Spielordner> Quests  (ergibt Data\Quests.mod bzw. die naechste freie Generation).
// Die Quest ist absichtlich eine, die in keinem Spielstand vorkommt: gespeicherte Quest-Laufzeitdaten koennten sonst den Wert der Datei ueberdecken.
// Abschalten: in g3asm\mods\load_order.txt "-packmod_demo" eintragen (das Archiv selbst aus Data\ loeschen, um den Test rueckgaengig zu machen).
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
