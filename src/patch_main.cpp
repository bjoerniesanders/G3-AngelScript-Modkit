// og3_patch.dll - offener Patch fuer Gothic 3 mit AngelScript-Modding (Meilenstein 1).
// Wird vom dbghelp-Proxy geladen (og3_hooks.ini [patch] enable=1) und arbeitet mit der geladenen SharedBase.dll (Original oder Nachbau).
//
// Einhaengen ohne Code-Eingriff (vertraeglich mit dem Parallel-Universe-Patch): die Importtabelle von Gothic3.exe wird fuer vier
// Lebenszyklus-Funktionen von eCApplication (Engine.dll) umgelenkt. Die EXE leitet ihre Anwendungsklasse von eCApplication ab und
// ruft diese virtuellen Funktionen ueber Import-Thunks auf:
//   OnPostInitializeModules -> Skript laden, main()        OnProcess -> OnFrame(dt), Neuladen bei Dateiaenderung
//   OnWorldActivate / OnWorldDeactivate -> gleichnamige Skript-Ereignisse
// Beim Laden des Proxys sind die Importe der EXE eventuell noch nicht gebunden: ein Hilfsthread wartet darauf (hoechstens 60 s).
//
// ini [patch]: enable=0|1, script=og3\scripts\main.as (relativ zum Spielordner), hotreload=1
// Protokoll: <Spielordner>\og3_patch.log
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <string>

#include "sb_api.h"
#include "script_host.h"

using namespace og3p;

namespace {

ScriptHost g_host;
std::string g_gameDir, g_scriptPath, g_modsDir;
bool g_hotReload = true, g_gameHooks = true, g_filePatches = true;
FILE* g_log = nullptr;
void (*g_hostLog)(const char*) = nullptr;
CRITICAL_SECTION g_logLock;
bool g_logLockInit = false;
std::atomic<bool> g_initDone{false};
bool g_mainRan = false;

void Log(const std::string& s) {
    if (!g_logLockInit) {
        InitializeCriticalSection(&g_logLock);
        g_logLockInit = true;
    }
    EnterCriticalSection(&g_logLock);
    SYSTEMTIME t;
    GetLocalTime(&t);
    if (g_log) {
        std::fprintf(g_log, "%02d:%02d:%02d.%03d %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, s.c_str());
        std::fflush(g_log);
    }
    LeaveCriticalSection(&g_logLock);
    if (g_hostLog) g_hostLog(("og3_patch: " + s).c_str());
}

// ---------------------------------------------------------------- Bildzeit aus der Engine
typedef void*(__stdcall* AppInstanceFn)();
typedef float(__thiscall* AppFrameTimeFn)(const void*);
AppInstanceFn g_appInstance = nullptr;
AppFrameTimeFn g_appFrameTime = nullptr;
float FrameTime() {
    if (!g_appInstance) {
        HMODULE eng = GetModuleHandleA("Engine.dll");
        if (!eng) return 0.0f;
        g_appInstance = reinterpret_cast<AppInstanceFn>(GetProcAddress(eng, "?GetInstance@eCApplication@@SGAAV1@XZ"));
        g_appFrameTime = reinterpret_cast<AppFrameTimeFn>(GetProcAddress(eng, "?GetScaledFrameTimeInSeconds@eCApplication@@QBEMXZ"));
        if (!g_appInstance || !g_appFrameTime) {
            g_appInstance = nullptr;
            return 0.0f;
        }
    }
    return g_appFrameTime(g_appInstance());
}

// ---------------------------------------------------------------- Importtabelle der EXE
typedef void(__thiscall* VoidMethod)(void*);
enum HookId { kPostInit = 0, kProcess, kWorldActivate, kWorldDeactivate, kHookCount };
const char* kHookNames[kHookCount] = {"?OnPostInitializeModules@eCApplication@@MAEXXZ", "?OnProcess@eCApplication@@UAEXXZ",
                                      "?OnWorldActivate@eCApplication@@UAEXXZ", "?OnWorldDeactivate@eCApplication@@UAEXXZ"};
VoidMethod g_orig[kHookCount] = {};
void** g_slot[kHookCount] = {};

void RunMainOnce() {
    if (g_mainRan) return;
    g_mainRan = true;
    if (g_gameHooks) InstallGameHooks(g_host);
    g_host.AddScript("main", g_scriptPath);   // Einzelskript (ini script=), falls vorhanden
    g_host.SetModsDir(g_modsDir);
    g_host.ScanMods();
    g_host.CallMain();
}
void __fastcall HPostInit(void* self, void*) {
    g_orig[kPostInit](self);
    Log("Engine initialisiert (OnPostInitializeModules)");
    RunMainOnce();
}
// ---------------------------------------------------------------- Windows-Menuetaste ohne Piepton
// F10 (und Alt allein / Alt+Buchstabe) schaltet Windows in den Menuemodus; das Spielfenster hat kein Menue,
// daher piept DefWindowProc beim naechsten Tastendruck. SC_KEYMENU und WM_SYSCHAR werden geschluckt,
// alles andere (auch WM_SYSKEYDOWN fuer Alt+F4/Alt+Enter) geht unveraendert an die Fensterprozedur des Spiels.
WNDPROC g_origWndProc = nullptr;
bool g_wndUnicode = false;
LRESULT CALLBACK QuietWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SYSCOMMAND && (wp & 0xFFF0) == SC_KEYMENU) return 0;
    if (msg == WM_SYSCHAR) return 0;
    if (msg == WM_MENUCHAR) return MAKELRESULT(0, MNC_CLOSE);
    // Tasten-Ereignis fuer Skripte (OnKey); Wiederholung (Bit 30) wird nicht gemeldet, die Nachricht geht immer weiter ans Spiel
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        if (!(lp & (1 << 30))) g_host.CallKey(static_cast<int>(wp), true);
    } else if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
        g_host.CallKey(static_cast<int>(wp), false);
    return g_wndUnicode ? CallWindowProcW(g_origWndProc, h, msg, wp, lp) : CallWindowProcA(g_origWndProc, h, msg, wp, lp);
}
void InstallQuietMenu() {
    static unsigned lastTry = 0;
    if (g_origWndProc) return;
    const unsigned now = GetTickCount();
    if (now - lastTry < 1000) return;   // Fenstersuche hoechstens einmal je Sekunde
    lastTry = now;
    HWND h = GameWindow();
    if (!h) return;
    g_wndUnicode = IsWindowUnicode(h) != FALSE;
    LONG_PTR prev = g_wndUnicode ? SetWindowLongPtrW(h, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&QuietWndProc))
                                 : SetWindowLongPtrA(h, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&QuietWndProc));
    if (!prev) {
        Log("Fensterprozedur nicht ersetzbar (Menueton bleibt)");
        return;
    }
    g_origWndProc = reinterpret_cast<WNDPROC>(prev);
    Log("Menuetaste stumm (F10/Alt ohne Windows-Piepton)");
}

