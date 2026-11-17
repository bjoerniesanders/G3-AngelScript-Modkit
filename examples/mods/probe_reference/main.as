// Reference tool (from API 28): when you push a key (numeric keypad /), writes lists that it read from the game to the log (g3asm.log, lines "[probe_reference] ..."):
//   - the attribute tags that quests of the game give, and the condition of the hero (value, base value, maximum) for known tags
//   - all templates (items, projectiles, spell items, ...) by name group, with the count; large groups are shortened
//   - each second push (2nd, 4th, ...): the FULL template list (all names) for docs/modkit/data/templates.txt; tools/reference_from_log.py makes it
// docs/modkit/reference.md contains the evaluation. The dump changes nothing in the game. To turn off: "-probe_reference" in g3asm\mods\load_order.txt.
const int TASTE_DUMP = 0x6F;   // numeric keypad /
// The search ignores upper case and lower case (measured): ask only for upper-case letters, if not the work doubles with each level!
const bCString ANFAENGE = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
int schritte = 0;             // protection: maximum SCHRITTE_MAX queries for each dump
const int SCHRITTE_MAX = 4000;
bool vollAktiv = false;
bCString warteschlange;       // name starts that are not queried yet, separated by ';'
const int GRUPPE_MAX = 120;   // this number of entries (with duplicates) for each query and frame (approximately 0.4 s); larger groups are divided more
const int TIEFE_MAX = 30;     // longer name starts are not divided more
int druecke = 0;              // counter: odd = short dump, even = all templates
const int GRUPPE_VOLL = 400;   // up to this number, all names are written, if not only the start

// write a list separated by ';' in lines of approximately 350 characters
void Zeilen(const bCString &in titel, const bCString &in liste, bool kuerzen = true) {
    if (liste.IsEmpty()) {
        print(titel + ": (keine)");
        return;
    }
    int n = 1;
    int p = liste.Find(";", 0);
    while (p >= 0) {
        n++;
        p = liste.Find(";", p + 1);
    }
    print(titel + ": " + n + " Eintraege" + (kuerzen && n > GRUPPE_VOLL ? " (nur der Anfang)" : ""));
    int len = liste.GetLength();
    if (kuerzen && n > GRUPPE_VOLL && len > 3000) len = 3000;
    int start = 0;
    while (start < len) {
        int e = liste.Find(";", start + 350);
        if (e < 0 || e > len) e = len;
        print("  " + liste.Mid(start, e - start));
        start = e + 1;
    }
}

void Gruppe(const bCString &in teil) {
    Zeilen("Vorlagen mit '" + teil + "'", FindTemplateNames(teil, 5000));
}

void HeldenWerte(Entity hero, const bCString &in tag) {
    print("  Held " + tag + ": Wert " + hero.GetAttribute(tag) + ", Grundwert " + hero.GetAttributeBase(tag) + ", Maximum " + hero.GetAttributeMax(tag));
}

// Full dump across many frames: one name start for each frame (the loop protection stops scripts after 5 seconds)
void OnFrame(float dt) {
    if (!vollAktiv) return;
    if (warteschlange.IsEmpty()) {
        vollAktiv = false;
        print("=== Ende der Vorlagen ===");
        return;
    }
    schritte++;
    if (schritte > SCHRITTE_MAX) {
        vollAktiv = false;
        warteschlange = "";
        print("=== Dump abgebrochen: mehr als " + SCHRITTE_MAX + " Abfragen (Sicherung) ===");
        return;
    }
    if ((schritte % 50) == 0) print("Fortschritt: " + schritte + " Abfragen, " + warteschlange.GetLength() + " Zeichen in der Warteschlange");
    int p = warteschlange.Find(";", 0);
    bCString pre = warteschlange.Left(p);
    warteschlange = warteschlange.Mid(p + 1, warteschlange.GetLength() - p - 1);
    int n = CountTemplateNames(pre);
    if (n <= 0) return;
    if (n <= GRUPPE_MAX || pre.GetLength() >= TIEFE_MAX) {
        Zeilen("Alle Vorlagen mit '" + pre + "'", FindTemplateNames(pre, 20000), false);
    } else {
        // too large: query each possible next letter separately; sum check, in case names have a different second letter
        bCString neue = "";
        int summe = 0;
        for (int i = 0; i < ANFAENGE.GetLength(); i++) {
            bCString kind = pre + ANFAENGE.Mid(i, 1);
            int k = CountTemplateNames(kind);
            if (k > 0) {
                summe += k;
                neue += kind + ";";
            }
        }
        if (summe < n) print("Hinweis: " + (n - summe) + " Eintraege mit Anfang '" + pre + "' haben ein anderes Zeichen danach und fehlen im Dump");
        warteschlange = neue + warteschlange;
    }
}

