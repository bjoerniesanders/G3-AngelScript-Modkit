// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// Spielskripte nach Namen (Skript-Schnittstelle ab Version 32, Roadmap 7.1).
// Die Spielskripte der Script_*.dll (z. B. "AssessHit", "MagicTeleport", "SetHitPoints") stehen in der Tabelle gCScriptAdmin::m_Scripts (Name -> Funktion
// int __stdcall(gCScriptProcessingUnit*, Entity* self, Entity* other, int arg)). Jeder Aufruf nach Namen laeuft ueber einen von zwei Exporten der Game.dll
// (gemessen am Assembler der Game.dll 2026-11-09):
//   RunScriptFromScript(Name, SPU, self, other, arg) - Self/Other als Script.dll-Entity-Objekte; CallScriptFromScript (exportiert) endet hier,
//   RunScript(Name, SPU)                             - Self/Other/Zahl stehen in der Processing Unit; gCScriptAdmin::CallScript (virtuell) endet hier.
// Beide schlagen den Namen in m_Scripts nach und rufen die Funktion; einen anderen Weg in die Tabelle gibt es in der Game.dll nicht. Wer die Funktion in der
// Tabelle ersetzt (so machen es Script-Plugins wie TeleportPartyMembers), liegt also HINTER unseren Hooks: unser Override_Script sieht auch deren Skripte.
//
// Fuer Mods:
//   int CallScript(name, self, other, arg)  - Skript aufrufen (ueber CallScriptFromScript, wie ein Spielskript es tut)
//   bool ScriptExists(name), bCString GetScriptDll(name)
//   WatchScript(name)                        - nur fuer beobachtete Namen ruft das Modkit Override_Script/OnScript (sonst kostet jeder Skriptaufruf Zeit)
//   bool Override_Script(const bCString &in name, Entity self, Entity other, int arg)   - Super() = Original; SetOverrideArg(3, x) aendert arg
//   void OnScript(const bCString &in name, Entity self, Entity other, int arg, int result)  - danach, nur wenn das Original lief
//   SetScriptResult(int)                     - Ergebnis fuers Spiel, wenn Override_Script das Original NICHT aufruft (sonst 1 bei true, 0 bei false)
// Messung (ini [g3asm] scriptlog=1): alle 60 s die haeufigsten Skriptnamen je Weg, Aufrufe aus anderen Threads und die Zeit in unseren Hooks.
//
// KI-Skripte (API 41): Neben m_Scripts hat gCScriptAdmin vier weitere Tabellen (gemessen am Assembler der Game.dll 2026-11-10): Zustaende (+0x1B0, "ZS_..."),
// KI-Funktionen (+0x1C0, "_AI_..."), Callbacks (+0x1D0) und Routinen (+0x1E0). Je Tabelle gibt es genau einen Export, der nachschlaegt und aufruft:
//   RunScriptState/RunScriptFunction(Name, Zustandsstapel, SPU) -> bool, aufgerufen von gCScriptProcessingUnit::ProcessScript und aus den Script-DLLs,
//   RunScriptCallback/RunScriptRoutine(Name, SPU) -> bool, aufgerufen von RunLocalCallback/RunTaskCallback bzw. der Tagesablauf-Erkennung.
// Zustaende und KI-Funktionen laufen in Abschnitten ueber mehrere Bilder: false = noch nicht fertig (naechstes Bild weiter), true = fertig. Ein Zustand
// wird darum jedes Bild erneut gerufen. Unbekannte Namen meldet das Spiel und liefert true.
// Fuer Mods (gleiche Namensliste wie WatchScript):
//   bool Override_AI(const bCString &in name, int kind, Entity self, Entity target)    - kind AI_STATE/AI_FUNCTION/AI_CALLBACK/AI_ROUTINE; Super() = Original;
//                                                                                         die Rueckgabe geht ans Spiel (auch ohne Super)
//   void OnAI(const bCString &in name, int kind, Entity self, Entity target, int result) - danach, nur wenn das Original lief
#include <windows.h>

#include <angelscript.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <MinHook.h>

#include "sb_api.h"
#include "script_host.h"

