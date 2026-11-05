// Spiel-Ereignisse fuer Skripte (Skript-Schnittstelle ab Version 5): Inline-Hooks (MinHook) auf die Ereignis-Eintrittspunkte des
// Questsystems in Game.dll. Das Spiel meldet dort Kills, Itemuebergaben, Betreten/Verlassen von Bereichen und Zauber an
// gCQuestManager_PS; der Hook ruft zuerst das Originalverhalten und danach die Skript-Ereignisse aller Mods
// (OnNPCKilled, OnNPCDefeated, OnReceiveItem, OnEnter, OnLeave, OnSpellCast).
// Aufrufkonvention: Memberfunktionen = thiscall, hier als __fastcall(this, edx, ...) abgefangen (gleiche Stapelbereinigung).
// bCString-Wertparameter (OnSpellCast) sind 4 Byte und gehen unveraendert an das Original weiter (der Aufgerufene zerstoert sie);
// fuer das Skript wird vorher eine Referenz (Kopie) gehalten.
#include <windows.h>

#include <cstdio>
#include <functional>
#include <cstring>
#include <string>

#include <MinHook.h>

#include "sb_api.h"
#include "script_host.h"

namespace og3p {
namespace {

ScriptHost* g_host = nullptr;

typedef bool(__thiscall* Fn1)(void*, const void*);
typedef bool(__thiscall* FnItem)(void*, const void*, const void*, const void*, DWORD);
typedef bool(__thiscall* FnArea)(void*, const void*, const void*);
typedef bool(__thiscall* FnSpell)(void*, const void*, const void*, void*);
typedef void*(__thiscall* TextFn)(const void*, void*);

Fn1 o_killed, o_defeated;
FnItem o_item;
FnArea o_enter, o_leave;
FnSpell o_spell;
TextFn g_idText = nullptr;
long g_count[7] = {};
bool g_navEvents = false;

bool IsNav(const void* area) {
    const char* t = area ? g_sb.strGetText(area) : nullptr;
    return t && std::strncmp(t, "Nav_", 4) == 0;
}

void Seen(int i, const char* name) {
    if (InterlockedIncrement(&g_count[i]) == 1 && g_host) g_host->Log(std::string("Spielereignis erstmals ausgeloest: ") + name);
}

// Kopie eines bCString-Rohwerts (erhaelt die Zeichen ueber den Aufruf des Originals hinweg)
struct StrHold {
    void* raw;
    explicit StrHold(void* r) { g_sb.strCopy(&raw, &r); }
    ~StrHold() { g_sb.strDtor(&raw); }
};

// Aufruf mit Override-Kette: liefert das Ergebnis fuer das Spiel; *called sagt, ob das Original lief (Nachereignisse On<Name> nur dann)
bool RunHook(const char* overrideDecl, const EvArg* a, int n, const std::function<bool(const EvArg*)>& original, bool* called) {
    bool res = false;
    *called = false;
    if (g_host->CallOverride(overrideDecl, a, n, original, &res, called)) return res;
    res = original(a);
    *called = true;
    return res;
}

bool __fastcall HKilled(void* self, void*, const void* e) {
    EvArg a[] = {EvArg::Ent(e)};
    bool called;
    const bool r = RunHook("bool Override_NPCKilled(Entity)", a, 1, [&](const EvArg*) { return o_killed(self, e); }, &called);
    if (called) { Seen(0, "OnNPCKilled"); g_host->CallEvent("void OnNPCKilled(Entity)", a, 1); }
    return r;
}
bool __fastcall HDefeated(void* self, void*, const void* e) {
    EvArg a[] = {EvArg::Ent(e)};
    bool called;
    const bool r = RunHook("bool Override_NPCDefeated(Entity)", a, 1, [&](const EvArg*) { return o_defeated(self, e); }, &called);
    if (called) { Seen(1, "OnNPCDefeated"); g_host->CallEvent("void OnNPCDefeated(Entity)", a, 1); }
    return r;
}
bool __fastcall HItem(void* self, void*, const void* giver, const void* receiver, const void* itemId, DWORD count) {
    void* text = nullptr;   // bCString (4 Byte), vom Export in den Rohspeicher gebaut
    if (g_idText && itemId) g_idText(itemId, &text);
    if (!text) g_sb.strCtor(&text);
    const std::string nm = TemplateNameForId(itemId);
    SStr name(nm.empty() ? g_sb.strGetText(&text) : nm.c_str());   // ohne Namen: GUID-Text
    EvArg a[] = {EvArg::Ent(giver), EvArg::Ent(receiver), EvArg::Str(&name), EvArg::Str(&text), EvArg::U(count)};
    bool called;
    // ar[4] = Menge: ein Mod kann sie mit SetOverrideArg(4, ..) aendern, das Original bekommt dann den neuen Wert
    const bool r = RunHook("bool Override_ReceiveItem(Entity, Entity, const bCString &in, const bCString &in, uint)", a, 5,
                           [&](const EvArg* ar) {
                               if (ar[4].value != count) g_host->Log("OnReceiveItem: das Original bekommt Menge " + std::to_string(ar[4].value) + " (Spiel meldete " + std::to_string(count) + ")");
                               return o_item(self, giver, receiver, itemId, ar[4].value);
                           }, &called);
    if (called) { Seen(2, "OnReceiveItem"); g_host->CallEvent("void OnReceiveItem(Entity, Entity, const bCString &in, const bCString &in, uint)", a, 5); }
    g_sb.strDtor(&text);
    return r;
}
bool __fastcall HEnter(void* self, void*, const void* e, const void* area) {
    if (!g_navEvents && IsNav(area)) return o_enter(self, e, area);
    EvArg a[] = {EvArg::Ent(e), EvArg::Str(area)};
    bool called;
    const bool r = RunHook("bool Override_Enter(Entity, const bCString &in)", a, 2, [&](const EvArg*) { return o_enter(self, e, area); }, &called);
    if (called) { Seen(3, "OnEnter"); g_host->CallEvent("void OnEnter(Entity, const bCString &in)", a, 2); }
    return r;
}
bool __fastcall HLeave(void* self, void*, const void* e, const void* area) {
    if (!g_navEvents && IsNav(area)) return o_leave(self, e, area);
    EvArg a[] = {EvArg::Ent(e), EvArg::Str(area)};
    bool called;
    const bool r = RunHook("bool Override_Leave(Entity, const bCString &in)", a, 2, [&](const EvArg*) { return o_leave(self, e, area); }, &called);
    if (called) { Seen(4, "OnLeave"); g_host->CallEvent("void OnLeave(Entity, const bCString &in)", a, 2); }
    return r;
}
bool __fastcall HSpell(void* self, void*, const void* caster, const void* target, void* spell) {
    StrHold keep(spell);
    EvArg a[] = {EvArg::Ent(caster), EvArg::Ent(target), EvArg::Str(&keep.raw)};
    bool called;
    const bool r = RunHook("bool Override_SpellCast(Entity, Entity, const bCString &in)", a, 3, [&](const EvArg*) { return o_spell(self, caster, target, spell); }, &called);
    if (called) { Seen(5, "OnSpellCast"); g_host->CallEvent("void OnSpellCast(Entity, Entity, const bCString &in)", a, 3); }
    return r;
}

// Schaden: gCDamageReceiver_PS::SetHitPoints(const long&). Gemessen im Spiel (Feuerball 200 -> 115 bei OnDamage-Menge 85, Nahkampf 400 -> 323 bei 77): der Schaden
// wird ueber SetHitPoints angewendet, gCEntity::OnDamage kommt erst danach und ist nur die Meldung. gCEntity::DoDamage wird dabei nicht benutzt.
// Sinkt der Wert, ist das ein Treffer: Override_Damage(Opfer, Angreifer, Menge, Typ) darf die Menge aendern (SetOverrideArg(2, ..)) oder den Treffer durch
// Verzicht auf Super() ganz verhindern; Menge = Lebenspunkte-Verlust (bei Tod auf 0 begrenzt).
// Feldlage gCDamageReceiver_PS: +0x14 HitPoints, +0x18 HitPointsMax, +0x30 Schadenstyp, +0x34 DamageAmount, +0x38 LastInflictor (eCEntityProxy).
typedef void(__thiscall* FnSetHP)(void*, const long*);
typedef void*(__thiscall* FnGetEnt)(void*);
FnSetHP o_setHP = nullptr;
FnGetEnt g_psEntity = nullptr, g_proxyEntity = nullptr;
bool g_inDamage = false;   // Schutz: SetHitPoints aus dem Skript heraus (Override) nicht erneut als Treffer melden
void* SafeCall0(FnGetEnt f, void* obj) {
    if (!f || !obj || !ReadableMem(obj, sizeof(void*))) return nullptr;
    __try { return f(obj); } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
void __fastcall HSetHP(void* self, void*, const long* v) {
    if (g_inDamage || !v || !self || !ReadableMem(self, 0x40)) { o_setHP(self, v); return; }
    char* ps = static_cast<char*>(self);
    const long alt = *reinterpret_cast<long*>(ps + 0x14), neu = *v;
    if (neu >= alt || alt <= 0) { o_setHP(self, v); return; }
    void* victim = SafeCall0(g_psEntity, self);
    void* attacker = SafeCall0(g_proxyEntity, ps + 0x38);
    const long amount = alt - neu;
    const int type = *reinterpret_cast<int*>(ps + 0x30);
    EvArg a[] = {EvArg::Ent(victim), EvArg::Ent(attacker), EvArg::U(static_cast<unsigned>(amount)), EvArg::U(static_cast<unsigned>(type))};
    bool called = false;
    g_inDamage = true;
    RunHook("bool Override_Damage(Entity, Entity, int, int)", a, 4,
            [&](const EvArg* ar) {
                long neuHp = neu;
                const long m = static_cast<long>(static_cast<int>(ar[2].value));
                if (m != amount) {
                    const long max = *reinterpret_cast<long*>(ps + 0x18);
                    neuHp = alt - m;
                    if (neuHp < 0) neuHp = 0;
                    if (max > 0 && neuHp > max) neuHp = max;
                    g_host->Log("Schaden: das Original bekommt " + std::to_string(m) + " (Spiel meldete " + std::to_string(amount) + "), Lebenspunkte " + std::to_string(alt) + " -> " + std::to_string(neuHp));
                }
                o_setHP(self, &neuHp);
                return true;
            }, &called);
    g_inDamage = false;
    if (called) { Seen(6, "OnDamage"); g_host->CallEvent("void OnDamage(Entity, Entity, int, int)", a, 4); }
}

// Treffer auf den Helden: sein gCDamageReceiver_PS steht immer auf 1, die Lebenspunkte liegen im Attribut "HP" und sind beim Aufruf von gCEntity::OnDamage
// (this = a = b = Held, Menge, Typ) schon abgezogen (gemessen). Der Hook meldet den Treffer an dieselbe Kette Override_Damage wie bei NPCs; eine geaenderte Menge
// wirkt nachtraeglich: die Differenz wird dem Attribut zurueckgegeben bzw. zusaetzlich abgezogen. Ohne Super() entfaellt der Treffer (volle Rueckgabe).
// Ein schon toter Held wird nicht wiederbelebt. Falls der Fall-/Umgebungsschaden einen anderen Weg nimmt, sieht der Hook ihn nicht.
typedef void(__thiscall* FnOnDmg)(void*, void*, void*, int, int, void*);
FnOnDmg o_gOnDmg = nullptr;
void HeroHit(void* hero, int amount, int type) {
    if (g_inDamage || amount <= 0) return;
    EvArg a[] = {EvArg::Ent(hero), EvArg::Ent(nullptr), EvArg::U(static_cast<unsigned>(amount)), EvArg::U(static_cast<unsigned>(type))};
    int neu = amount;
    bool called = false;
    g_inDamage = true;
    RunHook("bool Override_Damage(Entity, Entity, int, int)", a, 4, [&](const EvArg* ar) { neu = static_cast<int>(ar[2].value); return true; }, &called);
    g_inDamage = false;
    if (!called) neu = 0;   // Override ohne Super(): Treffer verhindert
    if (neu < 0) neu = 0;
    if (neu != amount) {
        g_inDamage = true;   // unsere Korrektur nicht als Fremd-Aenderung melden
        const bool ok = AdjustHeroHitPoints(hero, amount - neu);
        g_inDamage = false;
        g_host->Log("Schaden am Helden: " + std::to_string(amount) + " -> " + std::to_string(neu) + (ok ? " (Lebenspunkte korrigiert)" : " (Korrektur nicht moeglich)"));
    }
    if (called) { Seen(6, "OnDamage"); g_host->CallEvent("void OnDamage(Entity, Entity, int, int)", a, 4); }
}
void __fastcall HGOnDmg(void* self, void*, void* a, void* b, int amount, int type, void* it) {
    if (self && a == self && b == self && IsPlayerEntity(self)) HeroHit(self, amount, type);
    o_gOnDmg(self, a, b, amount, type, it);
}

// gCInfo_PS::AreConditionsFulfilled: sind die Bedingungen einer Dialogoption erfuellt, ist sie also gerade waehlbar? Der Info-Manager ruft diese Funktion direkt
// (IsAvailable hat keine Aufrufer). Mod kann mit Override_InfoAvailable eingreifen.
typedef bool(__thiscall* InfoAvailFn)(const void*);
InfoAvailFn o_infoAvail = nullptr;
bool __fastcall HInfoAvail(const void* self, void*) {
    static const char* kDecl = "bool Override_InfoAvailable(const bCString &in, const bCString &in, const bCString &in)";
    if (!g_host->HasHandler(kDecl)) return o_infoAvail(self);
    std::string n, o, q;
    if (!InfoNames(self, &n, &o, &q)) return o_infoAvail(self);
    SStr sn(n), so(o), sq(q);
    EvArg a[] = {EvArg::Str(&sn), EvArg::Str(&so), EvArg::Str(&sq)};
    bool called;
    return RunHook(kDecl, a, 3, [&](const EvArg*) { return o_infoAvail(self); }, &called);
}

bool Hook(HMODULE game, const char* name, void* detour, void** original, ScriptHost& host) {
    void* target = reinterpret_cast<void*>(GetProcAddress(game, name));
    if (!target) { host.Log(std::string("Hook: Export fehlt: ") + name); return false; }
    if (MH_CreateHook(target, detour, original) != MH_OK || MH_EnableHook(target) != MH_OK) {
        host.Log(std::string("Hook: Einhaengen fehlgeschlagen: ") + name);
        return false;
    }
    return true;
}

// ---- Laden/Speichern: Quests und Infos werden beim Laden neu aus den ini-Dateien aufgebaut; eigene (per Skript angelegte) Quests/Infos muessen schon DA sein,
// bevor das Spiel die gespeicherten Laufzeitdaten (Status, Questlog) einliest. Die Hooks melden das Ende des Aufbaus an die Skripte:
//   OnQuestsLoaded()  nach gCQuestManager_PS::LoadQuestsFromInis        OnInfosLoaded()  nach gCInfoManager_PS::LoadInfosFromInis
// und protokollieren die Reihenfolge (Lesen/Schreiben der Quest-Laufzeitdaten) zur Fehlersuche.
typedef bool(__thiscall* BoolMethod)(void*);
typedef void(__thiscall* VoidMethodT)(void*);
typedef int(__thiscall* RuntimeFn)(void*, void*);
typedef const void*(__thiscall* GetNameFn)(const void*);
BoolMethod o_loadQuests = nullptr, o_loadInfos = nullptr;
VoidMethodT o_resetQuests = nullptr;
RuntimeFn o_readRuntime = nullptr, o_writeRuntime = nullptr, o_readInfoSave = nullptr;
GetNameFn g_questName = nullptr;
long g_readCount = 0, g_writeCount = 0;

void LogLine(const char* s) { if (g_host) g_host->Log(s); }
const char* QuestNameOf(const void* q) {
    if (!ReadableMem(q, sizeof(void*))) return nullptr;
    __try { const void* n = g_questName ? g_questName(q) : nullptr; return n ? g_sb.strGetText(n) : nullptr; } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool __fastcall HLoadQuests(void* self, void*) {
    const bool r = o_loadQuests(self);
    LogLine("Laden: LoadQuestsFromInis fertig -> OnQuestsLoaded");
    SetQuestManagerOverride(self);   // Bindings arbeiten jetzt auf dem Manager, der gerade aufgebaut wurde
    g_host->CallEvent("void OnQuestsLoaded()", nullptr, 0);
    SetQuestManagerOverride(nullptr);
    return r;
}
bool __fastcall HLoadInfos(void* self, void*) {
    const bool r = o_loadInfos(self);
    LogLine("Laden: LoadInfosFromInis fertig -> OnInfosLoaded");
    SetInfoManagerOverride(self);
    g_host->CallEvent("void OnInfosLoaded()", nullptr, 0);
    SetInfoManagerOverride(nullptr);
    return r;
}
typedef bool(__thiscall* ReadCompiledFn)(void*, const void*);
ReadCompiledFn o_readCompiled = nullptr;
bool __fastcall HReadCompiled(void* self, void*, const void* path) {   // so laedt das Spiel die Infos beim Laden (kompiliert), nicht aus ini
    const bool r = o_readCompiled(self, path);
    LogLine("Laden: ReadCompiledInfoManager fertig -> OnInfosLoaded");
    SetInfoManagerOverride(self);
    g_host->CallEvent("void OnInfosLoaded()", nullptr, 0);
    SetInfoManagerOverride(nullptr);
    return r;
}
void __fastcall HResetQuests(void* self, void*) { LogLine("Laden: ResetQuests"); o_resetQuests(self); }
int __fastcall HReadRuntime(void* self, void*, void* stream) {
    const long n = InterlockedIncrement(&g_readCount);
    const int r = o_readRuntime(self, stream);
    const char* nm = QuestNameOf(self);
    if (n <= 3 || (nm && std::strncmp(nm, "OG3", 3) == 0)) {
        char b[160];
        std::snprintf(b, sizeof b, "Laden: Quest-Laufzeitdaten gelesen #%ld fuer '%s'", n, nm ? nm : "?");
        LogLine(b);
    }
    return r;
}
int __fastcall HWriteRuntime(void* self, void*, void* stream) {
    const long n = InterlockedIncrement(&g_writeCount);
    const char* nm = QuestNameOf(self);
    if (nm && std::strncmp(nm, "OG3", 3) == 0) {
        char b[160];
        std::snprintf(b, sizeof b, "Speichern: Quest-Laufzeitdaten geschrieben #%ld fuer '%s'", n, nm);
        LogLine(b);
    }
    return o_writeRuntime(self, stream);
}
int __fastcall HReadInfoSave(void* self, void*, void* stream) { LogLine("Laden: Info-Manager ReadSaveGame"); return o_readInfoSave(self, stream); }

void InstallSaveHooks(HMODULE game, ScriptHost& host) {
    g_questName = reinterpret_cast<GetNameFn>(GetProcAddress(game, "?GetName@gCQuest_PS@@QBEABVbCString@@XZ"));
    int n = 0;
    n += Hook(game, "?LoadQuestsFromInis@gCQuestManager_PS@@QAE_NXZ", (void*)&HLoadQuests, (void**)&o_loadQuests, host);
    n += Hook(game, "?LoadInfosFromInis@gCInfoManager_PS@@QAE_NXZ", (void*)&HLoadInfos, (void**)&o_loadInfos, host);
    n += Hook(game, "?ReadCompiledInfoManager@gCInfoManager_PS@@QAE_NABVbCString@@@Z", (void*)&HReadCompiled, (void**)&o_readCompiled, host);
    n += Hook(game, "?ResetQuests@gCQuestManager_PS@@QAEXXZ", (void*)&HResetQuests, (void**)&o_resetQuests, host);
    n += Hook(game, "?ReadRuntimeData@gCQuest_PS@@QAE?AW4bEResult@@AAVbCIStream@@@Z", (void*)&HReadRuntime, (void**)&o_readRuntime, host);
    n += Hook(game, "?WriteRuntimeData@gCQuest_PS@@QAE?AW4bEResult@@AAVbCOStream@@@Z", (void*)&HWriteRuntime, (void**)&o_writeRuntime, host);
    n += Hook(game, "?ReadSaveGame@gCInfoManager_PS@@UAE?AW4bEResult@@AAVbCIStream@@@Z", (void*)&HReadInfoSave, (void**)&o_readInfoSave, host);
    char b[96];
    std::snprintf(b, sizeof b, "Lade-/Speicher-Hooks: %d von 7 aktiv", n);
    host.Log(b);
}

}  // namespace

void SetGameHookOptions(bool navEvents) { g_navEvents = navEvents; }

// Haengt die Spielereignisse ein (einmal, nachdem Game.dll geladen ist). Rueckgabe: Zahl der aktiven Hooks.
int InstallGameHooks(ScriptHost& host) {
    static bool done = false;
    if (done) return 0;
    done = true;
    g_host = &host;
    HMODULE game = GetModuleHandleA("Game.dll");
    if (!game) { host.Log("Spielereignisse: Game.dll nicht geladen"); return 0; }
    MH_STATUS st = MH_Initialize();
    if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) { host.Log("Spielereignisse: MinHook nicht startbar"); return 0; }
    if (HMODULE sb = GetModuleHandleA("SharedBase.dll"))
        g_idText = reinterpret_cast<TextFn>(GetProcAddress(sb, "?GetText@bCPropertyID@@QBE?AVbCString@@XZ"));
    int n = 0;
    n += Hook(game, "?OnNPCKilled@gCQuestManager_PS@@QAE_NABVeCEntity@@@Z", (void*)&HKilled, (void**)&o_killed, host);
    n += Hook(game, "?OnNPCDefeated@gCQuestManager_PS@@QAE_NABVeCEntity@@@Z", (void*)&HDefeated, (void**)&o_defeated, host);
    n += Hook(game, "?OnReceiveItem@gCQuestManager_PS@@QAE_NPBVeCEntity@@0ABVbCPropertyID@@K@Z", (void*)&HItem, (void**)&o_item, host);
    n += Hook(game, "?OnEnter@gCQuestManager_PS@@QAE_NABVeCEntity@@ABVbCString@@@Z", (void*)&HEnter, (void**)&o_enter, host);
    n += Hook(game, "?OnLeave@gCQuestManager_PS@@QAE_NABVeCEntity@@ABVbCString@@@Z", (void*)&HLeave, (void**)&o_leave, host);
    n += Hook(game, "?OnSpellCast@gCQuestManager_PS@@QAE_NABVeCEntity@@0VbCString@@@Z", (void*)&HSpell, (void**)&o_spell, host);
    n += Hook(game, "?AreConditionsFulfilled@gCInfo_PS@@IBE_NXZ", (void*)&HInfoAvail, (void**)&o_infoAvail, host);
    if (HMODULE eng = GetModuleHandleA("Engine.dll")) {
        g_psEntity = reinterpret_cast<FnGetEnt>(GetProcAddress(eng, "?GetEntity@eCEntityPropertySet@@UAEPAVeCEntity@@XZ"));
        g_proxyEntity = reinterpret_cast<FnGetEnt>(GetProcAddress(eng, "?GetEntity@eCEntityProxy@@QAEPAVeCEntity@@XZ"));
    }
    n += Hook(game, "?SetHitPoints@gCDamageReceiver_PS@@QAEXABJ@Z", (void*)&HSetHP, (void**)&o_setHP, host);
    Hook(game, "?OnDamage@gCEntity@@UAEXPAVeCEntity@@0HHAAVeCContactIterator@@@Z", (void*)&HGOnDmg, (void**)&o_gOnDmg, host);
    char b[96];
    std::snprintf(b, sizeof b, "Spielereignisse: %d von 8 Hooks aktiv (Game.dll, gCQuestManager_PS, gCDamageReceiver_PS::SetHitPoints)", n);
    host.Log(b);
    InstallInfoHook(host);
    InstallSaveHooks(game, host);
    return n;
}

}  // namespace og3p
