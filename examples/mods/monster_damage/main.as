// Mess-Mod Monsterschaden (ab API 42), Vorbild: Plugin MonsterDamage.
// Monsterschaden auf NPCs (ab Version 42): Das Spiel teilt den Schaden von Monstern an humanoiden NPCs durch 10 (beschworene Wesen: durch 2). Dieser Mod
// ersetzt die Teilung durch einen Faktor wie das Plugin MonsterDamage (Game.MonsterDamageMultiplicator=0.2, also doppelt so viel wie im Original).
const float kFactor = 0.2f;
int g_logged = 0;

bool Override_MonsterDamage(Entity victim, Entity attacker, int damage, int divisor) {
    const int neu = int(damage * kFactor);
    if (g_logged < 10) {
        ++g_logged;
        print(attacker.GetName() + " trifft " + victim.GetName() + ": roh " + damage + ", Original " + (divisor > 1 ? damage / divisor : damage) + ", neu " + neu);
    }
    SetOverrideArg(2, neu);
    return true;   // ohne Super(): das Spiel bekommt damage ungeteilt, hier also den neuen Wert
}
