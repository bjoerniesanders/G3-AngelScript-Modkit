// Measurement mod for monster damage (from API 42), model: plugin MonsterDamage.
// Monster damage on NPCs (from version 42): the game divides the damage of monsters on humanoid NPCs by 10 (summoned creatures: by 2). This mod
// replaces the division with a factor, as the plugin MonsterDamage does (Game.MonsterDamageMultiplicator=0.2, thus 2 times the original).
const float kFactor = 0.2f;
int g_logged = 0;

bool Override_MonsterDamage(Entity victim, Entity attacker, int damage, int divisor) {
    const int neu = int(damage * kFactor);
    if (g_logged < 10) {
        ++g_logged;
        print(attacker.GetName() + " trifft " + victim.GetName() + ": roh " + damage + ", Original " + (divisor > 1 ? damage / divisor : damage) + ", neu " + neu);
    }
    SetOverrideArg(2, neu);
    return true;   // without Super(): the game gets damage without division, here thus the new value
}
