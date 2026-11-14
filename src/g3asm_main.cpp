// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// g3asm.dll - G3 AngelScript Modkit: AngelScript-Modding fuer Gothic 3.
// Wird vom Loader (src/loader, Ersatz-dbghelp.dll; g3asm.ini [g3asm] enable=1) geladen und arbeitet mit der geladenen SharedBase.dll (Original oder Nachbau).
//
// Einhaengen ohne Code-Eingriff (vertraeglich mit dem Parallel-Universe-Patch): die Importtabelle von Gothic3.exe wird fuer vier
// Lebenszyklus-Funktionen von eCApplication (Engine.dll) umgelenkt. Die EXE leitet ihre Anwendungsklasse von eCApplication ab und
// ruft diese virtuellen Funktionen ueber Import-Thunks auf:
//   OnPostInitializeModules -> Skript laden, main()        OnProcess -> OnFrame(dt), Neuladen bei Dateiaenderung
//   OnWorldActivate / OnWorldDeactivate -> gleichnamige Skript-Ereignisse
// Beim Laden durch den Loader sind die Importe der EXE eventuell noch nicht gebunden: ein Hilfsthread wartet darauf (hoechstens 60 s).
//
// ini [g3asm]: enable=0|1, script=g3asm\scripts\main.as (relativ zum Spielordner), hotreload=1
// Protokoll: <Spielordner>\g3asm.log
#include <windows.h>
#include <xmmintrin.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ini_file.h"
#include "sb_api.h"
#include "script_host.h"

using namespace g3asm;

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

