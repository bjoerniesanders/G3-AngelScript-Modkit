// Jens dialog: Jens gets two new dialog options (real dialogs of the game, with a connection to the quest log).
//   "Hast du Arbeit fuer mich?"  -> Jens asks for a wolf skin, the quest starts (title, description and dialog text in the quest log).
//   "Ich habe das Wolfsfell."     -> shows while the quest runs and the hero has a wolf skin (the game checks it, from v0.47.2).
//                                    Takes the skin, completes the quest, reward.
// When the hero has a wolf skin while the quest runs (killed, found, bought, received), an entry goes into the quest log without a dialog
// (AddQuestLogEntry, from v0.47.1).
// Save and load: the quest and the dialogs are run-time objects and are lost at a load. Thus the progress is in two switches that the
// game saves in the saved game (SetGameEvent). After each load, the mod makes the quest and the dialogs again and restores the progress.
// Note: Jens already has his own wolf skin quest in the game (KapDun_Hunter_Fur). This mod quest is independent of it.
// Turn off the mod jens_quest (nearness + Scroll Lock key): write "-jens_quest" in g3asm\mods\load_order.txt.
const bCString NPC = "Jens";
const bCString QUEST = "G3ASM_Jens_Wolfsfell";
const bCString ITEM = "It_Wolf_Fur";
const bCString BOGEN = "It_Bow_Ranger";
const bCString PFEILE = "Arrow";
const bCString REWARD_ATTRIBUTE = "DEX";   // attribute reward of the quest (success message "Jagdgeschick"); DescribeQuestRewards() lists all tags
const bCString INFO_JOB = "G3ASM_Jens_Job";
const bCString INFO_TURNIN = "G3ASM_Jens_Turnin";
const bCString EV_STARTED = "G3ASM_JensWolf_Started";   // switches in the saved game
const bCString EV_DONE = "G3ASM_JensWolf_Done";
const bCString EV_FAILED = "G3ASM_JensWolf_Failed";
const bCString EV_FUR = "G3ASM_JensWolf_HasFur";   // entry "I have a wolf skin" already in the quest log

float timer = 0;
bool angelegt = false;

void Zeige(const bCString &in t, int r = 255, int g = 255, int b = 180) { print(t); PrintScreen(t, 5, r, g, b); }

// Quest: if the mod archive is installed (ModFiles\Quests.m0x -> Data\Quests.mod/.m00/..., see docs\modkit\archives.md), the game itself gives the quest from the
// file G3_World_01\G3ASM_Jens_Wolfsfell_quest_G3_World_01.quest (type, XP 100, reward DEX +1; the title and the description are in ModFiles\stringtable.ini).
// Without the archive, the script makes the quest at run time (fallback) and sets the same values. Status at first "open".
bool selbstErzeugt = false;   // this script made the quest (not from the archive)
void QuestVorbereiten() {
    if (!QuestExists(QUEST)) { CreateQuest(QUEST); selbstErzeugt = true; }
    if (selbstErzeugt) {
        SetQuestTopic(QUEST, "Wolfsfell fuer Jens");
        SetQuestDescription(QUEST, "Jens braucht ein Wolfsfell. Bring es ihm, dann bezahlt er dich dafuer.");
        SetQuestXP(QUEST, 100);
        SetQuestReward(QUEST, REWARD_ATTRIBUTE, 1);
    }
}

// Emergency recovery, if the game did not restore the quest and the quest log itself at the load: progress from the switches in the saved game.
// (Normal case: the mod makes the quest and the dialogs during the load - OnQuestsLoaded and OnInfosLoaded - and the game restores the status and the log itself.)
void Wiederherstellen() {
    bool fertig = IsGameEventSet(EV_DONE);
    bool begonnen = IsGameEventSet(EV_STARTED);
    bool gescheitert = IsGameEventSet(EV_FAILED);
    print("Stand aus dem Spielstand: begonnen " + begonnen + ", fertig " + fertig + ", gescheitert " + gescheitert);
    if (GetQuestStatus(QUEST) > QUEST_OPEN) { print("Quest-Status wurde schon vom Spiel hergestellt: " + GetQuestStatus(QUEST)); return; }
    if (gescheitert) {
        SetQuestXP(QUEST, 0);
        SetQuestReward(QUEST, REWARD_ATTRIBUTE, 0);
        SetQuestStatus(QUEST, QUEST_FAILED);   // set directly: no "Quest angenommen" message as with RunQuest
        SetInfoGiven(INFO_JOB, true);
    } else if (fertig) {
        // complete: set the reward to 0 first, so that the status change does not pay two times
        SetQuestXP(QUEST, 0);
        SetQuestReward(QUEST, REWARD_ATTRIBUTE, 0);
        SetQuestStatus(QUEST, QUEST_SUCCESS);
        SetInfoGiven(INFO_JOB, true);
        SetInfoGiven(INFO_TURNIN, true);
    } else if (begonnen) {
        SetInfoGiven(INFO_JOB, true);   // request already heard: hides the option
        SetQuestStatus(QUEST, QUEST_RUNNING);
    }
}

