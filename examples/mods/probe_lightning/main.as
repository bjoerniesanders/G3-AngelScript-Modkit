// Lightning test, step 1e (for "Klaue Beliars"): Ctrl + numeric keypad - strikes the focused NPC with lightning.
// Damage: the summon Smn_Lightning with the hero as the owner (gCInteraction_PS.Owner) - goes through the real hit chain (measured: inflictor Smn_Lightning, owner PC_Hero, type 6, kills).
// Visuals: the summon itself shows no lightning. Each push tries the next lightning visual in sequence (see OPTIK_*): a named effect (StartEffect) or the effect template (SpawnNPC).
// Protection: only NPCs as the target, a minimum of WARTE seconds between two lightning strikes. To turn off: "-probe_lightning" in g3asm\mods\load_order.txt.
const int TASTE = 0x6D;          // numeric keypad -, together with Ctrl (without Ctrl, the key belongs to demo_spawn)
const float WARTE = 3.0f;        // seconds between two lightning strikes
const bCString SCHADEN_VORLAGE = "Smn_Lightning";

int druck = 0;
float sperre = 0;
float fenster = -1;
bool gemeldet = false;

bCString Optik(int i) {
    if (i == 0) return "eff_magic_summonlightning_target_01";   // target effect of the spell "Blitz beschwoeren" (summon lightning)
    if (i == 1) return "eff_magic_lightningbolt_projectile_01"; // projectile effect of the spell "Blitzschlag" (lightning bolt)
    return "VFX_Magic_Electricity_LightningBolt_TargetBurst_01"; // effect template (visible until now, visuals only)
}

void OnKey(int vk, bool down) {
    if (!down || vk != TASTE || !IsKeyDown(0x11)) return;   // only with Ctrl
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
