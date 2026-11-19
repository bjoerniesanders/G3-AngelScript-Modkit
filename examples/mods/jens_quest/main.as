// Quest "Wolfsfell fuer Jens": when the hero comes near Jens, Jens asks for a wolf skin (the quest starts). Delivery: Scroll Lock key near Jens
// (takes one wolf skin from the inventory) or by a transfer in the game (OnReceiveItem). Reward: quest XP. If you kill Jens, the quest fails.
// To turn off: write "-jens_quest" in g3asm\mods\load_order.txt.
const bCString NPC = "Jens";
const bCString QUEST = "G3ASM_Jens_Wolfsfell";
const bCString ITEM = "It_Wolf_Fur";
const float REICHWEITE = 400.0f;   // Zentimeter (4 m)

float timer = 0;
bool gebeten = false;   // request already said (only text, not the quest)

void Zeige(const bCString &in t, int r = 255, int g = 255, int b = 180) { print(t); PrintScreen(t, 5, r, g, b); }

bool InReichweite(Entity held, Entity jens) {
    if (!held.IsValid() || !jens.IsValid()) return false;
    float d = held.GetDistanceTo(jens);
    return d >= 0 && d <= REICHWEITE;
}

// text with digits -> number ("13050" -> 13050; stops at the first character that is not a digit)
int Zahl(const bCString &in s) {
    int v = 0;
    for (int i = 0; i < s.GetLength(); i++) {
        bCString c = s.Mid(i, 1);
        int d = -1;
        for (int k = 0; k < 10; k++)
            if (c == ToString(k)) d = k;
        if (d < 0) break;
        v = v * 10 + d;
    }
    return v;
}
// The attribute "XP" reads 0. The experience of the hero is in the player memory.
int HeldXP(Entity held) { return Zahl(GetProperty(held, "PlayerMemory", "XP")); }

void Abschliessen(Entity held) {
    SetQuestXP(QUEST, 100);
    int xpVorher = HeldXP(held);
    print("Quest-XP laut Quest: " + GetQuestXP(QUEST) + ", Held-XP vorher: " + xpVorher);
    if (SucceedQuest(QUEST)) {
        Zeige("Quest abgeschlossen: Wolfsfell fuer Jens (Quest-XP " + GetQuestXP(QUEST) + ")", 140, 255, 140);
        print("Held-XP nachher: " + HeldXP(held) + " (+" + (HeldXP(held) - xpVorher) + ")");
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
        // texts for the quest log (test: does the game show the text ID as plain text?)
        print("Topic gesetzt: " + SetQuestTopic(QUEST, "Wolfsfell fuer Jens"));
        print("Beschreibung gesetzt: " + SetQuestDescription(QUEST, "Jens braucht ein Wolfsfell. Bring es ihm."));
        print("Quest-Dateipfad laut Spiel: " + GetQuestFilePath(QUEST) + ", Quest-Verzeichnis: " + GetQuestDir());
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

// transfer in the game (trade or give): the hero gives Jens a wolf skin
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