void __fastcall HProcess(void* self, void*) {
    g_orig[kProcess](self);
    InstallQuietMenu();
    if (!g_mainRan) RunMainOnce();   // falls OnPostInitializeModules vor dem Einhaengen lief
    if (g_hotReload) g_host.CheckReload();
    g_host.CallFrame(FrameTime());
}
void __fastcall HWorldActivate(void* self, void*) {
    g_orig[kWorldActivate](self);
    g_host.CallSimple("void OnWorldActivate()");
}
void __fastcall HWorldDeactivate(void* self, void*) {
    g_host.CallSimple("void OnWorldDeactivate()");
    g_orig[kWorldDeactivate](self);
}
void* const kHookFns[kHookCount] = {reinterpret_cast<void*>(&HPostInit), reinterpret_cast<void*>(&HProcess),
                                    reinterpret_cast<void*>(&HWorldActivate), reinterpret_cast<void*>(&HWorldDeactivate)};

bool FindExeSlots() {
    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleA(nullptr));
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dd.VirtualAddress) return false;
    int found = 0;
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dd.VirtualAddress); d->Name; ++d) {
        if (_stricmp(reinterpret_cast<const char*>(base + d->Name), "Engine.dll") || !d->OriginalFirstThunk) continue;
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->OriginalFirstThunk);
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (size_t i = 0; names[i].u1.AddressOfData; ++i) {
            if (IMAGE_SNAP_BY_ORDINAL(names[i].u1.Ordinal)) continue;
            const char* n = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names[i].u1.AddressOfData)->Name;
            for (int h = 0; h < kHookCount; ++h)
                if (!strcmp(n, kHookNames[h])) {
                    g_slot[h] = reinterpret_cast<void**>(&iat[i].u1.Function);
                    ++found;
                }
        }
    }
    return found > 0;
}

bool InExe(const void* p) {
    auto* base = reinterpret_cast<const unsigned char*>(GetModuleHandleA(nullptr));
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
    return p >= base && p < base + nt->OptionalHeader.SizeOfImage;
}

