// Begleiter teleportieren mit (ab API 33): Teleportiert sich der Held per Rune oder Zauber, kommen seine Begleiter mit.
// Ablauf: Das Spielskript "MagicTeleport" (Script_Game.dll) wird per WatchScript/Override_Script abgefangen. VOR dem Original merkt sich die Mod, wer
// mitkommt; nach dem Original (Super) holt sie diese Mitglieder mit Entity.TeleportTo zum Helden. Weil der Held erst nach dem Ueberblenden am Ziel stehen
// kann, prueft die Mod NACHHOLEN Sekunden spaeter noch einmal und holt alle nach, die weiter als WEIT entfernt sind.
// Mitglieder: gCParty_PS.PartyMemberType 1 Party (Begleiter), 3 Slave, 5 Summoned (Beschwoerung) kommen mit; 4 Controlled (Verwandlung/Kontrolle) und 2 Mob nicht.
// Wartende Begleiter (der Held hat "Warte hier" gesagt) bleiben, wenn WARTENDE_MIT = false.
// Gemessen 2026-11-09: MagicTeleport selbst schaltet alle Begleiter auf Warten (im normalen Spiel bleiben sie so am alten Ort stehen). Darum zaehlt der
// Wartestatus von VOR dem Teleport, und die Mod hebt das Warten der Mitgenommenen danach wieder auf (sofort und nach NACHHOLEN Sekunden).
// Nach TeleportTo setzt die Mod jeden Begleiter NEBEN cm neben den Helden, mehrere im Kreis verteilt (genau auf dem Helden spricht er ihn sofort an).
// Protokoll: og3_patch.log (Positionen vor/nach, je Mitglied Typ, Abstand, Ergebnis).
const bool WARTENDE_MIT = false;
const float NACHHOLEN = 1.5f;   // Sekunden
const float WEIT = 1500.0f;     // Abstand in Spieleinheiten (cm)
const float NEBEN = 150.0f;     // Abstand neben dem Helden

float nachholen = -1;
array<Entity> mitnehmen;   // vor dem Teleport bestimmt

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

// Mitglieder bestimmen, die mitkommen (vor dem Teleport, solange der Wartestatus noch der des Spielers ist)
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

// die bestimmten Mitglieder holen; nurWeit: nur die, die weiter als WEIT weg sind. Rueckgabe: Zahl geholter
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
            float w = 2.4f * float(i) + 1.0f;   // je Begleiter ein anderer Platz im Kreis
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
