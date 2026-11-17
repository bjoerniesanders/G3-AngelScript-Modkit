// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Additional permissions: LICENSE-EXCEPTIONS.md
// G3ASM loader: small replacement "dbghelp.dll" for G3ASM (Gothic 3, 32 bit). Standalone, without the OpenGothic3 main repository.
//
// SharedBase.dll imports dbghelp.dll (only MiniDumpWriteDump) and loads it from the game folder. This loader is at that location:
//   - it sends MiniDumpWriteDump to the renamed original DLL (dbghelp_orig.dll), so that the game can write crash reports as usual,
//   - it loads g3asm.dll (next to the loader) and calls g3asm_start(),
//   - optionally, it writes a simple crash log (first chance, module+offset).
// Settings in g3asm.ini next to the DLL (fallback: og3_hooks.ini of the main repository):
//   [g3asm]   enable=1 (default 1)   script=...  hotreload=...  mods=...  (for the other settings, see docs/modkit/mod-package.md)
//   [loader]  crashlog=1 (default 1)
// Log: g3asm_loader.log next to the DLL.
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "../ini_file.h"

namespace {
char g_dir[MAX_PATH];
bool g_logStarted = false;

void Log(const char* fmt, ...) {
    char line[640], path[MAX_PATH + 32];
    va_list ap;
    va_start(ap, fmt);
    int n = _vsnprintf_s(line, sizeof line - 2, _TRUNCATE, fmt, ap);
    va_end(ap);
    if (n < 0) n = static_cast<int>(strlen(line));
    line[n++] = '\r';
    line[n++] = '\n';
    _snprintf_s(path, sizeof path, _TRUNCATE, "%sg3asm_loader.log", g_dir);
    // the first entry of a start makes the file again; after that, the loader appends
    HANDLE f = CreateFileA(path, g_logStarted ? FILE_APPEND_DATA : GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           g_logStarted ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    g_logStarted = true;
    DWORD w;
    WriteFile(f, line, static_cast<DWORD>(n), &w, nullptr);
    CloseHandle(f);
}

// "module+0xoffset" for an address
void Where(DWORD addr, char* out, size_t n) {
    HMODULE m = nullptr;
    char name[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(addr), &m) && m) {
        GetModuleFileNameA(m, name, MAX_PATH);
        const char* s = strrchr(name, '\\');
        _snprintf_s(out, n, _TRUNCATE, "%s+0x%lX", s ? s + 1 : name, static_cast<unsigned long>(addr - reinterpret_cast<DWORD>(m)));
    } else {
        _snprintf_s(out, n, _TRUNCATE, "%08lX", static_cast<unsigned long>(addr));
    }
}

// Crash log: address, registers and return addresses of the stack into game modules. It is not a crash handler: the exception continues without changes.
LONG CALLBACK CrashLogger(EXCEPTION_POINTERS* ep) {
    static volatile LONG count = 0;
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_INT_DIVIDE_BY_ZERO &&
        code != EXCEPTION_PRIV_INSTRUCTION && code != EXCEPTION_IN_PAGE_ERROR)
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&count) > 40) return EXCEPTION_CONTINUE_SEARCH;
    const CONTEXT* c = ep->ContextRecord;
    const DWORD eip = reinterpret_cast<DWORD>(ep->ExceptionRecord->ExceptionAddress);
    char w[MAX_PATH + 32];
    Where(eip, w, sizeof w);
    Log("ABSTURZ-Kandidat (erste Chance, nicht zwingend fatal): Ausnahme %08lX bei %s; Thread %lu", code, w, GetCurrentThreadId());
    if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2)
        Log("  Zugriff %s auf %08lX", ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "lesend" : (ep->ExceptionRecord->ExceptionInformation[0] == 1 ? "schreibend" : "ausfuehrend"),
            static_cast<unsigned long>(ep->ExceptionRecord->ExceptionInformation[1]));
    Log("  EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX ESI=%08lX EDI=%08lX EBP=%08lX ESP=%08lX", c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
    int shown = 0;
    const DWORD* sp = reinterpret_cast<const DWORD*>(c->Esp);
    for (int i = 0; i < 400 && shown < 24; ++i) {
        DWORD v;
        if (!ReadProcessMemory(GetCurrentProcess(), sp + i, &v, 4, nullptr)) break;
        HMODULE hm = nullptr;
        if (v > 0x10000 && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(v), &hm) && hm) {
            Where(v, w, sizeof w);
            Log("  Stapel[+%03X] %s", i * 4, w);
            ++shown;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void PatchLog(const char* s) { Log("%s", s); }
}   // namespace

// Forward to the original DLL (one export, 7 arguments, stdcall). The 32-bit linker does not accept a pure forwarder in the .def file.
extern "C" BOOL __stdcall MiniDumpWriteDump(HANDLE process, DWORD pid, HANDLE file, int type, void* exc, void* user, void* cb) {
    using Fn = BOOL(__stdcall*)(HANDLE, DWORD, HANDLE, int, void*, void*, void*);
    static Fn fn = nullptr;
    if (!fn) {
        char path[MAX_PATH + 32];
        _snprintf_s(path, sizeof path, _TRUNCATE, "%sdbghelp_orig.dll", g_dir);
        if (HMODULE m = LoadLibraryA(path)) fn = reinterpret_cast<Fn>(GetProcAddress(m, "MiniDumpWriteDump"));
        else Log("dbghelp_orig.dll nicht ladbar (Fehler %lu) - Absturzberichte des Spiels fehlen", GetLastError());
    }
    return fn ? fn(process, pid, file, type, exc, user, cb) : FALSE;
}

BOOL APIENTRY DllMain(HMODULE self, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(self);
    GetModuleFileNameA(self, g_dir, MAX_PATH);
    char* slash = strrchr(g_dir, '\\');
    if (slash) slash[1] = 0;

    char ini[MAX_PATH + 32];
    _snprintf_s(ini, sizeof ini, _TRUNCATE, "%sg3asm.ini", g_dir);
    if (GetFileAttributesA(ini) == INVALID_FILE_ATTRIBUTES) _snprintf_s(ini, sizeof ini, _TRUNCATE, "%sog3_hooks.ini", g_dir);   // file of the main repository

    Log("G3ASM-Loader gestartet (Einstellungen: %s)", GetFileAttributesA(ini) == INVALID_FILE_ATTRIBUTES ? "keine ini, Standardwerte" : ini);
    const g3asm::IniFile cfg(ini);
    if (cfg.Bom()) Log("Hinweis: %s beginnt mit UTF-8-BOM (wird beruecksichtigt)", ini);
    if (cfg.Int("loader", "crashlog", 1) != 0) AddVectoredExceptionHandler(1, CrashLogger);
    // section [g3asm]; if it is missing, the previous name [patch] applies
    const char* sec = cfg.HasSection("g3asm") ? "g3asm" : "patch";
    if (cfg.Int(sec, "enable", 1) == 0) {
        Log("G3ASM per ini abgeschaltet");
        return TRUE;
    }
    char dll[MAX_PATH + 32];
    _snprintf_s(dll, sizeof dll, _TRUNCATE, "%sg3asm.dll", g_dir);
    HMODULE pp = LoadLibraryA(dll);
    using StartFn = int (*)(HMODULE, const char*, const char*, void (*)(const char*));
    StartFn start = pp ? reinterpret_cast<StartFn>(GetProcAddress(pp, "g3asm_start")) : nullptr;
    if (!start) Log("g3asm.dll nicht ladbar oder g3asm_start fehlt (Fehler %lu)", GetLastError());
    else if (!start(GetModuleHandleA("SharedBase.dll"), ini, g_dir, PatchLog)) Log("G3ASM: Start fehlgeschlagen");
    else Log("G3ASM gestartet");
    return TRUE;
}