// Patcht alle Plaetze, deren Import schon gebunden ist (Zeiger ausserhalb der EXE). Rueckgabe: Anzahl noch offener Plaetze.
int PatchBoundSlots() {
    int open = 0;
    for (int h = 0; h < kHookCount; ++h) {
        if (!g_slot[h] || g_orig[h]) continue;
        void* cur = *g_slot[h];
        if (!cur || InExe(cur)) {
            ++open;
            continue;
        }   // noch nicht gebunden (enthaelt RVA des Namens)
        HMODULE eng = GetModuleHandleA("Engine.dll");
        void* exp = eng ? reinterpret_cast<void*>(GetProcAddress(eng, kHookNames[h])) : nullptr;
        g_orig[h] = reinterpret_cast<VoidMethod>(cur);
        DWORD old;
        VirtualProtect(g_slot[h], sizeof(void*), PAGE_READWRITE, &old);
        *g_slot[h] = kHookFns[h];
        VirtualProtect(g_slot[h], sizeof(void*), old, &old);
        Log(std::string("eingehaengt: ") + kHookNames[h] +
            (cur == exp ? "" : " (Import zeigt nicht auf den Engine-Export - fremder Hook davor, wird weitergereicht)"));
    }
    return open;
}

DWORD WINAPI WaitForBinding(void*) {
    for (int i = 0; i < 30000; ++i) {
        if (PatchBoundSlots() == 0) return 0;
        Sleep(2);
    }
    Log("WARNUNG: Importe der EXE nach 60 s nicht gebunden - Ereignisse fehlen");
    return 0;
}

bool InitCommon(HMODULE sb, const char* gameDir, void (*hostLog)(const char*)) {
    if (g_initDone.exchange(true)) return true;
    g_hostLog = hostLog;
    g_gameDir = gameDir ? gameDir : "";
    if (!g_gameDir.empty() && g_gameDir.back() != '\\' && g_gameDir.back() != '/') g_gameDir += '\\';
    g_log = _fsopen((g_gameDir + "og3_patch.log").c_str(), "w", _SH_DENYNO);   // waehrend des Spiels lesbar
    if (!g_sb.Resolve(sb ? sb : GetModuleHandleA("SharedBase.dll"))) {
        Log("SharedBase-Exporte fuer bCString fehlen - Abbruch: " + g_sb.missing);
        return false;
    }
    if (!g_sb.missing.empty()) Log("Hinweis: nicht gefundene SharedBase-Exporte: " + g_sb.missing);
    g_host.frameTime = &FrameTime;
    return g_host.Init([](const std::string& s) { Log(s); });
}

}   // namespace

// Fuer Tests: Host ohne Spiel-Hooks anlegen
extern "C" __declspec(dllexport) int og3_patch_init(HMODULE sharedBase, const char* logDir, void (*hostLog)(const char*)) {
    return InitCommon(sharedBase, logDir, hostLog) ? 1 : 0;
}

// Fuer Tests: Datei-Patches auf ein Testverzeichnis anwenden
extern "C" __declspec(dllexport) int og3_patch_apply_patches(const char* gameDir, const char* modsDir) {
    return ApplyScriptPatches(gameDir, modsDir, [](const std::string& s) { Log(s); });
}

// Fuer Tests: Spielereignis-Hooks einhaengen (Game.dll muss geladen sein); Rueckgabe = Zahl aktiver Hooks
extern "C" __declspec(dllexport) int og3_patch_install_hooks() { return InstallGameHooks(g_host); }

