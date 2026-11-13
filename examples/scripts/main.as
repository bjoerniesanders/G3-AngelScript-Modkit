// G3ASM Mini-Mod "Charakter-Info" (API 3) - Gothic-3-Gegenstueck zum Remake-Beispiel "ExampleStatsPageExtension".
// Liegt im Spiel unter <Gothic3>\g3asm\scripts\main.as; Aenderungen werden im laufenden Spiel neu geladen.
//  - Bildschirmtext im Spielstil (PrintScreen) statt Windows-Toenen
//  - F11:        Info auf dem Bildschirm: Position, Spielzeit, Attribute (wie der Text-Block im Beispiel)
//  - Shift+F11:  Staerke +1 (wie der Knopf im Beispiel: IncreaseStrengthAttributeBy(1))
//  - F10:        aktuelle Position merken, Shift+F10: dorthin teleportieren
//  - Menueseite: beim Oeffnen einer Spielseite (Inventar, Charakter, ...) steht ihre Nummer im Protokoll;
//                ist CHARAKTER_SEITE gesetzt, erscheint die Info automatisch beim Oeffnen dieser Seite.
// (F5/F9 = Schnellspeichern/-laden, F1-F3, F6-F8, F12 sind vom Spiel belegt)

const int VK_SHIFT = 0x10;
const int CHARAKTER_SEITE = -100;   // Nummer der Charakterseite aus dem Protokoll ("Spielseite: N") hier eintragen

float sekunde = 0;
int bilder = 0;
float fps = 0;
float gesamt = 0;
bool f11vorher = false, f10vorher = false;
bool gemerkt = false;
bCVector merkPos;
int seiteVorher = -1;

bCString Zwei(int v) { return (v < 10 ? "0" : "") + ToString(v); }
bCString Pos(const bCVector &in v) { return "(" + int(v.x) + ", " + int(v.y) + ", " + int(v.z) + ")"; }
bCString Spielzeit() {
    if (GetGameHour() < 0) return "unbekannt";
    return "Tag " + GetGameDay() + ", " + Zwei(GetGameHour()) + ":" + Zwei(GetGameMinute());
}
bCString Attr(const Entity &in e, const bCString &in kennung, const bCString &in name) {
    return name + " " + e.GetAttribute(kennung);
}

void Zeige(const bCString &in text, float dauer = 4) {
    print(text);
    if (!PrintScreen(text, dauer, 255, 230, 160)) SetWindowTitle("Gothic 3 | " + text);
}

void ZeigeInfo(const Entity &in held) {
    Zeige("Ort: " + Pos(held.GetPosition()), 6);
    Zeige("Zeit: " + Spielzeit(), 6);
    Zeige(Attr(held, "STR", "Staerke") + " | " + Attr(held, "DEX", "Geschick") + " | " + Attr(held, "INT", "Intelligenz"), 6);
    Zeige("Leben " + held.GetAttribute("HP") + "/" + held.GetAttributeMax("HP") + " | Mana " + held.GetAttribute("MP") + "/" +
          held.GetAttributeMax("MP") + " | Lernpunkte " + held.GetAttribute("LP"), 6);
}

void main() {
    print("G3ASM aktiv - AngelScript laeuft (API " + GetApiVersion() + ")");
}

void OnFrame(float dt) {
    gesamt += dt;
    sekunde += dt;
    bilder++;
    Entity held = GetHero();
    bool f11 = IsKeyDown(KEY_F11), f10 = IsKeyDown(KEY_F10);
    bool f11neu = f11 && !f11vorher, f10neu = f10 && !f10vorher;
    f11vorher = f11;
    f10vorher = f10;
    bool shift = IsKeyDown(VK_SHIFT);

    if (f11neu && held.IsValid()) {
        if (shift) {
            // der "Knopf": Staerke +1 (Grundwert, wie PSPlayerMemory::AddStrength)
            if (held.AddAttributeBase("STR", 1)) Zeige("Staerke +1 -> " + held.GetAttribute("STR"));
            else Zeige("Staerke aendern nicht moeglich");
        } else {
            ZeigeInfo(held);
        }
    }
    if (f10neu && held.IsValid()) {
        if (shift) {
            if (!gemerkt) Zeige("Noch keine Position gemerkt (erst F10)");
            else if (held.SetPosition(merkPos)) Zeige("Teleportiert nach " + Pos(merkPos));
            else Zeige("Teleport nicht moeglich");
        } else {
            merkPos = held.GetPosition();
            gemerkt = true;
            Zeige("Position gemerkt: " + Pos(merkPos) + " (Shift+F10 = zurueck)");
        }
    }

    // Menueseiten beobachten (Nummer fuer CHARAKTER_SEITE ermitteln)
    int seite = GetActivePage();
    if (seite != seiteVorher) {
        print("Spielseite: " + seite);
        if (seite == CHARAKTER_SEITE && held.IsValid()) ZeigeInfo(held);
        seiteVorher = seite;
    }

    if (sekunde >= 1.0f) {
        fps = bilder / sekunde;
        sekunde = 0;
        bilder = 0;
        bCString titel = "Gothic 3 | G3ASM | " + int(fps) + " FPS";
        if (held.IsValid()) titel += " | " + Spielzeit() + " | STR " + held.GetAttribute("STR");
        SetWindowTitle(titel + " | F11 Info, Shift+F11 Staerke+1, F10/Shift+F10 Teleport");
    }
}

void OnWorldActivate() { print("Welt geladen"); }
void OnWorldDeactivate() { print("Welt entladen"); }
void OnShutdown() { print("Mini-Mod wird neu geladen/beendet"); }
