// Mess-Mod fuer Spielskripte nach Namen (ab API 32): WatchScript, Override_Script, OnScript, CallScript, ScriptExists.
//   erste Welt: welche bekannten Skriptnamen es gibt und in welcher Script_*.dll sie stehen
//   immer:      die beobachteten Skripte (BEOBACHTEN) mit Self, Other, Zahl und Ergebnis ins Protokoll, je Name hoechstens MAX_ZEILEN Zeilen
//   Strg + Ziffernblock +: lesende Spielskripte per CallScript fuer den Helden und das anvisierte Entity aufrufen
// Override_Script reicht nur durch (Super()); es misst, ob die Kette und das Ergebnis stimmen. Protokoll: og3_patch.log.
const int TASTE = 0x6B;        // Ziffernblock +, zusammen mit Strg
const int MAX_ZEILEN = 15;

array<bCString> BEOBACHTEN = {"MagicTeleport", "OnHit", "GetStaminaPoints", "GetAttitude", "IsDeadlyDamage", "CanBurn", "Respawn", "AssessTarget"};
array<bCString> PRUEFEN = {"MagicTeleport", "AssessHit", "IsDeadlyDamage", "CanBurn", "CanFreeze", "Respawn", "AssessTarget", "PS_DuskToDawn",
                           "SetHitPoints", "IsHumanoid", "IsInFistMode", "GetStaminaPoints", "GetLevelMax", "GetAttitude", "GibtEsNicht_OG3"};
array<int> zeilen;
int overrides = 0;

bCString N(Entity e) { return e.IsValid() ? e.GetName() : bCString("-"); }

bool geprueft = false;

// Die Script_*.dll laedt das Spiel erst spaet (beim ersten OnWorldActivate im Hauptmenue fehlen noch alle Skripte, gemessen 2026-11-09);
// deshalb bei jedem Weltstart pruefen, bis ein bekanntes Skript da ist
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
    print("  GibtEsNicht_OG3 = " + CallScript("GibtEsNicht_OG3", held, none));
    PrintScreen("CallScript-Test: siehe og3_patch.log", 3, 180, 255, 180);
}
