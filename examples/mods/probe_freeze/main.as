// Mess-Mod fuer die Freeze-Liste (ab API 46). Alles nur Protokoll ([probe_freeze] ...).
//   automatisch nach dem Laden: Quest-Datei lesen mit verschiedenen Pfaden (LoadQuestFile)
//   Strg + Umschalt + Ziffernblock *  anvisiertes Wesen toeten (Entity::Kill, Taeter = Held) - vorher ein unwichtiges Tier anvisieren!
//   Strg + Umschalt + Ziffernblock -  Haendlerware des anvisierten NPCs neu erzeugen (GenerateTradeGoods), Stapel und Gold vorher/nachher
//   Strg + Umschalt + Ziffernblock +  naechster Schritt: Fadenkreuz aus, Fadenkreuz an, Uhr 22:00, Uhr 10:00 (dann von vorn)
//   Verwandlung (Rolle benutzen): OnTransformStart/OnTransformEnd werden gemeldet
// Mit Strg + Umschalt reagieren die anderen Beispiel-Mods auf diese Tasten nicht (demo_spawn, demo_lifesteal, probe_reference pruefen das).
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
    // Lesen: so wie das Spiel (virtueller Pfad Data/Quests/<Welt>/<Datei>, auch aus den Archiven)
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
