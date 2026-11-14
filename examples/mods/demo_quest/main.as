// Quest-Demo. Numblock 1 = Status der Demo-Quest zeigen, 2 = anlegen + starten, 3 = erfolgreich abschliessen, 4 = fehlgeschlagen, 5 = entfernen, 6 = XP auf 100 setzen.
// Bild hoch = Quests auflisten (Statusverteilung, einzelne Eintraege im Protokoll), Bild runter = Infos/Dialoge im Protokoll ansehen, Einfg = Quest-Dateien neu laden.
// (Numblock 7, 8, 9 sind Debug-Tasten des Spiels und werden von den Beispiel-Mods nicht benutzt; siehe docs/modkit/examples.md, Tastenbelegung.)
// Zeigt, was das Questsystem des Spiels fuer Skripte kann. Abschalten: in g3asm\mods\load_order.txt "-demo_quest" eintragen.
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
        array<int> zaehler(9, 0);   // Status 0..7, 8 = unbekannt
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
        // Bild runter: Infos (Dialoge) des Spiels ansehen
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
