// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 OG3-Patch authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// Weitere Ereignisse aus exportierten Funktionen (Skript-Schnittstelle ab Version 35, Roadmap 7.3; Beruehrung, Haendlerware, Verwandlung ab 37).
//   Konsole:      Engine.dll eCConsole::OnExecute(const bCString &eingabe, bCString &ausgabe) -> bool
//                 Override_ConsoleCommand(zeile): true ohne Super() = Befehl gehoert dem Mod; Antwort mit SetConsoleReply(text)
//   Quest-Status: Game.dll gCQuest_PS::SetStatus(const gEQuestStatus &) -> bool
//                 Override_QuestStatus(name, alt, neu): SetOverrideArg(2, x) aendert den neuen Status, ohne Super() bleibt er; danach OnQuestStatus(name, alt, neu)
// Beide laufen nur im Spiel-Thread in die Skripte (OnGameThread). Messung: die ersten Aufrufe stehen im Protokoll.
#include <windows.h>

#include <angelscript.h>

#include <cstdio>
#include <cstring>
#include <string>

#include <MinHook.h>

#include "sb_api.h"
#include "script_host.h"

namespace og3p {
namespace {

ScriptHost* g_host = nullptr;

// ---- Konsole
typedef bool(__thiscall* ExecFn)(void*, const void*, void*);
ExecFn o_exec = nullptr;
void* g_reply = nullptr;   // Ausgabe-bCString des laufenden Befehls (nur waehrend Override_ConsoleCommand)
int g_execLogged = 0;


bool __fastcall HExec(void* console, void*, const void* in, void* out) {
    char b[300];
    if (g_execLogged < 50) {
        ++g_execLogged;
        std::snprintf(b, sizeof b, "Konsole: Befehl '%.200s'", SafeStrText(in, ""));
        if (g_host) g_host->Log(b);
    }
    if (!in || !out) NoteNull("Override_ConsoleCommand: Ein-/Ausgabetext");
    if (!g_host || !in || !out || !OnGameThread()) return o_exec(console, in, out);
    EvArg a[] = {EvArg::Str(in)};
    bool res = false, called = false;
    void* prev = g_reply;
    g_reply = out;
    const bool taken = g_host->CallOverride(
        "bool Override_ConsoleCommand(const bCString &in)", a, 1, [&](const EvArg*) { return o_exec(console, in, out); }, &res, &called);
    g_reply = prev;
    if (!taken) return o_exec(console, in, out);
    if (!called && g_execLogged < 50) {
        std::snprintf(b, sizeof b, "Konsole: Befehl von einer Mod uebernommen (Ergebnis %d, Antwort '%.150s')", res ? 1 : 0, SafeStrText(out, ""));
        g_host->Log(b);
    }
    return res;
}

typedef void*(__stdcall* AccessConsoleFn)();
typedef void(__thiscall* ConsoleVoidFn)(void*);
typedef bool(__thiscall* ConsoleBoolFn)(void*);
AccessConsoleFn g_accessConsole = nullptr;
ConsoleVoidFn g_consoleOpen = nullptr;
ConsoleBoolFn g_consoleIsOpen = nullptr;
bool SafeOpenConsole() {
    __try {
        void* c = g_accessConsole ? g_accessConsole() : nullptr;
        if (!c || !g_consoleOpen) return false;
        if (!(g_consoleIsOpen && g_consoleIsOpen(c))) g_consoleOpen(c);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool OpenConsoleScript() { return SafeOpenConsole(); }

void SetConsoleReplyScript(const SStr& text) {
    if (!g_reply) {
        if (ScriptHost::s_current) ScriptHost::s_current->Log("SetConsoleReply: nur innerhalb von Override_ConsoleCommand");
        return;
    }
    g_sb.strAssign(g_reply, &text);
}

// ---- Quest-Status
typedef bool(__thiscall* SetStatusFn)(void*, const int*);
typedef const void*(__thiscall* GetRefFn)(const void*);
SetStatusFn o_setStatus = nullptr;
GetRefFn g_getStatus = nullptr, g_getName = nullptr;
int g_statusLogged = 0;
unsigned g_statusCalls = 0;

int SafeOldStatus(void* q) {
    __try {
        const unsigned char* c = static_cast<const unsigned char*>(g_getStatus(q));   // bTPropertyContainer: vtable, dann der Wert
        return c ? *reinterpret_cast<const int*>(c + 4) : -1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
const char* SafeQuestName(void* q) {
    __try {
        const void* n = g_getName ? g_getName(q) : nullptr;
        const char* t = n ? g_sb.strGetText(n) : nullptr;
        return t ? t : "?";
    } __except (EXCEPTION_EXECUTE_HANDLER) { return "?"; }
}

bool __fastcall HSetStatus(void* q, void*, const int* status) {
    ++g_statusCalls;
    const int old = g_getStatus ? SafeOldStatus(q) : -1;
    const int want = status && ReadableMem(status, sizeof(int)) ? *status : -1;
    const std::string name = SafeQuestName(q);
    if (g_statusLogged < 40 && g_host) {
        ++g_statusLogged;
        char b[220];
        std::snprintf(b, sizeof b, "Quest-Status: '%.120s' %d -> %d (Aufruf %u, Thread %s)", name.c_str(), old, want, g_statusCalls,
                      OnGameThread() ? "Spiel" : "anderer");
        g_host->Log(b);
    }
    if (!g_host || !OnGameThread() || want < 0) return o_setStatus(q, status);
    SStr sn(name.c_str());
    EvArg a[] = {EvArg::Str(&sn), EvArg::U(static_cast<unsigned>(old)), EvArg::U(static_cast<unsigned>(want))};
    bool res = false, called = false;
    int applied = want;
    const bool taken = g_host->CallOverride(
        "bool Override_QuestStatus(const bCString &in, int, int)", a, 3,
        [&](const EvArg* ar) {
            applied = static_cast<int>(ar[2].value);
            return o_setStatus(q, &applied);
        },
        &res, &called);
    if (!taken) {
        res = o_setStatus(q, status);
        called = true;
    }
    // gemeldet wird der Status, der danach wirklich gilt (lehnt das Spiel ab, bleibt er gleich und es gibt kein Ereignis)
    const int now = called && g_getStatus ? SafeOldStatus(q) : (called ? applied : old);
    if (called && now >= 0 && now != old) {
        EvArg e[] = {EvArg::Str(&sn), EvArg::U(static_cast<unsigned>(old)), EvArg::U(static_cast<unsigned>(now))};
        g_host->CallEvent("void OnQuestStatus(const bCString &in, int, int)", e, 3);
    }
    return res;
}

// ---- Namen fuer das Protokoll
typedef const void*(__thiscall* EntNameFn)(const void*);
typedef void*(__thiscall* PsEntityFn)(void*);
EntNameFn g_entName = nullptr;
PsEntityFn g_psEntity = nullptr;
const char* SafeEntName(const void* e) {
    if (!e || !g_entName) return "-";
    __try {
        const void* n = g_entName(e);
        const char* t = n ? g_sb.strGetText(n) : nullptr;
        return t && *t ? t : "?";
    } __except (EXCEPTION_EXECUTE_HANDLER) { return "?"; }
}
std::string EntName(const void* e) { return SafeEntName(e); }
void* SafePsEntity(void* ps) {
    if (!ps || !g_psEntity) return nullptr;
    __try {
        return g_psEntity(ps);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

// ---- Beruehrung: Engine eCDynamicEntity::OnTouch(eCEntity *anderes, eCContactIterator &) auf dem beruehrten Entity -> OnTouch(self, other)
// Sehr haeufig: gemeldet wird nur, wenn ein Mod OnTouch hat (einmal je Bild geprueft); gezaehlt wird immer.
typedef void(__thiscall* TouchFn)(void*, void*, void*);
TouchFn o_touch = nullptr;
const char* const kTouchDecl = "void OnTouch(Entity, Entity)";
bool g_touchHandler = false;
int g_touchDepth = 0, g_touchLogged = 0;
unsigned g_touchCalls = 0, g_touchEvents = 0;
LARGE_INTEGER g_touchTicks = {};   // Zeit in den Skripten

void __fastcall HTouch(void* self, void*, void* other, void* contacts) {
    o_touch(self, other, contacts);
    ++g_touchCalls;
    if (g_touchLogged < 15 && g_host && OnGameThread()) {
        ++g_touchLogged;
        g_host->Log("Beruehrung: '" + EntName(self) + "' von '" + EntName(other) + "'");
    }
    if (!g_touchHandler || !g_host || g_touchDepth > 0 || !OnGameThread()) return;
    ++g_touchDepth;
    ++g_touchEvents;
    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);
    EvArg a[] = {EvArg::Ent(self), EvArg::Ent(other)};
    g_host->CallEvent(kTouchDecl, a, 2);
    QueryPerformanceCounter(&t1);
    g_touchTicks.QuadPart += t1.QuadPart - t0.QuadPart;
    --g_touchDepth;
}

// ---- Haendlerware: Game gCInventory_PS::GenerateTradeInvFromTreasureSets() -> Override_TradeGenerate(Haendler), danach OnTradeGenerated(Haendler, ok)
typedef bool(__thiscall* GenTradeFn)(void*);
GenTradeFn o_genTrade = nullptr;
int g_tradeLogged = 0, g_tradeDepth = 0;   // Tiefe: GenerateTradeGoods() aus Override_TradeGenerate/OnTradeGenerated laeuft ohne Ereignis

bool __fastcall HGenTrade(void* inv) {
    void* trader = SafePsEntity(inv);
    if (g_tradeLogged < 40 && g_host) {
        ++g_tradeLogged;
        g_host->Log("Haendlerware: erzeugen fuer '" + EntName(trader) + "' (Thread " + (OnGameThread() ? "Spiel" : "anderer") + ")");
    }
    if (!g_host || !trader || !OnGameThread() || g_tradeDepth > 0) return o_genTrade(inv);
    ++g_tradeDepth;
    EvArg a[] = {EvArg::Ent(trader)};
    bool res = false, called = false;
    if (!g_host->CallOverride("bool Override_TradeGenerate(Entity)", a, 1, [&](const EvArg*) { return o_genTrade(inv); }, &res, &called)) {
        res = o_genTrade(inv);
        called = true;
    }
    if (called) {
        EvArg e[] = {EvArg::Ent(trader), EvArg::U(res ? 1u : 0u)};
        g_host->CallEvent("void OnTradeGenerated(Entity, int)", e, 2);
    }
    --g_tradeDepth;
    return res;
}

// ---- Verwandlung: Script.dll Entity::StartTransform(Entity &ziel, float, bool) und Entity::EndTransform(); this ist ein Script-Entity (erstes Feld eCEntity*)
typedef void(__thiscall* StartTrFn)(void*, void*, float, bool);
typedef void(__thiscall* EndTrFn)(void*);
StartTrFn o_startTr = nullptr;
EndTrFn o_endTr = nullptr;
int g_trLogged = 0;
bool g_trHooked = false;

void* ScriptEntPtr(const void* se) {
    if (!se || !ReadableMem(se, sizeof(void*))) return nullptr;
    return *static_cast<void* const*>(se);
}
void __fastcall HStartTr(void* self, void*, void* target, float seconds, bool b) {
    void* e = ScriptEntPtr(self);
    void* t = ScriptEntPtr(target);
    if (g_trLogged < 30 && g_host) {
        ++g_trLogged;
        char buf[240];
        std::snprintf(buf, sizeof buf, "Verwandlung: Start '%.80s' -> '%.80s', %.1f s, %d", EntName(e).c_str(), EntName(t).c_str(), seconds, b ? 1 : 0);
        g_host->Log(buf);
    }
    o_startTr(self, target, seconds, b);
    if (!g_host || !OnGameThread()) return;
    EvArg a[] = {EvArg::Ent(e), EvArg::Ent(t)};
    g_host->CallEvent("void OnTransformStart(Entity, Entity)", a, 2);
}
void __fastcall HEndTr(void* self) {
    void* e = ScriptEntPtr(self);
    if (g_trLogged < 30 && g_host) {
        ++g_trLogged;
        g_host->Log("Verwandlung: Ende '" + EntName(e) + "'");
    }
    o_endTr(self);
    if (!g_host || !OnGameThread()) return;
    void* now = ScriptEntPtr(self);
    if (g_trLogged < 30 && now != e) g_host->Log("Verwandlung: Entity nach dem Ende '" + EntName(now) + "'");
    EvArg a[] = {EvArg::Ent(now ? now : e)};
    g_host->CallEvent("void OnTransformEnd(Entity)", a, 1);
}


// ---- Sprachausgabe (API 41): Game gCScriptProcessingUnit::sAIOutputInstr(args, SPU, fullStop), die KI-Anweisung "Satz sprechen" (gemessen am Assembler
// 2026-11-10). Sie laeuft jedes Bild, solange der Satz laeuft: Beim Start ist args = {eCEntity* Sprecher, bCString Satz-ID} (die ID ist auch der Name der
// Sprachdatei ohne ".wav", die Mindestdauer setzt das Spiel auf (Laenge der ID + 10) * 100 ms); danach args = nullptr, zum Abbrechen fullStop = true.
// OnAIOutput(sprecher, id) kommt einmal je Satz, nach dem Start der Ausgabe. Den Text liefert GetLocText(id).
typedef bool(__stdcall* OutputFn)(void*, void*, bool);
OutputFn o_output = nullptr;
int g_outLogged = 0;

bool __stdcall HOutput(void* args, void* spu, bool fullStop) {
    const bool r = o_output(args, spu, fullStop);
    if (fullStop || !args || !g_host || !OnGameThread() || !ReadableMem(args, 8)) return r;
    void* speaker = *static_cast<void**>(args);
    const void* id = static_cast<const char*>(args) + 4;
    const char* t = SafeStrText(id, nullptr);
    if (!speaker || !t || !*t) return r;
    if (g_outLogged < 20) {
        ++g_outLogged;
        g_host->Log("Sprachausgabe: '" + EntName(speaker) + "' sagt '" + t + "'");
    }
    EvArg a[] = {EvArg::Ent(speaker), EvArg::Str(id)};
    g_host->CallEvent("void OnAIOutput(Entity, const bCString &in)", a, 2);
    return r;
}

// ---- Kampfbewegung (API 45): Game.dll gCScriptProcessingUnit::sAICombatMoveStart(void* args, gCScriptProcessingUnit*) stdcall -> bool.
// Gemessen (2026-11-11, Assembler + PlayMotion-Messung): args+0x0C bCString (das Spiel prueft ihn auf "Hit"), args+0x10 float = Abspielgeschwindigkeit
// der Kampfbewegung; die Funktion kopiert sie nach SPU+0x160 und in die Bewegungsbeschreibung (SPU+0x13C = eSMotionDesc+8) fuer PlayMotion. Ein NPC
// (Gamal) griff mit 1,3 an. Override_CombatMove(self, move, speedPercent): SetOverrideArg(2, p) setzt die Geschwindigkeit in Prozent fuer diesen Aufruf;
// der Wert in args wird nach dem Original zurueckgestellt (falls dieselben Argumente erneut benutzt werden, soll nichts aufaddiert werden).
typedef bool(__stdcall* CmStartFn)(void*, void*);
typedef void*(__thiscall* SpuEntityFn)(void*);
CmStartFn o_cmStart = nullptr;
SpuEntityFn g_spuSelf = nullptr;
int g_cmLogged = 0;

void* SafeSpuSelf(void* spu) {
    if (!spu || !g_spuSelf) return nullptr;
    __try {
        return g_spuSelf(spu);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool __stdcall HCmStart(void* args, void* spu) {
    float* speed = args && ReadableMem(args, 0x14) ? reinterpret_cast<float*>(static_cast<char*>(args) + 0x10) : nullptr;
    if (!speed || !g_host) {
        if (!speed) NoteNull("Override_CombatMove: Argumente der Kampfbewegung");
        return o_cmStart(args, spu);
    }
    const float orig = *speed;
    void* self = SafeSpuSelf(spu);
    const SStr move(SafeStrText(static_cast<char*>(args) + 0x0C, ""));
    float applied = orig;
    bool res = false, called = false, taken = false;
    if (OnGameThread()) {
        const unsigned pct = orig > 0 && orig < 1000 ? static_cast<unsigned>(orig * 100.0f + 0.5f) : 0;   // NOLINT(bugprone-incorrect-roundings): nur positive Werte
        EvArg a[] = {EvArg::Ent(self), EvArg::Str(&move), EvArg::U(pct)};
        taken = g_host->CallOverride(
            "bool Override_CombatMove(Entity, const bCString &in, uint)", a, 3,
            [&](const EvArg* ar) {
                applied = ar[2].value != pct ? static_cast<float>(ar[2].value) / 100.0f : orig;   // unveraendert: Originalwert ohne Rundung
                *speed = applied;
                const bool r = o_cmStart(args, spu);
                *speed = orig;
                return r;
            },
            &res, &called);
    }
    if (!taken) res = o_cmStart(args, spu);
    if (g_cmLogged < 40) {
        ++g_cmLogged;
        char b[260];
        std::snprintf(b, sizeof b, "Kampfbewegung: %.60s '%.60s' Geschwindigkeit %.3f%s", SafeEntName(self), move.c_str(), orig,
                      taken ? (called ? (applied != orig ? " -> geaendert" : " (Override, unveraendert)") : " (Override ohne Original)") : "");
        if (taken && called && applied != orig) {
            char c[40];
            std::snprintf(c, sizeof c, " %.3f", applied);
            std::strncat(b, c, sizeof b - std::strlen(b) - 1);
        }
        g_host->Log(b);
    }
    return res;
}

}   // namespace

bool RegisterMoreEventBindings(asIScriptEngine* e) {
    bool ok = true;
    OG3_REG(e->RegisterGlobalFunction("void SetConsoleReply(const bCString &in text)", asFUNCTION(SetConsoleReplyScript), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool OpenConsole()", asFUNCTION(OpenConsoleScript), asCALL_CDECL));
    return ok;
}

int InstallMoreEventHooks(ScriptHost& host) {
    g_host = &host;
    HMODULE eng = GetModuleHandleA("Engine.dll");
    HMODULE game = GetModuleHandleA("Game.dll");
    if (game) {
        g_getStatus = reinterpret_cast<GetRefFn>(GetProcAddress(game, "?GetStatus@gCQuest_PS@@QBEABV?$bTPropertyContainer@W4gEQuestStatus@@@@XZ"));
        g_getName = reinterpret_cast<GetRefFn>(GetProcAddress(game, "?GetName@gCQuest_PS@@QBEABVbCString@@XZ"));
        g_spuSelf = reinterpret_cast<SpuEntityFn>(GetProcAddress(game, "?GetSelfEntity@gCScriptProcessingUnit@@QAEPAVeCEntity@@XZ"));
    }
    if (eng) {
        g_accessConsole = reinterpret_cast<AccessConsoleFn>(GetProcAddress(eng, "?AccessConsole@eCConsole@@SGAAV1@XZ"));
        g_consoleOpen = reinterpret_cast<ConsoleVoidFn>(GetProcAddress(eng, "?Open@eCConsole@@QAEXXZ"));
        g_consoleIsOpen = reinterpret_cast<ConsoleBoolFn>(GetProcAddress(eng, "?IsOpen@eCConsole@@QAE_NXZ"));
        g_entName = reinterpret_cast<EntNameFn>(GetProcAddress(eng, "?GetName@eCEntity@@QBEABVbCString@@XZ"));
        g_psEntity = reinterpret_cast<PsEntityFn>(GetProcAddress(eng, "?GetEntity@eCEntityPropertySet@@UAEPAVeCEntity@@XZ"));
    }
    int n = 0;
    n += HookExport(eng, "?OnExecute@eCConsole@@MAE_NABVbCString@@AAV2@@Z", reinterpret_cast<void*>(&HExec), reinterpret_cast<void**>(&o_exec), host);
    n += HookExport(game, "?SetStatus@gCQuest_PS@@QAE_NABW4gEQuestStatus@@@Z", reinterpret_cast<void*>(&HSetStatus),
                 reinterpret_cast<void**>(&o_setStatus), host);
    n += HookExport(eng, "?OnTouch@eCDynamicEntity@@UAEXPAVeCEntity@@AAVeCContactIterator@@@Z", reinterpret_cast<void*>(&HTouch),
                 reinterpret_cast<void**>(&o_touch), host);
    n += HookExport(game, "?GenerateTradeInvFromTreasureSets@gCInventory_PS@@QAE_NXZ", reinterpret_cast<void*>(&HGenTrade),
                 reinterpret_cast<void**>(&o_genTrade), host);
    n += HookExport(game, "?sAIOutputInstr@gCScriptProcessingUnit@@SG_NPAXPAV1@_N@Z", reinterpret_cast<void*>(&HOutput),
                 reinterpret_cast<void**>(&o_output), host);
    n += HookExport(game, "?sAICombatMoveStart@gCScriptProcessingUnit@@KG_NPAXPAV1@@Z", reinterpret_cast<void*>(&HCmStart),
                 reinterpret_cast<void**>(&o_cmStart), host);
    host.Log("Ereignisse 7.3: " + std::to_string(n) +
             " von 6 Hooks aktiv (Konsole, Quest-Status, Beruehrung, Haendlerware, Sprachausgabe, Kampfbewegung)");
    return n;
}

// Einmal je Bild: OnTouch-Handler pruefen, Verwandlungs-Hooks nachholen (Script.dll), Beruehrungen je 10 s (erste 6 Fenster mit Aufrufen)
void MoreEventsFrame() {
    if (!g_host) return;
    g_touchHandler = g_host->HasHandler(kTouchDecl);
    if (!g_trHooked) {
        static DWORD lastTry = 0;
        if (GetTickCount() - lastTry >= 1000u) {
            lastTry = GetTickCount();
            if (HMODULE script = GetModuleHandleA("Script.dll")) {
                g_trHooked = true;
                int n = HookExport(script, "?StartTransform@Entity@@QAEXAAV1@M_N@Z", reinterpret_cast<void*>(&HStartTr),
                                reinterpret_cast<void**>(&o_startTr), *g_host);
                n += HookExport(script, "?EndTransform@Entity@@QAEXXZ", reinterpret_cast<void*>(&HEndTr), reinterpret_cast<void**>(&o_endTr), *g_host);
                g_host->Log("Ereignisse 7.3: " + std::to_string(n) + " von 2 Verwandlungs-Hooks aktiv");
            }
        }
    }
    static DWORD last = GetTickCount();
    static int windows = 0;
    if (GetTickCount() - last < 10000u) return;
    last = GetTickCount();
    if (g_touchCalls && windows < 6) {
        ++windows;
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        char b[200];
        std::snprintf(b, sizeof b, "Beruehrung: %u Aufrufe in 10 s, %u an Skripte gemeldet (%.1f us je Meldung)", g_touchCalls, g_touchEvents,
                      g_touchEvents ? 1e6 * double(g_touchTicks.QuadPart) / double(f.QuadPart) / g_touchEvents : 0.0);
        g_host->Log(b);
    }
    g_touchCalls = g_touchEvents = 0;
    g_touchTicks.QuadPart = 0;
}

}   // namespace og3p
