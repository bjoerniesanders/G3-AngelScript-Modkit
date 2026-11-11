// Test (API 44): Ziel fuer tests/mods/rufer
bCString letzte;
bCString Verdoppeln(const bCString &in a) { return a + a; }
bCString WerRuft(const bCString &in a) { return GetModSetting("wer", "?"); }
bCString Letzte(const bCString &in a) { return letzte; }
bCString Rekursion(const bCString &in a) { return "x" + CallMod("ziel", "Rekursion", a); }
void OnModMessage(const bCString &in sender, const bCString &in topic, const bCString &in data) { letzte = sender + "|" + topic + "|" + data; }
