// Companions teleport with the hero (from API 33): if the hero teleports with a rune or a spell, the companions come too.
// Sequence: the mod intercepts the game script "MagicTeleport" (Script_Game.dll) through WatchScript and Override_Script. BEFORE the original, the mod keeps a list of who
// comes along. After the original (Super), it brings these members to the hero with Entity.TeleportTo. Because the hero can be at the target only after the fade,
// the mod checks again NACHHOLEN seconds later and brings all members that are farther away than WEIT.
// Members: gCParty_PS.PartyMemberType 1 Party (companion), 3 Slave, 5 Summoned (summon) come along. 4 Controlled (transformation or control) and 2 Mob do not.
// Waiting companions (the hero said "Warte hier", wait here) stay if WARTENDE_MIT = false.
// Measured 2026-11-09: MagicTeleport itself sets all companions to wait (in the normal game, they thus stay at the old location). Thus the
// wait status from BEFORE the teleport counts, and the mod cancels the wait of the members that came along after this (immediately and after NACHHOLEN seconds).
// After TeleportTo, the mod puts each companion NEBEN cm next to the hero, more than one in a circle (exactly on the hero, the companion speaks to the hero immediately).
// Log: g3asm.log (positions before and after; for each member type, distance, result).
const bool WARTENDE_MIT = false;
const float NACHHOLEN = 1.5f;   // seconds
const float WEIT = 1500.0f;     // distance in game units (cm)
const float NEBEN = 150.0f;     // distance next to the hero

float nachholen = -1;
array<Entity> mitnehmen;   // set before the teleport

void main() {
    WatchScript("MagicTeleport");
}

// type as a number from the property text ("1 : gEPartyMemberType_Party")
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

// find the members that come along (before the teleport, while the wait status is still the status from the player)
void Bestimmen() {
    mitnehmen.resize(0);
    Entity held = GetHero();
    int n = held.GetPartyMemberCount();
    for (int i = 0; i < n; ++i) {
        Entity m = held.GetPartyMember(i);
        if (!m.IsValid() || m == held) continue;
        if (KommtMit(m))
            mitnehmen.insertLast(m);
        else
            print("Vorher: " + m.GetName() + " (Typ " + Typ(m) + ", wartet " + m.IsPartyWaiting() + ") bleibt");
    }
}

// bring the selected members; nurWeit: only those that are farther away than WEIT. Return value: the number brought
int Holen(bool nurWeit, const bCString &in anlass) {
    Entity held = GetHero();
    int geholt = 0;
    for (uint i = 0; i < mitnehmen.length(); ++i) {
        Entity m = mitnehmen[i];
        if (!m.IsValid()) continue;
        if (m.IsPartyWaiting()) print(anlass + ": " + m.GetName() + " wurde vom Spiel auf Warten gesetzt, aufgehoben: " + m.SetPartyWaiting(false));
        float d = held.GetDistanceTo(m);
        if (nurWeit && d <= WEIT) continue;
        bool ok = m.TeleportTo(held);
        if (ok) {
            float w = 2.4f * float(i) + 1.0f;   // a different place in the circle for each companion
            m.SetPosition(held.GetPosition() + bCVector(NEBEN * cos(w), 0, NEBEN * sin(w)));
            geholt++;
        }
        print(anlass + ": " + m.GetName() + " (Typ " + Typ(m) + ", Abstand " + int(d) + ") -> " + (ok ? "geholt, jetzt " + int(held.GetDistanceTo(m)) : bCString("Teleport fehlgeschlagen")));
    }
    return geholt;
}

bool Override_Script(const bCString &in name, Entity self, Entity other, int arg) {
    if (name != "MagicTeleport") return Super();
    Entity held = GetHero();
    if (!(self == held)) return Super();
    Bestimmen();
    bCVector vorher = held.GetPosition();
    bool r = Super();
    bCVector nachher = held.GetPosition();
    print("MagicTeleport des Helden: Ergebnis " + r + ", Position vorher " + vorher.ToString() + ", nachher " + nachher.ToString() + ", Mitglieder " + held.GetPartyMemberCount() + ", mit " + mitnehmen.length());
    if (r && mitnehmen.length() > 0) {
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
    mitnehmen.resize(0);
}

// Ctrl + numeric keypad , (comma or Delete key of the numeric keypad) shows the group of the hero in the log
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
