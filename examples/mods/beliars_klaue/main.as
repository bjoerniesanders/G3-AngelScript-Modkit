// Klaue Beliars (ab API 30): Wie die Klaue Beliars aus Gothic 1 und 2 loest ein Treffer mit dem Orkschlaechter mit einer gewissen Chance einen Blitz aus - nur bei starken Treffern (MIN_TREFFER) und nur gegen Gegner (Haltung zum Helden).
// Ablauf: Trifft der Held einen NPC mit WAFFE (OnDamage: Besitzer = Held, Quelle = die Waffe), wird mit Wahrscheinlichkeit CHANCE_PROZENT am Opfer die Beschwoerung Smn_Lightning
// erzeugt, der Held als ihr Besitzer eingetragen (gCInteraction_PS.Owner; so laeuft der Schaden durch die echte Treffer-Kette: Quelle Smn_Lightning, Besitzer Held, Art 6 = Blitz)
// und der Blitz-Effekt des Zaubers "Blitz beschwoeren" am Opfer gestartet.
// Grenzen (gemessen): Die Schadenshoehe rechnet das Skript ZS_Lightning selbst (17 bis ueber 500), der Blitz kann mehrere NPCs im Umkreis treffen, auch Verbuendete.
// Deshalb gibt es eine Wartezeit WARTE zwischen zwei Blitzen. Abschalten: "-beliars_klaue" in og3\mods\load_order.txt.
const bCString WAFFE = "It_1H_OrcSlayer_01";
const bCString SCHADEN_VORLAGE = "Smn_Lightning";
const bCString OPTIK = "eff_magic_summonlightning_target_01";
const int MIN_TREFFER = 55;     // "starker Treffer": nur Schlaege mit mindestens so viel Schaden koennen den Blitz ausloesen (Naeherung fuer einen kritischen Treffer)
const int CHANCE_PROZENT = 30;  // Chance bei einem starken Treffer
int spaeterLog = 0;
const float WARTE = 4.0f;       // Sekunden zwischen zwei Blitzen

float sperre = 0;
uint zufall = 12345;
int blitze = 0;

// kleiner Zufallsgenerator (der Modkit hat keinen eigenen): lineare Kongruenz, die Eingabe mischt die Spielzeit und den Schaden unter
int Wuerfeln(int mischung) {
    zufall = zufall * 1103515245 + 12345 + uint(mischung);
    return int((zufall >> 16) % 100);
}

// Tier = Art (gCNPC_PS.Species, Text wie "10 : gESpecies_Wolf") ist keine der Humanoiden/Untoten/Golems/Daemonen
bool IstTier(Entity e) {
    bCString a = GetProperty(e, "NPC", "Species");
    if (a.IsEmpty()) return false;
    if (a.Find("Human", 0) >= 0 || a.Find("Skeleton", 0) >= 0 || a.Find("Zombie", 0) >= 0 || a.Find("Golem", 0) >= 0 || a.Find("Demon", 0) >= 0) return false;
    if (a.Find("Orc", 0) >= 0 || a.Find("Goblin", 0) >= 0 || a.Find("Troll", 0) >= 0 || a.Find("Ogre", 0) >= 0 || a.Find("Dragon", 0) >= 0) return false;
    return true;
}

// Gegner = Haltung gegenueber dem Helden Feindlich (rot) oder Raubtier, bei Tieren auch Veraergert (orange); Neutrale und veraergerte Menschen zaehlen nicht. Die Haltung aendert sich im Kampf (gemessen: Neutral -> Feindlich), (gCNPC_PS.AttitudeToPlayer2: 1 Freund, 2 Neutral, 3 Veraergert, 4 Feindlich, 5 Raubtier, 6 Panik).
// Die Haltung steht im Protokoll (bis 30 Zeilen), damit sich pruefen laesst, ob die Einteilung stimmt.
bool Feindlich(Entity victim) {
    bCString h = GetProperty(victim, "NPC", "AttitudeToPlayer2");
    bool tier = IstTier(victim);
    bool feind = h.Find("Hostile", 0) >= 0 || h.Find("Predator", 0) >= 0 || (tier && h.Find("Angry", 0) >= 0);   // Angry (orange): nur bei Tieren, sie sind es vor dem ersten Schlag
    if (spaeterLog < 30) {
        spaeterLog++;
        print("Haltung " + victim.GetName() + ": '" + h + "'" + (tier ? " (Tier)" : "") + " -> " + (feind ? "Gegner" : "kein Gegner"));
    }
    return feind;
}

void OnFrame(float dt) {
    if (sperre > 0) sperre -= dt;
    zufall += 1;   // jeder Frame verschiebt den Zufall etwas, damit er nicht nur von den Treffern abhaengt
}

void OnDamage(Entity victim, Entity inflictor, Entity owner, int amount, int type) {
    if (sperre > 0) return;
    Entity hero = GetHero();
    if (!hero.IsValid() || !owner.IsValid() || owner != hero) return;   // nur Treffer des Helden
    if (victim == hero || !IsNPC(victim)) return;
    if (inflictor.GetName() != WAFFE) return;                            // nur mit dem Orkschlaechter (nicht mit dem Blitz selbst)
    if (amount < MIN_TREFFER) return;                                    // nur starke Treffer
    if (!Feindlich(victim)) return;                                      // nur gegen Gegner, nicht gegen Freunde und Neutrale
    if (Wuerfeln(amount) >= CHANCE_PROZENT) return;

    sperre = WARTE;
    Entity b = SpawnNPC(SCHADEN_VORLAGE, victim.GetPosition());
    bool besitzer = b.IsValid() && SetEntityRef(b, "gCInteraction_PS", "Owner", hero);
    bool optik = StartEffect(OPTIK, victim);
    blitze++;
    print("Klaue Beliars #" + blitze + ": Blitz auf " + victim.GetName() + " (Treffer " + amount + "), Beschwoerung=" + b.IsValid() + ", Besitzer=" + besitzer + ", Optik=" + optik);
    PrintScreen("Klaue Beliars!", 1.5f, 120, 160, 255);
}