namespace g3asm {
namespace {

struct Entity {
    void* p;
};

typedef void*(__stdcall* InstFn)();
typedef void*(__thiscall* FindModuleFn)(const void*, const void*);
typedef int(__thiscall* CallFromScriptFn)(void*, const void*, void*, void*, int);
typedef bool(__thiscall* ScriptDllFn)(const void*, const void*, void*);
typedef int(__thiscall* RunScriptFn)(void*, const void*, void*);
typedef int(__thiscall* RunFromScriptFn)(void*, const void*, void*, void*, void*, int);
typedef void*(__thiscall* SpuEntFn)(void*);
typedef int(__thiscall* SpuIntFn)(void*);
typedef void(__thiscall* SpuSetIntFn)(void*, int);

struct Api {
    bool tried = false;
    HMODULE game = nullptr, engine = nullptr;
    InstFn moduleAdmin = nullptr;
    FindModuleFn findModule = nullptr;
    CallFromScriptFn callFromScript = nullptr;
    ScriptDllFn scriptDll = nullptr;
    SpuEntFn spuSelf = nullptr, spuOther = nullptr, spuTarget = nullptr;
    SpuIntFn spuInt = nullptr;
    SpuSetIntFn spuSetInt = nullptr;
    void* admin = nullptr;
} g_a;

bool ResolveApi() {
    if (g_a.tried) return g_a.game != nullptr;
    g_a.game = GetModuleHandleA("Game.dll");
    g_a.engine = GetModuleHandleA("Engine.dll");
    if (!g_a.game || !g_a.engine) {
        g_a.game = nullptr;
        return false;   // spaeter erneut (Testprogramm: nie vorhanden)
    }
    g_a.tried = true;
    auto G = [](const char* n) { return reinterpret_cast<void*>(GetProcAddress(g_a.game, n)); };
    g_a.moduleAdmin = reinterpret_cast<InstFn>(GetProcAddress(g_a.engine, "?GetInstance@eCModuleAdmin@@SGAAV1@XZ"));
    g_a.findModule = reinterpret_cast<FindModuleFn>(GetProcAddress(g_a.engine, "?FindModule@eCModuleAdmin@@QBEPAVeCEngineComponentBase@@ABVbCString@@@Z"));
    g_a.callFromScript = reinterpret_cast<CallFromScriptFn>(G("?CallScriptFromScript@gCScriptAdmin@@QAEHABVbCString@@PAX1H@Z"));
    g_a.scriptDll = reinterpret_cast<ScriptDllFn>(G("?GetScriptDLLWithScript@gCScriptAdmin@@UBE_NABVbCString@@AAV2@@Z"));
    g_a.spuSelf = reinterpret_cast<SpuEntFn>(G("?GetSelfEntity@gCScriptProcessingUnit@@QAEPAVeCEntity@@XZ"));
    g_a.spuOther = reinterpret_cast<SpuEntFn>(G("?GetOtherEntity@gCScriptProcessingUnit@@QAEPAVeCEntity@@XZ"));
    g_a.spuTarget = reinterpret_cast<SpuEntFn>(G("?GetTargetEntity@gCScriptProcessingUnit@@QAEPAVeCEntity@@XZ"));
    g_a.spuInt = reinterpret_cast<SpuIntFn>(G("?GetIntParameter@gCScriptProcessingUnit@@QAEHXZ"));
    g_a.spuSetInt = reinterpret_cast<SpuSetIntFn>(G("?SetIntParameter@gCScriptProcessingUnit@@QAEXH@Z"));
    return true;
}

bool InModule(HMODULE m, const void* p) {
    if (!m) return false;
    auto* b = reinterpret_cast<const unsigned char*>(m);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(b + reinterpret_cast<const IMAGE_DOS_HEADER*>(b)->e_lfanew);
    return p >= b && p < b + nt->OptionalHeader.SizeOfImage;
}

void* SafeFindAdmin(const void* nameStr) {
    __try {
        void* ma = g_a.moduleAdmin ? g_a.moduleAdmin() : nullptr;
        return ma && g_a.findModule ? g_a.findModule(ma, nameStr) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
// gCScriptAdmin lebt die ganze Sitzung (Engine-Modul); einmal suchen
void* ScriptAdmin() {
    if (!ResolveApi()) return nullptr;
    if (!g_a.admin) {
        SStr n("gCScriptAdmin");
        g_a.admin = SafeFindAdmin(&n);
    }
    return g_a.admin;
}

// eCEntity* aus einem Skript-Argument: normal ein Script.dll-Entity-Objekt (erstes Feld eCEntity*, keine vtable); falls ein Aufrufer direkt ein eCEntity*
// uebergibt (erstes Feld = vtable in der Engine.dll), dieses selbst
void* EntityOfArg(void* arg) {
    if (!arg || !ReadableMem(arg, sizeof(void*))) return nullptr;
    void* first = *static_cast<void**>(arg);
    if (InModule(g_a.engine, first) || InModule(g_a.game, first)) return arg;
    if (!first || !ReadableMem(first, sizeof(void*))) return nullptr;
    void* vt = *static_cast<void**>(first);
    return InModule(g_a.engine, vt) || InModule(g_a.game, vt) ? first : nullptr;
}


void* SafeSpuEnt(SpuEntFn fn, void* spu) {
    if (!fn || !spu) return nullptr;
    __try {
        return fn(spu);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
int SafeSpuInt(void* spu) {
    if (!g_a.spuInt || !spu) return 0;
    __try {
        return g_a.spuInt(spu);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// ---- Bindings

bool g_callFailLogged = false;
int SafeCallFromScript(void* admin, const void* name, void* self, void* other, int arg, bool* ok) {
    __try {
        *ok = true;
        return g_a.callFromScript(admin, name, self, other, arg);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *ok = false;
        return 0;
    }
}
bool SafeScriptDll(void* admin, const void* name, void* out) {
    __try {
        return g_a.scriptDll(admin, name, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

SStr GetScriptDllScript(const SStr& name) {
    void* admin = ScriptAdmin();
    SStr dll;
    if (admin && g_a.scriptDll && !SafeScriptDll(admin, &name, &dll)) return SStr();
    return dll;
}
bool ScriptExistsScript(const SStr& name) {
    void* admin = ScriptAdmin();
    SStr dll;
    return admin && g_a.scriptDll && SafeScriptDll(admin, &name, &dll);
}

// Script.dll-Entity-Objekt fuer den Aufruf: 0xA8 Byte (gemessen am Konstruktor in Script.dll 2026-11-09: erstes Feld eCEntity*, danach 41 zwischengespeicherte
// Property-Set-Zeiger, die Entity(eCEntity const*) fuellt). Ohne sie lesen Skripte wie GetStaminaPoints 0. Destruktor ist leer (nur ret).
typedef void*(__thiscall* EntityCtorFn)(void*, const void*);
typedef void*(__thiscall* EntityDefCtorFn)(void*);
struct ScriptEntityBuf {
    alignas(8) unsigned char raw[0x100];
};
bool SafeMakeEntity(ScriptEntityBuf* b, void* ent) {
    static EntityCtorFn ctor = nullptr;
    static EntityDefCtorFn defCtor = nullptr;
    if (!ctor || !defCtor) {   // Script.dll laedt erst nach main(): bis dahin bei jedem Aufruf erneut suchen
        if (HMODULE script = GetModuleHandleA("Script.dll")) {
            ctor = reinterpret_cast<EntityCtorFn>(GetProcAddress(script, "??0Entity@@QAE@PBVeCEntity@@@Z"));
            defCtor = reinterpret_cast<EntityDefCtorFn>(GetProcAddress(script, "??0Entity@@QAE@XZ"));
        }
    }
    std::memset(b->raw, 0, sizeof b->raw);
    if (!ctor || !defCtor) return false;
    __try {
        if (ent)
            ctor(b->raw, ent);
        else
            defCtor(b->raw);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

int CallScriptScript(const SStr& name, const Entity& self, const Entity& other, int arg) {
    void* admin = ScriptAdmin();
    if (!admin || !g_a.callFromScript) return 0;
    if (!ScriptExistsScript(name)) {
        if (ScriptHost::s_current) ScriptHost::s_current->Log(std::string("CallScript: Skript '") + name.c_str() + "' gibt es nicht");
        return 0;
    }
    ScriptEntityBuf s, o;
    if (!SafeMakeEntity(&s, self.p) || !SafeMakeEntity(&o, other.p)) {
        if (ScriptHost::s_current) ScriptHost::s_current->Log("CallScript: Entity-Konstruktor der Script.dll fehlt oder schlug fehl");
        return 0;
    }
    bool ok = false;
    const int r = SafeCallFromScript(admin, &name, &s, &o, arg, &ok);
    if (!ok && !g_callFailLogged && ScriptHost::s_current) {
        g_callFailLogged = true;
        ScriptHost::s_current->Log(std::string("CallScript('") + name.c_str() + "'): Ausnahme im Spielskript abgefangen (weitere werden nicht gemeldet)");
    }
    return r;
}

std::vector<std::string> g_watch;   // beobachtete Skriptnamen (Gross-/Kleinschreibung egal)
volatile long g_watchCount = 0;
void WatchScriptScript(const SStr& name) {
    const std::string n = name.c_str();
    if (n.empty()) return;
    for (const std::string& w : g_watch)
        if (_stricmp(w.c_str(), n.c_str()) == 0) return;
    g_watch.push_back(n);
    InterlockedExchange(&g_watchCount, long(g_watch.size()));
    if (ScriptHost::s_current) ScriptHost::s_current->Log("WatchScript: '" + n + "' wird beobachtet (Override_Script/OnScript)");
}
bool IsWatched(const char* n) {
    if (!g_watchCount || !n) return false;
    for (const std::string& w : g_watch)
        if (_stricmp(w.c_str(), n) == 0) return true;
    return false;
}

// Ergebnis fuer das Spiel, wenn ein Override_Script das Original nicht aufruft (je verschachteltem Aufruf eins)
struct ResultFrame {
    bool set = false;
    int value = 0;
};
std::vector<ResultFrame> g_results;
void SetScriptResultScript(int v) {
    if (g_results.empty()) {
        if (ScriptHost::s_current) ScriptHost::s_current->Log("SetScriptResult: nur innerhalb von Override_Script");
        return;
    }
    g_results.back().set = true;
    g_results.back().value = v;
}

// ---- Hooks

ScriptHost* g_host = nullptr;
RunScriptFn o_run = nullptr;
RunFromScriptFn o_runFrom = nullptr;
DWORD g_gameThread = 0;
long g_depth = 0;
const char* kOverride = "bool Override_Script(const bCString &in, Entity, Entity, int)";
const char* kEvent = "void OnScript(const bCString &in, Entity, Entity, int, int)";

// ---- Messung
bool g_stats = false;
struct Stat {
    char name[64];
    unsigned hash;
    unsigned run, fromScript, foreign;
};
const int kStatSlots = 2048;
Stat* g_stat = nullptr;   // offene Adressierung; erst mit scriptlog=1 angelegt
int g_statUsed = 0;
unsigned long long g_hookTicks = 0, g_hookCalls = 0;
DWORD g_statStart = 0;

void Count(const char* n, bool fromScript, bool foreign) {
    if (!g_stat || !n) return;
    unsigned h = 2166136261u;
    for (const char* c = n; *c; ++c) h = (h ^ static_cast<unsigned char>(*c)) * 16777619u;
    for (int i = 0; i < kStatSlots; ++i) {
        Stat& s = g_stat[(h + unsigned(i)) % kStatSlots];
        if (s.name[0] == 0) {
            if (g_statUsed >= kStatSlots - 1) return;
            ++g_statUsed;
            s.hash = h;
            strncpy_s(s.name, n, _TRUNCATE);
        } else if (s.hash != h || std::strncmp(s.name, n, sizeof s.name - 1) != 0)
            continue;
        if (fromScript)
            ++s.fromScript;
        else
            ++s.run;
        if (foreign) ++s.foreign;
        return;
    }
}

void LogStats() {
    if (!g_stat || !g_host) return;
    std::vector<const Stat*> v;
    unsigned long long total = 0, foreign = 0;
    for (int i = 0; i < kStatSlots; ++i)
        if (g_stat[i].name[0]) {
            v.push_back(&g_stat[i]);
            total += g_stat[i].run + g_stat[i].fromScript;
            foreign += g_stat[i].foreign;
        }
    std::sort(v.begin(), v.end(), [](const Stat* a, const Stat* b) { return a->run + a->fromScript > b->run + b->fromScript; });
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    const double us = g_hookCalls ? double(g_hookTicks) * 1e6 / double(f.QuadPart) / double(g_hookCalls) : 0.0;
    char b[256];
    std::snprintf(b, sizeof b, "Skriptaufrufe seit %lu s: %llu Aufrufe, %d Namen, %llu aus fremden Threads, eigener Aufwand je Aufruf %.2f us",
                  (GetTickCount() - g_statStart) / 1000u, total, int(v.size()), foreign, us);
    g_host->Log(b);
    const size_t n = v.size() < 40 ? v.size() : 40;
    for (size_t i = 0; i < n; ++i) {
        std::snprintf(b, sizeof b, "  %-40s RunScript %u, RunScriptFromScript %u%s", v[i]->name, v[i]->run, v[i]->fromScript,
                      v[i]->foreign ? " (auch fremder Thread)" : "");
        g_host->Log(b);
    }
}

struct HookTimer {
    LARGE_INTEGER t0{};
    HookTimer() {
        if (g_stats) QueryPerformanceCounter(&t0);
    }
    void Stop() {
        if (!g_stats) return;
        LARGE_INTEGER t1;
        QueryPerformanceCounter(&t1);
        g_hookTicks += static_cast<unsigned long long>(t1.QuadPart - t0.QuadPart);
        ++g_hookCalls;
    }
};

// Gemeinsamer Ablauf beider Hooks fuer beobachtete Namen: Override-Kette, dann OnScript
template <class Orig>
int Dispatch(const void* name, void* self, void* other, int arg, Orig original) {
    const char* nm = SafeStrText(name, nullptr);   // Kopie fuer die Skripte: der Name des Aufrufers muss das Original nicht ueberleben
    const SStr copy(nm ? nm : "");
    EvArg a[] = {EvArg::Str(&copy), EvArg::Ent(self), EvArg::Ent(other), EvArg::U(static_cast<unsigned>(arg))};
    int result = 0, used = arg;   // used: Zahl, mit der das Original wirklich lief (SetOverrideArg)
    bool origRan = false;
    g_results.emplace_back();
    bool res = false, called = false;
    const bool taken = g_host->CallOverride(
        kOverride, a, 4,
        [&](const EvArg* ar) {
            used = static_cast<int>(ar[3].value);
            result = original(used);
            origRan = true;
            return result != 0;
        },
        &res, &called);
    const ResultFrame rf = g_results.back();
    g_results.pop_back();
    if (!taken) {
        result = original(arg);
        origRan = true;
    } else if (!origRan)
        result = rf.set ? rf.value : (res ? 1 : 0);
    if (origRan) {
        EvArg e[] = {EvArg::Str(&copy), EvArg::Ent(self), EvArg::Ent(other), EvArg::U(static_cast<unsigned>(used)), EvArg::U(static_cast<unsigned>(result))};
        g_host->CallEvent(kEvent, e, 5);
    }
    return result;
}

int __fastcall HRun(void* admin, void*, const void* name, void* spu) {
    HookTimer t;
    const bool foreign = g_gameThread && GetCurrentThreadId() != g_gameThread;
    const char* n = (g_stats || g_watchCount) ? SafeStrText(name, nullptr) : nullptr;
    if (g_stats) Count(n, false, foreign);
    const bool watch = !foreign && g_gameThread && g_depth < 8 && IsWatched(n);
    t.Stop();
    if (!watch) return o_run(admin, name, spu);
    ++g_depth;
    void* self = SafeSpuEnt(g_a.spuSelf, spu);
    void* other = SafeSpuEnt(g_a.spuOther, spu);
    const int arg = SafeSpuInt(spu);
    const int r = Dispatch(name, self, other, arg, [&](int x) {
        if (x == arg || !g_a.spuSetInt) return o_run(admin, name, spu);
        g_a.spuSetInt(spu, x);
        const int rr = o_run(admin, name, spu);
        g_a.spuSetInt(spu, arg);   // geaenderte Zahl nicht in der Verarbeitungseinheit stehen lassen
        return rr;
    });
    --g_depth;
    return r;
}

int __fastcall HRunFrom(void* admin, void*, const void* name, void* spu, void* self, void* other, int arg) {
    HookTimer t;
    const bool foreign = g_gameThread && GetCurrentThreadId() != g_gameThread;
    const char* n = (g_stats || g_watchCount) ? SafeStrText(name, nullptr) : nullptr;
    if (g_stats) Count(n, true, foreign);
    const bool watch = !foreign && g_gameThread && g_depth < 8 && IsWatched(n);
    t.Stop();
    if (!watch) return o_runFrom(admin, name, spu, self, other, arg);
    ++g_depth;
    const int r = Dispatch(name, EntityOfArg(self), EntityOfArg(other), arg,
                           [&](int x) { return o_runFrom(admin, name, spu, self, other, x); });
    --g_depth;
    return r;
}

// ---- KI-Skripte (API 41)
typedef bool(__thiscall* RunAIStackFn)(void*, const void*, void*, void*);
typedef bool(__thiscall* RunAISpuFn)(void*, const void*, void*);
RunAIStackFn o_runState = nullptr, o_runFunction = nullptr;
RunAISpuFn o_runCallback = nullptr, o_runRoutine = nullptr;
const int kAIState = 1, kAIFunction = 2, kAICallback = 3, kAIRoutine = 4;
const char* kOverrideAI = "bool Override_AI(const bCString &in, int, Entity, Entity)";
const char* kEventAI = "void OnAI(const bCString &in, int, Entity, Entity, int)";

// Der Name zeigt in den Zustandsstapel des NPCs (ProcessScript uebergibt &Eintrag.m_strScriptName, gemessen 2026-11-10). Zustaende und KI-Funktionen bauen
// den Stapel beim Laufen um, danach zeigt der Zeiger auf freigegebenen Speicher: im Spieltest kam OnAI fuer ZS_ObserveSuspect mit zerstoertem Namen an.
// Skripte bekommen darum eine Kopie des Namens; das Original bekommt den Zeiger des Spiels.
// Messzeilen: die ersten 8 Aufrufe je Art. Die Tiefe wird ueber ein Objekt zurueckgezaehlt.
int g_aiDiag[5] = {};
bool g_aiDeepLogged = false;
void AIDiag(int kind, const std::string& s) {
    if (kind < 0 || kind > 4 || g_aiDiag[kind] >= 8 * 5 || !g_host) return;
    ++g_aiDiag[kind];
    g_host->Log("KI-Messung: " + s);
}
struct DepthGuard {
    long before;
    DepthGuard() : before(g_depth) { ++g_depth; }
    ~DepthGuard() { g_depth = before; }
};
bool WatchAI(const void* name) {
    if (!g_watchCount || !g_gameThread || GetCurrentThreadId() != g_gameThread) return false;
    const char* n = SafeStrText(name, nullptr);
    if (!IsWatched(n)) return false;
    if (g_depth >= 8) {
        if (!g_aiDeepLogged && g_host) g_host->Log(std::string("KI: '") + (n ? n : "?") + "' ausgelassen, Verschachtelung " + std::to_string(g_depth) + " (nur einmal gemeldet)");
        g_aiDeepLogged = true;
        return false;
    }
    return true;
}
template <class Orig>
bool DispatchAI(const void* name, int kind, void* spu, Orig original) {
    DepthGuard guard;
    const char* nm = SafeStrText(name, nullptr);
    const SStr copy(nm ? nm : "");
    const std::string tag = std::string(nm ? nm : "?") + " (Art " + std::to_string(kind) + ", Tiefe " + std::to_string(guard.before) + ")";
    AIDiag(kind, "Beginn " + tag);
    void* self = SafeSpuEnt(g_a.spuSelf, spu);
    void* target = SafeSpuEnt(g_a.spuTarget, spu);
    EvArg a[] = {EvArg::Str(&copy), EvArg::U(unsigned(kind)), EvArg::Ent(self), EvArg::Ent(target)};
    bool result = false, origRan = false, res = false, called = false;
    const bool taken = g_host->CallOverride(
        kOverrideAI, a, 4,
        [&](const EvArg*) {
            AIDiag(kind, "Original startet " + tag);
            result = original();
            origRan = true;
            AIDiag(kind, std::string("Original zurueck ") + tag + ", Ergebnis " + (result ? "1" : "0"));
            return result;
        },
        &res, &called);
    AIDiag(kind, std::string("Override fertig ") + tag + ": uebernommen " + (taken ? "1" : "0") + ", Original lief " + (origRan ? "1" : "0") + ", Rueckgabe " +
           (res ? "1" : "0"));
    if (!taken) {
        result = original();
        origRan = true;
    } else
        result = res;
    if (origRan) {
        EvArg e[] = {EvArg::Str(&copy), EvArg::U(unsigned(kind)), EvArg::Ent(self), EvArg::Ent(target), EvArg::U(result ? 1u : 0u)};
        g_host->CallEvent(kEventAI, e, 5);
        AIDiag(kind, "OnAI gesendet " + tag);
    }
    return result;
}
bool __fastcall HRunState(void* admin, void*, const void* name, void* stack, void* spu) {
    if (!WatchAI(name)) return o_runState(admin, name, stack, spu);
    return DispatchAI(name, kAIState, spu, [&] { return o_runState(admin, name, stack, spu); });
}
bool __fastcall HRunFunction(void* admin, void*, const void* name, void* stack, void* spu) {
    if (!WatchAI(name)) return o_runFunction(admin, name, stack, spu);
    return DispatchAI(name, kAIFunction, spu, [&] { return o_runFunction(admin, name, stack, spu); });
}
bool __fastcall HRunCallback(void* admin, void*, const void* name, void* spu) {
    if (!WatchAI(name)) return o_runCallback(admin, name, spu);
    return DispatchAI(name, kAICallback, spu, [&] { return o_runCallback(admin, name, spu); });
}
bool __fastcall HRunRoutine(void* admin, void*, const void* name, void* spu) {
    if (!WatchAI(name)) return o_runRoutine(admin, name, spu);
    return DispatchAI(name, kAIRoutine, spu, [&] { return o_runRoutine(admin, name, spu); });
}

}   // namespace

// Spielskript nach Namen fuer andere Bindings (z. B. Entity::Kill): wie CallScript, self/other als eCEntity* (nullptr = None)
int CallGameScript(const char* name, void* self, void* other, int arg) {
    return CallScriptScript(SStr(name), Entity{self}, Entity{other}, arg);
}

// Script.dll-Entity fuer andere Bindings (z. B. StartEffect): buf muss mindestens kScriptEntityBytes gross sein
bool MakeScriptEntity(void* buf, void* ent) {
    static_assert(sizeof(ScriptEntityBuf) >= kScriptEntityBytes, "Puffergroesse");
    return SafeMakeEntity(static_cast<ScriptEntityBuf*>(buf), ent);
}

bool RegisterScriptCallBindings(asIScriptEngine* e) {
    bool ok = true;
    G3ASM_REG(e->RegisterGlobalFunction("int CallScript(const bCString &in name, const Entity &in self, const Entity &in other, int arg = 0)",
                                      asFUNCTION(CallScriptScript), asCALL_CDECL));   // experimentell
    G3ASM_REG(e->RegisterGlobalFunction("bool ScriptExists(const bCString &in name)", asFUNCTION(ScriptExistsScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bCString GetScriptDll(const bCString &in name)", asFUNCTION(GetScriptDllScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("void WatchScript(const bCString &in name)", asFUNCTION(WatchScriptScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("void SetScriptResult(int value)", asFUNCTION(SetScriptResultScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalProperty("const int AI_STATE", const_cast<int*>(&kAIState)));
    G3ASM_REG(e->RegisterGlobalProperty("const int AI_FUNCTION", const_cast<int*>(&kAIFunction)));
    G3ASM_REG(e->RegisterGlobalProperty("const int AI_CALLBACK", const_cast<int*>(&kAICallback)));
    G3ASM_REG(e->RegisterGlobalProperty("const int AI_ROUTINE", const_cast<int*>(&kAIRoutine)));
    return ok;
}

void SetScriptCallOptions(bool stats) { g_stats = stats; }

bool OnGameThread() { return g_gameThread != 0 && GetCurrentThreadId() == g_gameThread; }

void ScriptCallsReport() {
    if (g_stats && g_stat && g_hookCalls) LogStats();
}

// Einmal je Bild aus dem Spiel-Takt: merkt den Thread der Skripte; mit scriptlog=1 alle 60 s die Messung
void ScriptCallsFrame() {
    if (!g_gameThread) g_gameThread = GetCurrentThreadId();
    if (!g_stats || !g_stat) return;
    static DWORD last = GetTickCount();
    if (GetTickCount() - last >= 60000u) {
        last = GetTickCount();
        LogStats();
    }
}

int InstallScriptCallHooks(ScriptHost& host) {
    HMODULE game = GetModuleHandleA("Game.dll");
    if (!game) return 0;
    g_host = &host;
    ResolveApi();
    if (g_stats) {
        g_stat = static_cast<Stat*>(VirtualAlloc(nullptr, sizeof(Stat) * kStatSlots, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        g_statStart = GetTickCount();
    }
    int n = 0;
    n += HookExport(game, "?RunScript@gCScriptAdmin@@QAEHABVbCString@@PAVgCScriptProcessingUnit@@@Z", reinterpret_cast<void*>(&HRun),
                    reinterpret_cast<void**>(&o_run), host);
    n += HookExport(game, "?RunScriptFromScript@gCScriptAdmin@@QAEHABVbCString@@PAVgCScriptProcessingUnit@@PAX2H@Z",
                    reinterpret_cast<void*>(&HRunFrom), reinterpret_cast<void**>(&o_runFrom), host);
    const char* aiState = "?RunScriptState@gCScriptAdmin@@QAE_NABVbCString@@AAV?$bTObjStack@UgScriptRunTimeSingleState@@@@PAVgCScriptProcessingUnit@@@Z";
    const char* aiFunction = "?RunScriptFunction@gCScriptAdmin@@QAE_NABVbCString@@AAV?$bTObjStack@UgScriptRunTimeSingleState@@@@PAVgCScriptProcessingUnit@@@Z";
    int ai = 0;
    ai += HookExport(game, aiState, reinterpret_cast<void*>(&HRunState), reinterpret_cast<void**>(&o_runState), host);
    ai += HookExport(game, aiFunction, reinterpret_cast<void*>(&HRunFunction), reinterpret_cast<void**>(&o_runFunction), host);
    ai += HookExport(game, "?RunScriptCallback@gCScriptAdmin@@QAE_NABVbCString@@PAVgCScriptProcessingUnit@@@Z", reinterpret_cast<void*>(&HRunCallback),
                     reinterpret_cast<void**>(&o_runCallback), host);
    ai += HookExport(game, "?RunScriptRoutine@gCScriptAdmin@@QAE_NABVbCString@@PAVgCScriptProcessingUnit@@@Z", reinterpret_cast<void*>(&HRunRoutine),
                     reinterpret_cast<void**>(&o_runRoutine), host);
    n += ai;
    char b[200];
    std::snprintf(b, sizeof b, "Skriptaufrufe: %d von 6 Hooks aktiv (RunScript, RunScriptFromScript, KI: %d von 4)%s; CallScript %s", n, ai,
                  g_stats ? ", Messung an (scriptlog=1)" : "", g_a.callFromScript ? "bereit" : "NICHT verfuegbar");
    host.Log(b);
    return n;
}

}   // namespace g3asm
