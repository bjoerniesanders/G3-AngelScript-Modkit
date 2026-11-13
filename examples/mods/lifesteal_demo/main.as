// Lebensraub-Demo (ab API 28): "Lebensraub" ist eine Eigenschaft der Waffe, wie "geschaerft" oder "gesegnet": ein Qualitaetsbit (QUALITY_LIFESTEAL), das in der Beschreibung der Waffe als
// Zeile "(20 %) Lebensraub" erscheint (Text aus stringtable.ini, Schluessel HUD_Quality_Lifesteal) und mit dem Spielstand gespeichert wird.
// Taste Ziffernblock * (Mal): setzt bzw. entfernt die Eigenschaft an der Waffe des Helden (rechte Hand, sonst am Ruecken rechts bzw. links). Trifft der Held mit einer solchen Waffe in der Hand einen NPC,
// bekommt er LEBENSRAUB_PROZENT des angerichteten Schadens als Lebenspunkte zurueck (bis zum Maximum). Angerichteter Schaden = Menge von OnDamage, bei einem toedlichen Treffer
// nur der Rest der Lebenspunkte des Opfers.
// Perk der Waffe: mit AUTO_PERK bekommt die Waffe PERK_WAFFE (Orkschlaechter) die Eigenschaft von selbst, sobald sie in der Hand oder auf dem Ruecken liegt, wie eine eingebaute Eigenschaft
// der Waffe (die Taste schaltet dann nur noch andere Waffen um; an dieser Waffe setzt das Skript die Eigenschaft sofort wieder).
// Zeigt: owner/inflictor in OnDamage, Qualitaet pro Slot lesen/setzen, eigener Text ueber die Mod-Stringtabelle. Abschalten: "-lifesteal_demo" in g3asm\mods\load_order.txt.
const int LEBENSRAUB_PROZENT = 20;
const int TASTE_LEBENSRAUB = 0x6A;   // Ziffernblock * (Ende, 0x23, kam bei einem Test nicht an)

const bool AUTO_PERK = false;   // aus: der Lebensraub haengt jetzt am Bonustalent (Mod perk_lifesteal), nicht mehr am Qualitaetsbit
const bCString PERK_WAFFE = "It_1H_OrcSlayer_01";
float perkZeit = 0;
int heilLog = 0;

// Setzt die Eigenschaft an der Waffe im Slot, wenn dort PERK_WAFFE liegt; true, wenn etwas geaendert wurde
bool PerkSetzen(Entity hero, int slot) {
    Entity item = hero.GetSlotItem(slot);
    if (!item.IsValid() || item.GetName() != PERK_WAFFE) return false;
    uint q = hero.GetSlotQuality(slot);
    if ((q & QUALITY_LIFESTEAL) != 0) return false;
    if (!hero.SetSlotQuality(slot, q | QUALITY_LIFESTEAL)) return false;
    print("Perk Lebensraub gesetzt an " + PERK_WAFFE + " (Slot " + slot + ")");
    return true;
}

// Einmal pro Sekunde pruefen (Waffe ziehen oder wegstecken wechselt den Slot)
void OnFrame(float dt) {
    if (!AUTO_PERK) return;
    perkZeit += dt;
    if (perkZeit < 1.0f) return;
    perkZeit = 0;
    Entity hero = GetHero();
    if (!hero.IsValid()) return;
    PerkSetzen(hero, SLOT_RIGHT_HAND);
    PerkSetzen(hero, SLOT_BACK_RIGHT);
    PerkSetzen(hero, SLOT_BACK_LEFT);
}

// Liegt im Slot ueberhaupt ein Stapel? (SetSlotQuality mit dem unveraenderten Wert gelingt nur dann)
bool SlotBelegt(Entity hero, int slot) {
    return hero.SetSlotQuality(slot, hero.GetSlotQuality(slot));
}

// Eigenschaft an der Waffe im Slot umschalten; false, wenn der Slot leer ist
bool Umschalten(Entity hero, int slot, const bCString &in name) {
    if (!SlotBelegt(hero, slot)) return false;
    uint q = hero.GetSlotQuality(slot);
    uint neu = q ^ QUALITY_LIFESTEAL;
    if (!hero.SetSlotQuality(slot, neu)) return false;
    bool an = (neu & QUALITY_LIFESTEAL) != 0;
    PrintScreen(an ? "Lebensraub: an (" + name + ")" : "Lebensraub: aus (" + name + ")", 2.0f, 80, 255, 80);
    print("Lebensraub " + (an ? "gesetzt" : "entfernt") + " (" + name + ", Qualitaet " + q + " -> " + neu + ")");
    return true;
}

void OnKey(int vk, bool down) {
    if (!down || vk != TASTE_LEBENSRAUB || IsKeyDown(0x11)) return;   // Strg + Ziffernblock * gehoert event_probe
    Entity hero = GetHero();
    if (!hero.IsValid()) return;
    if (Umschalten(hero, SLOT_RIGHT_HAND, "rechte Hand")) return;
    if (Umschalten(hero, SLOT_BACK_RIGHT, "Ruecken rechts")) return;
    if (Umschalten(hero, SLOT_BACK_LEFT, "Ruecken links")) return;
    PrintScreen("Lebensraub: keine Waffe gefunden", 2.0f, 255, 80, 80);
}

void OnDamage(Entity victim, Entity inflictor, Entity owner, int amount, int type) {
    Entity hero = GetHero();
    if (!hero.IsValid() || !owner.IsValid() || owner != hero) return;   // nur Treffer, die der Held verursacht
    if (victim == hero) return;                                         // nicht bei Schaden am Helden selbst
    if ((hero.GetSlotQuality(SLOT_RIGHT_HAND) & QUALITY_LIFESTEAL) == 0) return;   // die gefuehrte Waffe hat die Eigenschaft nicht

    int heal = amount * LEBENSRAUB_PROZENT / 100;
    if (heal <= 0) return;
    int fehlt = hero.GetAttributeMax("HP") - hero.GetAttribute("HP");   // nicht ueber das Maximum heilen
    if (fehlt <= 0) return;
    if (heal > fehlt) heal = fehlt;

    hero.AddAttribute("HP", heal);
    PrintScreen("Lebensraub: +" + heal + " HP", 1.5f, 80, 255, 80);
    if (heilLog < 100) {
        heilLog++;
        print("Lebensraub: " + victim.GetName() + " verlor " + amount + " HP, Held +" + heal + " (Typ " + type + ", Waffe " + inflictor.GetName() + ")");
    }
}
