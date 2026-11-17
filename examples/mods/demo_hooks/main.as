// Hooks demo: intercept game functions (override with Super()).
// 1) Jens gives his mod work ("G3ASM_Jens_Job") only between 8:00 and 20:00 game time. At other times, the dialog option is hidden.
// 2) The mod logs each item that goes to the hero, before the game does the action (the original continues to run as usual).
// To turn off: write "-demo_hooks" in g3asm\mods\load_order.txt.
bool Override_InfoAvailable(const bCString &in info, const bCString &in owner, const bCString &in quest) {
    if (info == "G3ASM_Jens_Job") {
        int h = GetGameHour();
        if (h >= 0 && (h < 8 || h >= 20)) return false;   // hidden: the original does not get the question
    }
    return Super();   // in all other cases, the game decides
}

// The mod doubles bows (each item with "bow" in its name, also crossbows) that the hero gets (loot, trade, take from an NPC).
// Two things occur, with different effects:
//  1. SetOverrideArg(4, count * 2) changes the quantity that the ORIGINAL gets. But the original function is the message to the quest system
//     (gCQuestManager_PS::OnReceiveItem): the item is already in the inventory, and the message counts only for quests. The inventory does not change.
//  2. receiver.AddItems(name, count) really puts more items into the inventory. A flag prevents that this causes this hook again.
// To turn off, set BOEGEN_DOPPELT to false.
const bool BOEGEN_DOPPELT = true;
bool bonusLaeuft = false;

bool Override_ReceiveItem(Entity giver, Entity receiver, const bCString &in name, const bCString &in id, uint count) {
    bool ok = true;
    bool boegen = BOEGEN_DOPPELT && !bonusLaeuft && count > 0 && receiver.IsValid() && receiver == GetHero() && name.GetLower().Contains("bow");
    if (receiver.IsValid() && receiver == GetHero()) print("Held erhaelt " + name + " x" + count + " von '" + giver.GetName() + "'");
    if (boegen) {
        SetOverrideArg(4, count * 2);   // message to the quest system (no effect on the inventory)
        print("  Meldung an Questsystem: " + count + " -> " + GetOverrideArg(4));
    }
    ok = Super();
    if (boegen) {
        bonusLaeuft = true;
        int rc = receiver.AddItems(name, int(count));   // real extra items
        bonusLaeuft = false;
        print("  Extra-Bogen: AddItems(" + name + ", " + count + ") -> " + rc + ", jetzt " + receiver.GetItemCount(name));
    }
    return ok;
}

// Damage: Override_Damage(victim, weapon or projectile, cause, quantity, type) (from API 25) changes each hit. It always comes before the hit points decrease (NPCs: SetHitPoints,
// hero: SetBaseValue on the attribute HP). SetOverrideArg(3, ..) changes the quantity. Without Super(), there is no hit. owner = cause (NPC; in a group fight an
// approximation), inflictor = weapon or projectile (for the hero the weapon of the attacker, for animals "Fist"). At death, the quantity is only the remaining hit points.
// HELD_SCHADEN_HALB halves the damage to the hero. To turn off, set it to false.
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
    if (victim.IsValid() && victim == GetHero()) heldGetroffen = true;   // for the check in each frame below
    if (HELD_SCHADEN_HALB && victim.IsValid() && victim == GetHero() && amount > 1) {
        SetOverrideArg(3, uint(amount / 2));
        print("  Held: Schaden " + amount + " -> " + GetOverrideArg(3));
    }
    return Super();
}

// Status effects (from API 27): OnStatusEffect reports each change (disease, poison, fire, ...). For the measurement, the mod also writes the stamina of the affected person ("SP" is
// probably the stamina in accordance with the attribute tags; tags that are not valid give -1).
int statusLog = 0;
void OnStatusEffect(Entity npc, int effect, int enabled) {
    if (statusLog >= 200) return;
    statusLog++;
    print("Statuseffekt " + effect + (enabled == 1 ? " AN" : " AUS") + " bei '" + npc.GetName() + "' (Maske " + npc.GetStatusEffects() + ", SP " + npc.GetAttribute("SP") + ")");
}

// Fall damage and other losses without a hit: the hero loses "HP", but gCEntity::OnDamage and Override_Damage report nothing (measured: fall
// 176 -> 145 and 176 -> 0). The game does not decrease the attribute through gCPlayerMemory_PS, thus the script compares the hit points in each frame. With HELD_SCHADEN_HALB,
// the hero gets half of the loss back. A fatal fall (HP 0) cannot be stopped in this way (the death is already reported in the same frame).
// Stamina measurement (a player saw that a disease stops the stamina regeneration): each 2 seconds, log "SP" and the disease status, maximum 150 lines.
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
