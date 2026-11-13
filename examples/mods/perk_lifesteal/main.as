// Perk-Experiment (ab API 30): Ein eigenes Talent "Lebensraub" als Bonustalent des Orkschlaechters.
// Hintergrund: Talente stehen im Inventar des Helden (gelernt) und haben dort einen Aktivierungszaehler; eine Waffe mit ActivateSkill1/2 zaehlt ihn hoch, solange sie gefuehrt wird.
// ActivateSkill2 der Waffenvorlage laesst sich nicht per Text setzen (Referenz, siehe limitations.md), deshalb schaltet diese Mod das Talent selbst:
// Wird PERK_WAFFE in der rechten Hand gefuehrt, bekommt der Held Perk_Lifesteal (Entity.LearnSkill + ActivateSkill), sonst wird es wieder deaktiviert.
// Perk_Lifesteal ist ein Klon von Perk_OrcSlayer (CloneTemplate) mit Tag "Lifesteal"; der Name kommt aus stringtable.ini (FO_It_Perk_Lifesteal).
// Zusaetzlich misst die Mod den Zustand echter Talente (Schwertkaempfer 1 = Perk_1H_1, Orktoeter) mit und ohne gezogene Waffe ("[perk_lifesteal] Messung ...").
const bCString BASIS = "Perk_OrcSlayer";
const bCString NEU = "Perk_Lifesteal";
const bCString WAFFE = "It_1H_OrcSlayer_01";
const bCString LERNITEM_BASIS = "It_Perk_Orcslayer";   // Lern-Item des Orktoeters (Name genau so geschrieben)
const bCString LERNITEM = "It_Perk_Lifesteal";
const int LEBENSRAUB_PROZENT = 20;
int heilLog = 0;
float putzZeit = 0;
// ENTFERNEN = true: Abschaltmodus. Nimmt den Lern-Item-Stapel und das Talent wieder aus dem Inventar des Helden. Danach speichern, DANN die Mod abschalten (load_order.txt: -perk_lifesteal).
// Warum: Der Stapel steht im Spielstand. Fehlt die Mod, fehlt die Vorlage It_Perk_Lifesteal, und das Spiel stuerzt beim Oeffnen des Charakterbogens ab (gemessen 2026-11-06).
const bool ENTFERNEN = false;
bool entfernt = false;
bool refGesetzt = false;
bool stapelGeprueft = false;
bool manuell = false;   // Die Mod hat das Talent selbst hochgezaehlt (Nachaktivierung): beim Ablegen auch selbst zuruecknehmen, die Engine kennt diese Zaehlung nicht
bool fertig = false;
bool aktiv = false;
float zeit = 0;
int messLog = 0;
bCString letzteMessung = "";

void Versuch() {
    if (fertig) return;
    if (!GetTemplateEntity(BASIS).IsValid()) return;   // Vorlagen noch nicht bereit (Hauptmenue)
    int rc = CloneTemplate(BASIS, NEU);
    if (rc < 0) return;
    fertig = true;
    Entity neu = GetTemplateEntity(NEU);
    bool ok = neu.IsValid() && SetProperty(neu, "gCSkill_PS", "Tag", "Lifesteal");
    print("Talent " + NEU + " angelegt: rc=" + rc + ", Tag gesetzt " + ok);

    // Lern-Item klonen (Icon/Kachel im Charakterbogen) und dessen Skill-Verweis auf das neue Talent setzen
    int rci = CloneTemplate(LERNITEM_BASIS, LERNITEM);
    Entity li = GetTemplateEntity(LERNITEM);
    bool r1 = li.IsValid() && SetTemplateRef(li, "gCItem_PS", "Skill", NEU);
    print("Lern-Item " + LERNITEM + ": rc=" + rci + ", Skill-Verweis gesetzt " + r1 + ", gelesen: " + (li.IsValid() ? GetProperty(li, "Item", "Skill") : bCString("-")));

    // Bonustalent der Waffe: ActivateSkill2 auf das neue Talent
    Entity w = GetTemplateEntity(WAFFE);
    bool r2 = w.IsValid() && SetTemplateRef(w, "gCItem_PS", "ActivateSkill2", NEU);
    refGesetzt = r2 && GetProperty(w, "Item", "ActivateSkill2").Find(NEU, 0) >= 0;
    print("Waffe " + WAFFE + ": ActivateSkill2 gesetzt " + r2 + ", gelesen: " + (w.IsValid() ? GetProperty(w, "Item", "ActivateSkill2") : bCString("-")));
}