// Protokoll. Standard: sofort schreiben. Optional asynchron (ini [g3asm] asynclog=1): Der aufrufende Thread legt die Zeile nur in eine Warteschlange, ein
// Hintergrund-Thread schreibt g3asm.log und reicht die Zeilen an den Loader (g3asm_loader.log) weiter. Entstanden beim Suchen der verwaschenen Texturen;
// das Schreiben war nicht die Ursache (die war fninit in der AngelScript-Aufrufbruecke, siehe CMakeLists.txt CLEAR_FPU_STACK).
bool g_asyncLog = false;
bool g_logWrite = true;   // ini [g3asm] logwrite=0: nach dem Start (main gelaufen) keine Protokollzeilen mehr (nur fuer Tests)
// Eine Meldung: Dateizeile mit Zeitstempel und Zeile fuer den Loader (ohne Zeitstempel, ganze Meldung wie frueher)
struct LogItem {
    std::string fileLine, hostLine;
};
std::vector<LogItem> g_logQueue;   // nur asynchron, geschuetzt durch g_logLock
CRITICAL_SECTION g_writeLock;       // serialisiert das Schreiben (Aufrufer, Hintergrund-Thread, FlushLog)
HANDLE g_logEvent = nullptr;
bool g_logThread = false;
void InitLogLocks() {
    if (g_logLockInit) return;
    InitializeCriticalSection(&g_logLock);
    InitializeCriticalSection(&g_writeLock);
    g_logLockInit = true;
}
// Schreibt die Datei unter g_writeLock; den Loader ruft sie danach ausserhalb der Sperre auf
void WriteItems(const std::vector<LogItem>& items) {
    if (items.empty()) return;
    EnterCriticalSection(&g_writeLock);
    if (g_log) {
        for (const LogItem& it : items) std::fputs(it.fileLine.c_str(), g_log);
        std::fflush(g_log);
    }
    LeaveCriticalSection(&g_writeLock);
    if (g_hostLog)
        for (const LogItem& it : items) g_hostLog(it.hostLine.c_str());
}
DWORD WINAPI LogWriter(void*) {
    for (;;) {
        WaitForSingleObject(g_logEvent, 200);
        std::vector<LogItem> batch;
        EnterCriticalSection(&g_logLock);
        batch.swap(g_logQueue);
        LeaveCriticalSection(&g_logLock);
        WriteItems(batch);
    }
}
void Log(const std::string& s) {
    if (!g_logWrite && g_mainRan) return;
    InitLogLocks();
    SYSTEMTIME t;
    GetLocalTime(&t);
    char stamp[24];
    std::snprintf(stamp, sizeof stamp, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    LogItem it{std::string(stamp) + s + "\n", "g3asm: " + s};
    if (!g_asyncLog) {
        WriteItems(std::vector<LogItem>{std::move(it)});
        return;
    }
    EnterCriticalSection(&g_logLock);
    g_logQueue.push_back(std::move(it));
    if (!g_logThread) {
        g_logThread = true;
        g_logEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        if (HANDLE h = CreateThread(nullptr, 0, LogWriter, nullptr, 0, nullptr)) CloseHandle(h);
    }
    LeaveCriticalSection(&g_logLock);
    if (g_logEvent) SetEvent(g_logEvent);
}
// Restliche Zeilen sofort schreiben. Nur beim Entladen per FreeLibrary aufrufen, NICHT beim Prozessende: dann sind die anderen Threads schon beendet,
// einer koennte mitten in einer Sperre gestorben sein, und das Warten darauf liesse das Spiel beim Schliessen haengen.
void FlushLog() {
    if (!g_logLockInit) return;
    std::vector<LogItem> batch;
    EnterCriticalSection(&g_logLock);
    batch.swap(g_logQueue);
    LeaveCriticalSection(&g_logLock);
    WriteItems(batch);
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

// Gleitkomma-Zustand der Engine (Diagnose): Genome rechnet mit x87-Steuerwort 007F (24 Bit, bCFPUAdmin). Geschuetzt wird er zentral in
// ScriptHost::Execute (script_host.cpp); hier nur feste Messpunkte im Protokoll ("FPU-Zustand ..."), um eine Abweichung zu sehen.
unsigned short FpuCw() {
    unsigned short cw = 0;
    __asm fnstcw cw
    return cw;
}
unsigned Mxcsr() { return _mm_getcsr(); }

// Zustand protokollieren (fester Messpunkt): x87-Steuerwort und MXCSR
void FpuState(const char* where) {
    char b[160];
    std::snprintf(b, sizeof b, "FPU-Zustand %s: x87-Steuerwort %04X, MXCSR %08X", where, FpuCw(), Mxcsr());
    Log(b);
}

void RunMainOnce() {
    if (g_mainRan) return;
    g_mainRan = true;
    if (g_gameHooks) InstallGameHooks(g_host);
    g_host.AddScript("main", g_scriptPath);   // Einzelskript (ini script=), falls vorhanden
    g_host.SetModsDir(g_modsDir);
    g_host.ScanMods();
    FpuState("vor main");
    g_host.CallMain();
    FpuState("nach main");
}
// Versionszeilen (Roadmap 2.2): Dateigroesse und PE-Zeitstempel der wichtigen Module, damit Fehlerberichte von anderen Installationen vergleichbar sind.
// "Spielordner ja/nein" zeigt z. B. ein DXVK-d3d9.dll (liegt im Spielordner) gegenueber dem System-d3d9.dll. Der Pfad selbst wird nicht ausgegeben.
// CRC32 (zlib-Polynom, wie 7-Zip/Python zlib.crc32) ueber den Dateiinhalt. Noetig, weil binaer gepatchte DLLs (Community-/PU-Patch-Varianten) dieselbe Groesse und
// denselben PE-Zeitstempel haben koennen (gemessen 2026-11-07: Game.dll Steam-Installation vs. Entwicklerinstallation, 15701 Bytes anders). 0 bei Lesefehler.
unsigned FileCrc32(const char* path, bool* ok) {
    static unsigned table[256];
    static bool init = false;
    if (!init) {
        for (unsigned i = 0; i < 256; ++i) {
            unsigned c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    *ok = false;
    FILE* f = std::fopen(path, "rb");
    if (!f) return 0;
    std::vector<unsigned char> buf(1 << 16);
    unsigned crc = 0xFFFFFFFFu;
    size_t n;
    while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0)
        for (size_t i = 0; i < n; ++i) crc = table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    *ok = !std::ferror(f);
    std::fclose(f);
    return crc ^ 0xFFFFFFFFu;
}
void LogModuleVersion(const char* name) {
    HMODULE m = GetModuleHandleA(name);
    if (!m) {
        Log(std::string("Modul ") + name + ": nicht geladen");
        return;
    }
    char path[MAX_PATH] = {};
    GetModuleFileNameA(m, path, MAX_PATH);
    bool crcOk = false;
    const unsigned crc = FileCrc32(path, &crcOk);
    WIN32_FILE_ATTRIBUTE_DATA fa = {};
    const bool haveSize = GetFileAttributesExA(path, GetFileExInfoStandard, &fa) != 0;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(m);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const char*>(m) + dos->e_lfanew);
    const bool inGameDir = !g_gameDir.empty() && _strnicmp(path, g_gameDir.c_str(), g_gameDir.size()) == 0;
    char b[240], crcText[16] = "-";
    if (crcOk) std::snprintf(crcText, sizeof crcText, "%08X", crc);
    std::snprintf(b, sizeof b, "Modul %s: %lu Bytes, PE-Zeitstempel %08lX, CRC32 %s, Spielordner %s", name, haveSize ? fa.nFileSizeLow : 0ul,
                  static_cast<unsigned long>(nt->FileHeader.TimeDateStamp), crcText, inGameDir ? "ja" : "nein");
    Log(b);
}
// Voraussetzungen: Community Patch 1.75 und Parallel-Universe-Patch (PU). Nur Pruefung und Protokoll, das Modkit laeuft in jedem Fall weiter.
// CP 1.75: PE-Zeitstempel der Gothic3.exe 4F959F4F (gleich in Entwicklerinstallation, Steam sauber und Steam gemoddet, gemessen 2026-11-13; die
// Game.dll taugt nicht, gemoddete Installationen patchen sie binaer). PU: paru_s1.dll und scripts\Script_paru.dll im Spielordner.
void LogPrerequisites() {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(GetModuleHandleA(nullptr));
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const char*>(dos) + dos->e_lfanew);
    const bool cp = nt->FileHeader.TimeDateStamp == 0x4F959F4Fu;
    const bool pu = GetFileAttributesA((g_gameDir + "paru_s1.dll").c_str()) != INVALID_FILE_ATTRIBUTES &&
                    GetFileAttributesA((g_gameDir + "scripts\\Script_paru.dll").c_str()) != INVALID_FILE_ATTRIBUTES;
    Log(std::string("Voraussetzungen: Community Patch 1.75 ") + (cp ? "ja" : "NEIN") + ", Parallel-Universe-Patch " + (pu ? "ja" : "NEIN"));
    if (!cp || !pu)
        Log("WARNUNG: G3ASM setzt Community Patch 1.75 und Parallel-Universe-Patch voraus; ohne sie ist es nicht getestet (Hooks pruefen ihre Bytes selbst)");
}
void LogModuleVersions() {
    // Script.dll und Script_Game.dll laden erst nach OnPostInitializeModules (gemessen 2026-11-07); sie werden bei der ersten Weltaktivierung nachgetragen.
    static const char* kMods[] = {"Gothic3.exe", "Game.dll", "Engine.dll", "SharedBase.dll", "d3d9.dll"};
    for (const char* n : kMods) LogModuleVersion(n);
    LogPrerequisites();
}
void LogScriptModuleVersions() {
    // Bei der ersten Weltaktivierung (Hauptmenue) sind beide oft noch nicht geladen (gemessen 2026-11-13, Steam sauber): dann bei der naechsten Aktivierung
    // erneut versuchen, statt zweimal "nicht geladen" festzuschreiben.
    static bool done = false;
    if (done) return;
    if (!GetModuleHandleA("Script.dll") || !GetModuleHandleA("Script_Game.dll")) return;
    done = true;
    LogModuleVersion("Script.dll");
    LogModuleVersion("Script_Game.dll");
}
void __fastcall HPostInit(void* self, void*) {
    g_orig[kPostInit](self);
    Log("Engine initialisiert (OnPostInitializeModules)");
    LogModuleVersions();
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

// Speichermessung (Diagnose, ini [g3asm] memlog=1, Standard aus): alle 30 s freier Speicher und Adressraum wie von GlobalMemoryStatusEx (das auch die Engine liest)
// ins Protokoll, Zeilen "Speicher: ..." (nur Lesen, aendert nichts).
bool g_memLog = false;
void MemoryProbe() {
    if (!g_memLog) return;
    static unsigned last = 0;
    static bool intro = false;
    const unsigned now = GetTickCount();
    if (intro && now - last < 30000) return;
    last = now;
    MEMORYSTATUSEX m = {sizeof m};
    if (!GlobalMemoryStatusEx(&m)) return;
    char b[320];
    if (!intro) {
        intro = true;
        auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleA(nullptr));
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
        const bool laa = (nt->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE) != 0;
        std::snprintf(b, sizeof b, "Speicher: Exe Large-Address-Aware=%s, Adressraum gesamt %llu MB", laa ? "ja" : "nein", m.ullTotalVirtual >> 20);
        Log(b);
    }
    const unsigned long long freeV = m.ullAvailVirtual >> 20;
    std::snprintf(b, sizeof b, "Speicher: physisch frei %llu von %llu MB, Adressraum frei %llu von %llu MB, Auslastung %lu %%%s", m.ullAvailPhys >> 20, m.ullTotalPhys >> 20, freeV,
                  m.ullTotalVirtual >> 20, m.dwMemoryLoad, freeV < 400 ? "  <-- Adressraum knapp" : "");
    Log(b);
}
// Ladepause (ini [g3asm] loadpause=<Sekunden>, 0 = aus): Waehrend eine Welt geladen wird (OnWorldDeactivate bis OnWorldActivate) und bis loadpause Sekunden
// nach dem letzten OnWorldActivate wird OnFrame der Skripte nicht aufgerufen. Test fuer "verwaschene Gebaeude-Texturen nach dem Laden": Der Lader holt die
// Gebaeude in dieser Zeit nach; jeder Skriptaufruf in C++ hat sie gestoert (gemessen 2026-11-06).
unsigned g_loadPauseMs = 0;
bool g_inWorldLoad = false;
unsigned g_lastActivate = 0;
bool g_pauseActive = false;
bool InLoadPause() {
    if (!g_loadPauseMs) return false;
    const bool p = g_inWorldLoad || (g_lastActivate && GetTickCount() - g_lastActivate < g_loadPauseMs);
    if (p != g_pauseActive) {
        g_pauseActive = p;
        Log(p ? "Ladepause: OnFrame der Skripte ausgesetzt" : "Ladepause: OnFrame der Skripte wieder an");
    }
    return p;
}
void __fastcall HProcess(void* self, void*) {
    g_orig[kProcess](self);
    MemoryProbe();
    InstallQuietMenu();
    if (!g_mainRan) RunMainOnce();   // falls OnPostInitializeModules vor dem Einhaengen lief
    if (g_hotReload) g_host.CheckReload();
    static bool firstFrame = true;
    if (firstFrame) {
        firstFrame = false;
        FpuState("erstes Bild");
    }
    ScriptCallsFrame();
    DrawFrameTick();
    MoreEventsFrame();
    if (InLoadPause()) return;
    g_host.CallFrame(FrameTime());
}
void __fastcall HWorldActivate(void* self, void*) {
    g_orig[kWorldActivate](self);
    FpuState("Welt aktiviert");
    LogScriptModuleVersions();
    InstallScriptGamePatches(g_host);
    g_inWorldLoad = false;
    g_lastActivate = GetTickCount();
    if (!g_lastActivate) g_lastActivate = 1;
    g_host.CallSimple("void OnWorldActivate()");
}
void __fastcall HWorldDeactivate(void* self, void*) {
    g_inWorldLoad = true;
    ScriptCallsReport();
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
        if (_stricmp(reinterpret_cast<const char*>(base + d->Name), "Engine.dll") != 0 || !d->OriginalFirstThunk) continue;
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
    g_log = _fsopen((g_gameDir + "g3asm.log").c_str(), "w", _SH_DENYNO);   // waehrend des Spiels lesbar
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
extern "C" __declspec(dllexport) int g3asm_init(HMODULE sharedBase, const char* logDir, void (*hostLog)(const char*)) {
    SetLoadDefaultOffMods(true);   // Testprogramm: auch Beispiel-Mods mit mod.ini enabled=0 laden (Kompilierpruefung aller Beispiele)
    if (const char* ai = std::getenv("G3ASM_TEST_AITRACE")) {   // Testprogramm: Einhaengen von KI-Tracer und Geschwindigkeitsmessung pruefen
        SetAiTraceOptions(*ai == '1', "", 50);
        SetAniSpeedLog(*ai == '1');
    }
    return InitCommon(sharedBase, logDir, hostLog) ? 1 : 0;
}

// Fuer Tests: Datei-Patches auf ein Testverzeichnis anwenden
extern "C" __declspec(dllexport) int g3asm_apply_patches(const char* gameDir, const char* modsDir) {
    return ApplyScriptPatches(gameDir, modsDir, [](const std::string& s) { Log(s); });
}

// Fuer Tests: Spielereignis-Hooks einhaengen (Game.dll muss geladen sein); Rueckgabe = Zahl aktiver Hooks
extern "C" __declspec(dllexport) int g3asm_install_hooks() { return InstallGameHooks(g_host); }

// Fuer Tests: Skriptdatei laden und main() ausfuehren; Rueckgabe = Anzahl fehlgeschlagener check()-Aufrufe, -1 bei Ladefehler
extern "C" __declspec(dllexport) int g3asm_run_file(const char* path) {
    const DWORD attr = GetFileAttributesA(path);
    const bool dirMode = attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
    if (dirMode) {   // Mod-Ordner (Tests)
        g_host.SetModsDir(path);
        g_host.ScanMods();
        if (!g_host.Loaded()) return -1;
        const std::vector<std::string> names = DiscoverModNames(path);
        auto idx = [&](const char* n) {
            for (size_t i = 0; i < names.size(); ++i)
                if (_stricmp(names[i].c_str(), n) == 0) return int(i);
            return -1;
        };
        if (idx("a_nach_alpha") >= 0) {   // tests\mods: Abhaengigkeiten aus mod.ini (API 40)
            g_host.Check(idx("a_nach_alpha") > idx("alpha") && idx("a_nach_alpha") > idx("zeta"),
                         "Abhaengigkeit: benoetigte Mods laden vor der Mod, die sie braucht");
            g_host.Check(idx("braucht_fehlt") < 0 && idx("braucht_gesperrt") < 0 && idx("braucht_aus") < 0 && idx("braucht_neu") < 0 &&
                             idx("api_zukunft") < 0,
                         "Abhaengigkeit: Mods mit unerfuellten Bedingungen werden nicht geladen");
        }
    } else if (!g_host.Load(path))
        return -1;
    g_host.CallMain();
    {   // Ereignisse mit Argumenten (ohne Spiel: ungueltige Entities)
        SStr id("GUID-123"), nm("It_Test");
        EvArg a[] = {EvArg::Ent(nullptr), EvArg::Ent(nullptr), EvArg::Str(&nm), EvArg::Str(&id), EvArg::U(7)};
        g_host.CallEvent("void OnReceiveItem(Entity, Entity, const bCString &in, const bCString &in, uint)", a, 5);
        g_host.CallEvent("void OnNPCKilled(Entity)", a, 1);
        EvArg n[] = {EvArg::Ent(nullptr), EvArg::Str(nullptr)};   // Nullzeiger aus der Engine: das Skript bekommt einen leeren Text
        g_host.CallEvent("void OnEnter(Entity, const bCString &in)", n, 2);
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
        if (!dirMode) {   // Argument aendern: die Mod verdoppelt es (SetOverrideArg), Super() reicht es ans Original weiter
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
extern "C" __declspec(dllexport) int g3asm_start(HMODULE sharedBase, const char* ini, const char* gameDir,
                                                     void (*hostLog)(const char*)) {
    if (!InitCommon(sharedBase, gameDir, hostLog)) return 0;
    char script[MAX_PATH] = "g3asm\\scripts\\main.as", modsRel[MAX_PATH] = "g3asm\\mods";
    if (ini && *ini) {
        // Abschnitt [g3asm]; fehlt er, gilt der fruehere Name [patch] (aeltere ini, og3_hooks.ini des Hauptrepos)
        // IniFile: wie GetPrivateProfile*, liest aber auch Dateien mit UTF-8-BOM (ini_file.h)
        const IniFile cfg(ini);
        if (cfg.Bom()) Log(std::string("Hinweis: ") + ini + " beginnt mit UTF-8-BOM (wird beruecksichtigt)");
        const char* sec = cfg.HasSection("g3asm") ? "g3asm" : "patch";
        cfg.Str(sec, "script", script, script, MAX_PATH);
        g_hotReload = cfg.Int(sec, "hotreload", 1) != 0;
        g_gameHooks = cfg.Int(sec, "hooks", 1) != 0;
        SetGameHookOptions(cfg.Int(sec, "navevents", 0) != 0);
        g_filePatches = cfg.Int(sec, "filepatches", 1) != 0;
        SetPrintToGameLog(cfg.Int(sec, "printgame", 1) != 0);
        g_asyncLog = cfg.Int(sec, "asynclog", 0) != 0;
        g_logWrite = cfg.Int(sec, "logwrite", 1) != 0;
        g_loadPauseMs = unsigned(cfg.Int(sec, "loadpause", 0)) * 1000u;
        g_memLog = cfg.Int(sec, "memlog", 0) != 0;
        SetScriptCallOptions(cfg.Int(sec, "scriptlog", 0) != 0);
        char aiFilter[128] = "";
        cfg.Str(sec, "aitracefilter", "", aiFilter, sizeof aiFilter);
        SetAiTraceOptions(cfg.Int(sec, "aitrace", 0) != 0, aiFilter, cfg.Int(sec, "aitracemax", 2000));
        SetAniSpeedLog(cfg.Int(sec, "anispeedlog", 0) != 0);
        cfg.Str(sec, "mods", modsRel, modsRel, MAX_PATH);
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
    if (reason == DLL_PROCESS_DETACH && !reserved) FlushLog();   // nur beim Entladen per FreeLibrary, nicht beim Prozessende (siehe FlushLog)
    return TRUE;
}
