// Hooks-Demo: Spielfunktionen abfangen (Override mit Super()).
// 1) Jens gibt seine Mod-Arbeit ("G3ASM_Jens_Job") nur zwischen 8 und 20 Uhr Spielzeit: ausserhalb ist die Gespraechsoption verborgen.
// 2) Jede Itemuebergabe an den Helden wird mitgeschrieben, bevor das Spiel sie ausfuehrt (Original laeuft wie gewohnt weiter).
// Abschalten: in g3asm\mods\load_order.txt "-hooks_demo" eintragen.
bool Override_InfoAvailable(const bCString &in info, const bCString &in owner, const bCString &in quest) {
    if (info == "G3ASM_Jens_Job") {
        int h = GetGameHour();
        if (h >= 0 && (h < 8 || h >= 20)) return false;   // verborgen: Original nicht einmal gefragt
    }
    return Super();   // sonst entscheidet das Spiel
}

// Boegen (jeder Gegenstand, dessen Name "bow" enthaelt, auch Armbrueste), die der Held bekommt (Pluendern, Handeln, Nehmen von einem NPC), werden verdoppelt.
// Zwei Dinge geschehen dabei, mit unterschiedlicher Wirkung:
//  1. SetOverrideArg(4, count * 2) aendert die Menge, die das ORIGINAL bekommt. Die Original-Funktion ist aber die Meldung an das Questsystem
//     (gCQuestManager_PS::OnReceiveItem): der Gegenstand ist schon im Inventar, die Meldung zaehlt nur fuer Quests. Das Inventar bleibt unveraendert.
//  2. receiver.AddItems(name, count) legt wirklich zusaetzliche Stuecke ins Inventar. Ein Merker verhindert, dass das selbst wieder diesen Hook ausloest.
// Zum Abschalten BOEGEN_DOPPELT auf false setzen.
const bool BOEGEN_DOPPELT = true;
bool bonusLaeuft = false;

bool Override_ReceiveItem(Entity giver, Entity receiver, const bCString &in name, const bCString &in id, uint count) {
    bool ok = true;
    bool boegen = BOEGEN_DOPPELT && !bonusLaeuft && count > 0 && receiver.IsValid() && receiver == GetHero() && name.GetLower().Contains("bow");
    if (receiver.IsValid() && receiver == GetHero()) print("Held erhaelt " + name + " x" + count + " von '" + giver.GetName() + "'");
    if (boegen) {
        SetOverrideArg(4, count * 2);   // Meldung an das Questsystem (kein Inventar-Effekt)
        print("  Meldung an Questsystem: " + count + " -> " + GetOverrideArg(4));
    }
    ok = Super();
    if (boegen) {
        bonusLaeuft = true;
        int rc = receiver.AddItems(name, int(count));   // wirkliche Extra-Stuecke
        bonusLaeuft = false;
        print("  Extra-Bogen: AddItems(" + name + ", " + count + ") -> " + rc + ", jetzt " + receiver.GetItemCount(name));
    }
    return ok;
}

// Schaden: Override_Damage(Opfer, Waffe/Geschoss, Verursacher, Menge, Typ) (ab API 25) greift in jeden Treffer ein. Er kommt immer vor dem Abziehen der Lebenspunkte (NPCs: SetHitPoints,
// Held: SetBaseValue auf das Attribut HP). SetOverrideArg(3, ..) aendert die Menge; ohne Super() entfaellt der Treffer. owner = Verursacher (NPC, im Gruppenkampf eine
// Naeherung), inflictor = Waffe/Geschoss (beim Helden die Waffe des Angreifers, bei Tieren "Fist"). Bei Tod ist die Menge nur der Rest der Lebenspunkte.
// HELD_SCHADEN_HALB halbiert den Schaden, den der Held erleidet. Zum Abschalten auf false setzen.
const bool HELD_SCHADEN_HALB = true;
int schadenLog = 0;
bool heldGetroffen = false;
int letzteHP = -1;
int hpLog = 0;

