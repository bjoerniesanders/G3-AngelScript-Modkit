// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Additional permissions: LICENSE-EXCEPTIONS.md
// Runtime as a Gothic 3 script DLL: <game>\scripts\Script_G3ASM_<API>.dll (target g3asm_script, same sources as g3asm.dll).
// The game loads each scripts\Script_*.dll itself and calls _ScriptInit@0 (gFScriptInit: gSScriptInit const* __stdcall()). Thus the modkit gets into the game without
// the loader. Each mod for players contains this DLL (Union Starter: ScriptFiles\scripts\).
//
// Which one starts:
//   - If g3asm.dll already runs (loader path, development), the script DLL does nothing.
//   - If there are more than one Script_G3ASM_<n>.dll (mods with different runtime versions), only the one with the highest number starts.
//   - g3asm.ini with [g3asm] enable=0 also turns off this path.
// Measured 2026-11-16 (measurement stub, clean Steam installation): ScriptInit comes after OnPostInitializeModules and before Script_Game.dll. All hooks work,
// main() runs through the OnProcess fallback. File patches come too late on this path (Data is already read) - give game files as an archive.
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "ini_file.h"

extern const char* g_startOrigin;
extern "C" int g3asm_start(HMODULE sharedBase, const char* ini, const char* gameDir, void (*hostLog)(const char*));

namespace {
// gSScriptInit: five bTObjArray (each pointer, count, capacity) = 60 bytes; empty = all 0. The modkit registers no own scripts with the game.
alignas(4) unsigned char g_emptyInit[60] = {};

// number from "Script_G3ASM_<n>.dll", -1 if the name does not agree
int VersionOf(const char* file) {
    const char* p = file;
    if (_strnicmp(p, "Script_G3ASM_", 13) != 0) return -1;
    p += 13;
    if (*p < '0' || *p > '9') return -1;
    char* end = nullptr;
    const long v = std::strtol(p, &end, 10);
    return (end && _stricmp(end, ".dll") == 0) ? int(v) : -1;
}

void Start() {
    static bool done = false;
    if (done) return;
    done = true;
    if (GetModuleHandleA("g3asm.dll")) return;   // loader path active: the modkit already runs there
    HMODULE me = nullptr;                        // DllMain is in g3asm_main.cpp; find the own module through an address in it
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&g_emptyInit), &me);
    char self[MAX_PATH];
    if (!me || !GetModuleFileNameA(me, self, MAX_PATH)) return;
    std::string dir(self);   // <game>\scripts\Script_G3ASM_046.dll
    const size_t s1 = dir.find_last_of('\\');
    if (s1 == std::string::npos) return;
    const std::string myFile = dir.substr(s1 + 1), scripts = dir.substr(0, s1);
    const size_t s2 = scripts.find_last_of('\\');
    if (s2 == std::string::npos) return;
    const std::string game = scripts.substr(0, s2 + 1);
    // the highest number wins; for the same number (different spelling), the file name that is first in alphabetical sequence
    const int mine = VersionOf(myFile.c_str());
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((scripts + "\\Script_G3ASM_*.dll").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        bool newer = false;
        do {
            const int v = VersionOf(fd.cFileName);
            if (v > mine || (v == mine && _stricmp(fd.cFileName, myFile.c_str()) < 0)) newer = true;
        } while (!newer && FindNextFileA(h, &fd));
        FindClose(h);
        if (newer) return;
    }
    const std::string ini = game + "g3asm.ini";
    const bool hasIni = GetFileAttributesA(ini.c_str()) != INVALID_FILE_ATTRIBUTES;
    if (hasIni) {
        const g3asm::IniFile cfg(ini.c_str());
        const char* sec = cfg.HasSection("g3asm") ? "g3asm" : "patch";
        if (cfg.Int(sec, "enable", 1) == 0) return;
    }
    static std::string origin;
    origin = "Script-DLL scripts\\" + myFile;
    g_startOrigin = origin.c_str();
    g3asm_start(GetModuleHandleA("SharedBase.dll"), hasIni ? ini.c_str() : "", game.c_str(), nullptr);
}
}   // namespace

extern "C" __declspec(dllexport) const void* __stdcall ScriptInit() {
    Start();
    return g_emptyInit;
}
