// Jens-Dialog: Jens bekommt zwei neue Gespraechsoptionen (echte Dialoge des Spiels, mit Questlog-Verbindung).
//   "Hast du Arbeit fuer mich?"  -> Jens bittet um ein Wolfsfell, die Quest startet (Titel/Beschreibung/Dialogtext im Questlog).
//   "Ich habe das Wolfsfell."     -> erscheint, solange die Quest laeuft; nimmt das Fell, schliesst die Quest ab, Belohnung.
// Speichern/Laden: Quest und Dialoge sind Laufzeit-Objekte und gehen beim Laden verloren. Der Fortschritt steht deshalb in zwei Schaltern, die das
// Spiel im Spielstand mitspeichert (SetGameEvent). Nach jedem Laden legt die Mod Quest und Dialoge neu an und stellt den Stand wieder her.
// Hinweis: Jens hat im Spiel schon eine eigene Wolfsfell-Quest (KapDun_Hunter_Fur); diese Mod-Quest ist davon unabhaengig.
// Die Mod jens_quest (Naehe + Rollen-Taste) sollte abgeschaltet sein: in g3asm\mods\load_order.txt "-jens_quest" eintragen.
const bCString NPC = "Jens";
const bCString QUEST = "G3ASM_Jens_Wolfsfell";
const bCString ITEM = "It_Wolf_Fur";
const bCString BOGEN = "It_Bow_Ranger";
const bCString PFEILE = "Arrow";
const bCString REWARD_ATTRIBUTE = "DEX";   // Attribut-Belohnung der Quest (Erfolgsmeldung "Jagdgeschick"); DescribeQuestRewards() listet alle Kennungen
const bCString INFO_JOB = "G3ASM_Jens_Job";
const bCString INFO_TURNIN = "G3ASM_Jens_Turnin";
const bCString EV_STARTED = "G3ASM_JensWolf_Started";   // Schalter im Spielstand
const bCString EV_DONE = "G3ASM_JensWolf_Done";
const bCString EV_FAILED = "G3ASM_JensWolf_Failed";

float timer = 0;
bool angelegt = false;

void Zeige(const bCString &in t, int r = 255, int g = 255, int b = 180) { print(t); PrintScreen(t, 5, r, g, b); }

// Quest: Ist das Mod-Archiv installiert (ModFiles\Quests.m0x -> Data\Quests.mod/.m00/..., siehe docs\modkit\archives.md), liefert das Spiel die Quest selbst aus der
// Datei G3_World_01\G3ASM_Jens_Wolfsfell_quest_G3_World_01.quest (Typ, XP 100, Belohnung DEX +1; Titel und Beschreibung stehen in ModFiles\stringtable.ini).
// Ohne Archiv legt das Skript die Quest zur Laufzeit an (Rueckfall) und setzt dieselben Werte. Status zunaechst "offen".
bool selbstErzeugt = false;   // Quest wurde von diesem Skript angelegt (nicht aus dem Archiv)
void QuestVorbereiten() {
    if (!QuestExists(QUEST)) { CreateQuest(QUEST); selbstErzeugt = true; }
    if (selbstErzeugt) {
        SetQuestTopic(QUEST, "Wolfsfell fuer Jens");
        SetQuestDescription(QUEST, "Jens braucht ein Wolfsfell. Bring es ihm, dann bezahlt er dich dafuer.");
        SetQuestXP(QUEST, 100);
        SetQuestReward(QUEST, REWARD_ATTRIBUTE, 1);
    }
}

// Notfall-Wiederherstellung, falls das Spiel Quest/Questlog beim Laden nicht selbst zurueckgebracht hat: Stand aus den Schaltern im Spielstand.
// (Normalfall: Quest und Dialoge werden schon waehrend des Ladens angelegt - OnQuestsLoaded/OnInfosLoaded - und das Spiel stellt Status und Log selbst her.)
void Wiederherstellen() {
    bool fertig = IsGameEventSet(EV_DONE);
    bool begonnen = IsGameEventSet(EV_STARTED);
    bool gescheitert = IsGameEventSet(EV_FAILED);
    print("Stand aus dem Spielstand: begonnen " + begonnen + ", fertig " + fertig + ", gescheitert " + gescheitert);
    if (GetQuestStatus(QUEST) > QUEST_OPEN) { print("Quest-Status wurde schon vom Spiel hergestellt: " + GetQuestStatus(QUEST)); return; }
    if (gescheitert) {
        SetQuestXP(QUEST, 0);
        SetQuestReward(QUEST, REWARD_ATTRIBUTE, 0);
        SetQuestStatus(QUEST, QUEST_FAILED);   // direkt setzen: keine "Quest angenommen"-Meldung wie bei RunQuest
        SetInfoGiven(INFO_JOB, true);
    } else if (fertig) {
        // abgeschlossen: Belohnung vorher auf 0, damit das Setzen des Status nichts doppelt auszahlt
        SetQuestXP(QUEST, 0);
        SetQuestReward(QUEST, REWARD_ATTRIBUTE, 0);
        SetQuestStatus(QUEST, QUEST_SUCCESS);
        SetInfoGiven(INFO_JOB, true);
        SetInfoGiven(INFO_TURNIN, true);
    } else if (begonnen) {
        SetInfoGiven(INFO_JOB, true);   // Bitte schon gehoert: blendet die Option aus
        SetQuestStatus(QUEST, QUEST_RUNNING);
    }
}

