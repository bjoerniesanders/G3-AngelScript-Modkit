// Eigenschaften-Inspektion (ab API 29): Strg + Ziffernblock / schreibt alle Eigenschaften eines Items samt Werten ins Protokoll (g3asm.log, Zeilen "[property_inspect] ...").
// Ziel: das Item in der rechten Hand, sonst in der linken Hand oder am Ruecken, sonst das Entity unter dem Fadenkreuz (z. B. ein Item am Boden oder ein NPC).
// Gelesen werden die Eigenschaftssets Item (Wert, Kategorie, Voraussetzungen, Fertigkeiten, Attributmodifikationen), Damage (Schadensart und -menge) und Interaction.
// Nur Lesen, aendert nichts im Spiel. Die Werte sind der Text, den die Engine fuer die Eigenschaft liefert (Verweise auf Vorlagen sind nicht immer aussagekraeftig).
// Abschalten: "-property_inspect" in g3asm\mods\load_order.txt.
const int TASTE_INSPEKT = 0x6F;   // Ziffernblock /, zusammen mit Strg
const int VK_STRG = 0x11;

// Eine ';'-getrennte Liste in Zeilen von etwa 350 Zeichen ausgeben
void Zeilen(const bCString &in titel, const bCString &in liste) {
    if (liste.IsEmpty()) {
        print(titel + ": (nicht lesbar oder leer)");
        return;
    }
    int n = 1;
    int p = liste.Find(";", 0);
    while (p >= 0) {
        n++;
        p = liste.Find(";", p + 1);
    }
    print(titel + ": " + n + " Eigenschaften");
    int len = liste.GetLength();
    int start = 0;
    while (start < len) {
        int e = liste.Find(";", start + 350);
        if (e < 0 || e > len) e = len;
        print("  " + liste.Mid(start, e - start));
        start = e + 1;
    }
}

Entity Ziel(Entity hero) {
    Entity e = hero.GetSlotItem(SLOT_RIGHT_HAND);
    if (e.IsValid()) return e;
    e = hero.GetSlotItem(SLOT_LEFT_HAND);
    if (e.IsValid()) return e;
    e = hero.GetSlotItem(SLOT_BACK_RIGHT);
    if (e.IsValid()) return e;
    e = hero.GetSlotItem(SLOT_BACK_LEFT);
    if (e.IsValid()) return e;
    return GetCrosshairEntity();
}

void OnKey(int vk, bool down) {
    if (!down || vk != TASTE_INSPEKT || !IsKeyDown(VK_STRG)) return;
    Entity hero = GetHero();
    if (!hero.IsValid()) return;
    Entity e = Ziel(hero);
    if (!e.IsValid()) {
        PrintScreen("Eigenschaften: kein Item in der Hand und nichts unter dem Fadenkreuz", 2.0f, 255, 80, 80);
        return;
    }
    print("=== Eigenschaften von '" + e.GetName() + "' (Vorlage " + e.GetTemplateName() + ") ===");
    Zeilen("Item", ListProperties(e, "Item", 400));
    Zeilen("Damage", ListProperties(e, "Damage", 400));
    Zeilen("Interaction", ListProperties(e, "Interaction", 400));
    print("=== Ende ===");
    PrintScreen("Eigenschaften von " + e.GetName() + " im Protokoll", 2.0f, 80, 255, 80);
}
