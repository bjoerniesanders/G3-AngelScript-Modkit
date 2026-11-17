// Measurement mod for the drawing API (from API 34): OnDraw, CreateFont, DrawText, TextWidth, WorldToScreen; from API 38 LoadImage and DrawImage; from API 39 above the HUD.
// Shows the screen size and the frame counter at the top left, and writes 'Held' 1 m above and 'Mitte' at the position of the hero (check of WorldToScreen).
// Below: images: compass (path from g3.gui.res), the same file as a short name, a turning compass (one turn in 4 s) and a location symbol.
// At the bottom left and right a compass and red text above the status bar, in each of the four corners a corner mark, and in the center a text: these check
// that OnDraw draws above the HUD (also above the inventory) and at the correct position.
// Log (1 time): font handle, text width, screen position of the hero, image numbers.
int schrift = 0;
int bilder = 0;
bool gemeldet = false;
int bildPfad = -1, bildKurz = -1, bildGross = -1;
float winkel = 0;

// load before the first world: frequently gives 0 (GUI data still missing). The same number must still come later in OnDraw
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
    DrawText(schrift, 30, 26, "G3ASM OnDraw " + w + "x" + h + "  Bild " + bilder, 0xFFFFFF80);
    bool a = DrawImage(bildPfad, 30, 60, 64, 64);
    bool b = DrawImage(bildKurz, 110, 60, 64, 64);
    bool c = DrawImage(bildPfad, 190, 60, 64, 64, winkel);
    bool d = DrawImage(bildGross, 270, 60, 128, 128);
    // above the HUD: status bar at the bottom
    DrawImage(bildPfad, 10, h - 70, 64, 64);
    DrawText(schrift, 80, h - 50, "G3ASM ueber dem HUD", 0xFFFF6060);
    bCString rechts = "G3ASM ueber dem HUD";
    DrawText(schrift, w - TextWidth(schrift, rechts) - 20, h - 50, rechts, 0xFFFF6060);
    // corners and center: are they exactly there?
    DrawText(schrift, 2, 2, "[oben links]", 0xFF40FFFF);
    bCString orr = "[oben rechts]";
    DrawText(schrift, w - TextWidth(schrift, orr) - 2, 2, orr, 0xFF40FFFF);
    DrawText(schrift, 2, h - 22, "[unten links]", 0xFF40FFFF);
    bCString ur = "[unten rechts]";
    DrawText(schrift, w - TextWidth(schrift, ur) - 2, h - 22, ur, 0xFF40FFFF);
    bCString mitte = "G3ASM Mitte - auch im Inventar sichtbar?";
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