// Eine ';'-getrennte Liste in Zeilen von etwa 350 Zeichen ausgeben
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
// Nach jedem Laden neu pruefen: Der Stapel steht (wegen OnBeforeSave) nicht im Spielstand und muss wieder angelegt werden, sonst fehlen Kachel und Talent
// bis zum naechsten Speichern (gemessen 2026-11-13). Die Nachaktivierung des alten Helden gilt fuer den neu geladenen nicht.
void OnWorldActivate() {
    Versuch();
    ladeZaehler++;
    stapelGeprueft = false;
    manuell = false;
}

// Die frueheren Qualitaetszeile "(20 %) Lebensraub" (Bit QUALITY_LIFESTEAL aus lifesteal_demo) steckt im Spielstand an der Waffe: hier einmal pro Sekunde entfernen,
// damit nur noch das Bonustalent an der Waffe steht.
void Aufraeumen(Entity hero, int slot) {
    uint q = hero.GetSlotQuality(slot);
    if ((q & QUALITY_LIFESTEAL) == 0) return;
    if (hero.SetSlotQuality(slot, q & ~QUALITY_LIFESTEAL)) print("Qualitaet Lebensraub entfernt (Slot " + slot + ")");
}

// Ausgeruestet = in der rechten Hand oder auf dem Ruecken; das ist die Bedingung, unter der auch das eingebaute Bonustalent (Perk_OrcSlayer) aktiv ist
bool WaffeAusgeruestet(Entity hero) {
    Entity a = hero.GetSlotItem(SLOT_RIGHT_HAND);
    if (a.IsValid() && a.GetName() == WAFFE) return true;
    Entity b = hero.GetSlotItem(SLOT_BACK_RIGHT);
    if (b.IsValid() && b.GetName() == WAFFE) return true;
    Entity c = hero.GetSlotItem(SLOT_BACK_LEFT);
    return c.IsValid() && c.GetName() == WAFFE;
}

// Wirkung: Der Held heilt sich um LEBENSRAUB_PROZENT des angerichteten Schadens, solange der Orkschlaechter ausgeruestet ist.
// (Das Talent Perk_Lifesteal selbst laesst sich nicht aktivieren: LearnSkill/ActivateSkill liefern false, gemessen 2026-11-06.)
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

// Messung fuer die Kachel im Charakterbogen: Hat der Held Stapel fuer Lern-Items (auch fuer nicht gelernte Talente)? Fehlt unser Item, einmal anlegen und beobachten.
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
        // Die Engine zaehlt Bonustalente nur beim Ausruesten. War die Waffe schon ausgeruestet, als der Stapel entstand, einmal nachholen
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
    int r = n > 0 ? hero.DeleteItemStack(LERNITEM) : 0;   // RemoveItems laesst bei dauerhaften Items einen leeren Stapel stehen
    entfernt = true;
    print("Abschalten: Stapel " + LERNITEM + " entfernt: vorher " + n + ", Stapel geloescht=" + r + ", jetzt " + hero.GetItemCount(LERNITEM) + " | " + Zustand(hero, NEU));
    PrintScreen("Lebensraub entfernt: jetzt speichern, dann die Mod abschalten", 8.0f, 255, 220, 80);
}

// Speichern (ab API 31): Der Lern-Item-Stapel steht sonst im Spielstand und kann dort mit fremden Mods oder ohne diese Mod den Charakterbogen zum Absturz bringen.
// Darum vor dem Schreiben Talent deaktivieren und Stapel loeschen, danach (auch bei Fehlschlag) wieder anlegen und, wenn die Waffe ausgeruestet ist, neu aktivieren.
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
    stapelGeprueft = false;   // StapelPruefen legt den Stapel wieder an und holt die Aktivierung nach
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

    // Messung: bei jeder Aenderung eine Zeile, hoechstens 60 Zeilen
    bCString m = "Orktoeter " + (halten ? "aktiv" : "nicht aktiv") + " | " + Zustand(hero, "Perk_1H_1") + " | " + Zustand(hero, BASIS) + " | " + Zustand(hero, NEU);
    if (m != letzteMessung && messLog < 60) {
        messLog++;
        letzteMessung = m;
        print("Messung: " + m);
    }
}