bool Override_Damage(Entity victim, Entity inflictor, Entity owner, int amount, int type) {
    if (schadenLog < 300) {
        schadenLog++;
        print("Schaden: '" + victim.GetName() + "' <- '" + owner.GetName() + "' mit '" + inflictor.GetName() + "', Menge " + amount + ", Typ " + type);
    }
    if (victim.IsValid() && victim == GetHero()) heldGetroffen = true;   // fuer die Pro-Bild-Pruefung unten
    if (HELD_SCHADEN_HALB && victim.IsValid() && victim == GetHero() && amount > 1) {
        SetOverrideArg(3, uint(amount / 2));
        print("  Held: Schaden " + amount + " -> " + GetOverrideArg(3));
    }
    return Super();
}

// Statuseffekte (ab API 27): OnStatusEffect meldet jede Aenderung (Krankheit, Gift, Feuer, ...). Zum Messen wird die Ausdauer des Betroffenen mit ausgegeben ("SP" ist laut
// Attribut-Kennungen vermutlich die Ausdauer; ungueltige Kennungen liefern -1).
int statusLog = 0;
void OnStatusEffect(Entity npc, int effect, int enabled) {
    if (statusLog >= 200) return;
    statusLog++;
    print("Statuseffekt " + effect + (enabled == 1 ? " AN" : " AUS") + " bei '" + npc.GetName() + "' (Maske " + npc.GetStatusEffects() + ", SP " + npc.GetAttribute("SP") + ")");
}

// Fallschaden und andere Verluste ohne Treffer: der Held verliert dabei "HP", ohne dass gCEntity::OnDamage oder Override_Damage etwas melden (gemessen: Sturz
// 176 -> 145 und 176 -> 0). Das Attribut wird nicht ueber gCPlayerMemory_PS gesenkt, daher vergleicht das Skript die Lebenspunkte jedes Bild. Mit HELD_SCHADEN_HALB
// bekommt der Held die Haelfte des Verlusts zurueck; ein toedlicher Sturz (HP 0) laesst sich so nicht mehr abfangen (der Tod ist im selben Bild schon gemeldet).
// Ausdauer-Messung (Krankheit sperrt laut Spielerbeobachtung die Ausdauer-Regeneration): alle 2 Sekunden "SP" und den Krankheitsstatus protokollieren, hoechstens 150 Zeilen.
float spZeit = 0;
int spLog = 0;
void SpMessen(Entity hero, float dt) {
    spZeit += dt;
    if (spZeit < 2.0f || spLog >= 150) return;
    spZeit = 0;
    spLog++;
    print("Ausdauer-Messung: SP " + hero.GetAttribute("SP") + " / " + hero.GetAttributeMax("SP") + ", krank " + (hero.HasStatusEffect(STATUS_DISEASED) ? "ja" : "nein") + ", HP " + hero.GetAttribute("HP"));
}

void OnFrame(float dt) {
    Entity hero = GetHero();
    if (!hero.IsValid()) { letzteHP = -1; return; }
    SpMessen(hero, dt);
    int hp = hero.GetAttribute("HP");
    if (heldGetroffen) { heldGetroffen = false; letzteHP = hp; return; }
    if (letzteHP >= 0 && hp < letzteHP && hpLog < 40) {
        hpLog++;
        int verlust = letzteHP - hp;
        int zurueck = 0;
        if (HELD_SCHADEN_HALB && hp > 0 && verlust > 1) {
            zurueck = verlust / 2;
            hero.AddAttribute("HP", zurueck);
            hp = hero.GetAttribute("HP");
        }
        print("HP-Verlust ohne Treffer-Meldung: " + letzteHP + " -> " + (hp - zurueck) + " (" + verlust + "), Gegner: '" + hero.GetCurrentAttacker().GetName() + "', zurueckgegeben " + zurueck + ", jetzt " + hp);
    }
    letzteHP = hp;
}
