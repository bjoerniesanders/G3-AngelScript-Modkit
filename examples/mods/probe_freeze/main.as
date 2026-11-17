// Measurement mod for the freeze list (from API 46). Everything only in the log ([probe_freeze] ...).
//   automatically after a load: read a quest file with different paths (LoadQuestFile)
//   Ctrl + Shift + numeric keypad *  kill the focused creature (Entity::Kill, killer = hero) - focus an unimportant animal first!
//   Ctrl + Shift + numeric keypad -  make the trader goods of the focused NPC again (GenerateTradeGoods), stacks and gold before and after
//   Ctrl + Shift + numeric keypad +  next step: crosshair off, crosshair on, clock 22:00, clock 10:00 (then from the start)
//   transformation (use a scroll): the mod reports OnTransformStart and OnTransformEnd
// With Ctrl + Shift, the other example mods do not react to these keys (demo_spawn, demo_lifesteal, probe_reference check this).
const int VK_UMSCHALT = 0x10, VK_STRG = 0x11;
int schritt = 0;
bool questGeprueft = false;
float warte = 0;

void QuestDateien() {
    if (GetQuestCount() <= 0) return;
    questGeprueft = true;
    const bCString q = GetQuestNameAt(0);
    const bCString pfad = GetQuestFilePath(q);
    print("Quest '" + q + "': GetQuestDir='" + GetQuestDir() + "' GetQuestFilePath='" + pfad + "'");
    // read: as the game does (virtual path Data/Quests/<world>/<file>, also from the archives)
    print("LoadQuestFile(GetQuestFilePath) = " + LoadQuestFile(q, pfad));
    print("LoadQuestFile(Data/Quests/G3_World_01/" + q + "_quest_G3_World_01.quest) = " + LoadQuestFile(q, "Data/Quests/G3_World_01/" + q + "_quest_G3_World_01.quest"));
}

void OnFrame(float dt) {
    if (questGeprueft || !GetHero().IsValid()) return;
    warte += dt;
    if (warte > 5) QuestDateien();
}

void OnKey(int vk, bool down) {
    if (!down || !IsKeyDown(VK_STRG) || !IsKeyDown(VK_UMSCHALT)) return;
    Entity held = GetHero();
    Entity ziel = GetFocusedEntity();
    if (vk == 0x6A) {
        if (!ziel.IsValid()) { print("Kill: nichts anvisiert"); return; }
        print("Kill(" + ziel.GetName() + ", Held) = " + ziel.Kill(held));
    } else if (vk == 0x6D) {
        if (!ziel.IsValid()) { print("Haendlerware: nichts anvisiert"); return; }
        const int s0 = ziel.GetInventoryStackCount(), g0 = ziel.GetItemCount("It_Gold");
        const bool ok = ziel.GenerateTradeGoods();
        print("GenerateTradeGoods(" + ziel.GetName() + ") = " + ok + ": Stapel " + s0 + " -> " + ziel.GetInventoryStackCount() + ", Gold " + g0 + " -> " + ziel.GetItemCount("It_Gold"));
    } else if (vk == 0x6B) {
        schritt = schritt % 4 + 1;
        if (schritt == 1) print("Schritt 1: SetCrosshairVisible(false) = " + SetCrosshairVisible(false) + " (ist das Fadenkreuz weg?)");
        else if (schritt == 2) print("Schritt 2: SetCrosshairVisible(true) = " + SetCrosshairVisible(true));
        else {
            const int stunde = schritt == 3 ? 22 : 10;
            const bCString vorher = "" + GetGameDay() + " " + GetGameHour() + ":" + GetGameMinute();
            const bool ok = SetGameTime(-1, stunde, 0);
            print("Schritt " + schritt + ": SetGameTime(-1, " + stunde + ", 0) = " + ok + ", vorher " + vorher + ", nachher " + GetGameDay() + " " + GetGameHour() + ":" + GetGameMinute() + " (wird es dunkel/hell?)");
        }
    }
}

void OnTransformStart(Entity self, Entity target) { print("OnTransformStart: " + self.GetName() + " -> " + target.GetName()); }
void OnTransformEnd(Entity self) { print("OnTransformEnd: " + self.GetName()); }
void OnNPCKilled(Entity victim) { print("OnNPCKilled: " + victim.GetName()); }
