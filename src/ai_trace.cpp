// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// KI-Tracer (Roadmap 6.2, nur Protokoll, keine Skript-Schnittstelle): schreibt Wechsel von KI-Zustand, -Aufgabe, -Routine, KI-Modus und Aktion
// der NPCs ins Protokoll, um zu sehen, wann und warum die KI etwas abbricht (z. B. Animationen). Standard aus; g3asm.ini:
//   [patch] aitrace=1            einschalten (Hooks werden nur dann eingehaengt)
//           aitracefilter=Diego  nur NPCs, deren Name diesen Text enthaelt (Gross-/Kleinschreibung egal; leer = alle)
//           aitracemax=2000      hoechstens so viele Zeilen je Spielstart, danach eine Schlusszeile
// Eingehaengt (Game.dll-Exporte, thiscall, ein Referenzargument):
//   gCScriptRoutine_PS::SetCurrentState(const bCString&), SetCurrentTask(const bCString&), SetRoutine(const bCScriptString&),
//   SetAIMode(const bTPropertyContainer<gEAIMode>&), SetAction(const bTPropertyContainer<gEAction>&); gCNavigation_PS::SetRoutine(const bCString&)
// bCScriptString erbt von bCString; bTPropertyContainer<Enum> = {vtable, Wert} (8 Byte, Schema-Dump). Gemeldet wird nur ein geaenderter Wert
// je NPC und Art (Wiederholungen desselben Werts zaehlen nur mit). Was die Aufrufe im Spiel ausloest, ist noch nicht gemessen.
// Ausserdem (eigener Schalter, unten): [patch] anispeedlog=1 misst eCWrapper_emfx2Actor::SetPlaySpeed/GetPlaySpeed (Roadmap 6.3).
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
std::unordered_map<const void*, std::string> g_last[kKinds];   // Entity -> zuletzt gemeldeter Wert

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
int ContainerValue(const void* c) {   // bTPropertyContainer<Enum>: Wert hinter dem vtable-Zeiger
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

// ---- Messung Abspielgeschwindigkeit (Roadmap 6.3, ini [patch] anispeedlog=1): Engine.dll eCWrapper_emfx2Actor::SetPlaySpeed(eEMotionType, float)
// und GetPlaySpeed(eEMotionType) (beide von Game.dll importiert). Ziel: herausfinden, wer die Angriffsgeschwindigkeit setzt, bevor es einen Override gibt.
// Erste Messung (2026-11-11): Einzelwerte fuellten 300 Zeilen in 2 s (fliessende Werte von Game.dll+0xDC5AA). Jetzt: eine neue Aufrufstelle wird sofort
// gemeldet (hoechstens 100), sonst alle 10 s je Aufrufstelle und Art eine Zusammenfassung (Anzahl, kleinster/groesster Wert, Zahl der Werte > 1,05),
// hoechstens 120 Fenster.
bool g_aniSpeed = false;
struct SpeedStat {
    unsigned count = 0, above = 0;
    float lo = 0, hi = 0;
};
std::unordered_map<std::string, SpeedStat> g_aniWin;   // "Set Game.dll+0x... Art 0" -> Werte im laufenden 10-s-Fenster
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

// ---- Messung PlayMotion (2026-11-11, gleicher Schalter anispeedlog): SetPlaySpeed setzt nur Laufbewegung (Game.dll+0xDC5AA) und Drehen (+0xDA031/+0xDA21A),
// Kampfbewegungen starten ueber Engine.dll eCVisualAnimation_PS::PlayMotion(eEMotionType, eSMotionDesc*). Der Aufbau von eSMotionDesc ist im SDK leer;
// an Game.dll+0xDA1A5 werden darin u. a. zwei 1.0f gesetzt (Kandidat Geschwindigkeit). Gemeldet: je Aufrufstelle, Art und Feldmuster der ersten 12
// Doppelwoerter (Zeiger als "p", Zahlen als Zahl) eine Zeile mit dem Entity-Namen, hoechstens 400 Zeilen.
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
        else if (exp >= 100 && exp <= 140)   // etwa 1e-8 bis 1e4
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
