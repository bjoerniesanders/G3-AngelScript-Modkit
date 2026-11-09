// Mess-Mod fuer die Zeichen-API (ab API 34): OnDraw, CreateFont, DrawText, TextWidth, WorldToScreen.
// Zeigt oben links Bildgroesse und Bildzaehler und schreibt 'Held' 1 m ueber und 'Mitte' an die Position des Helden (Pruefung von WorldToScreen).
// Protokoll (einmal): Schrift-Handle, Textbreite, Bildschirmposition des Helden.
int schrift = 0;
int bilder = 0;
bool gemeldet = false;

void OnDraw() {
    if (schrift == 0) schrift = CreateFont("Arial", 18, true);
    bilder++;
    DrawText(schrift, 30, 26, "OG3 OnDraw " + GetScreenWidth() + "x" + GetScreenHeight() + "  Bild " + bilder, 0xFFFFFF80);
    Entity held = GetHero();
    if (!held.IsValid()) return;
    float hx, hy, fx, fy;
    bool sichtbar = WorldToScreen(held.GetPosition() + bCVector(0, 100, 0), hx, hy);
    bool fuss = WorldToScreen(held.GetPosition(), fx, fy);
    if (fuss) DrawText(schrift, int(fx) - 15, int(fy), "Mitte", 0xFF80C0FF);
    if (!gemeldet) {
        gemeldet = true;
        print("Schrift " + schrift + ", Textbreite 'Hallo Welt' " + TextWidth(schrift, "Hallo Welt") + ", Held auf dem Bildschirm " + sichtbar + " bei " + int(hx) + "," + int(hy) + " von " + GetScreenWidth() + "x" + GetScreenHeight());
    }
    if (sichtbar) DrawText(schrift, int(hx) - 20, int(hy), "Held", 0xFF80FF80);
}