// diagnostics: what is already there at the load? (log)
void Diagnose() {
    int n = GetQuestCount();
    int g3asm = 0;
    for (int i = 0; i < n; i++) {
        bCString name = GetQuestNameAt(i);
        if (name.Left(5) == "G3ASM") { g3asm++; print("  Quest im Manager [" + i + "]: '" + name + "' Status " + GetQuestStatus(name) + ", XP " + GetQuestXP(name)); }
    }
    print("Vor dem Anlegen: Quests " + n + ", eigene " + g3asm + ", Quest existiert " + QuestExists(QUEST) + " (Status " + GetQuestStatus(QUEST) + "), Info Job existiert " + InfoExists(INFO_JOB) +
          ", Job gegeben " + IsInfoGiven(INFO_JOB));
}

// Make the two dialogs (without progress). The mod calls this more than 1 time (OnInfosLoaded and later Anlegen). CreateInfo gives "true" for an existing info and changes
// nothing, but AddInfoLine adds more lines each time: without this check, the NPC would say each line two times.
bool ErstelleInfos() {
    if (InfoExists(INFO_JOB) && InfoExists(INFO_TURNIN)) return true;
    if (!CreateInfo(INFO_JOB, NPC)) return false;
    SetInfoQuest(INFO_JOB, QUEST);   // the dialog lines of this info go into the quest log of this quest
    SetInfoType(INFO_JOB, INFO_TYPE_INFO);
    SetInfoCondition(INFO_JOB, INFO_COND_GENERAL);
    AddInfoLine(INFO_JOB, "Say", "player", "", "", "", "Hast du Arbeit fuer mich?");
    AddInfoLine(INFO_JOB, "Say", "npc", "", "", "", "Arbeit? Ja, tatsaechlich. Ich brauche ein Wolfsfell.");
    AddInfoLine(INFO_JOB, "Say", "npc", "", "", "", "Bring mir eins, dann bezahle ich dich dafuer.");

    CreateInfo(INFO_TURNIN, NPC);
    SetInfoType(INFO_TURNIN, INFO_TYPE_INFO);
    SetInfoCondition(INFO_TURNIN, INFO_COND_RUNNING);   // shows while the quest runs
    SetInfoQuest(INFO_TURNIN, QUEST);
    // condition: the hero has at least one wolf skin (the game checks it itself; without it, the option does not show)
    SetInfoProperty(INFO_TURNIN, "ConditionItemContainer", "PC_Hero");
    SetInfoProperty(INFO_TURNIN, "CondItems", "[" + ITEM + "]");
    SetInfoProperty(INFO_TURNIN, "CondItemAmounts", "[1]");
    AddInfoLine(INFO_TURNIN, "Say", "player", "", "", "", "Ich habe das Wolfsfell.");
    AddInfoLine(INFO_TURNIN, "Say", "npc", "", "", "", "Zeig her.");
    AddInfoLine(INFO_TURNIN, "Say", "npc", "", "", "", "Gute Arbeit. Hier, nimm diesen Bogen, ich brauche ihn nicht mehr.");
    RebuildInfos();
    return InfoExists(INFO_JOB) && InfoExists(INFO_TURNIN);
}

bool Anlegen() {
    Diagnose();
    QuestVorbereiten();
    bool ok = ErstelleInfos();
    Wiederherstellen();
    RebuildInfos();
    print("Dialoge angelegt: " + InfoExists(INFO_JOB) + ", " + InfoExists(INFO_TURNIN) + ", Quest-Status " + GetQuestStatus(QUEST));
    return ok;
}

