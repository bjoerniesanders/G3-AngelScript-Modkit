// Measurement mod for learn costs (from API 42), model: plugin LearningThresholdTweaks.
// At teachers, halves the learn points (rounded up) and the gold for attributes. If "Alternative Balance" is on in the game and the attribute is already 250, the
// game has doubled the two (lp = 2 * amount). The mod first removes this doubling, as the mode NoThreshold of the plugin does.
int g_logged = 0;

bool Override_LearnCost(const bCString &in info, const bCString &in attrib, int amount, int lp, int gold) {
    int baseLp = lp;
    int baseGold = gold;
    if (amount > 0 && lp == 2 * amount) {   // threshold of the game active
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
