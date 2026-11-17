// Measurement mod for game scripts by name (from API 32): WatchScript, Override_Script, OnScript, CallScript, ScriptExists.
//   first world: which known script names exist and in which Script_*.dll they are
//   always:      the watched scripts (BEOBACHTEN) with self, other, number and result to the log, maximum MAX_ZEILEN lines for each name
//   Ctrl + numeric keypad +: call read-only game scripts with CallScript for the hero and the focused entity
// Override_Script only passes through (Super()). It measures if the chain and the result are correct. Log: g3asm.log.
const int TASTE = 0x6B;        // numeric keypad +, together with Ctrl
const int MAX_ZEILEN = 15;

array<bCString> BEOBACHTEN = {"MagicTeleport", "OnHit", "GetStaminaPoints", "GetAttitude", "IsDeadlyDamage", "CanBurn", "Respawn", "AssessTarget"};
array<bCString> PRUEFEN = {"MagicTeleport", "AssessHit", "IsDeadlyDamage", "CanBurn", "CanFreeze", "Respawn", "AssessTarget", "PS_DuskToDawn",
                           "SetHitPoints", "IsHumanoid", "IsInFistMode", "GetStaminaPoints", "GetLevelMax", "GetAttitude", "GibtEsNicht_G3ASM"};
array<int> zeilen;
int overrides = 0;

bCString N(Entity e) { return e.IsValid() ? e.GetName() : bCString("-"); }

bool geprueft = false;

// The game loads the Script_*.dll files late (at the first OnWorldActivate in the main menu, all scripts are still missing, measured 2026-11-09);
// thus check at each world start until a known script is there
void Pruefen() {
    for (uint i = 0; i < PRUEFEN.length(); ++i)
        print("Skript '" + PRUEFEN[i] + "': " + (ScriptExists(PRUEFEN[i]) ? "vorhanden in " + GetScriptDll(PRUEFEN[i]) : bCString("fehlt")));
}
void OnWorldActivate() {
    if (geprueft) return;
    if (!ScriptExists("IsHumanoid")) {
        print("Script-DLLs noch nicht geladen, Pruefung beim naechsten Weltstart");
        return;
    }
    geprueft = true;
    Pruefen();
}

void main() {
    zeilen.resize(BEOBACHTEN.length());
    for (uint i = 0; i < BEOBACHTEN.length(); ++i) {
        WatchScript(BEOBACHTEN[i]);
        zeilen[i] = 0;
    }
}

int Index(const bCString &in name) {
    for (uint i = 0; i < BEOBACHTEN.length(); ++i)
        if (BEOBACHTEN[i] == name) return int(i);
    return -1;
}

bool Override_Script(const bCString &in name, Entity self, Entity other, int arg) {
    overrides++;
    return Super();
}

void OnScript(const bCString &in name, Entity self, Entity other, int arg, int result) {
    int i = Index(name);
    if (i < 0 || zeilen[i] >= MAX_ZEILEN) return;
    zeilen[i]++;
    print(name + "(self " + N(self) + ", other " + N(other) + ", arg " + arg + ") = " + result + "  [Override-Aufrufe bisher " + overrides + "]");
}

void OnKey(int vk, bool down) {
    if (!down || vk != TASTE || !IsKeyDown(0x11)) return;
    Entity held = GetHero();
    Entity ziel = GetFocusedEntity();
    Entity none;
    print("CallScript-Test: Held " + N(held) + ", Ziel " + N(ziel));
    if (!geprueft) { geprueft = true; Pruefen(); }
    print("  IsHumanoid(Held) = " + CallScript("IsHumanoid", held, none));
    print("  IsInFistMode(Held) = " + CallScript("IsInFistMode", held, none));
    print("  GetStaminaPoints(Held) = " + CallScript("GetStaminaPoints", held, none));
    print("  GetLevelMax(Held) = " + CallScript("GetLevelMax", held, none));
    if (ziel.IsValid()) {
        print("  IsHumanoid(Ziel) = " + CallScript("IsHumanoid", ziel, none));
        print("  GetAttitude(Ziel, Held) = " + CallScript("GetAttitude", ziel, held));
    }
    print("  GibtEsNicht_G3ASM = " + CallScript("GibtEsNicht_G3ASM", held, none));
    PrintScreen("CallScript-Test: siehe g3asm.log", 3, 180, 255, 180);
}
