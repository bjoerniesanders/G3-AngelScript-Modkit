// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Additional permissions: LICENSE-EXCEPTIONS.md
// g3asm.dll - G3 AngelScript Modkit: AngelScript mods for Gothic 3.
// The loader (src/loader, replacement dbghelp.dll; g3asm.ini [g3asm] enable=1) loads it. It works with the loaded SharedBase.dll (original or rebuild).
//
// Connection without a code change (compatible with the Parallel Universe Patch): the code redirects the import table of Gothic3.exe for four
// life-cycle functions of eCApplication (Engine.dll). The EXE derives its application class from eCApplication and
// calls these virtual functions through import thunks:
//   OnPostInitializeModules -> load the script, main()        OnProcess -> OnFrame(dt), reload when a file changes
//   OnWorldActivate / OnWorldDeactivate -> script events with the same names
// When the loader loads the DLL, the imports of the EXE are possibly not bound yet: a helper thread waits for this (maximum 60 s).
//
// ini [g3asm]: enable=0|1, script=g3asm\scripts\main.as (relative to the game folder), hotreload=1
// Log: <game folder>\g3asm.log
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
bool g_scriptIsDefault = true;   // script= not set or set to the default value
bool g_hotReload = true, g_gameHooks = true, g_filePatches = true;
FILE* g_log = nullptr;
void (*g_hostLog)(const char*) = nullptr;
CRITICAL_SECTION g_logLock;
bool g_logLockInit = false;
std::atomic<bool> g_initDone{false};
bool g_mainRan = false;
}   // namespace
// which component started the modkit (log line); script_dll.cpp sets it before g3asm_start
const char* g_startOrigin = "Loader (dbghelp.dll)";
namespace {

// Log. Default: write immediately. Optionally asynchronous (ini [g3asm] asynclog=1): the calling thread only puts the line into a queue, and a
// background thread writes g3asm.log and sends the lines to the loader (g3asm_loader.log). Made during the search for the blurred textures;
// the write was not the cause (the cause was fninit in the AngelScript call bridge, see CMakeLists.txt CLEAR_FPU_STACK).
bool g_asyncLog = false;
bool g_logWrite = true;   // ini [g3asm] logwrite=0: after the start (main ran), no more log lines (only for tests)
// One message: file line with a time stamp, and a line for the loader (without a time stamp, the full message as before)
struct LogItem {
    std::string fileLine, hostLine;
};
std::vector<LogItem> g_logQueue;   // only asynchronous, protected by g_logLock
CRITICAL_SECTION g_writeLock;       // serializes the write (caller, background thread, FlushLog)
HANDLE g_logEvent = nullptr;
bool g_logThread = false;
void InitLogLocks() {
    if (g_logLockInit) return;
    InitializeCriticalSection(&g_logLock);
    InitializeCriticalSection(&g_writeLock);
    g_logLockInit = true;
}
// writes the file under g_writeLock; then calls the loader outside the lock
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
// Write the remaining lines immediately. Call this only at an unload with FreeLibrary, NOT at the end of the process: then the other threads are already stopped,
// one could have died in the middle of a lock, and the wait for it would make the game hang when it closes.
void FlushLog() {
    if (!g_logLockInit) return;
    std::vector<LogItem> batch;
    EnterCriticalSection(&g_logLock);
    batch.swap(g_logQueue);
    LeaveCriticalSection(&g_logLock);
    WriteItems(batch);
}

// ---------------------------------------------------------------- frame time from the engine
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

// ---------------------------------------------------------------- import table of the EXE
typedef void(__thiscall* VoidMethod)(void*);
enum HookId { kPostInit = 0, kProcess, kWorldActivate, kWorldDeactivate, kHookCount };
const char* kHookNames[kHookCount] = {"?OnPostInitializeModules@eCApplication@@MAEXXZ", "?OnProcess@eCApplication@@UAEXXZ",
                                      "?OnWorldActivate@eCApplication@@UAEXXZ", "?OnWorldDeactivate@eCApplication@@UAEXXZ"};
VoidMethod g_orig[kHookCount] = {};
void** g_slot[kHookCount] = {};

// Floating-point state of the engine (diagnostics): Genome calculates with the x87 control word 007F (24 bit, bCFPUAdmin). It is protected centrally in
// ScriptHost::Execute (script_host.cpp). Here only fixed measurement points in the log ("FPU-Zustand ..."), to see a difference.
unsigned short FpuCw() {
    unsigned short cw = 0;
    __asm fnstcw cw
    return cw;
}
unsigned Mxcsr() { return _mm_getcsr(); }

// log the state (fixed measurement point): x87 control word and MXCSR
void FpuState(const char* where) {
    char b[160];
    std::snprintf(b, sizeof b, "FPU-Zustand %s: x87-Steuerwort %04X, MXCSR %08X", where, FpuCw(), Mxcsr());
    Log(b);
}

void RunMainOnce() {
    if (g_mainRan) return;
    g_mainRan = true;
    if (g_gameHooks) InstallGameHooks(g_host);
    // Single script (ini script=). If the file is missing at the default location, this is the normal case for players (only mods, script DLL without ini): then no entry
    // and no error message. A path that is set explicitly is always added (message; hot reload works when the file is made).
    if (!g_scriptIsDefault || GetFileAttributesA(g_scriptPath.c_str()) != INVALID_FILE_ATTRIBUTES) g_host.AddScript("main", g_scriptPath);
    g_host.SetModsDir(g_modsDir);
    g_host.ScanMods();
    FpuState("vor main");
    g_host.CallMain();
    FpuState("nach main");
}
// Version lines (roadmap 2.2): file size and PE time stamp of the important modules, so that error reports from other installations are comparable.
// "Spielordner ja/nein" (in the game folder yes/no) shows, for example, a DXVK d3d9.dll (in the game folder) against the system d3d9.dll. The path itself is not written.
// CRC32 (zlib polynomial, as 7-Zip and Python zlib.crc32) over the file contents. Necessary, because DLLs that are patched in binary form (Community Patch or PU patch variants) can have the same size and
// the same PE time stamp (measured 2026-11-07: Game.dll of the Steam installation against the development installation, 15701 bytes different). 0 for a read error.
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
// Requirements: Community Patch 1.75 and Parallel Universe Patch (PU). Only a check and a log line. The modkit continues to run in all cases.
// CP 1.75: PE time stamp of Gothic3.exe 4F959F4F (the same in the development installation, Steam clean and Steam modded, measured 2026-11-13; the
// Game.dll is not usable, because modded installations patch it in binary form). PU: paru_s1.dll and scripts\Script_paru.dll in the game folder.
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
    // Script.dll and Script_Game.dll load only after OnPostInitializeModules (measured 2026-11-07); the code adds them at the first world activation.
    static const char* kMods[] = {"Gothic3.exe", "Game.dll", "Engine.dll", "SharedBase.dll", "d3d9.dll"};
    for (const char* n : kMods) LogModuleVersion(n);
    LogPrerequisites();
}
void LogScriptModuleVersions() {
    // At the first world activation (main menu), the two are frequently not loaded yet (measured 2026-11-13, Steam clean): then try again at the next activation,
    // in place of writing "nicht geladen" (not loaded) two times.
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
// ---------------------------------------------------------------- Windows menu key without a beep
// F10 (and Alt alone or Alt+letter) puts Windows into the menu mode. The game window has no menu,
// thus DefWindowProc beeps at the next key push. The code discards SC_KEYMENU and WM_SYSCHAR.
// All other messages (also WM_SYSKEYDOWN for Alt+F4/Alt+Enter) go without changes to the window procedure of the game.
WNDPROC g_origWndProc = nullptr;
bool g_wndUnicode = false;
LRESULT CALLBACK QuietWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SYSCOMMAND && (wp & 0xFFF0) == SC_KEYMENU) return 0;
    if (msg == WM_SYSCHAR) return 0;
    if (msg == WM_MENUCHAR) return MAKELRESULT(0, MNC_CLOSE);
    // key event for scripts (OnKey); a repeat (bit 30) is not reported, and the message always continues to the game
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
    if (now - lastTry < 1000) return;   // window search maximum 1 time each second
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

