// Testprogramm fuer og3_patch.dll ohne Spiel: laedt eine SharedBase.dll (Original oder Nachbau), dann og3_patch.dll, und fuehrt ein
// Skript aus (main, ein OnFrame, OnShutdown). Rueckgabe = Zahl der fehlgeschlagenen check()-Aufrufe (-1 Ladefehler, >=100 Startfehler).
// Aufruf: og3_patch_test <SharedBase.dll> <skript.as> [Dialogs.dll zum Vorladen]
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <string>

static void HostLog(const char* s) { std::printf("%s\n", s); }

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 3) {
        std::printf("Aufruf: og3_patch_test <SharedBase.dll> <skript.as> [Dialogs.dll]\n");
        return 100;
    }
    char full[MAX_PATH];
    GetFullPathNameA(argv[1], MAX_PATH, full, nullptr);
    std::string dir(full);
    dir = dir.substr(0, dir.find_last_of("\\/"));
    SetDllDirectoryA(dir.c_str());
    if (argc > 3) LoadLibraryA(argv[3]);
    HMODULE sb = LoadLibraryA(full);
    if (!sb) {
        std::printf("%s nicht ladbar (%lu)\n", full, GetLastError());
        return 101;
    }
    char self[MAX_PATH];
    GetModuleFileNameA(nullptr, self, MAX_PATH);
    std::string selfDir(self);
    selfDir = selfDir.substr(0, selfDir.find_last_of('\\'));
    HMODULE p = LoadLibraryA((selfDir + "\\og3_patch.dll").c_str());
    typedef int (*InitFn)(HMODULE, const char*, void (*)(const char*));
    typedef int (*RunFn)(const char*);
    auto init = p ? reinterpret_cast<InitFn>(GetProcAddress(p, "og3_patch_init")) : nullptr;
    auto run = p ? reinterpret_cast<RunFn>(GetProcAddress(p, "og3_patch_run_file")) : nullptr;
    if (!init || !run) {
        std::printf("og3_patch.dll fehlt (%lu)\n", GetLastError());
        return 102;
    }
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    if (!std::getenv("OG3_SETTINGS_DIR"))   // Einstellungen je Mod (API 44) in den Temp-Ordner statt neben den Test-Mods
        _putenv_s("OG3_SETTINGS_DIR", (std::string(tmp) + "og3_test_settings").c_str());
    if (!init(sb, tmp, HostLog)) {
        std::printf("og3_patch_init fehlgeschlagen\n");
        return 103;
    }
    if (const char* pt = std::getenv("OG3_TEST_PATCHES")) {   // "<Spielordner>|<Modordner>": Datei-Patches anwenden (ohne Spiel)
        std::string v(pt);
        const size_t bar = v.find('|');
        auto apply = reinterpret_cast<int (*)(const char*, const char*)>(GetProcAddress(p, "og3_patch_apply_patches"));
        if (apply && bar != std::string::npos)
            std::printf("Datei-Patches: %d Datei(en) gepatcht\n", apply(v.substr(0, bar).c_str(), v.substr(bar + 1).c_str()));
    }
    if (const char* gd = std::getenv("OG3_TEST_GAMEDLL")) {   // optional: echte Game.dll laden und die Spielereignis-Hooks pruefen
        std::string gdir(gd);
        SetDllDirectoryA(gdir.substr(0, gdir.find_last_of("\\/")).c_str());
        HMODULE g = LoadLibraryA(gd);
        auto hooks = reinterpret_cast<int (*)()>(GetProcAddress(p, "og3_patch_install_hooks"));
        std::printf("Game.dll %s, Hooks aktiv: %d\n", g ? "geladen" : "NICHT ladbar", g && hooks ? hooks() : -1);
    }
    const int r = run(argv[2]);
    std::printf("Ergebnis: %s\n", r == 0 ? "alle Pruefungen bestanden" : r < 0 ? "Skript nicht geladen" : "Pruefungen fehlgeschlagen");
    std::fflush(stdout);
    TerminateProcess(GetCurrentProcess(), DWORD(r < 0 ? 104 : r));   // Original-DLL nicht aufraeumen lassen (bekanntes Haengen)
}