void OnKey(int vk, bool down) {
    if (vk == TASTE_DUMP && (IsKeyDown(0x11) || IsKeyDown(0x10))) return;   // Ctrl + / belongs to demo_properties, Shift + / to probe_events
    if (down && vk == TASTE_DUMP && vollAktiv) {   // if the full dump runs, the key stops it
        vollAktiv = false;
        warteschlange = "";
        druecke = 0;
        print("=== Dump abgebrochen (Taste) ===");
        return;
    }
    if (down && vk == TASTE_DUMP && (druecke % 2) == 1) {
        druecke++;
        print("=== Alle Vorlagen ===");
        schritte = 0;
        // FindTemplateNames searches by the START OF THE NAME: query each first letter. OnFrame divides groups that are too large with the next letter
        warteschlange = "";
        int gesamt = 0;
        for (int i = 0; i < ANFAENGE.GetLength(); i++) {
            int k = CountTemplateNames(ANFAENGE.Mid(i, 1));
            if (k > 0) {
                gesamt += k;
                warteschlange += ANFAENGE.Mid(i, 1) + ";";
            }
        }
        // estimate: approximately 3 ms for each entry, the game stutters during the dump. Push the key again to stop.
        print("Vorlagen insgesamt (mit Mehrfachnennungen): " + gesamt + ", geschaetzte Dauer etwa " + (gesamt * 3 / 1000) + " Sekunden. Zum Abbrechen die Taste erneut druecken.");
        vollAktiv = gesamt > 0;
        return;
    }
    if (!down || vk != TASTE_DUMP) return;
    druecke++;
    Entity hero = GetHero();
    print("=== Referenz-Dump (API " + GetApiVersion() + ") ===");
    print("Attribut-Kennungen, die Quests vergeben:");
    print(DescribeQuestRewards());
    if (hero.IsValid()) {
        print("Attribute des Helden (-1 = Kennung unbekannt):");
        HeldenWerte(hero, "STR");
        HeldenWerte(hero, "DEX");
        HeldenWerte(hero, "INT");
        HeldenWerte(hero, "HP");
        HeldenWerte(hero, "MP");
        HeldenWerte(hero, "SP");
        HeldenWerte(hero, "XP");
        HeldenWerte(hero, "LP");
        HeldenWerte(hero, "ALC");
        HeldenWerte(hero, "SMI");
        HeldenWerte(hero, "THF");
        print("Statuseffekte des Helden: Maske " + hero.GetStatusEffects());
    }
    Gruppe("It_1H_");
    Gruppe("It_2H_");
    Gruppe("It_Staff_");
    Gruppe("It_Bow_");
    Gruppe("It_Crossbow");
    Gruppe("Arrow");
    Gruppe("Bolt");
    Gruppe("It_Armor_");
    Gruppe("It_Helmet");
    Gruppe("It_Shield");
    Gruppe("It_Spell");
    Gruppe("Mis_");
    Gruppe("Spl_");
    Gruppe("Perk");
    Gruppe("It_Scroll");
    Gruppe("Teleport");
    Gruppe("It_Potion");
    Gruppe("It_Plant_");
    Gruppe("It_Ring");
    Gruppe("It_Am_");        // amulets, for example It_Am_HP_01 (life force), It_Am_Hunter_01 (hunter) in accordance with World of Gothic
    Gruppe("It_Ri_");        // rings (to compare with It_Ring)
    Gruppe("It_Artefact");   // artifacts, for example It_Artefact_Amulet (amulet of divinity, in accordance with the Gothic wiki)
    Gruppe("It_Belt");
    Gruppe("It_Key");
    Gruppe("It_Book");
    Gruppe("It_Rune");
    Gruppe("It_Recipe");
    Gruppe("It_Gold");
    print("=== Ende des Dumps ===");
}
