// OG3-Loader: schlanke Ersatz-"dbghelp.dll" fuer den OG3-Patch (Gothic 3, 32 Bit). Eigenstaendig, ohne das OpenGothic3-Hauptrepo.
//
// SharedBase.dll importiert dbghelp.dll (nur MiniDumpWriteDump) und laedt sie aus dem Spielordner. Dieser Loader steht an dieser Stelle:
//   - leitet MiniDumpWriteDump an die umbenannte Original-DLL (dbghelp_orig.dll) weiter, damit das Spiel wie gewohnt Absturzberichte schreiben kann,
//   - laedt og3_patch.dll (neben dem Loader) und ruft og3_patch_start() auf,
//   - schreibt optional ein einfaches Absturzprotokoll (erste Chance, Modul+Offset).
// Einstellungen in og3_patch.ini neben der DLL (Fallback: og3_hooks.ini des Hauptrepos):
//   [patch]   enable=1 (Standard 1)   script=...  hotreload=...  mods=...  (Rest siehe docs/og3_patch.md)
//   [loader]  crashlog=1 (Standard 1)
// Protokoll: og3_loader.log neben der DLL.
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

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
    _snprintf_s(path, sizeof path, _TRUNCATE, "%sog3_loader.log", g_dir);
    // erster Eintrag eines Starts legt die Datei neu an, danach wird angehaengt
    HANDLE f = CreateFileA(path, g_logStarted ? FILE_APPEND_DATA : GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           g_logStarted ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    g_logStarted = true;
    DWORD w;
    WriteFile(f, line, static_cast<DWORD>(n), &w, nullptr);
    CloseHandle(f);
}

// "Modul+0xOffset" zu einer Adresse
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

// Absturzprotokoll: Adresse, Register und Rueckverweise des Stapels in Spielmodule. Es ist kein Absturz-Handler: die Ausnahme geht unveraendert weiter.
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

// Weiterleitung an die Original-DLL (ein Export, 7 Argumente, stdcall); ein reiner Forwarder im .def wird vom 32-Bit-Linker nicht angenommen.
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
    _snprintf_s(ini, sizeof ini, _TRUNCATE, "%sog3_patch.ini", g_dir);
    if (GetFileAttributesA(ini) == INVALID_FILE_ATTRIBUTES) _snprintf_s(ini, sizeof ini, _TRUNCATE, "%sog3_hooks.ini", g_dir);   // Hauptrepo-Datei

    Log("OG3-Loader gestartet (Einstellungen: %s)", GetFileAttributesA(ini) == INVALID_FILE_ATTRIBUTES ? "keine ini, Standardwerte" : ini);
    if (GetPrivateProfileIntA("loader", "crashlog", 1, ini) != 0) AddVectoredExceptionHandler(1, CrashLogger);
    if (GetPrivateProfileIntA("patch", "enable", 1, ini) == 0) {
        Log("OG3-Patch per ini abgeschaltet");
        return TRUE;
    }
    char dll[MAX_PATH + 32];
    _snprintf_s(dll, sizeof dll, _TRUNCATE, "%sog3_patch.dll", g_dir);
    HMODULE pp = LoadLibraryA(dll);
    using StartFn = int (*)(HMODULE, const char*, const char*, void (*)(const char*));
    StartFn start = pp ? reinterpret_cast<StartFn>(GetProcAddress(pp, "og3_patch_start")) : nullptr;
    if (!start) Log("og3_patch.dll nicht ladbar oder og3_patch_start fehlt (Fehler %lu)", GetLastError());
    else if (!start(GetModuleHandleA("SharedBase.dll"), ini, g_dir, PatchLog)) Log("OG3-Patch: Start fehlgeschlagen");
    else Log("OG3-Patch gestartet");
    return TRUE;
}
