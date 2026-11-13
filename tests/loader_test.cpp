// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// Test des G3ASM-Loaders (src/loader): laedt SharedBase.dll mit angepasstem Suchpfad (die dbghelp.dll aus demselben Ordner ist der Loader), prueft
//   1. der Loader ist geladen und hat g3asm.dll gestartet (g3asm_loader.log: "G3ASM gestartet", g3asm.log entsteht),
//   2. MiniDumpWriteDump wird an dbghelp_orig.dll weitergereicht (Aufruf mit ungueltigen Argumenten liefert FALSE, ohne Absturz).
// Aufruf: loader_test <Ordner>   (Ordner enthaelt SharedBase.dll, dbghelp.dll = Loader, dbghelp_orig.dll, g3asm.dll, g3asm.ini); tests/run_loader_test.ps1 richtet ihn ein.
#include <windows.h>

#include <cstdio>
#include <string>

static bool Exists(const std::string& p) { return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
static std::string ReadAll(const std::string& p) {
    std::string s;
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return s;
    char b[4096];
    size_t n;
    while ((n = std::fread(b, 1, sizeof b, f)) > 0) s.append(b, n);
    std::fclose(f);
    return s;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("Aufruf: loader_test <Ordner>\n");
        return 2;
    }
    std::string dir = argv[1];
    if (!dir.empty() && dir.back() != '\\') dir += '\\';
    int fails = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("%s: %s\n", ok ? "OK      " : "FEHLER  ", what);
        if (!ok) ++fails;
    };

    HMODULE sb = LoadLibraryExA((dir + "SharedBase.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    check(sb != nullptr, "SharedBase.dll geladen");
    if (!sb) return 1;

    HMODULE dbg = GetModuleHandleA("dbghelp.dll");
    char path[MAX_PATH] = "";
    if (dbg) GetModuleFileNameA(dbg, path, MAX_PATH);
    std::string loaded = path;
    check(dbg && _strnicmp(loaded.c_str(), dir.c_str(), dir.size()) == 0, "dbghelp.dll kommt aus dem Testordner (der Loader)");

    const std::string log = ReadAll(dir + "g3asm_loader.log");
    check(log.find("G3ASM-Loader gestartet") != std::string::npos, "g3asm_loader.log: Loader gestartet");
    check(log.find("G3ASM gestartet") != std::string::npos, "g3asm_loader.log: G3ASM gestartet");
    check(Exists(dir + "g3asm.log"), "g3asm.log angelegt");
    check(GetModuleHandleA("g3asm.dll") != nullptr, "g3asm.dll im Prozess");

    typedef BOOL(__stdcall * Dump)(HANDLE, DWORD, HANDLE, int, void*, void*, void*);
    Dump fn = dbg ? reinterpret_cast<Dump>(GetProcAddress(dbg, "MiniDumpWriteDump")) : nullptr;
    check(fn != nullptr, "MiniDumpWriteDump exportiert");
    if (fn) {
        const BOOL r = fn(nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr);
        check(r == FALSE, "MiniDumpWriteDump leitet weiter (ungueltige Argumente -> FALSE, kein Absturz)");
        check(GetModuleHandleA((dir + "dbghelp_orig.dll").c_str()) != nullptr, "dbghelp_orig.dll wurde dafuer geladen");
    }

    std::printf("%s (%d Fehler)\n", fails ? "FEHLGESCHLAGEN" : "alle Pruefungen bestanden", fails);
    return fails ? 1 : 0;
}
