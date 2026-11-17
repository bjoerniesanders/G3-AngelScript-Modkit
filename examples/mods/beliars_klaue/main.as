// Claw of Beliar (from API 30): as the Claw of Beliar in Gothic 1 and 2, a hit with the Orkschlaechter can cause lightning. This occurs only for strong hits (MIN_TREFFER) and only against opponents (attitude to the hero).
// Sequence: if the hero hits an NPC with WAFFE (OnDamage: owner = hero, inflictor = the weapon), the mod makes the summon Smn_Lightning at the victim with the chance CHANCE_PROZENT,
// sets the hero as its owner (gCInteraction_PS.Owner; thus the damage goes through the real hit chain: inflictor Smn_Lightning, owner hero, type 6 = lightning)
// and starts the lightning effect of the spell "Blitz beschwoeren" (summon lightning) at the victim.
// Limits (measured): the script ZS_Lightning calculates the damage itself (17 to more than 500). The lightning can hit more than one NPC near the victim, also allies.
// Thus there is a wait time WARTE between two lightning strikes. To turn off: "-beliars_klaue" in g3asm\mods\load_order.txt.
const bCString WAFFE = "It_1H_OrcSlayer_01";
const bCString SCHADEN_VORLAGE = "Smn_Lightning";
const bCString OPTIK = "eff_magic_summonlightning_target_01";
const int MIN_TREFFER = 55;     // "strong hit": only hits with this damage or more can cause the lightning (approximation of a critical hit)
const int CHANCE_PROZENT = 30;  // chance for a strong hit
int spaeterLog = 0;
const float WARTE = 4.0f;       // seconds between two lightning strikes

float sperre = 0;
uint zufall = 12345;
int blitze = 0;

// small random number generator (the modkit has none): linear congruential. The input mixes in the game time and the damage
int Wuerfeln(int mischung) {
    zufall = zufall * 1103515245 + 12345 + uint(mischung);
    return int((zufall >> 16) % 100);
}

// animal = the species (gCNPC_PS.Species, text such as "10 : gESpecies_Wolf") is not one of humanoids, undead, golems, demons
bool IstTier(Entity e) {
    bCString a = GetProperty(e, "NPC", "Species");
    if (a.IsEmpty()) return false;
    if (a.Find("Human", 0) >= 0 || a.Find("Skeleton", 0) >= 0 || a.Find("Zombie", 0) >= 0 || a.Find("Golem", 0) >= 0 || a.Find("Demon", 0) >= 0) return false;
    if (a.Find("Orc", 0) >= 0 || a.Find("Goblin", 0) >= 0 || a.Find("Troll", 0) >= 0 || a.Find("Ogre", 0) >= 0 || a.Find("Dragon", 0) >= 0) return false;
    return true;
}

// opponent = attitude to the hero is hostile (red) or predator, for animals also angry (orange). Neutral and angry humans do not count. The attitude changes in combat (measured: neutral -> hostile) (gCNPC_PS.AttitudeToPlayer2: 1 friend, 2 neutral, 3 angry, 4 hostile, 5 predator, 6 panic).
// The log shows the attitude (up to 30 lines), so that you can check if the classification is correct.
bool Feindlich(Entity victim) {
    bCString h = GetProperty(victim, "NPC", "AttitudeToPlayer2");
    bool tier = IstTier(victim);
    bool feind = h.Find("Hostile", 0) >= 0 || h.Find("Predator", 0) >= 0 || (tier && h.Find("Angry", 0) >= 0);   // Angry (orange): only for animals. They have this attitude before the first hit
    if (spaeterLog < 30) {
        spaeterLog++;
        print("Haltung " + victim.GetName() + ": '" + h + "'" + (tier ? " (Tier)" : "") + " -> " + (feind ? "Gegner" : "kein Gegner"));
    }
    return feind;
}

void OnFrame(float dt) {
    if (sperre > 0) sperre -= dt;
    zufall += 1;   // each frame changes the random value a little, so that it does not depend only on the hits
}

void OnDamage(Entity victim, Entity inflictor, Entity owner, int amount, int type) {
    if (sperre > 0) return;
    Entity hero = GetHero();
    if (!hero.IsValid() || !owner.IsValid() || owner != hero) return;   // only hits of the hero
    if (victim == hero || !IsNPC(victim)) return;
    if (inflictor.GetName() != WAFFE) return;                            // only with the Orkschlaechter (not with the lightning itself)
    if (amount < MIN_TREFFER) return;                                    // only strong hits
    if (!Feindlich(victim)) return;                                      // only against opponents, not against friends and neutral persons
    if (Wuerfeln(amount) >= CHANCE_PROZENT) return;

    sperre = WARTE;
    Entity b = SpawnNPC(SCHADEN_VORLAGE, victim.GetPosition());
    bool besitzer = b.IsValid() && SetEntityRef(b, "gCInteraction_PS", "Owner", hero);
    bool optik = StartEffect(OPTIK, victim);
    blitze++;
    print("Klaue Beliars #" + blitze + ": Blitz auf " + victim.GetName() + " (Treffer " + amount + "), Beschwoerung=" + b.IsValid() + ", Besitzer=" + besitzer + ", Optik=" + optik);
    PrintScreen("Klaue Beliars!", 1.5f, 120, 160, 255);
}
