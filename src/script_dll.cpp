// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// Laufzeit als Gothic-3-Script-DLL: <Spiel>\scripts\Script_G3ASM_<API>.dll (Ziel g3asm_script, gleiche Quellen wie g3asm.dll).
// Das Spiel laedt jede scripts\Script_*.dll selbst und ruft _ScriptInit@0 auf (gFScriptInit: gSScriptInit const* __stdcall()). So kommt das Modkit ohne
// Loader ins Spiel; jede Mod fuer Spieler bringt diese DLL mit (Union Starter: ScriptFiles\scripts\).
//
// Wer startet:
//   - Laeuft g3asm.dll schon (Loader-Weg, Entwicklung), tut die Script-DLL nichts.
//   - Liegen mehrere Script_G3ASM_<n>.dll nebeneinander (Mods mit verschiedenen Laufzeit-Versionen), startet nur die mit der hoechsten Nummer.
//   - g3asm.ini mit [g3asm] enable=0 schaltet auch diesen Weg ab.
// Gemessen 2026-11-16 (Mess-Stub, saubere Steam-Installation): ScriptInit kommt nach OnPostInitializeModules und vor Script_Game.dll; alle Hooks greifen,
// main() laeuft ueber den OnProcess-Rueckfall. Datei-Patches kommen auf diesem Weg zu spaet (Data ist schon erfasst) - Spieldateien als Archiv ausliefern.
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "ini_file.h"

extern const char* g_startOrigin;
extern "C" int g3asm_start(HMODULE sharedBase, const char* ini, const char* gameDir, void (*hostLog)(const char*));

namespace {
// gSScriptInit: fuenf bTObjArray (je Zeiger, Anzahl, Kapazitaet) = 60 Byte; leer = alles 0. Wir melden dem Spiel keine eigenen Skripte.
alignas(4) unsigned char g_emptyInit[60] = {};

// Nummer aus "Script_G3ASM_<n>.dll", -1 wenn der Name nicht passt
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
    if (GetModuleHandleA("g3asm.dll")) return;   // Loader-Weg aktiv: dort laeuft das Modkit schon
    HMODULE me = nullptr;                        // DllMain steht in g3asm_main.cpp; eigenes Modul ueber eine Adresse darin bestimmen
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&g_emptyInit), &me);
    char self[MAX_PATH];
    if (!me || !GetModuleFileNameA(me, self, MAX_PATH)) return;
    std::string dir(self);   // <Spiel>\scripts\Script_G3ASM_046.dll
    const size_t s1 = dir.find_last_of('\\');
    if (s1 == std::string::npos) return;
    const std::string myFile = dir.substr(s1 + 1), scripts = dir.substr(0, s1);
    const size_t s2 = scripts.find_last_of('\\');
    if (s2 == std::string::npos) return;
    const std::string game = scripts.substr(0, s2 + 1);
    // hoechste Nummer gewinnt; bei gleicher Nummer (anders geschrieben) der alphabetisch erste Dateiname
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