// Memory measurement (diagnostics, ini [g3asm] memlog=1, default off): each 30 s, the free memory and the address space as from GlobalMemoryStatusEx (which the engine also reads)
// to the log, lines "Speicher: ..." (read only, changes nothing).
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
// Load pause (ini [g3asm] loadpause=<seconds>, 0 = off): while a world loads (OnWorldDeactivate to OnWorldActivate) and until loadpause seconds
// after the last OnWorldActivate, the modkit does not call OnFrame of the scripts. Test for "blurred building textures after a load": the loader gets the
// buildings during this time. Each script call in C++ disturbed them (measured 2026-11-06).
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
    if (!g_mainRan) RunMainOnce();   // in case OnPostInitializeModules ran before the hook was installed
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

// Patches all slots whose import is already bound (pointer outside the EXE). Return value: the number of slots that are still open.
int PatchBoundSlots() {
    int open = 0;
    for (int h = 0; h < kHookCount; ++h) {
        if (!g_slot[h] || g_orig[h]) continue;
        void* cur = *g_slot[h];
        if (!cur || InExe(cur)) {
            ++open;
            continue;
        }   // not bound yet (contains the RVA of the name)
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
    g_log = _fsopen((g_gameDir + "g3asm.log").c_str(), "w", _SH_DENYNO);   // readable during the game
    if (!g_sb.Resolve(sb ? sb : GetModuleHandleA("SharedBase.dll"))) {
        Log("SharedBase-Exporte fuer bCString fehlen - Abbruch: " + g_sb.missing);
        return false;
    }
    if (!g_sb.missing.empty()) Log("Hinweis: nicht gefundene SharedBase-Exporte: " + g_sb.missing);
    g_host.frameTime = &FrameTime;
    return g_host.Init([](const std::string& s) { Log(s); });
}

}   // namespace