// Load of the game: the game makes the quests and infos again from the ini files. Ours must be there AT THAT TIME, before the game reads the
// saved data (status, quest log, "info given").
void OnQuestsLoaded() {
    // In the main menu, the quest manager is empty (GetQuestCount() = -1, the quests of the game are also missing): make nothing here. The game makes it again at the load.
    if (GetQuestCount() < 0) { print("OnQuestsLoaded: Quest-Manager noch nicht lesbar (Hauptmenue), uebersprungen"); return; }
    selbstErzeugt = false;   // new quest manager: if the quest was already there, it comes from the archive
    bool imArchiv = QuestExists(QUEST);
    QuestVorbereiten();
    print("OnQuestsLoaded: Quest vorbereitet (" + (imArchiv ? "aus dem Mod-Archiv, XP " + GetQuestXP(QUEST) : "zur Laufzeit angelegt") + "), Status " + GetQuestStatus(QUEST));
}
void OnInfosLoaded() { print("OnInfosLoaded: Dialoge " + (ErstelleInfos() ? "angelegt" : "NICHT angelegt")); }

// reward: Jens gives a bow and arrows (the attribute point comes through the quest reward)
void Belohnung(Entity held) {
    int bogen = held.AddItems(BOGEN, 1);
    int pfeile = held.AddItems(PFEILE, 15);
    print("Belohnung: Bogen " + bogen + ", Pfeile " + pfeile);
    print("Attribute: DEX " + held.GetAttribute("DEX") + "/" + held.GetAttributeBase("DEX") + ", STR " + held.GetAttribute("STR") + ", INT " + held.GetAttribute("INT") +
          "; Quest-Belohnung: " + GetQuestReward(QUEST));
    if (bogen >= 0) Zeige("Jens schenkt dir einen Bogen" + (pfeile >= 0 ? " und 15 Pfeile." : "."), 255, 220, 140);
}

void OnFrame(float dt) {
    timer += dt;
    if (timer < 1.0f) return;
    timer = 0;
    if (GetInfoCount() <= 0 || !GetHero().IsValid()) return;
    // after the load, the quest or the dialogs are missing: make them again (the switches can be read only when the hero is loaded)
    if (!angelegt || !InfoExists(INFO_JOB) || !QuestExists(QUEST)) angelegt = Anlegen();
    FellEintrag();
}

// The hero has a wolf skin while the quest runs: one entry in the quest log. The check of the inventory covers each way to get the skin.
// The game saves the entry with the quest; the switch prevents a second entry.
void FellEintrag() {
    if (GetQuestStatus(QUEST) != QUEST_RUNNING || IsGameEventSet(EV_FUR) || GetHero().GetItemCount(ITEM) < 1) return;
    int n = AddQuestLogEntry(QUEST, "", "Ich habe ein Wolfsfell. Jetzt muss ich es nur noch zu Jens bringen.");
    if (n > 0) SetGameEvent(EV_FUR);
    print("Questlog-Eintrag 'Wolfsfell': " + n + " Eintraege");
}

void OnWorldActivate() { angelegt = false; }
void OnWorldDeactivate() { angelegt = false; }

void OnInfoFinished(const bCString &in info, const bCString &in owner, const bCString &in quest) {
    print("Info beendet: " + info + " (Besitzer " + owner + ", Quest '" + quest + "')");
    if (info == INFO_JOB) {
        if (GetQuestStatus(QUEST) == QUEST_RUNNING) return;
        QuestVorbereiten();
        RunQuest(QUEST);
        SetGameEvent(EV_STARTED);   // keep in the saved game
        Zeige("Neue Quest: Wolfsfell fuer Jens", 140, 220, 255);
        print("Quest-Status: " + GetQuestStatus(QUEST));
    } else if (info == INFO_TURNIN) {
        Entity held = GetHero();
        if (held.GetItemCount(ITEM) < 1) { Zeige("Jens: Du hast gar kein Wolfsfell dabei.", 255, 180, 140); return; }
        if (held.RemoveItems(ITEM, 1) < 1) { Zeige("Das Fell liess sich nicht abgeben.", 255, 140, 140); return; }
        if (SucceedQuest(QUEST)) {
            SetGameEvent(EV_DONE);
            ClearGameEvent(EV_STARTED);
            Zeige("Quest abgeschlossen: Wolfsfell fuer Jens", 140, 255, 140);
        }
        Belohnung(held);
        print("Quest-Status: " + GetQuestStatus(QUEST));
    }
}

// Jens killed before the quest is delivered: the quest fails
void OnNPCKilled(Entity opfer) {
    if (opfer.GetName() != NPC) return;
    int st = GetQuestStatus(QUEST);
    if (st == QUEST_RUNNING) {
        FailQuest(QUEST);
        SetGameEvent(EV_FAILED);
        ClearGameEvent(EV_STARTED);
        Zeige("Jens ist tot - die Quest ist gescheitert.", 255, 140, 140);
        print("Quest-Status: " + GetQuestStatus(QUEST));
    } else {
        print("Jens getoetet, Quest-Status " + st + " (keine Aenderung)");
    }
}
