// Quest demo. Numeric keypad 1 = show the status of the demo quest, 2 = make + start, 3 = complete with success, 4 = failed, 5 = remove, 6 = set XP to 100.
// Page Up = list the quests (status distribution, single entries in the log), Page Down = show infos and dialogs in the log, Insert = load the quest files again.
// (Numeric keypad 7, 8, 9 are debug keys of the game. The example mods do not use them; see docs/modkit/examples.md, key assignments.)
// Shows what the quest system of the game can do for scripts. To turn off: write "-demo_quest" in g3asm\mods\load_order.txt.
const bCString Q = "G3ASM_DemoQuest";

bCString StatusText(int s) {
    if (s == QUEST_OPEN) return "offen";
    if (s == QUEST_RUNNING) return "laeuft";
    if (s == QUEST_SUCCESS) return "erfolgreich";
    if (s == QUEST_FAILED) return "fehlgeschlagen";
    if (s == QUEST_OBSOLETE) return "hinfaellig";
    if (s == QUEST_CANCELLED) return "abgebrochen";
    if (s == QUEST_LOST) return "verloren";
    if (s == QUEST_WON) return "gewonnen";
    return "unbekannt";
}
void Zeige(const bCString &in t) { print(t); PrintScreen(t, 4, 255, 255, 140); }

void OnKey(int vk, bool down) {
    if (!down) return;
    if (vk == 0x61) {
        if (!QuestExists(Q)) Zeige("Demo-Quest existiert nicht (Numblock 2 legt sie an)");
        else Zeige("Demo-Quest: " + StatusText(GetQuestStatus(Q)) + " (Status " + GetQuestStatus(Q) + "), XP " + GetQuestXP(Q) +
                   ", beendet " + IsQuestFinished(Q) + ", erfolgreich " + IsQuestSucceeded(Q));
    } else if (vk == 0x62) {
        bool neu = CreateQuest(Q);
        bool lauf = RunQuest(Q);
        Zeige("Quest anlegen: " + neu + ", starten: " + lauf + " -> " + StatusText(GetQuestStatus(Q)));
    } else if (vk == 0x63) {
        bool ok = SucceedQuest(Q);
        Zeige("Quest erfolgreich: " + ok + " -> " + StatusText(GetQuestStatus(Q)));
    } else if (vk == 0x64) {
        bool ok = FailQuest(Q);
        Zeige("Quest fehlgeschlagen: " + ok + " -> " + StatusText(GetQuestStatus(Q)));
    } else if (vk == 0x65) {
        Zeige("Quest entfernt: " + RemoveQuest(Q));
    } else if (vk == 0x66) {
        Zeige("Quest-XP setzen: " + SetQuestXP(Q, 100) + " -> XP " + GetQuestXP(Q));
    } else if (vk == 0x21) {
        int n = GetQuestCount();
        print("Quests im Spiel: " + n);
        array<int> zaehler(9, 0);   // status 0..7, 8 = not known
        int laufend = 0;
        for (int i = 0; i < n; i++) {
            bCString name = GetQuestNameAt(i);
            int st = GetQuestStatus(name);
            zaehler[(st < 0 || st > 7) ? 8 : st]++;
            if (i < 6 || i >= n - 4 || st == QUEST_RUNNING)
                print("  [" + i + "] '" + name + "' Status " + st + ", XP " + GetQuestXP(name) + ", existiert " + QuestExists(name));
            if (st == QUEST_RUNNING) laufend++;
        }
        bCString h = "Status-Verteilung:";
        for (int k = 0; k < 9; k++) h += " " + k + "=" + zaehler[k];
        print(h);
        Zeige("Quests: " + n + ", laufend: " + laufend + " (Details im Protokoll)");
    } else if (vk == 0x22) {
        // Page Down: show the infos (dialogs) of the game
        int n = GetInfoCount();
        print("Infos im Spiel: " + n);
        print(DescribeCommands());
        print(DescribeQuestRewards());
        int gezeigt = 0;
        for (int i = 0; i < n && gezeigt < 12; i++) {
            if (GetInfoOwner(i) == "Jens") { print(DescribeInfo(GetInfoName(i))); gezeigt++; }
        }
        print("Infos von Jens: " + gezeigt);
        if (n > 0) {
            int stichprobe = n < 4 ? n : 4;
            for (int k = 0; k < stichprobe; k++) {
                int i = (n * k) / stichprobe + 7;
                if (i < n) print("Stichprobe [" + i + "]: " + DescribeInfo(GetInfoName(i)));
            }
        }
        Zeige("Infos: " + n + " (Details im Protokoll)");
    } else if (vk == 0x2D) {
        Zeige("Quest-Dateien neu geladen: " + ReloadQuestFiles());
    }
}