// for tests: make the host without game hooks
extern "C" __declspec(dllexport) int g3asm_init(HMODULE sharedBase, const char* logDir, void (*hostLog)(const char*)) {
    SetLoadDefaultOffMods(true);   // test program: also load example mods with mod.ini enabled=0 (compile check of all examples)
    if (const char* ai = std::getenv("G3ASM_TEST_AITRACE")) {   // test program: check the installation of the AI tracer and the speed measurement
        SetAiTraceOptions(*ai == '1', "", 50);
        SetAniSpeedLog(*ai == '1');
    }
    return InitCommon(sharedBase, logDir, hostLog) ? 1 : 0;
}

// for tests: unpack packed mods from <gameDir>\Data\G3ASM.* to modsDir; return value = number of ready mods
extern "C" __declspec(dllexport) int g3asm_extract_packed(const char* gameDir, const char* modsDir) {
    std::string g = gameDir ? gameDir : "";
    if (!g.empty() && g.back() != '\\' && g.back() != '/') g += '\\';
    return ExtractPackedMods(g, modsDir ? modsDir : "", [](const std::string& s) { Log(s); });
}

// for tests: apply file patches to a test directory
extern "C" __declspec(dllexport) int g3asm_apply_patches(const char* gameDir, const char* modsDir) {
    return ApplyScriptPatches(gameDir, modsDir, [](const std::string& s) { Log(s); });
}

// for tests: install game event hooks (Game.dll must be loaded); return value = number of active hooks
extern "C" __declspec(dllexport) int g3asm_install_hooks() { return InstallGameHooks(g_host); }

