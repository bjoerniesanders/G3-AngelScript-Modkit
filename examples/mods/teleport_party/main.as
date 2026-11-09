// Begleiter teleportieren mit (ab API 33): Teleportiert sich der Held per Rune oder Zauber, kommen seine Begleiter mit.
// Ablauf: Das Spielskript "MagicTeleport" (Script_Game.dll) wird per WatchScript/Override_Script abgefangen; nach dem Original (Super) holt die Mod jedes
// Gruppenmitglied des Helden mit Entity.TeleportTo zu ihm. Weil der Held erst nach dem Ueberblenden am Ziel stehen kann, prueft die Mod NACHHOLEN
// Sekunden spaeter noch einmal und holt alle nach, die weiter als WEIT entfernt sind.
// Mitglieder: gCParty_PS.PartyMemberType 1 Party (Begleiter), 3 Slave, 5 Summoned (Beschwoerung) kommen mit; 4 Controlled (Verwandlung/Kontrolle) und 2 Mob nicht.
// Wartende Begleiter (der Held hat "Warte hier" gesagt) bleiben, wenn WARTENDE_MIT = false.
// Protokoll: og3_patch.log (Positionen vor/nach, je Mitglied Typ, Abstand, Ergebnis).
const bool WARTENDE_MIT = false;
const float NACHHOLEN = 1.5f;   // Sekunden
const float WEIT = 1500.0f;     // Abstand in Spieleinheiten (cm)

float nachholen = -1;

void main() {
    WatchScript("MagicTeleport");
}

// Typ als Zahl aus dem Eigenschaftstext ("1 : gEPartyMemberType_Party")
int Typ(Entity m) {
    bCString t = GetProperty(m, "Party", "PartyMemberType");
    if (t.IsEmpty()) return -1;
    if (t.Find("_Party", 0) >= 0) return 1;
    if (t.Find("_Mob", 0) >= 0) return 2;
    if (t.Find("_Slave", 0) >= 0) return 3;
    if (t.Find("_Controlled", 0) >= 0) return 4;
    if (t.Find("_Summoned", 0) >= 0) return 5;
    return 0;
}

bool KommtMit(Entity m) {
    int typ = Typ(m);
    if (typ != 1 && typ != 3 && typ != 5) return false;
    if (!WARTENDE_MIT && m.IsPartyWaiting()) return false;
    return true;
}

// alle mitkommenden Mitglieder holen; nurWeit: nur die, die weiter als WEIT weg sind. Rueckgabe: Zahl geholter
int Holen(bool nurWeit, const bCString &in anlass) {
    Entity held = GetHero();
    int n = held.GetPartyMemberCount();
    int geholt = 0;
    for (int i = 0; i < n; ++i) {
        Entity m = held.GetPartyMember(i);
        if (!m.IsValid() || m == held) continue;
        float d = held.GetDistanceTo(m);
        bool mit = KommtMit(m);
        if (mit && (!nurWeit || d > WEIT)) {
            bool ok = m.TeleportTo(held);
            if (ok) geholt++;
            print(anlass + ": " + m.GetName() + " (Typ " + Typ(m) + ", Abstand " + int(d) + ") -> " + (ok ? "geholt, jetzt " + int(held.GetDistanceTo(m)) : bCString("Teleport fehlgeschlagen")));
        } else if (!nurWeit) {
            print(anlass + ": " + m.GetName() + " (Typ " + Typ(m) + ", wartet " + m.IsPartyWaiting() + ", Abstand " + int(d) + ") bleibt");
        }
    }
    return geholt;
}

bool Override_Script(const bCString &in name, Entity self, Entity other, int arg) {
    if (name != "MagicTeleport") return Super();
    Entity held = GetHero();
    if (!(self == held)) return Super();
    bCVector vorher = held.GetPosition();
    bool r = Super();
    bCVector nachher = held.GetPosition();
    print("MagicTeleport des Helden: Ergebnis " + r + ", Position vorher " + vorher.ToString() + ", nachher " + nachher.ToString() + ", Mitglieder " + held.GetPartyMemberCount());
    if (held.GetPartyMemberCount() > 0) {
        Holen(false, "Sofort");
        nachholen = NACHHOLEN;
    }
    return r;
}

void OnFrame(float dt) {
    if (nachholen < 0) return;
    nachholen -= dt;
    if (nachholen >= 0) return;
    int g = Holen(true, "Nachholen");
    if (g > 0) PrintScreen(g + " Begleiter nachgeholt", 3, 180, 255, 180);
}

// Strg + Ziffernblock , (Komma/Entf-Taste des Ziffernblocks) zeigt die Gruppe des Helden im Protokoll
void OnKey(int vk, bool down) {
    if (!down || vk != 0x6E || !IsKeyDown(0x11)) return;
    Entity held = GetHero();
    int n = held.GetPartyMemberCount();
    print("Gruppe des Helden: " + n + " Mitglieder");
    for (int i = 0; i < n; ++i) {
        Entity m = held.GetPartyMember(i);
        print("  " + i + ": " + (m.IsValid() ? m.GetName() : bCString("-")) + " Typ " + Typ(m) + " wartet " + m.IsPartyWaiting() + " Anfuehrer " + (m.GetPartyLeader().IsValid() ? m.GetPartyLeader().GetName() : bCString("-")) + " Abstand " + int(held.GetDistanceTo(m)));
    }
    PrintScreen("Gruppe: " + n + " Mitglieder (Protokoll)", 3, 180, 255, 180);
}