// Fuer Tests: Skriptdatei laden und main() ausfuehren; Rueckgabe = Anzahl fehlgeschlagener check()-Aufrufe, -1 bei Ladefehler
extern "C" __declspec(dllexport) int og3_patch_run_file(const char* path) {
    const DWORD attr = GetFileAttributesA(path);
    const bool dirMode = attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
    if (dirMode) {   // Mod-Ordner (Tests)
        g_host.SetModsDir(path);
        g_host.ScanMods();
        if (!g_host.Loaded()) return -1;
    } else if (!g_host.Load(path))
        return -1;
    g_host.CallMain();
    {   // Ereignisse mit Argumenten (ohne Spiel: ungueltige Entities)
        SStr id("GUID-123"), nm("It_Test");
        EvArg a[] = {EvArg::Ent(nullptr), EvArg::Ent(nullptr), EvArg::Str(&nm), EvArg::Str(&id), EvArg::U(7)};
        g_host.CallEvent("void OnReceiveItem(Entity, Entity, const bCString &in, const bCString &in, uint)", a, 5);
        g_host.CallEvent("void OnNPCKilled(Entity)", a, 1);
    }
    {   // Override-Kette: Super() ruft das "Original", Rueckgabe wird vom Skript veraendert; ohne Super() wird das Original uebersprungen
        EvArg a[] = {EvArg::U(5)};
        bool res = true, orig = false;
        bool handled = g_host.CallOverride(
            "bool Override_Test(uint)", a, 1,
            [&](const EvArg*) {
                orig = true;
                return true;
            },
            &res, &orig);
        if (dirMode) {   // Kette ueber mehrere Mods (zeta -> alpha -> Original): beide rufen Super(), Ergebnis bleibt wahr
            g_host.Check(handled && orig && res, "Override-Kette ueber mehrere Mods: Original am Ende, Ergebnis unveraendert");
        } else {
            g_host.Check(handled && orig && !res, "Override mit Super(): Original lief, Ergebnis vom Skript umgekehrt");
            res = false;
            orig = false;
            handled = g_host.CallOverride(
                "bool Override_TestCancel(uint)", a, 1,
                [&](const EvArg*) {
                    orig = true;
                    return false;
                },
                &res, &orig);
            g_host.Check(handled && !orig && res, "Override ohne Super(): Original uebersprungen, Ergebnis vom Skript");
        }
        if (!dirMode) {   // Argument aendern: der Mod verdoppelt es (SetOverrideArg), Super() reicht es ans Original weiter
            EvArg b[] = {EvArg::U(5)};
            unsigned seen = 0;
            res = false;
            orig = false;
            handled = g_host.CallOverride(
                "bool Override_TestArg(uint)", b, 1,
                [&](const EvArg* ar) {
                    seen = ar[0].value;
                    orig = true;
                    return true;
                },
                &res, &orig);
            g_host.Check(handled && orig && res && seen == 10, "SetOverrideArg: geaenderter Wert kommt beim Original an");
        }
        g_host.Check(!g_host.CallOverride(
                         "bool Override_Gibts_Nicht(uint)", a, 1, [](const EvArg*) { return true; }, &res, &orig),
                     "Override ohne Skriptfunktion: nicht uebernommen");
    }
    g_host.CallKey(0x7A, true);
    g_host.CallKey(0x7A, false);
    g_host.CallFrame(0.016f);
    g_host.CallSimple("void OnShutdown()");
    char b[96];
    std::snprintf(b, sizeof b, "Pruefungen: %d, fehlgeschlagen: %d", g_host.Checks(), g_host.Failures());
    Log(b);
    return g_host.Failures();
}

// Spielbetrieb (vom Proxy): Host anlegen und in die Importtabelle der EXE einhaengen
extern "C" __declspec(dllexport) int og3_patch_start(HMODULE sharedBase, const char* ini, const char* gameDir,
                                                     void (*hostLog)(const char*)) {
    if (!InitCommon(sharedBase, gameDir, hostLog)) return 0;
    char script[MAX_PATH] = "og3\\scripts\\main.as", modsRel[MAX_PATH] = "og3\\mods";
    if (ini && *ini) {
        GetPrivateProfileStringA("patch", "script", script, script, MAX_PATH, ini);
        g_hotReload = GetPrivateProfileIntA("patch", "hotreload", 1, ini) != 0;
        g_gameHooks = GetPrivateProfileIntA("patch", "hooks", 1, ini) != 0;
        SetGameHookOptions(GetPrivateProfileIntA("patch", "navevents", 0, ini) != 0);
        g_filePatches = GetPrivateProfileIntA("patch", "filepatches", 1, ini) != 0;
        GetPrivateProfileStringA("patch", "mods", modsRel, modsRel, MAX_PATH, ini);
    }
    g_modsDir = (modsRel[1] == ':' || modsRel[0] == '\\') ? std::string(modsRel) : g_gameDir + modsRel;
    while (!g_modsDir.empty() && (g_modsDir.back() == '\\' || g_modsDir.back() == '/')) g_modsDir.pop_back();
    g_scriptPath = (script[1] == ':' || script[0] == '\\') ? std::string(script) : g_gameDir + script;
    if (g_filePatches)
        ApplyScriptPatches(g_gameDir, g_modsDir, [](const std::string& s) { Log(s); });   // vor dem ersten Lesen der Daten durch das Spiel
    Log("Skript: " + g_scriptPath + (g_hotReload ? " (Neuladen bei Aenderung an)" : "") + ", Mod-Ordner: " + g_modsDir);
    if (!FindExeSlots()) {
        Log("Gothic3.exe importiert die eCApplication-Ereignisse nicht - nur ohne Spiel-Ereignisse nutzbar");
        return 1;
    }
    if (PatchBoundSlots() > 0) CloseHandle(CreateThread(nullptr, 0, WaitForBinding, nullptr, 0, nullptr));
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, void* reserved) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(h);
    if (reason == DLL_PROCESS_DETACH && !reserved && g_initDone) { Log("Ende"); }
    return TRUE;
}
