// Referenz-Werkzeug (ab API 28): schreibt auf Tastendruck (Ziffernblock /) geprueft aus dem Spiel gelesene Listen ins Protokoll (g3asm.log, Zeilen "[reference_dump] ..."):
//   - die Attribut-Kennungen, die Quests des Spiels vergeben, und den Zustand des Helden (Wert/Grundwert/Maximum) fuer bekannte Kennungen
//   - alle Vorlagen (Items, Geschosse, Zauber-Items, ...) nach Namensgruppen, mit Anzahl; grosse Gruppen werden gekuerzt
//   - jeder zweite Druck (2., 4., ...): die GANZE Vorlagenliste (alle Namen) fuer docs/modkit/data/templates.txt; erzeugt tools/reference_from_log.py
// Die Auswertung steht in docs/modkit/reference.md. Der Dump aendert nichts im Spiel. Abschalten: "-reference_dump" in g3asm\mods\load_order.txt.
const int TASTE_DUMP = 0x6F;   // Ziffernblock /
// Die Suche ignoriert Gross-/Kleinschreibung (gemessen): nur Grossbuchstaben abfragen, sonst verdoppelt sich die Arbeit mit jeder Ebene!
const bCString ANFAENGE = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
int schritte = 0;             // Sicherung: hoechstens SCHRITTE_MAX Abfragen je Dump
const int SCHRITTE_MAX = 4000;
bool vollAktiv = false;
bCString warteschlange;       // noch abzufragende Namensanfaenge, ';'-getrennt
const int GRUPPE_MAX = 120;   // so viele Eintraege (mit Mehrfachnennungen) pro Abfrage und Bild (etwa 0,4 s); groessere Gruppen werden weiter aufgeteilt
const int TIEFE_MAX = 30;     // laengere Namensanfaenge werden nicht weiter aufgeteilt
int druecke = 0;              // Zaehler: ungerade = Kurzdump, gerade = alle Vorlagen
const int GRUPPE_VOLL = 400;   // bis zu dieser Anzahl werden alle Namen ausgegeben, sonst nur der Anfang

// Eine ';'-getrennte Liste in Zeilen von etwa 350 Zeichen ausgeben
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

// Vollstaendiger Dump ueber mehrere Bilder: je Bild ein Namensanfang (der Schleifenschutz bricht Skripte nach 5 Sekunden ab)
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
        // zu gross: jeden moeglichen naechsten Buchstaben einzeln abfragen; Summenkontrolle, falls Namen einen anderen zweiten Buchstaben haben
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
    if (vk == TASTE_DUMP && (IsKeyDown(0x11) || IsKeyDown(0x10))) return;   // Strg + / gehoert property_inspect, Umschalt + / event_probe
    if (down && vk == TASTE_DUMP && vollAktiv) {   // laeuft der vollstaendige Dump, bricht die Taste ihn ab
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
        // FindTemplateNames sucht nach dem NAMENSANFANG: jeden Anfangsbuchstaben abfragen; zu grosse Gruppen werden in OnFrame mit dem naechsten Buchstaben weiter aufgeteilt
        warteschlange = "";
        int gesamt = 0;
        for (int i = 0; i < ANFAENGE.GetLength(); i++) {
            int k = CountTemplateNames(ANFAENGE.Mid(i, 1));
            if (k > 0) {
                gesamt += k;
                warteschlange += ANFAENGE.Mid(i, 1) + ";";
            }
        }
        // Schaetzung: etwa 3 ms je Eintrag, das Spiel ruckelt waehrend des Dumps. Erneutes Druecken der Taste bricht ab.
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
    Gruppe("It_Am_");        // Amulette, z. B. It_Am_HP_01 (Lebenskraft), It_Am_Hunter_01 (Jaeger) laut World of Gothic
    Gruppe("It_Ri_");        // Ringe (zum Vergleich mit It_Ring)
    Gruppe("It_Artefact");   // Artefakte, z. B. It_Artefact_Amulet (Amulett der Goettlichkeit, laut Gothic-Wiki)
    Gruppe("It_Belt");
    Gruppe("It_Key");
    Gruppe("It_Book");
    Gruppe("It_Rune");
    Gruppe("It_Recipe");
    Gruppe("It_Gold");
    print("=== Ende des Dumps ===");
}
