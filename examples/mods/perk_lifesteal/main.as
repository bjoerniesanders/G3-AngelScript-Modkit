// Perk experiment (from API 30): a custom skill "Lebensraub" (life steal) as the bonus skill of the Orkschlaechter.
// Background: skills are in the inventory of the hero (learned) and have an activation counter there. A weapon with ActivateSkill1/2 increases it while the hero uses the weapon.
// ActivateSkill2 of the weapon template cannot be set as text (reference, see limitations.md), thus this mod turns the skill on itself:
// if PERK_WAFFE is in the right hand, the hero gets Perk_Lifesteal (Entity.LearnSkill + ActivateSkill). If not, the mod turns it off again.
// Perk_Lifesteal is a clone of Perk_OrcSlayer (CloneTemplate) with the tag "Lifesteal". The name comes from stringtable.ini (FO_It_Perk_Lifesteal).
// The mod also measures the condition of real skills (Schwertkaempfer 1 = Perk_1H_1, Orktoeter) with and without a drawn weapon ("[perk_lifesteal] Messung ...").
const bCString BASIS = "Perk_OrcSlayer";
const bCString NEU = "Perk_Lifesteal";
const bCString WAFFE = "It_1H_OrcSlayer_01";
const bCString LERNITEM_BASIS = "It_Perk_Orcslayer";   // learn item of the Orktoeter (name written exactly so)
const bCString LERNITEM = "It_Perk_Lifesteal";
const int LEBENSRAUB_PROZENT = 20;
int heilLog = 0;
float putzZeit = 0;
// ENTFERNEN = true: turn-off mode. Removes the learn item stack and the skill from the inventory of the hero. Then save, THEN turn off the mod (load_order.txt: -perk_lifesteal).
// Why: the stack is in the saved game. If the mod is missing, the template It_Perk_Lifesteal is missing, and the game crashes when you open the character sheet (measured 2026-11-06).
const bool ENTFERNEN = false;
bool entfernt = false;
bool refGesetzt = false;
bool stapelGeprueft = false;
bool manuell = false;   // the mod increased the skill counter itself (late activation): also decrease it itself when the hero puts the weapon away. The engine does not know this count
bool fertig = false;
bool aktiv = false;
float zeit = 0;
int messLog = 0;
bCString letzteMessung = "";

void Versuch() {
    if (fertig) return;
    if (!GetTemplateEntity(BASIS).IsValid()) return;   // templates not ready yet (main menu)
    int rc = CloneTemplate(BASIS, NEU);
    if (rc < 0) return;
    fertig = true;
    Entity neu = GetTemplateEntity(NEU);
    bool ok = neu.IsValid() && SetProperty(neu, "gCSkill_PS", "Tag", "Lifesteal");
    print("Talent " + NEU + " angelegt: rc=" + rc + ", Tag gesetzt " + ok);

    // clone the learn item (icon and tile in the character sheet) and set its skill reference to the new skill
    int rci = CloneTemplate(LERNITEM_BASIS, LERNITEM);
    Entity li = GetTemplateEntity(LERNITEM);
    bool r1 = li.IsValid() && SetTemplateRef(li, "gCItem_PS", "Skill", NEU);
    print("Lern-Item " + LERNITEM + ": rc=" + rci + ", Skill-Verweis gesetzt " + r1 + ", gelesen: " + (li.IsValid() ? GetProperty(li, "Item", "Skill") : bCString("-")));

    // bonus skill of the weapon: ActivateSkill2 to the new skill
    Entity w = GetTemplateEntity(WAFFE);
    bool r2 = w.IsValid() && SetTemplateRef(w, "gCItem_PS", "ActivateSkill2", NEU);
    refGesetzt = r2 && GetProperty(w, "Item", "ActivateSkill2").Find(NEU, 0) >= 0;
    print("Waffe " + WAFFE + ": ActivateSkill2 gesetzt " + r2 + ", gelesen: " + (w.IsValid() ? GetProperty(w, "Item", "ActivateSkill2") : bCString("-")));
}

// write a list separated by ';' in lines of approximately 350 characters
void Zeilen(const bCString &in titel, const bCString &in liste) {
    if (liste.IsEmpty()) { print(titel + ": (nicht lesbar oder leer)"); return; }
    print(titel + ":");
    int len = liste.GetLength();
    int start = 0;
    while (start < len) {
        int e = liste.Find(";", start + 350);
        if (e < 0 || e > len) e = len;
        print("  " + liste.Mid(start, e - start));
        start = e + 1;
    }
}

bCString Zustand(Entity hero, const bCString &in talent) {
    return talent + ": gelernt=" + hero.HasLearnedSkill(talent) + " aktiv=" + hero.IsSkillActive(talent) + " zaehler=" + hero.GetSkillActivationCount(talent);
}

