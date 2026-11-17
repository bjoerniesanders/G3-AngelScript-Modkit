// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Additional permissions: LICENSE-EXCEPTIONS.md
// AI tracer (roadmap 6.2, only log, no script interface): writes changes of AI state, AI task, AI routine, AI mode and action
// of the NPCs to the log, to see when and why the AI stops something (for example animations). Off by default. g3asm.ini:
//   [g3asm] aitrace=1            turn on (the hooks are installed only in this case)
//           aitracefilter=Diego  only NPCs with a name that contains this text (no case sensitivity; empty = all)
//           aitracemax=2000      maximum number of lines for each game start, then one final line
// Installed (Game.dll exports, thiscall, one reference argument):
//   gCScriptRoutine_PS::SetCurrentState(const bCString&), SetCurrentTask(const bCString&), SetRoutine(const bCScriptString&),
//   SetAIMode(const bTPropertyContainer<gEAIMode>&), SetAction(const bTPropertyContainer<gEAction>&); gCNavigation_PS::SetRoutine(const bCString&)
// bCScriptString derives from bCString; bTPropertyContainer<Enum> = {vtable, value} (8 bytes, schema dump). The tracer reports only a changed value
// for each NPC and type (repeats of the same value are only counted). It is not measured yet what causes the calls in the game.
// Also (separate switch, below): [g3asm] anispeedlog=1 measures eCWrapper_emfx2Actor::SetPlaySpeed/GetPlaySpeed (roadmap 6.3).
#include <windows.h>
#include <intrin.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "sb_api.h"
#include "script_host.h"

