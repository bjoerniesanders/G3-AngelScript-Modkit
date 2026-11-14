// Mess-Mod Lernkosten (ab API 42), Vorbild: Plugin LearningThresholdTweaks.
// Halbiert bei Lehrern die Lernpunkte (aufgerundet) und das Gold fuer Attribute. Ist im Spiel "Alternative Balance" an und das Attribut schon 250, hat das
// Spiel beides verdoppelt (lp = 2 * amount); diese Verdopplung nimmt die Mod zuerst zurueck, wie der Modus NoThreshold des Plugins.
int g_logged = 0;

bool Override_LearnCost(const bCString &in info, const bCString &in attrib, int amount, int lp, int gold) {
    int baseLp = lp;
    int baseGold = gold;
    if (amount > 0 && lp == 2 * amount) {   // Schwelle des Spiels aktiv
        baseLp = amount;
        baseGold = gold / 2;
    }
    const int newLp = (baseLp + 1) / 2;
    const int newGold = baseGold / 2;
    if (g_logged < 10) {
        ++g_logged;
        print(info + ": " + attrib + " +" + amount + " kostet " + lp + " LP und " + gold + " Gold, neu " + newLp + " LP und " + newGold + " Gold");
    }
    SetOverrideArg(3, newLp);
    SetOverrideArg(4, newGold);
    return Super();
}