void OnQuestsLoaded() { Versuch(); }
int ladeZaehler = 0;
int gemessen = 0;
// Check again after each load: because of OnBeforeSave, the stack is not in the saved game and must be made again. If not, the tile and the skill are missing
// until the next save (measured 2026-11-13). The late activation of the old hero does not apply to the hero that the game loaded again.
void OnWorldActivate() {
    Versuch();
    ladeZaehler++;
    stapelGeprueft = false;
    manuell = false;
}

// The previous quality line "(20 %) Lebensraub" (bit QUALITY_LIFESTEAL from demo_lifesteal) is on the weapon in the saved game: remove it here 1 time each second,
// so that only the bonus skill is on the weapon.
void Aufraeumen(Entity hero, int slot) {
    uint q = hero.GetSlotQuality(slot);
    if ((q & QUALITY_LIFESTEAL) == 0) return;
    if (hero.SetSlotQuality(slot, q & ~QUALITY_LIFESTEAL)) print("Qualitaet Lebensraub entfernt (Slot " + slot + ")");
}

// equipped = in the right hand or on the back. This is the condition for the built-in bonus skill (Perk_OrcSlayer) too
bool WaffeAusgeruestet(Entity hero) {
    Entity a = hero.GetSlotItem(SLOT_RIGHT_HAND);
    if (a.IsValid() && a.GetName() == WAFFE) return true;
    Entity b = hero.GetSlotItem(SLOT_BACK_RIGHT);
    if (b.IsValid() && b.GetName() == WAFFE) return true;
    Entity c = hero.GetSlotItem(SLOT_BACK_LEFT);
    return c.IsValid() && c.GetName() == WAFFE;
}

// Effect: the hero heals by LEBENSRAUB_PROZENT of the damage while the Orkschlaechter is equipped.
// (The skill Perk_Lifesteal itself cannot be activated: LearnSkill and ActivateSkill give false, measured 2026-11-06.)
void OnDamage(Entity victim, Entity inflictor, Entity owner, int amount, int type) {
    Entity hero = GetHero();
    if (!hero.IsValid() || !owner.IsValid() || owner != hero || victim == hero) return;
    if (!WaffeAusgeruestet(hero)) return;
    int heal = amount * LEBENSRAUB_PROZENT / 100;
    int fehlt = hero.GetAttributeMax("HP") - hero.GetAttribute("HP");
    if (heal <= 0 || fehlt <= 0) return;
    if (heal > fehlt) heal = fehlt;
    hero.AddAttribute("HP", heal);
    PrintScreen("Lebensraub: +" + heal + " HP", 1.5f, 80, 255, 80);
    if (heilLog < 100) {
        heilLog++;
        print("Lebensraub: " + victim.GetName() + " verlor " + amount + " HP, Held +" + heal + " (Typ " + type + ")");
    }
}

// Measurement for the tile in the character sheet: does the hero have stacks for learn items (also for skills that are not learned)? If the item of this mod is missing, make it 1 time and look.
void StapelPruefen(Entity hero) {
    if (stapelGeprueft || !fertig) return;
    stapelGeprueft = true;
    { Entity vt = GetTemplateEntity(NEU); Entity vl = GetTemplateEntity(LERNITEM);
      print("Vorlagen nach Laden: " + NEU + " gueltig=" + vt.IsValid() + ", " + LERNITEM + " gueltig=" + vl.IsValid() + ", Skill=" + (vl.IsValid() ? GetProperty(vl, "Item", "Skill") : bCString("-"))); }
    print("Stapel: It_Perk_1H_1=" + hero.GetItemCount("It_Perk_1H_1") + " It_Perk_Orcslayer=" + hero.GetItemCount("It_Perk_Orcslayer") + " It_Perk_Axe_1=" + hero.GetItemCount("It_Perk_Axe_1")
          + " It_Perk_Bow_1=" + hero.GetItemCount("It_Perk_Bow_1") + " " + LERNITEM + "=" + hero.GetItemCount(LERNITEM));
    if (hero.GetItemCount(LERNITEM) <= 0) {
        int rc = hero.AddItems(LERNITEM, 1);
        print("Stapel angelegt: AddItems(" + LERNITEM + ") rc=" + rc + ", danach Anzahl " + hero.GetItemCount(LERNITEM) + " | " + Zustand(hero, NEU));
        // The engine counts bonus skills only when the hero equips the weapon. If the weapon was already equipped when the stack was made, do it now 1 time
        if (WaffeAusgeruestet(hero) && !hero.IsSkillActive(NEU)) {
            bool a = hero.ActivateSkill(NEU);
            manuell = a;
            print("Nachaktiviert (Waffe schon ausgeruestet): " + a + " | " + Zustand(hero, NEU));
        }
    }
}

