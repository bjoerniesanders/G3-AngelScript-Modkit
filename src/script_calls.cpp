// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Additional permissions: LICENSE-EXCEPTIONS.md
// Game scripts by name (script interface from version 32, roadmap 7.1).
// The game scripts of the Script_*.dll files (for example "AssessHit", "MagicTeleport", "SetHitPoints") are in the table gCScriptAdmin::m_Scripts (name -> function
// int __stdcall(gCScriptProcessingUnit*, Entity* self, Entity* other, int arg)). Each call by name goes through one of two exports of Game.dll
// (measured in the assembler code of Game.dll 2026-11-09):
//   RunScriptFromScript(name, SPU, self, other, arg) - self and other as Script.dll entity objects; CallScriptFromScript (exported) ends here,
//   RunScript(name, SPU)                             - self, other and number are in the processing unit; gCScriptAdmin::CallScript (virtual) ends here.
// The two look up the name in m_Scripts and call the function. Game.dll has no other path into the table. Code that replaces the function in the
// table (as script plugins such as TeleportPartyMembers do) is thus BEHIND the hooks of the modkit: Override_Script also sees their scripts.
//
// For mods:
//   int CallScript(name, self, other, arg)  - call a script (through CallScriptFromScript, as a game script does)
//   bool ScriptExists(name), bCString GetScriptDll(name)
//   WatchScript(name)                        - the modkit calls Override_Script/OnScript only for watched names (if not, each script call costs time)
//   bool Override_Script(const bCString &in name, Entity self, Entity other, int arg)   - Super() = original; SetOverrideArg(3, x) changes arg
//   void OnScript(const bCString &in name, Entity self, Entity other, int arg, int result)  - after it, only if the original ran
//   SetScriptResult(int)                     - result for the game if Override_Script does NOT call the original (if not set, 1 for true, 0 for false)
// Measurement (ini [g3asm] scriptlog=1): each 60 s, the most frequent script names for each path, calls from other threads and the time in the hooks of the modkit.
//
// AI scripts (API 41): in addition to m_Scripts, gCScriptAdmin has four more tables (measured in the assembler code of Game.dll 2026-11-10): states (+0x1B0, "ZS_..."),
// AI functions (+0x1C0, "_AI_..."), callbacks (+0x1D0) and routines (+0x1E0). For each table, there is exactly one export that looks up and calls:
//   RunScriptState/RunScriptFunction(name, state stack, SPU) -> bool, called by gCScriptProcessingUnit::ProcessScript and from the script DLLs,
//   RunScriptCallback/RunScriptRoutine(name, SPU) -> bool, called by RunLocalCallback/RunTaskCallback or by the daily routine detection.
// States and AI functions run in sections across many frames: false = not complete yet (continue in the next frame), true = complete. Thus the game calls a state
// again in each frame. The game reports unknown names and gives true.
// For mods (same name list as WatchScript):
//   bool Override_AI(const bCString &in name, int kind, Entity self, Entity target)    - kind AI_STATE/AI_FUNCTION/AI_CALLBACK/AI_ROUTINE; Super() = original;
//                                                                                         the return value goes to the game (also without Super)
//   void OnAI(const bCString &in name, int kind, Entity self, Entity target, int result) - after it, only if the original ran
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
        return false;   // later again (test program: never available)
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
// gCScriptAdmin exists for the full session (engine module); find it 1 time
void* ScriptAdmin() {
    if (!ResolveApi()) return nullptr;
    if (!g_a.admin) {
        SStr n("gCScriptAdmin");
        g_a.admin = SafeFindAdmin(&n);
    }
    return g_a.admin;
}

// eCEntity* from a script argument: normally a Script.dll entity object (first field eCEntity*, no vtable); if a caller gives an eCEntity* directly
// (first field = vtable in Engine.dll), that pointer itself
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

// Script.dll entity object for the call: 0xA8 bytes (measured on the constructor in Script.dll 2026-11-09: first field eCEntity*, then 41 cached
// property set pointers that Entity(eCEntity const*) fills). Without them, scripts such as GetStaminaPoints read 0. The destructor is empty (only ret).
typedef void*(__thiscall* EntityCtorFn)(void*, const void*);
typedef void*(__thiscall* EntityDefCtorFn)(void*);
struct ScriptEntityBuf {
    alignas(8) unsigned char raw[0x100];
};
bool SafeMakeEntity(ScriptEntityBuf* b, void* ent) {
    static EntityCtorFn ctor = nullptr;
    static EntityDefCtorFn defCtor = nullptr;
    if (!ctor || !defCtor) {   // Script.dll loads only after main(): until then, search again at each call
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

// result for the game if an Override_Script does not call the original (one for each nested call)
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
Stat* g_stat = nullptr;   // open addressing; made only with scriptlog=1
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

// shared sequence of the two hooks for watched names: override chain, then OnScript
template <class Orig>
int Dispatch(const void* name, void* self, void* other, int arg, Orig original) {
    const char* nm = SafeStrText(name, nullptr);   // copy for the scripts: the name of the caller does not have to outlive the original
    const SStr copy(nm ? nm : "");
    EvArg a[] = {EvArg::Str(&copy), EvArg::Ent(self), EvArg::Ent(other), EvArg::U(static_cast<unsigned>(arg))};
    int result = 0, used = arg;   // used: number with which the original really ran (SetOverrideArg)
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
        g_a.spuSetInt(spu, arg);   // do not leave the changed number in the processing unit
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

// The name points into the state stack of the NPC (ProcessScript gives &entry.m_strScriptName, measured 2026-11-10). States and AI functions change
// the stack while they run. After that, the pointer points to freed memory: in the game test, OnAI for ZS_ObserveSuspect came with a damaged name.
// Thus scripts get a copy of the name. The original gets the pointer of the game.
// Measurement lines (only with ini scriptlog=1): the first 40 calls for each kind. An object counts the depth back.
int g_aiDiag[5] = {};
bool g_aiDeepLogged = false;
void AIDiag(int kind, const std::string& s) {
    if (!g_stats || kind < 0 || kind > 4 || g_aiDiag[kind] >= 8 * 5 || !g_host) return;
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

// game script by name for other bindings (for example Entity::Kill): as CallScript, self and other as eCEntity* (nullptr = None)
int CallGameScript(const char* name, void* self, void* other, int arg) {
    return CallScriptScript(SStr(name), Entity{self}, Entity{other}, arg);
}

// Script.dll entity for other bindings (for example StartEffect): buf must be at least kScriptEntityBytes large
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
// different from !OnGameThread(): while the game thread is not known yet (before the first frame, test program), no thread counts as different
bool OnForeignThread() { return g_gameThread != 0 && GetCurrentThreadId() != g_gameThread; }

void ScriptCallsReport() {
    if (g_stats && g_stat && g_hookCalls) LogStats();
}

// 1 time each frame from the game cycle: keeps the thread of the scripts; with scriptlog=1, the measurement each 60 s
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
