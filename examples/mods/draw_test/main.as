// Mess-Mod fuer die Zeichen-API (ab API 34): OnDraw, CreateFont, DrawText, TextWidth, WorldToScreen; ab API 38 LoadImage/DrawImage; ab API 39 ueber dem HUD.
// Zeigt oben links Bildgroesse und Bildzaehler und schreibt 'Held' 1 m ueber und 'Mitte' an die Position des Helden (Pruefung von WorldToScreen).
// Darunter Bilder: Kompass (Pfad aus g3.gui.res), dieselbe Datei als kurzer Name, ein sich drehender Kompass (eine Umdrehung in 4 s) und ein Orts-Symbol.
// Unten links und rechts ein Kompass und roter Text ueber der Statusleiste, in den vier Ecken je eine Ecken-Marke und in der Mitte ein Text: Pruefung,
// dass OnDraw ueber dem HUD (auch ueber dem Inventar) und an der richtigen Stelle zeichnet.
// Protokoll (einmal): Schrift-Handle, Textbreite, Bildschirmposition des Helden, Bild-Nummern.
int schrift = 0;
int bilder = 0;
bool gemeldet = false;
int bildPfad = -1, bildKurz = -1, bildGross = -1;
float winkel = 0;

// Laden vor der ersten Welt: liefert oft 0 (GUI-Daten fehlen noch); dieselbe Nummer muss spaeter in OnDraw trotzdem kommen
int bildFrueh = -1;
void main() {
    bildFrueh = LoadImage("g3_hud_icon_location_silden.tga");
    print("LoadImage in main(): " + bildFrueh);
}

void OnFrame(float dt) {
    winkel += dt * 6.2832 / 4;
    if (winkel > 6.2832) winkel -= 6.2832;
}

void OnDraw() {
    if (schrift == 0) schrift = CreateFont("Arial", 18, true);
    if (bildPfad < 0) {
        bildPfad = LoadImage("\\data\\gui\\icons\\hud\\v3\\icons\\misc\\g3_hud_icon_misc_compass.tga");
        bildKurz = LoadImage("g3_hud_icon_misc_compass.tga");
        bildGross = LoadImage("\\data\\gui\\icons\\hud\\v3\\icons\\locations\\g3_hud_icon_location_silden.tga");
    }
    if (bildFrueh <= 0) {
        bildFrueh = LoadImage("g3_hud_icon_location_silden.tga");
        if (bildFrueh > 0) print("LoadImage nach main(): " + bildFrueh);
    }
    DrawImage(bildFrueh, 410, 60, 64, 64);
    bilder++;
    int w = GetScreenWidth(), h = GetScreenHeight();
    DrawText(schrift, 30, 26, "OG3 OnDraw " + w + "x" + h + "  Bild " + bilder, 0xFFFFFF80);
    bool a = DrawImage(bildPfad, 30, 60, 64, 64);
    bool b = DrawImage(bildKurz, 110, 60, 64, 64);
    bool c = DrawImage(bildPfad, 190, 60, 64, 64, winkel);
    bool d = DrawImage(bildGross, 270, 60, 128, 128);
    // ueber dem HUD: Statusleiste unten
    DrawImage(bildPfad, 10, h - 70, 64, 64);
    DrawText(schrift, 80, h - 50, "OG3 ueber dem HUD", 0xFFFF6060);
    bCString rechts = "OG3 ueber dem HUD";
    DrawText(schrift, w - TextWidth(schrift, rechts) - 20, h - 50, rechts, 0xFFFF6060);
    // Ecken und Mitte: liegen sie genau dort?
    DrawText(schrift, 2, 2, "[oben links]", 0xFF40FFFF);
    bCString orr = "[oben rechts]";
    DrawText(schrift, w - TextWidth(schrift, orr) - 2, 2, orr, 0xFF40FFFF);
    DrawText(schrift, 2, h - 22, "[unten links]", 0xFF40FFFF);
    bCString ur = "[unten rechts]";
    DrawText(schrift, w - TextWidth(schrift, ur) - 2, h - 22, ur, 0xFF40FFFF);
    bCString mitte = "OG3 Mitte - auch im Inventar sichtbar?";
    DrawText(schrift, (w - TextWidth(schrift, mitte)) / 2, h / 2, mitte, 0xFF40FF40);
    Entity held = GetHero();
    if (!held.IsValid()) return;
    float hx, hy, fx, fy;
    bool sichtbar = WorldToScreen(held.GetPosition() + bCVector(0, 100, 0), hx, hy);
    bool fuss = WorldToScreen(held.GetPosition(), fx, fy);
    if (fuss) DrawText(schrift, int(fx) - 15, int(fy), "Mitte", 0xFF80C0FF);
    if (!gemeldet) {
        gemeldet = true;
        print("Schrift " + schrift + ", Textbreite 'Hallo Welt' " + TextWidth(schrift, "Hallo Welt") + ", Held auf dem Bildschirm " + sichtbar + " bei " + int(hx) + "," + int(hy) + " von " + w + "x" + h);
        print("Bilder: Pfad " + bildPfad + " (" + a + "), kurz " + bildKurz + " (" + b + "), gedreht (" + c + "), Silden " + bildGross + " (" + d + ")");
    }
    if (sichtbar) DrawText(schrift, int(hx) - 20, int(hy), "Held", 0xFF80FF80);
}
