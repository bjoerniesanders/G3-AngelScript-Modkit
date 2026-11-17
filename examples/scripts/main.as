// G3ASM mini mod "Charakter-Info" (API 3) - the Gothic 3 equivalent of the remake example "ExampleStatsPageExtension".
// In the game, it is in <Gothic3>\g3asm\scripts\main.as. The modkit loads changes again while the game runs.
//  - screen text in the style of the game (PrintScreen) in place of Windows sounds
//  - F11:        info on the screen: position, game time, attributes (as the text block in the example)
//  - Shift+F11:  strength +1 (as the button in the example: IncreaseStrengthAttributeBy(1))
//  - F10:        keep the current position, Shift+F10: teleport there
//  - menu page:  when you open a game page (inventory, character, ...), the log shows its number;
//                if CHARAKTER_SEITE is set, the info shows automatically when you open this page.
// (F5/F9 = quick save and quick load. The game uses F1-F3, F6-F8, F12)

const int VK_SHIFT = 0x10;
const int CHARAKTER_SEITE = -100;   // write the number of the character page from the log ("Spielseite: N") here

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
            // the "button": strength +1 (base value, as PSPlayerMemory::AddStrength)
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

    // watch menu pages (find the number for CHARAKTER_SEITE)
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
