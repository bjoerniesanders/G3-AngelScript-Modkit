// Measurement mod for the events from API 35: console commands (Override_ConsoleCommand, SetConsoleReply) and quest status (OnQuestStatus).
//   Console: the command "g3asm" answers with the API version. All other commands go to the game without changes.
//   Ctrl + numeric keypad * opens the console (OpenConsole), if the game has no key for it.
//   Quests:  each status change goes to the log (0 Open, 1 Running, 2 Success, 3 Failed, 4 Obsolete, 5 Cancelled, 6 Lost, 7 Won).
// From API 37: touches with the hero (OnTouch, counted each 10 s), trader goods (OnTradeGenerated), transformation (OnTransformStart/End).
//   Shift + numeric keypad / makes the goods of the focused trader again (GenerateTradeGoods) and reports the stacks before and after.
bool Override_ConsoleCommand(const bCString &in line) {
    if (line.Find("g3asm", 0) != 0) return Super();
    SetConsoleReply("G3ASM, Skript-Schnittstelle " + GetApiVersion());
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