// for tests: load a script file and run main(); return value = number of failed check() calls, -1 for a load error
extern "C" __declspec(dllexport) int g3asm_run_file(const char* path) {
    const DWORD attr = GetFileAttributesA(path);
    const bool dirMode = attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
    if (dirMode) {   // mod folder (tests)
        g_host.SetModsDir(path);
        g_host.ScanMods();
        if (!g_host.Loaded()) return -1;
        const std::vector<std::string> names = DiscoverModNames(path);
        auto idx = [&](const char* n) {
            for (size_t i = 0; i < names.size(); ++i)
                if (_stricmp(names[i].c_str(), n) == 0) return int(i);
            return -1;
        };
        if (idx("a_nach_alpha") >= 0) {   // tests\mods: dependencies from mod.ini (API 40)
            g_host.Check(idx("a_nach_alpha") > idx("alpha") && idx("a_nach_alpha") > idx("zeta"),
                         "Abhaengigkeit: benoetigte Mods laden vor der Mod, die sie braucht");
            g_host.Check(idx("braucht_fehlt") < 0 && idx("braucht_gesperrt") < 0 && idx("braucht_aus") < 0 && idx("braucht_neu") < 0 &&
                             idx("api_zukunft") < 0,
                         "Abhaengigkeit: Mods mit unerfuellten Bedingungen werden nicht geladen");
        }
    } else if (!g_host.Load(path))
        return -1;
    g_host.CallMain();
    {   // events with arguments (without the game: entities that are not valid)
        SStr id("GUID-123"), nm("It_Test");
        EvArg a[] = {EvArg::Ent(nullptr), EvArg::Ent(nullptr), EvArg::Str(&nm), EvArg::Str(&id), EvArg::U(7)};
        g_host.CallEvent("void OnReceiveItem(Entity, Entity, const bCString &in, const bCString &in, uint)", a, 5);
        g_host.CallEvent("void OnNPCKilled(Entity)", a, 1);
        EvArg n[] = {EvArg::Ent(nullptr), EvArg::Str(nullptr)};   // null pointer from the engine: the script gets an empty text
        g_host.CallEvent("void OnEnter(Entity, const bCString &in)", n, 2);
    }
    {   // override chain: Super() calls the "original", the script changes the return value; without Super(), the original is skipped
        EvArg a[] = {EvArg::U(5)};
        bool res = true, orig = false;
        bool handled = g_host.CallOverride(
            "bool Override_Test(uint)", a, 1,
            [&](const EvArg*) {
                orig = true;
                return true;
            },
            &res, &orig);
        if (dirMode) {   // chain across more than one mod (zeta -> alpha -> original): the two call Super(), the result stays true
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
        if (!dirMode) {   // change an argument: the mod doubles it (SetOverrideArg), Super() gives it to the original
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

// game mode (from the proxy): make the host and connect it to the import table of the EXE
extern "C" __declspec(dllexport) int g3asm_start(HMODULE sharedBase, const char* ini, const char* gameDir,
                                                     void (*hostLog)(const char*)) {
    if (!InitCommon(sharedBase, gameDir, hostLog)) return 0;
    char script[MAX_PATH] = "g3asm\\scripts\\main.as", modsRel[MAX_PATH] = "g3asm\\mods";
    bool packedMods = true;
    if (ini && *ini) {
        // section [g3asm]; if it is missing, the previous name [patch] applies (older ini, og3_hooks.ini of the main repository)
        // IniFile: as GetPrivateProfile*, but also reads files with UTF-8 BOM (ini_file.h)
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
        packedMods = cfg.Int(sec, "packedmods", 1) != 0;
    }
    g_modsDir = (modsRel[1] == ':' || modsRel[0] == '\\') ? std::string(modsRel) : g_gameDir + modsRel;
    while (!g_modsDir.empty() && (g_modsDir.back() == '\\' || g_modsDir.back() == '/')) g_modsDir.pop_back();
    g_scriptIsDefault = _stricmp(script, "g3asm\\scripts\\main.as") == 0;
    g_scriptPath = (script[1] == ':' || script[0] == '\\') ? std::string(script) : g_gameDir + script;
    Log(std::string("Gestartet ueber: ") + g_startOrigin);
    if (packedMods) ExtractPackedMods(g_gameDir, g_modsDir, [](const std::string& s) { Log(s); });   // Data\G3ASM.* -> mod folder
    if (g_filePatches)
        ApplyScriptPatches(g_gameDir, g_modsDir, [](const std::string& s) { Log(s); });   // before the game reads the data for the first time
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
    if (reason == DLL_PROCESS_DETACH && !reserved) FlushLog();   // only at an unload with FreeLibrary, not at the end of the process (see FlushLog)
    return TRUE;
}