// Diagnose: was ist beim Laden schon da? (Protokoll)
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

// Die beiden Dialoge anlegen (ohne Zustand). Wird mehrfach aufgerufen (OnInfosLoaded und spaeter Anlegen). CreateInfo meldet bei einer vorhandenen Info "true" und aendert
// nichts, AddInfoLine haengt aber jedes Mal weitere Zeilen an: ohne diese Pruefung wuerde jede Zeile doppelt gesprochen.
bool ErstelleInfos() {
    if (InfoExists(INFO_JOB) && InfoExists(INFO_TURNIN)) return true;
    if (!CreateInfo(INFO_JOB, NPC)) return false;
    SetInfoQuest(INFO_JOB, QUEST);   // Dialogzeilen dieser Info landen im Questlog dieser Quest
    SetInfoType(INFO_JOB, INFO_TYPE_INFO);
    SetInfoCondition(INFO_JOB, INFO_COND_GENERAL);
    AddInfoLine(INFO_JOB, "Say", "player", "", "", "", "Hast du Arbeit fuer mich?");
    AddInfoLine(INFO_JOB, "Say", "npc", "", "", "", "Arbeit? Ja, tatsaechlich. Ich brauche ein Wolfsfell.");
    AddInfoLine(INFO_JOB, "Say", "npc", "", "", "", "Bring mir eins, dann bezahle ich dich dafuer.");

    CreateInfo(INFO_TURNIN, NPC);
    SetInfoType(INFO_TURNIN, INFO_TYPE_INFO);
    SetInfoCondition(INFO_TURNIN, INFO_COND_RUNNING);   // erscheint, solange die Quest laeuft
    SetInfoQuest(INFO_TURNIN, QUEST);
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

// Ladevorgang des Spiels: Quests/Infos werden aus den ini-Dateien neu aufgebaut. Unsere muessen DANN schon da sein, bevor das Spiel die
// gespeicherten Daten (Status, Questlog, "Info gegeben") einliest.
void OnQuestsLoaded() {
    // Im Hauptmenue ist der Quest-Manager leer (GetQuestCount() = -1, auch die Quests des Spiels fehlen): hier nichts anlegen, das Spiel baut ihn beim Laden neu auf.
    if (GetQuestCount() < 0) { print("OnQuestsLoaded: Quest-Manager noch nicht lesbar (Hauptmenue), uebersprungen"); return; }
    selbstErzeugt = false;   // neuer Quest-Manager: war die Quest vorher schon da, stammt sie aus dem Archiv
    bool imArchiv = QuestExists(QUEST);
    QuestVorbereiten();
    print("OnQuestsLoaded: Quest vorbereitet (" + (imArchiv ? "aus dem Mod-Archiv, XP " + GetQuestXP(QUEST) : "zur Laufzeit angelegt") + "), Status " + GetQuestStatus(QUEST));
}
void OnInfosLoaded() { print("OnInfosLoaded: Dialoge " + (ErstelleInfos() ? "angelegt" : "NICHT angelegt")); }

// Belohnung: Jens schenkt einen Bogen samt Pfeilen (der Attributpunkt kommt ueber die Quest-Belohnung)
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
    // Nach dem Laden fehlen Quest oder Dialoge: neu anlegen (Schalter lesen geht erst, wenn der Held geladen ist)
    if (!angelegt || !InfoExists(INFO_JOB) || !QuestExists(QUEST)) angelegt = Anlegen();
}

void OnWorldActivate() { angelegt = false; }
void OnWorldDeactivate() { angelegt = false; }

void OnInfoFinished(const bCString &in info, const bCString &in owner, const bCString &in quest) {
    print("Info beendet: " + info + " (Besitzer " + owner + ", Quest '" + quest + "')");
    if (info == INFO_JOB) {
        if (GetQuestStatus(QUEST) == QUEST_RUNNING) return;
        QuestVorbereiten();
        RunQuest(QUEST);
        SetGameEvent(EV_STARTED);   // im Spielstand merken
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

// Jens getoetet, bevor die Quest abgegeben ist: Quest scheitert
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
