// Blitz-Test, Schritt 1e (fuer "Klaue Beliars"): Strg + Ziffernblock - schlaegt einen Blitz auf den anvisierten NPC.
// Schaden: Beschwoerung Smn_Lightning mit dem Helden als Besitzer (gCInteraction_PS.Owner) - laeuft durch die echte Treffer-Kette (gemessen: Quelle Smn_Lightning, Besitzer PC_Hero, Typ 6, toetet).
// Optik: Die Beschwoerung selbst zeigt keinen Blitz. Jeder Druck probiert die naechste Blitz-Optik der Reihe nach aus (siehe OPTIK_*): ein benannter Effekt (StartEffect) oder die Effekt-Vorlage (SpawnNPC).
// Sicherungen: nur NPCs als Ziel, mindestens WARTE Sekunden zwischen zwei Blitzen. Abschalten: "-probe_lightning" in g3asm\mods\load_order.txt.
const int TASTE = 0x6D;          // Ziffernblock -, zusammen mit Strg (ohne Strg gehoert die Taste demo_spawn)
const float WARTE = 3.0f;        // Sekunden zwischen zwei Blitzen
const bCString SCHADEN_VORLAGE = "Smn_Lightning";

int druck = 0;
float sperre = 0;
float fenster = -1;
bool gemeldet = false;

bCString Optik(int i) {
    if (i == 0) return "eff_magic_summonlightning_target_01";   // Zieleffekt des Zaubers "Blitz beschwoeren"
    if (i == 1) return "eff_magic_lightningbolt_projectile_01"; // Geschosseffekt des Zaubers "Blitzschlag"
    return "VFX_Magic_Electricity_LightningBolt_TargetBurst_01"; // Effekt-Vorlage (bisher sichtbar, nur Optik)
}

void OnKey(int vk, bool down) {
    if (!down || vk != TASTE || !IsKeyDown(0x11)) return;   // nur mit Strg
    if (sperre > 0) return;
    Entity hero = GetHero();
    if (!hero.IsValid()) return;
    Entity t = GetFocusedEntity();
    if (!t.IsValid() || !IsNPC(t)) {
        PrintScreen("Blitz-Test: keinen NPC anvisiert", 3.0f, 255, 80, 80);
        return;
    }
    if (!gemeldet) {
        gemeldet = true;
        for (int i = 0; i < 3; i++) print("Optik " + i + ": " + Optik(i) + " Effekt bekannt=" + EffectExists(Optik(i)) + " Vorlage vorhanden=" + GetTemplateEntity(Optik(i)).IsValid());
    }
    int art = druck % 3;
    druck++;
    sperre = WARTE;
    bCString optik = Optik(art);

    Entity e = SpawnNPC(SCHADEN_VORLAGE, t.GetPosition());
    bool owner = e.IsValid() && SetEntityRef(e, "gCInteraction_PS", "Owner", hero);
    bool eff = false;
    if (art < 2) {
        eff = StartEffect(optik, t);
    } else {
        Entity v = SpawnNPC(optik, t.GetPosition());
        eff = v.IsValid();
    }
    print("Druck " + druck + ": Ziel " + t.GetName() + ", Schaden-Vorlage erzeugt=" + e.IsValid() + ", Besitzer=" + owner + ", Optik '" + optik + "' gestartet=" + eff);
    PrintScreen("Blitz " + druck + ": Optik " + art + (eff ? "" : " (Effekt FEHLGESCHLAGEN)"), 3.0f, eff ? 80 : 255, eff ? 255 : 80, 80);
    fenster = 4.0f;
}

void OnFrame(float dt) {
    if (sperre > 0) sperre -= dt;
    if (fenster >= 0) fenster -= dt;
}

void OnDamage(Entity victim, Entity inflictor, Entity owner, int amount, int type) {
    if (fenster < 0) return;
    print("  Treffer: " + victim.GetName() + " -" + amount + " (Quelle " + inflictor.GetName() + ", Besitzer " + (owner.IsValid() ? owner.GetName() : bCString("-")) + ", Typ " + type + ")");
}
