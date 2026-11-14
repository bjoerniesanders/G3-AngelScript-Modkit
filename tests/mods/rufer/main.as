// Test (API 44): Aufrufe zwischen Mods - Gegenstueck ist tests/mods/ziel
void main() {
    check(ModFunctionExists("ziel", "Verdoppeln") && !ModFunctionExists("ziel", "GibtsNicht") && !ModFunctionExists("gibts_nicht", "Verdoppeln"), "Zwischen Mods: ModFunctionExists");
    check(CallMod("ziel", "Verdoppeln", "ab") == "abab" && CallMod("ziel", "GibtsNicht", "x").IsEmpty(), "Zwischen Mods: CallMod liefert den Rueckgabetext");
    check(CallMod("ziel", "WerRuft", "") == "ziel", "Zwischen Mods: waehrend CallMod laeuft die Ziel-Mod (eigene Einstellungen)");
    check(SendModMessage("test", "x") >= 1 && CallMod("ziel", "Letzte", "") == "rufer|test|x", "Zwischen Mods: SendModMessage erreicht OnModMessage mit Absender");
    check(CallMod("ziel", "Rekursion", "") == "xxxxxxxx", "Zwischen Mods: Verschachtelung nach 8 Ebenen begrenzt");
    check(GetModSetting("wer") == "rufer", "Zwischen Mods: nach CallMod wieder die eigene Mod");
}
