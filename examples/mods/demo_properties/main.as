// Property inspection (from API 29): Ctrl + numeric keypad / writes all properties of an item with their values to the log (g3asm.log, lines "[demo_properties] ...").
// Target: the item in the right hand, if there is none in the left hand or on the back, if there is none the entity below the crosshair (for example an item on the ground or an NPC).
// The mod reads the property sets Item (value, category, requirements, skills, attribute changes), Damage (damage type and quantity) and Interaction.
// Read only, it changes nothing in the game. The values are the text that the engine gives for the property (references to templates do not always give useful data).
// To turn off: "-demo_properties" in g3asm\mods\load_order.txt.
const int TASTE_INSPEKT = 0x6F;   // numeric keypad /, together with Ctrl
const int VK_STRG = 0x11;

// write a list separated by ';' in lines of approximately 350 characters
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
