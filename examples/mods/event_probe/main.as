// Mess-Mod fuer die Ereignisse ab API 35: Konsolenbefehle (Override_ConsoleCommand, SetConsoleReply) und Quest-Status (OnQuestStatus).
//   Konsole: Befehl "og3" antwortet mit der API-Version; alle anderen Befehle laufen unveraendert ans Spiel.
//   Strg + Ziffernblock * oeffnet die Konsole (OpenConsole), falls das Spiel keine Taste dafuer hat.
//   Quests:  jeder Statuswechsel steht im Protokoll (0 Open, 1 Running, 2 Success, 3 Failed, 4 Obsolete, 5 Cancelled, 6 Lost, 7 Won).
// Ab API 37: Beruehrungen mit dem Helden (OnTouch, gezaehlt je 10 s), Haendlerware (OnTradeGenerated), Verwandlung (OnTransformStart/End).
//   Umschalt + Ziffernblock / erzeugt die Ware des anvisierten Haendlers neu (GenerateTradeGoods) und meldet die Stapel vorher/nachher.
bool Override_ConsoleCommand(const bCString &in line) {
    if (line.Find("og3", 0) != 0) return Super();
    SetConsoleReply("OG3-Patch, Skript-Schnittstelle " + GetApiVersion());
    print("Konsolenbefehl uebernommen: '" + line + "'");
    return true;
}

void OnQuestStatus(const bCString &in name, int oldStatus, int newStatus) {
    print("Quest '" + name + "': " + oldStatus + " -> " + newStatus);
}

int g_touches = 0, g_heroTouches = 0, g_heroLogged = 0;
float g_touchTime = 0;

void OnTouch(Entity self, Entity other) {
    ++g_touches;
    Entity hero = GetHero();
    if (!(self == hero) && !(other == hero)) return;
    ++g_heroTouches;
    if (g_heroLogged < 20) {
        ++g_heroLogged;
        print("Beruehrung mit dem Helden: '" + self.GetName() + "' von '" + other.GetName() + "'");
    }
}

void OnFrame(float dt) {
    g_touchTime += dt;
    if (g_touchTime < 10) return;
    if (g_touches > 0) print("Beruehrungen in " + int(g_touchTime) + " s: " + g_touches + ", davon mit dem Helden " + g_heroTouches);
    g_touchTime = 0;
    g_touches = g_heroTouches = 0;
}

void OnTradeGenerated(Entity trader, int ok) {
    print("Haendlerware erzeugt fuer '" + trader.GetName() + "' (Ergebnis " + ok + ", Stapel jetzt " + trader.GetInventoryStackCount() + ")");
}

void OnTransformStart(Entity self, Entity target) {
    print("Verwandlung beginnt: '" + self.GetName() + "' -> '" + target.GetName() + "'");
}

void OnTransformEnd(Entity self) {
    print("Verwandlung endet: '" + self.GetName() + "'");
}

void OnKey(int vk, bool down) {
    if (!down) return;
    if (vk == 0x6A && IsKeyDown(0x11)) print("Konsole oeffnen: " + OpenConsole());
    if (vk == 0x6F && IsKeyDown(0x10)) {
        Entity t = GetFocusedEntity();
        if (!t.IsValid()) {
            print("Ware neu: niemand anvisiert");
            return;
        }
        int vorher = t.GetInventoryStackCount();
        bool ok = t.GenerateTradeGoods();
        print("Ware neu fuer '" + t.GetName() + "': " + ok + ", Stapel " + vorher + " -> " + t.GetInventoryStackCount());
    }
}