// Abschaltmodus: Stapel entfernen, Talent deaktivieren
void Entfernen(Entity hero) {
    if (entfernt) return;
    if (hero.IsSkillActive(NEU)) {
        bool d = hero.DeactivateSkill(NEU);
        print("Abschalten: DeactivateSkill=" + d + " | " + Zustand(hero, NEU));
    }
    int n = hero.GetItemCount(LERNITEM);
    int r = n > 0 ? hero.DeleteItemStack(LERNITEM) : 0;   // RemoveItems leaves an empty stack for permanent items
    entfernt = true;
    print("Abschalten: Stapel " + LERNITEM + " entfernt: vorher " + n + ", Stapel geloescht=" + r + ", jetzt " + hero.GetItemCount(LERNITEM) + " | " + Zustand(hero, NEU));
    PrintScreen("Lebensraub entfernt: jetzt speichern, dann die Mod abschalten", 8.0f, 255, 220, 80);
}

// Save (from API 31): if not removed, the learn item stack is in the saved game. With other mods, or without this mod, it can crash the character sheet.
// Thus, before the write, turn off the skill and delete the stack. After it (also after a failure), make it again and, if the weapon is equipped, turn it on again.
bool stapelEntnommen = false;
void OnBeforeSave(const bCString &in name) {
    if (ENTFERNEN || !fertig) return;
    Entity hero = GetHero();
    if (!hero.IsValid()) return;
    Entity w = GetTemplateEntity(WAFFE);
    print("Speichern '" + name + "': ActivateSkill2 der Waffe = " + (w.IsValid() ? GetProperty(w, "Item", "ActivateSkill2") : bCString("-")) + " | " + Zustand(hero, NEU));
    if (hero.IsSkillActive(NEU)) hero.DeactivateSkill(NEU);
    manuell = false;
    int n = hero.GetItemCount(LERNITEM);
    int r = n > 0 ? hero.DeleteItemStack(LERNITEM) : 0;
    stapelEntnommen = true;
    print("Speichern: Stapel " + LERNITEM + " entnommen: vorher " + n + ", geloescht=" + r + ", jetzt " + hero.GetItemCount(LERNITEM) + " | " + Zustand(hero, NEU));
}
void OnAfterSave(const bCString &in name, int ok) {
    if (!stapelEntnommen) return;
    stapelEntnommen = false;
    Entity hero = GetHero();
    if (!hero.IsValid()) return;
    stapelGeprueft = false;   // StapelPruefen makes the stack again and does the late activation
    StapelPruefen(hero);
    print("Gespeichert '" + name + "' (ok=" + ok + "): Stapel wiederhergestellt: " + hero.GetItemCount(LERNITEM) + " | " + Zustand(hero, NEU));
}
void OnFrame(float dt) {
    zeit += dt;
    if (zeit < 0.5f) return;
    zeit = 0;
    Versuch();
    Entity hero = GetHero();
    if (!hero.IsValid()) return;
    if (ENTFERNEN) {
        if (fertig) Entfernen(hero);
        return;
    }
    if (gemessen != ladeZaehler) {
        gemessen = ladeZaehler;
        Entity vt = GetTemplateEntity(NEU); Entity vl = GetTemplateEntity(LERNITEM);
        print("Ladevorgang #" + gemessen + ": Vorlage " + NEU + " gueltig=" + vt.IsValid() + ", " + LERNITEM + " gueltig=" + vl.IsValid() + ", Skill=" + (vl.IsValid() ? GetProperty(vl, "Item", "Skill") : bCString("-")) + " | Stapel=" + hero.GetItemCount(LERNITEM) + " | " + Zustand(hero, NEU));
    }
    StapelPruefen(hero);
    if (manuell && !WaffeAusgeruestet(hero)) {
        manuell = false;
        if (hero.IsSkillActive(NEU)) {
            bool d = hero.DeactivateSkill(NEU);
            print("Waffe abgelegt: DeactivateSkill=" + d + " | " + Zustand(hero, NEU));
        }
    }
    Aufraeumen(hero, SLOT_RIGHT_HAND);
    Aufraeumen(hero, SLOT_BACK_RIGHT);
    Aufraeumen(hero, SLOT_BACK_LEFT);
    bool halten = hero.IsSkillActive(BASIS);

    // measurement: one line for each change, maximum 60 lines
    bCString m = "Orktoeter " + (halten ? "aktiv" : "nicht aktiv") + " | " + Zustand(hero, "Perk_1H_1") + " | " + Zustand(hero, BASIS) + " | " + Zustand(hero, NEU);
    if (m != letzteMessung && messLog < 60) {
        messLog++;
        letzteMessung = m;
        print("Messung: " + m);
    }
}