namespace g3asm {
namespace {

ScriptHost* g_host = nullptr;
bool g_enabled = false;
std::string g_filter;   // klein geschrieben
int g_max = 2000, g_lines = 0;
unsigned g_calls = 0, g_repeats = 0;
CRITICAL_SECTION g_lock;
bool g_lockInit = false;

enum Kind { kState, kTask, kRoutine, kNavRoutine, kMode, kAction, kKinds };
const char* const kKindName[kKinds] = {"Zustand", "Aufgabe", "Routine", "Navigation-Routine", "KI-Modus", "Aktion"};
std::unordered_map<const void*, std::string> g_last[kKinds];   // entity -> last reported value

typedef const void*(__thiscall* EntNameFn)(const void*);
typedef void*(__thiscall* PsEntityFn)(void*);
EntNameFn g_entName = nullptr;
PsEntityFn g_psEntity = nullptr;

const char* SafeEntName(const void* e) {
    if (!e || !g_entName) return "?";
    __try {
        const void* n = g_entName(e);
        const char* t = n ? g_sb.strGetText(n) : nullptr;
        return t && *t ? t : "?";
    } __except (EXCEPTION_EXECUTE_HANDLER) { return "?"; }
}
void* SafePsEntity(void* ps) {
    if (!ps || !g_psEntity) return nullptr;
    __try {
        return g_psEntity(ps);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
int ContainerValue(const void* c) {   // bTPropertyContainer<Enum>: value after the vtable pointer
    return c && ReadableMem(c, 8) ? reinterpret_cast<const int*>(c)[1] : -1;
}

void Note(void* ps, Kind kind, const std::string& value) {
    if (!g_host || !g_lockInit) return;
    void* ent = SafePsEntity(ps);
    std::string name = SafeEntName(ent);
    EnterCriticalSection(&g_lock);
    ++g_calls;
    bool report = false, last = false;
    if (g_lines < g_max) {
        std::string lower = name;
        for (char& c : lower) c = char(std::tolower(static_cast<unsigned char>(c)));
        if (g_filter.empty() || lower.find(g_filter) != std::string::npos) {
            std::string& prev = g_last[kind][ent ? ent : ps];
            if (prev == value) {
                ++g_repeats;
            } else {
                prev = value;
                report = true;
                last = ++g_lines == g_max;
            }
        }
    }
    LeaveCriticalSection(&g_lock);
    if (!report) return;
    char b[400];
    std::snprintf(b, sizeof b, "KI: %.80s %s -> '%.200s'", name.c_str(), kKindName[kind], value.c_str());
    g_host->Log(b);
    if (last) {
        std::snprintf(b, sizeof b, "KI-Tracer: Obergrenze %d Zeilen erreicht, weitere Wechsel werden nicht mehr gemeldet (%u Aufrufe, %u Wiederholungen)", g_max,
                      g_calls, g_repeats);
        g_host->Log(b);
    }
}

typedef void(__thiscall* SetRefFn)(void*, const void*);
SetRefFn o_state = nullptr, o_task = nullptr, o_routine = nullptr, o_navRoutine = nullptr, o_mode = nullptr, o_action = nullptr;

void __fastcall HState(void* ps, void*, const void* s) {
    Note(ps, kState, SafeStrText(s, ""));
    o_state(ps, s);
}
void __fastcall HTask(void* ps, void*, const void* s) {
    Note(ps, kTask, SafeStrText(s, ""));
    o_task(ps, s);
}
void __fastcall HRoutine(void* ps, void*, const void* s) {
    Note(ps, kRoutine, SafeStrText(s, ""));
    o_routine(ps, s);
}
void __fastcall HNavRoutine(void* ps, void*, const void* s) {
    Note(ps, kNavRoutine, SafeStrText(s, ""));
    o_navRoutine(ps, s);
}
void __fastcall HMode(void* ps, void*, const void* c) {
    Note(ps, kMode, std::to_string(ContainerValue(c)));
    o_mode(ps, c);
}
void __fastcall HAction(void* ps, void*, const void* c) {
    Note(ps, kAction, std::to_string(ContainerValue(c)));
    o_action(ps, c);
}

// ---- measurement of the playback speed (roadmap 6.3, ini [g3asm] anispeedlog=1): Engine.dll eCWrapper_emfx2Actor::SetPlaySpeed(eEMotionType, float)
// and GetPlaySpeed(eEMotionType) (both imported by Game.dll). Goal: find out which code sets the attack speed, before there is an override.
// First measurement (2026-11-11): single values filled 300 lines in 2 s (continuous values from Game.dll+0xDC5AA). Now: a new call location is reported
// immediately (maximum 100). If not, each 10 s one summary for each call location and type (count, smallest and largest value, number of values > 1.05),
// maximum 120 windows.
bool g_aniSpeed = false;
struct SpeedStat {
    unsigned count = 0, above = 0;
    float lo = 0, hi = 0;
};
std::unordered_map<std::string, SpeedStat> g_aniWin;   // "Set Game.dll+0x... type 0" -> values in the current 10 s window
std::unordered_map<std::string, bool> g_aniSites;
std::unordered_map<void*, std::string> g_siteCache;
DWORD g_aniWinStart = 0;
int g_aniWindows = 0;
typedef void(__thiscall* SetSpeedFn)(void*, int, float);
typedef float(__thiscall* GetSpeedFn)(const void*, int);
SetSpeedFn o_setSpeed = nullptr;
GetSpeedFn o_getSpeed = nullptr;

std::string Site(void* ret) {
    HMODULE m = nullptr;
    char name[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCSTR>(ret), &m) && m) {
        GetModuleFileNameA(m, name, MAX_PATH);
        const char* base = std::strrchr(name, '\\');
        char b[MAX_PATH + 32];
        std::snprintf(b, sizeof b, "%s+0x%X", base ? base + 1 : name, unsigned(reinterpret_cast<uintptr_t>(ret) - reinterpret_cast<uintptr_t>(m)));
        return b;
    }
    char b[32];
    std::snprintf(b, sizeof b, "%p", ret);
    return b;
}
void NoteSpeed(const char* what, void* ret, int type, float value) {
    if (!g_host || !g_lockInit || g_aniWindows >= 120) return;
    std::vector<std::string> out;
    EnterCriticalSection(&g_lock);
    auto sc = g_siteCache.find(ret);
    if (sc == g_siteCache.end()) sc = g_siteCache.emplace(ret, Site(ret)).first;
    char key[200];
    std::snprintf(key, sizeof key, "%s %s Art %d", what, sc->second.c_str(), type);
    const std::string siteKey = std::string(what) + " " + sc->second;
    if (g_aniSites.size() < 100 && g_aniSites.emplace(siteKey, true).second)
        out.push_back("Abspielgeschwindigkeit: neue Aufrufstelle " + std::string(key) + ", Wert " + std::to_string(value));
    SpeedStat& s = g_aniWin[key];
    if (s.count == 0) s.lo = s.hi = value;
    if (value < s.lo) s.lo = value;
    if (value > s.hi) s.hi = value;
    if (value > 1.05f) ++s.above;
    ++s.count;
    const DWORD now = GetTickCount();
    if (!g_aniWinStart) g_aniWinStart = now;
    if (now - g_aniWinStart >= 10000u) {
        ++g_aniWindows;
        for (const auto& kv : g_aniWin) {
            char b[300];
            std::snprintf(b, sizeof b, "Abspielgeschwindigkeit 10 s: %s: %ux, %.3f bis %.3f, %u ueber 1,05", kv.first.c_str(), kv.second.count, kv.second.lo,
                          kv.second.hi, kv.second.above);
            out.push_back(b);
        }
        if (g_aniWindows == 120) out.push_back("Abspielgeschwindigkeit: 120 Fenster erreicht, Messung endet");
        g_aniWin.clear();
        g_aniWinStart = now;
    }
    LeaveCriticalSection(&g_lock);
    for (const std::string& l : out) g_host->Log(l);
}
void __fastcall HSetSpeed(void* actor, void*, int type, float speed) {
    NoteSpeed("Set", _ReturnAddress(), type, speed);
    o_setSpeed(actor, type, speed);
}
float __fastcall HGetSpeed(const void* actor, void*, int type) {
    const float r = o_getSpeed(actor, type);
    NoteSpeed("Get", _ReturnAddress(), type, r);
    return r;
}

// ---- measurement PlayMotion (2026-11-11, same switch anispeedlog): SetPlaySpeed sets only the walk movement (Game.dll+0xDC5AA) and the turn (+0xDA031/+0xDA21A).
// Combat movements start through Engine.dll eCVisualAnimation_PS::PlayMotion(eEMotionType, eSMotionDesc*). The SDK gives no layout for eSMotionDesc.
// At Game.dll+0xDA1A5, the code sets two 1.0f values in it, among others (candidate speed). Reported: for each call location, type and field pattern of the first 12
// double words (pointers as "p", numbers as numbers) one line with the entity name, maximum 400 lines.
typedef void(__thiscall* PlayMotionFn)(void*, int, void*);
PlayMotionFn o_playMotion = nullptr;
std::unordered_map<std::string, bool> g_motionSeen;
int g_motionLines = 0;

std::string DescPattern(const void* desc) {
    if (!desc || !ReadableMem(desc, 48)) return "unlesbar";
    const unsigned* w = static_cast<const unsigned*>(desc);
    std::string s;
    for (int i = 0; i < 12; ++i) {
        char b[24];
        const unsigned v = w[i];
        float f;
        std::memcpy(&f, &v, 4);
        const unsigned exp = (v >> 23) & 0xFF;
        if (v == 0)
            std::snprintf(b, sizeof b, "0");
        else if (v == 0xFFFFFFFFu)
            std::snprintf(b, sizeof b, "-1");
        else if (v < 0x10000u)
            std::snprintf(b, sizeof b, "%u", v);
        else if (exp >= 100 && exp <= 140)   // approximately 1e-8 to 1e4
            std::snprintf(b, sizeof b, "%.3gf", f);
        else
            std::snprintf(b, sizeof b, "p");
        if (i) s += ' ';
        s += b;
    }
    return s;
}
void __fastcall HPlayMotion(void* anim, void*, int type, void* desc) {
    void* ret = _ReturnAddress();
    if (g_host && g_lockInit && g_motionLines < 400) {
        const std::string pat = DescPattern(desc);
        EnterCriticalSection(&g_lock);
        auto sc = g_siteCache.find(ret);
        if (sc == g_siteCache.end()) sc = g_siteCache.emplace(ret, Site(ret)).first;
        const std::string key = sc->second + " Art " + std::to_string(type) + " | " + pat;
        const bool fresh = g_motionLines < 400 && g_motionSeen.emplace(key, true).second;
        if (fresh) ++g_motionLines;
        LeaveCriticalSection(&g_lock);
        if (fresh) g_host->Log("PlayMotion: " + std::string(SafeEntName(SafePsEntity(anim))) + " " + key);
    }
    o_playMotion(anim, type, desc);
}

}   // namespace

void SetAiTraceOptions(bool enabled, const char* filter, int maxLines) {
    g_enabled = enabled;
    g_filter = filter ? filter : "";
    for (char& c : g_filter) c = char(std::tolower(static_cast<unsigned char>(c)));
    g_max = maxLines > 0 ? maxLines : 2000;
}
void SetAniSpeedLog(bool enabled) { g_aniSpeed = enabled; }

int InstallAiTrace(ScriptHost& host) {
    if ((!g_enabled && !g_aniSpeed) || g_host) return 0;
    g_host = &host;
    InitializeCriticalSection(&g_lock);
    g_lockInit = true;
    HMODULE eng = GetModuleHandleA("Engine.dll"), game = GetModuleHandleA("Game.dll");
    if (eng) {
        g_entName = reinterpret_cast<EntNameFn>(GetProcAddress(eng, "?GetName@eCEntity@@QBEABVbCString@@XZ"));
        g_psEntity = reinterpret_cast<PsEntityFn>(GetProcAddress(eng, "?GetEntity@eCEntityPropertySet@@UAEPAVeCEntity@@XZ"));
    }
    if (g_aniSpeed) {
        int s = HookExport(eng, "?SetPlaySpeed@eCWrapper_emfx2Actor@@QAEXW4eEMotionType@1@M@Z", reinterpret_cast<void*>(&HSetSpeed),
                           reinterpret_cast<void**>(&o_setSpeed), host);
        s += HookExport(eng, "?GetPlaySpeed@eCWrapper_emfx2Actor@@QBEMW4eEMotionType@1@@Z", reinterpret_cast<void*>(&HGetSpeed),
                        reinterpret_cast<void**>(&o_getSpeed), host);
        s += HookExport(eng, "?PlayMotion@eCVisualAnimation_PS@@QAEXW4eEMotionType@eCWrapper_emfx2Actor@@PAUeSMotionDesc@eCWrapper_emfx2Motion@@@Z",
                        reinterpret_cast<void*>(&HPlayMotion), reinterpret_cast<void**>(&o_playMotion), host);
        host.Log("Abspielgeschwindigkeit-Messung: " + std::to_string(s) + " von 3 Hooks aktiv (SetPlaySpeed, GetPlaySpeed, PlayMotion)");
    }
    if (!g_enabled) return 0;
    int n = 0;
    n += HookExport(game, "?SetCurrentState@gCScriptRoutine_PS@@QAEXABVbCString@@@Z", reinterpret_cast<void*>(&HState), reinterpret_cast<void**>(&o_state),
                    host);
    n += HookExport(game, "?SetCurrentTask@gCScriptRoutine_PS@@QAEXABVbCString@@@Z", reinterpret_cast<void*>(&HTask), reinterpret_cast<void**>(&o_task), host);
    n += HookExport(game, "?SetRoutine@gCScriptRoutine_PS@@QAEXABVbCScriptString@@@Z", reinterpret_cast<void*>(&HRoutine),
                    reinterpret_cast<void**>(&o_routine), host);
    n += HookExport(game, "?SetRoutine@gCNavigation_PS@@QAEXABVbCString@@@Z", reinterpret_cast<void*>(&HNavRoutine), reinterpret_cast<void**>(&o_navRoutine),
                    host);
    n += HookExport(game, "?SetAIMode@gCScriptRoutine_PS@@QAEXABV?$bTPropertyContainer@W4gEAIMode@@@@@Z", reinterpret_cast<void*>(&HMode),
                    reinterpret_cast<void**>(&o_mode), host);
    n += HookExport(game, "?SetAction@gCScriptRoutine_PS@@QAEXABV?$bTPropertyContainer@W4gEAction@@@@@Z", reinterpret_cast<void*>(&HAction),
                    reinterpret_cast<void**>(&o_action), host);
    char b[200];
    std::snprintf(b, sizeof b, "KI-Tracer: %d von 6 Hooks aktiv (Filter '%s', hoechstens %d Zeilen)", n, g_filter.c_str(), g_max);
    host.Log(b);
    return n;
}

}   // namespace g3asm
