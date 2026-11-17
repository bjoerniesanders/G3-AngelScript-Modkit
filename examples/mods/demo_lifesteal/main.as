// Life steal demo (from API 28): "Lebensraub" (life steal) is a property of the weapon, as "geschaerft" (sharpened) or "gesegnet" (blessed): a quality bit (QUALITY_LIFESTEAL) that shows in the description of the weapon as
// the line "(20 %) Lebensraub" (text from stringtable.ini, key HUD_Quality_Lifesteal). The game saves it with the saved game.
// Key numeric keypad * (multiply): sets or removes the property on the weapon of the hero (right hand, if empty the right or left back slot). If the hero hits an NPC with such a weapon in the hand,
// the hero gets LEBENSRAUB_PROZENT of the damage back as hit points (up to the maximum). Damage = quantity from OnDamage. For a fatal hit,
// only the remaining hit points of the victim.
// Perk of the weapon: with AUTO_PERK, the weapon PERK_WAFFE (Orkschlaechter) gets the property automatically when it is in the hand or on the back, as a built-in property
// of the weapon (the key then changes only other weapons; on this weapon, the script sets the property again immediately).
// Shows: owner and inflictor in OnDamage, read and set the quality for each slot, a custom text through the mod string table. To turn off: "-demo_lifesteal" in g3asm\mods\load_order.txt.
const int LEBENSRAUB_PROZENT = 20;
const int TASTE_LEBENSRAUB = 0x6A;   // numeric keypad * (End, 0x23, did not get to the script in a test)

const bool AUTO_PERK = false;   // off: the life steal is now on the bonus skill (mod perk_lifesteal), not on the quality bit
const bCString PERK_WAFFE = "It_1H_OrcSlayer_01";
float perkZeit = 0;
int heilLog = 0;

// Sets the property on the weapon in the slot if PERK_WAFFE is there. true if something changed
bool PerkSetzen(Entity hero, int slot) {
    Entity item = hero.GetSlotItem(slot);
    if (!item.IsValid() || item.GetName() != PERK_WAFFE) return false;
    uint q = hero.GetSlotQuality(slot);
    if ((q & QUALITY_LIFESTEAL) != 0) return false;
    if (!hero.SetSlotQuality(slot, q | QUALITY_LIFESTEAL)) return false;
    print("Perk Lebensraub gesetzt an " + PERK_WAFFE + " (Slot " + slot + ")");
    return true;
}

// Check 1 time each second (draw or put away the weapon changes the slot)
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

// Is there a stack in the slot? (SetSlotQuality with the same value is successful only in this case)
bool SlotBelegt(Entity hero, int slot) {
    return hero.SetSlotQuality(slot, hero.GetSlotQuality(slot));
}

// Toggle the property on the weapon in the slot. false if the slot is empty
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
    if (!down || vk != TASTE_LEBENSRAUB || IsKeyDown(0x11)) return;   // Ctrl + numeric keypad * belongs to probe_events
    Entity hero = GetHero();
    if (!hero.IsValid()) return;
    if (Umschalten(hero, SLOT_RIGHT_HAND, "rechte Hand")) return;
    if (Umschalten(hero, SLOT_BACK_RIGHT, "Ruecken rechts")) return;
    if (Umschalten(hero, SLOT_BACK_LEFT, "Ruecken links")) return;
    PrintScreen("Lebensraub: keine Waffe gefunden", 2.0f, 255, 80, 80);
}

void OnDamage(Entity victim, Entity inflictor, Entity owner, int amount, int type) {
    Entity hero = GetHero();
    if (!hero.IsValid() || !owner.IsValid() || owner != hero) return;   // only hits that the hero causes
    if (victim == hero) return;                                         // not for damage to the hero
    if ((hero.GetSlotQuality(SLOT_RIGHT_HAND) & QUALITY_LIFESTEAL) == 0) return;   // the weapon in use does not have the property

    int heal = amount * LEBENSRAUB_PROZENT / 100;
    if (heal <= 0) return;
    int fehlt = hero.GetAttributeMax("HP") - hero.GetAttribute("HP");   // do not heal above the maximum
    if (fehlt <= 0) return;
    if (heal > fehlt) heal = fehlt;

    hero.AddAttribute("HP", heal);
    PrintScreen("Lebensraub: +" + heal + " HP", 1.5f, 80, 255, 80);
    if (heilLog < 100) {
        heilLog++;
        print("Lebensraub: " + victim.GetName() + " verlor " + amount + " HP, Held +" + heal + " (Typ " + type + ", Waffe " + inflictor.GetName() + ")");
    }
}
