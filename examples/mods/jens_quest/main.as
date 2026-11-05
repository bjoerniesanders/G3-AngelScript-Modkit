// Quest "Wolfsfell fuer Jens": Naehert sich der Held Jens, bittet er um ein Wolfsfell (Quest startet). Abgabe: Rollen-Taste in Jens' Naehe
// (nimmt ein Wolfsfell aus dem Inventar) oder durch Uebergabe im Spiel (OnReceiveItem). Belohnung: Quest-XP. Toetet man Jens, scheitert die Quest.
// Abschalten: in og3\mods\load_order.txt "-jens_quest" eintragen.
const bCString NPC = "Jens";
const bCString QUEST = "OG3_Jens_Wolfsfell";
const bCString ITEM = "It_Wolf_Fur";
const float REICHWEITE = 400.0f;   // Zentimeter (4 m)

float timer = 0;
bool gebeten = false;   // Bitte schon ausgesprochen (nur Text, nicht die Quest)

void Zeige(const bCString &in t, int r = 255, int g = 255, int b = 180) { print(t); PrintScreen(t, 5, r, g, b); }

bool InReichweite(Entity held, Entity jens) {
    if (!held.IsValid() || !jens.IsValid()) return false;
    float d = held.GetDistanceTo(jens);
    return d >= 0 && d <= REICHWEITE;
}

void Abschliessen(Entity held) {
    SetQuestXP(QUEST, 100);
    int xpVorher = held.GetAttribute("XP");
    print("XP-Attribute vorher: Wert " + xpVorher + ", Basis " + held.GetAttributeBase("XP") + ", Maximum " + held.GetAttributeMax("XP"));
    print("Quest-XP laut Quest: " + GetQuestXP(QUEST) + ", Held-XP vorher: " + xpVorher);
    if (SucceedQuest(QUEST)) {
        int xpNachher = held.GetAttribute("XP");
        Zeige("Quest abgeschlossen: Wolfsfell fuer Jens (Quest-XP " + GetQuestXP(QUEST) + ")", 140, 255, 140);
        print("Held-XP nachher: " + xpNachher + ", Basis " + held.GetAttributeBase("XP"));
    } else Zeige("Quest konnte nicht abgeschlossen werden", 255, 140, 140);
    print("Status danach: " + GetQuestStatus(QUEST));
}

void OnFrame(float dt) {
    timer += dt;
    if (timer < 0.5f) return;
    timer = 0;
    Entity held = GetHero();
    Entity jens = FindEntity(NPC);
    if (!InReichweite(held, jens)) { gebeten = false; return; }
    int s = GetQuestStatus(QUEST);
    if (s == QUEST_SUCCESS || s == QUEST_FAILED) return;
    if (s != QUEST_RUNNING && !gebeten) {
        gebeten = true;
        CreateQuest(QUEST);
        // Texte fuer das Questlog (Test: ob die Text-ID als Klartext angezeigt wird)
        print("Topic gesetzt: " + SetQuestTopic(QUEST, "Wolfsfell fuer Jens"));
        print("Beschreibung gesetzt: " + SetQuestDescription(QUEST, "Jens braucht ein Wolfsfell. Bring es ihm."));
        print("Quest-Dateipfad laut Spiel: " + GetQuestFilePath(QUEST) + ", Quest-Verzeichnis: " + GetQuestDir());
        print("Quest-Datei geschrieben (Spielordner): " + DumpQuest(QUEST, GetGameDir() + "og3_jens_quest_dump.ini"));
        print("Quest-Datei geschrieben (Questpfad): " + DumpQuest(QUEST, GetQuestFilePath(QUEST) + ".og3dump"));
        QuestDebug();
        if (RunQuest(QUEST)) Zeige("Jens: Hey du! Bring mir ein Wolfsfell, ich bezahle gut.", 255, 220, 140);
        Zeige("Neue Quest: Wolfsfell fuer Jens (Abgabe: Rollen-Taste bei Jens)", 140, 220, 255);
        print("Quest-Status nach Start: " + GetQuestStatus(QUEST));
    }
}

void OnKey(int vk, bool down) {
    if (!down || vk != 0x91) return;   // Rollen-Taste (Scroll Lock)
    Entity held = GetHero();
    Entity jens = FindEntity(NPC);
    if (!InReichweite(held, jens)) { Zeige("Hier ist niemand, dem du das Fell geben kannst."); return; }
    if (GetQuestStatus(QUEST) != QUEST_RUNNING) { Zeige("Jens: Ich brauche gerade nichts von dir."); return; }
    int hat = held.GetItemCount(ITEM);
    print("Wolfsfelle im Inventar: " + hat);
    if (hat < 1) { Zeige("Jens: Du hast kein Wolfsfell dabei.", 255, 180, 140); return; }
    if (held.RemoveItems(ITEM, 1) < 1) { Zeige("Das Fell liess sich nicht abgeben.", 255, 140, 140); return; }
    Zeige("Jens: Das ist ein schoenes Fell, danke!", 255, 220, 140);
    Abschliessen(held);
}

// Uebergabe im Spiel (Handel/Geben): Held gibt Jens ein Wolfsfell
void OnReceiveItem(Entity von, Entity an, const bCString &in item, const bCString &in guid, uint anzahl) {
    if (item != ITEM || GetQuestStatus(QUEST) != QUEST_RUNNING) return;
    if (an.IsValid() && an.GetName() == NPC && von.IsValid() && von == GetHero()) {
        Zeige("Jens: Das ist ein schoenes Fell, danke!", 255, 220, 140);
        Abschliessen(von);
    }
}

void OnNPCKilled(Entity opfer) {
    if (opfer.GetName() == NPC && GetQuestStatus(QUEST) == QUEST_RUNNING) {
        FailQuest(QUEST);
        Zeige("Jens ist tot - die Quest ist gescheitert.", 255, 140, 140);
    }
}
