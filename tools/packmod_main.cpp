// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// g3asm_packmod: Kommandozeilenwerkzeug zum Packen und Pruefen von Gothic-3-Mod-Archiven (G3V0), Gegenstueck zu G3PakDir.
//   g3asm_packmod pack    <Ordner> <Archiv>                  Ordner als Archiv schreiben (z. B. Quests.mod)
//   g3asm_packmod install <Ordner> <Data-Ordner> <Volume> [m|n]   als naechste freie Generation ablegen (Quests.mod, Quests.m00, ...; n = .nod/.n00)
//   g3asm_packmod copy    <Archiv> <Spielordner|Data-Ordner> <Volume> [m|n]   fertiges Archiv (z. B. ModFiles\Quests.m0x) als naechste Generation einspielen
//   g3asm_packmod next    <Data-Ordner> <Volume> [m|n]       naechsten freien Dateinamen anzeigen
//   g3asm_packmod list    <Archiv>                           Inhalt anzeigen
//   g3asm_packmod release <Mod-Ordner> <Ziel-Ordner> [--runtime <Script_G3ASM_NNN.dll>]
//                                                            Paket fuer Spieler (Union-Starter-Aufbau) bauen, siehe Release()
// Optionen (vor den Argumenten): --comments (Quellpfade speichern), --no-deleted (".deleted"-Dateien als normale Dateien packen)
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../src/pack_mod.h"

using namespace g3asm;

static int Usage() {
    std::printf("g3asm_packmod pack    <Ordner> <Archiv>\n"
                "g3asm_packmod install <Ordner> <Data-Ordner> <Volume> [m|n]\n"
                "g3asm_packmod copy    <Archiv> <Spielordner|Data-Ordner> <Volume> [m|n]\n"
                "g3asm_packmod next    <Data-Ordner> <Volume> [m|n]\n"
                "g3asm_packmod list    <Archiv>\n"
                "g3asm_packmod release <Mod-Ordner> <Ziel-Ordner> [--runtime <Script_G3ASM_NNN.dll>]\n"
                "Optionen: --comments, --no-deleted\n");
    return 2;
}

// ---------------------------------------------------------------- release: Paket fuer Spieler
// Aus einem Mod-Ordner (wie unter g3asm\mods\) wird ein Paket im Aufbau des Union Starters (MDS\Mods\<Paket>\):
//   <Ziel>\<Mod>\Mod.ini                                  [Main] Name, Category, Required (aus [mod] title/category/requires)
//   <Ziel>\<Mod>\ScriptFiles\scripts\Script_G3ASM_NNN.dll  Laufzeit (das Spiel laedt sie selbst; kein Modkit beim Spieler noetig)
//   <Ziel>\<Mod>\ModFiles\G3ASM.m0x                       Skripte, mod.ini, Texte unter <Mod>/ (das Modkit entpackt sie beim Start)
//   <Ziel>\<Mod>\ModFiles\<Volume>.m0x / .n0x            Spieldaten-Archive der Mod (aus <Mod>\ModFiles\), unveraendert
// Nicht ins Skript-Archiv: ModFiles\*.m0x/.n0x (eigene Archive), Source\ (Quellen der Archive), ScriptPatches\ (Datei-Patches wirken nur mit dem Loader),
// Dateien/Ordner mit Punkt am Anfang. enabled=0 in mod.ini wird im Paket zu enabled=1 (Spieler haben kein load_order.txt).
namespace {
bool IsDirA(const std::string& p) {
    const DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
bool IsFileA(const std::string& p) {
    const DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
std::string LowerA(std::string s) {
    for (char& c : s) c = char(tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string Trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return std::string();
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}
bool MakeDirs(const std::string& dir) {
    if (dir.empty() || IsDirA(dir)) return true;
    const size_t s = dir.find_last_of("\\/");
    if (s != std::string::npos && s > 2 && !MakeDirs(dir.substr(0, s))) return false;
    return CreateDirectoryA(dir.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}
bool ReadText(const std::string& path, std::string* out) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return false;
    char buf[8192];
    size_t n;
    out->clear();
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out->append(buf, n);
    std::fclose(f);
    return true;
}
bool WriteText(const std::string& path, const std::string& data) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return false;
    const bool ok = data.empty() || std::fwrite(data.data(), 1, data.size(), f) == data.size();
    return std::fclose(f) == 0 && ok;
}
void RemoveTree(const std::string& dir) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!std::strcmp(fd.cFileName, ".") || !std::strcmp(fd.cFileName, "..")) continue;
            const std::string p = dir + "\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                RemoveTree(p);
            else
                DeleteFileA(p.c_str());
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(dir.c_str());
}
// [mod]-Abschnitt einer mod.ini: Schluessel klein -> Wert (UTF-8-BOM erlaubt)
std::vector<std::pair<std::string, std::string>> ModSection(const std::string& text) {
    std::vector<std::pair<std::string, std::string>> out;
    bool in = false;
    size_t b = text.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
    while (b < text.size()) {
        size_t e = text.find('\n', b);
        if (e == std::string::npos) e = text.size();
        const std::string l = Trim(text.substr(b, e - b));
        b = e + 1;
        if (l.empty() || l[0] == ';' || l[0] == '#') continue;
        if (l[0] == '[') {
            in = LowerA(l) == "[mod]";
            continue;
        }
        const size_t eq = l.find('=');
        if (in && eq != std::string::npos) out.emplace_back(LowerA(Trim(l.substr(0, eq))), Trim(l.substr(eq + 1)));
    }
    return out;
}
std::string Get(const std::vector<std::pair<std::string, std::string>>& sec, const char* key) {
    for (const auto& kv : sec)
        if (kv.first == key) return kv.second;
    return std::string();
}
// mod.ini fuer das Paket: Zeilen "enabled=0" im [mod]-Abschnitt werden zu "enabled=1"
std::string EnableModIni(const std::string& text, bool* changed) {
    std::string out;
    bool in = false;
    size_t b = 0;
    *changed = false;
    while (b < text.size()) {
        size_t e = text.find('\n', b);
        const size_t end = e == std::string::npos ? text.size() : e + 1;
        std::string line = text.substr(b, end - b);
        b = end;
        std::string t = Trim(line);
        if (t.compare(0, 3, "\xEF\xBB\xBF") == 0) t = Trim(t.substr(3));
        if (!t.empty() && t[0] == '[') in = LowerA(t) == "[mod]";
        const size_t eq = t.find('=');
        if (in && eq != std::string::npos && LowerA(Trim(t.substr(0, eq))) == "enabled" && std::atoi(Trim(t.substr(eq + 1)).c_str()) == 0) {
            const bool bom = line.compare(0, 3, "\xEF\xBB\xBF") == 0;
            line = std::string(bom ? "\xEF\xBB\xBF" : "") + "enabled=1" + (line.find("\r\n") != std::string::npos ? "\r\n" : "\n");
            *changed = true;
        }
        out += line;
    }
    return out;
}
struct Copied {
    int files = 0;
    std::vector<std::string> skipped;
};
// Mod-Ordner in den Bereitstellungsordner kopieren, ohne die Teile, die nicht ins Skript-Archiv gehoeren (rel = Pfad relativ zum Mod-Ordner)
bool CopyModTree(const std::string& src, const std::string& dst, const std::string& rel, Copied* c, std::string* err) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((src + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return true;
    bool ok = true;
    do {
        const std::string n = fd.cFileName;
        if (n == "." || n == "..") continue;
        const std::string r = rel.empty() ? n : rel + "\\" + n, ln = LowerA(r);
        const bool dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        const std::string ext = LowerA(n.size() > 4 ? n.substr(n.size() - 4) : std::string());
        if (n[0] == '.' || ln == "source" || ln == "scriptpatches" ||
            (!dir && ln.compare(0, 9, "modfiles\\") == 0 && (ext == ".m0x" || ext == ".n0x"))) {
            c->skipped.push_back(r + (dir ? "\\" : ""));
            continue;
        }
        if (dir) {
            if (!MakeDirs(dst + "\\" + n) || !CopyModTree(src + "\\" + n, dst + "\\" + n, r, c, err)) ok = false;
        } else if (!CopyFileA((src + "\\" + n).c_str(), (dst + "\\" + n).c_str(), FALSE)) {
            *err = "Kopieren fehlgeschlagen: " + r;
            ok = false;
        } else
            ++c->files;
    } while (ok && FindNextFileA(h, &fd));
    FindClose(h);
    return ok;
}
// Laufzeit: --runtime oder die Script_G3ASM_<n>.dll mit der hoechsten Nummer neben diesem Programm
std::string FindRuntime() {
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir(exe);
    dir = dir.substr(0, dir.find_last_of('\\'));
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\Script_G3ASM_*.dll").c_str(), &fd);
    std::string best;
    long bestV = -1;
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const long v = std::strtol(fd.cFileName + 13, nullptr, 10);
            if (v > bestV) {
                bestV = v;
                best = dir + "\\" + fd.cFileName;
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    return best;
}

int Release(std::string modDir, const std::string& outRoot, std::string runtime, const PackOptions& opt) {
    while (!modDir.empty() && (modDir.back() == '\\' || modDir.back() == '/')) modDir.pop_back();
    char full[MAX_PATH];
    if (GetFullPathNameA(modDir.c_str(), MAX_PATH, full, nullptr)) modDir = full;
    const std::string name = modDir.substr(modDir.find_last_of("\\/") + 1);
    if (!IsDirA(modDir) || name.empty()) {
        std::printf("Fehler: Mod-Ordner fehlt: %s\n", modDir.c_str());
        return 1;
    }
    for (char c : name)
        if (c == ' ' || c == ';' || c == '=' || static_cast<unsigned char>(c) > 126) {
            std::printf("Fehler: Mod-Name '%s' nur mit Buchstaben, Ziffern, _ und - (er wird Ordnername beim Spieler)\n", name.c_str());
            return 1;
        }
    if (!IsFileA(modDir + "\\main.as") && !IsFileA(modDir + "\\stringtable.ini") && !IsFileA(modDir + "\\ModFiles\\stringtable.ini")) {
        std::printf("Fehler: %s hat weder main.as noch stringtable.ini - keine G3ASM-Mod\n", modDir.c_str());
        return 1;
    }
    if (runtime.empty()) runtime = FindRuntime();
    const std::string rtFile = runtime.substr(runtime.find_last_of("\\/") + 1);
    if (runtime.empty() || !IsFileA(runtime) || _strnicmp(rtFile.c_str(), "Script_G3ASM_", 13) != 0) {
        std::printf("Fehler: Laufzeit Script_G3ASM_<API>.dll nicht gefunden (neben g3asm_packmod.exe oder mit --runtime angeben)\n");
        return 1;
    }
    const std::string pkg = outRoot + "\\" + name;
    if (IsDirA(pkg) || IsFileA(pkg)) {
        std::printf("Fehler: Ziel existiert schon: %s (vorher loeschen)\n", pkg.c_str());
        return 1;
    }

    std::string iniText;
    ReadText(modDir + "\\mod.ini", &iniText);
    const auto sec = ModSection(iniText);
    std::string title = Get(sec, "title"), category = Get(sec, "category");
    if (title.empty()) title = name;
    if (category.empty()) category = "G3ASM";
    std::string required;   // requires=a >= 1.0, b  ->  Required=a;b
    {
        const std::string req = Get(sec, "requires");
        size_t p = 0;
        while (p < req.size()) {
            size_t c = req.find(',', p);
            if (c == std::string::npos) c = req.size();
            std::string item = Trim(req.substr(p, c - p));
            p = c + 1;
            const size_t ge = item.find(">=");
            if (ge != std::string::npos) item = Trim(item.substr(0, ge));
            if (!item.empty()) required += (required.empty() ? "" : ";") + item;
        }
    }

    // Bereitstellen: <tmp>\<Mod>\... -> packen als <Mod>/... ; danach entfernen
    char tmpBase[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpBase);
    const std::string stage = std::string(tmpBase) + "g3asm_release_" + std::to_string(GetCurrentProcessId()),
                      stageMod = stage + "\\" + name;
    RemoveTree(stage);
    Copied copied;
    std::string err;
    if (!MakeDirs(stageMod) || !CopyModTree(modDir, stageMod, "", &copied, &err)) {
        RemoveTree(stage);
        std::printf("Fehler: %s\n", err.empty() ? "Bereitstellungsordner nicht anlegbar" : err.c_str());
        return 1;
    }
    bool enabledChanged = false;
    if (!iniText.empty() && !WriteText(stageMod + "\\mod.ini", EnableModIni(iniText, &enabledChanged))) {
        RemoveTree(stage);
        std::printf("Fehler: mod.ini nicht schreibbar\n");
        return 1;
    }
    PackStats st;
    const bool packed = MakeDirs(pkg + "\\ModFiles") && PackFolder(stage, pkg + "\\ModFiles\\G3ASM.m0x", opt, &st);
    RemoveTree(stage);
    if (!packed) {
        std::printf("Fehler: Skript-Archiv: %s\n", st.error.c_str());
        RemoveTree(pkg);
        return 1;
    }
    // Spieldaten-Archive der Mod unveraendert uebernehmen
    int archives = 0;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((modDir + "\\ModFiles\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const std::string n = fd.cFileName, ext = LowerA(n.size() > 4 ? n.substr(n.size() - 4) : std::string());
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (ext != ".m0x" && ext != ".n0x")) continue;
            if (LowerA(n) == "g3asm.m0x") {
                std::printf("Hinweis: %s\\ModFiles\\G3ASM.m0x ignoriert (der Name gehoert dem Skript-Archiv)\n", modDir.c_str());
                continue;
            }
            std::vector<ArchiveEntry> e;
            if (!ListArchive(modDir + "\\ModFiles\\" + n, &e, &err)) {
                std::printf("Fehler: %s ist kein gueltiges Archiv: %s\n", n.c_str(), err.c_str());
                RemoveTree(pkg);
                return 1;
            }
            if (!CopyFileA((modDir + "\\ModFiles\\" + n).c_str(), (pkg + "\\ModFiles\\" + n).c_str(), TRUE)) {
                std::printf("Fehler: %s nicht kopierbar\n", n.c_str());
                RemoveTree(pkg);
                return 1;
            }
            ++archives;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    if (!MakeDirs(pkg + "\\ScriptFiles\\scripts") ||
        !CopyFileA(runtime.c_str(), (pkg + "\\ScriptFiles\\scripts\\" + rtFile).c_str(), TRUE)) {
        std::printf("Fehler: Laufzeit nicht kopierbar\n");
        RemoveTree(pkg);
        return 1;
    }
    std::string modIni = "[Main]\r\nName=" + title + "\r\nCategory=" + category + "\r\n";
    if (!required.empty()) modIni += "Required=" + required + "\r\n";
    if (!WriteText(pkg + "\\Mod.ini", modIni)) {
        std::printf("Fehler: Mod.ini nicht schreibbar\n");
        RemoveTree(pkg);
        return 1;
    }

    std::printf("Paket: %s\n", pkg.c_str());
    std::printf("  Mod.ini                 Name=%s, Category=%s%s%s\n", title.c_str(), category.c_str(),
                required.empty() ? "" : ", Required=", required.c_str());
    std::printf("  ModFiles\\G3ASM.m0x      %d Datei(en) unter %s/\n", copied.files, name.c_str());
    if (archives) std::printf("  ModFiles\\               %d Spieldaten-Archiv(e) uebernommen\n", archives);
    std::printf("  ScriptFiles\\scripts\\    %s\n", rtFile.c_str());
    if (enabledChanged) std::printf("Hinweis: enabled=0 in mod.ini steht im Paket als enabled=1 (Spieler haben kein load_order.txt)\n");
    for (const std::string& s : copied.skipped)
        std::printf("Hinweis: nicht im Skript-Archiv: %s%s\n", s.c_str(),
                    LowerA(s).compare(0, 13, "scriptpatches") == 0 ? " (Datei-Patches wirken nur mit dem Loader, nicht bei Spielern)" : "");
    std::printf(
        "Spieler: Ordner nach <Gothic 3>\\MDS\\Mods\\ legen und im Union Starter installieren, oder ScriptFiles\\* in den Spielordner und\n"
        "ModFiles\\*.m0x als naechste freie Generation nach Data\\ kopieren (g3asm_packmod copy).\n");
    return 0;
}
}   // namespace

static void Report(const PackStats& s) {
    std::printf("%d Dateien, %d Verzeichnisse, %d als geloescht markiert, %llu Byte Nutzdaten\n", s.files, s.dirs, s.deleted,
                (unsigned long long)s.bytes);
}

int main(int argc, char** argv) {
    PackOptions opt;
    std::vector<std::string> a;
    std::string runtime;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--comments"))
            opt.comments = true;
        else if (!std::strcmp(argv[i], "--no-deleted"))
            opt.deletions = false;
        else if (!std::strcmp(argv[i], "--runtime") && i + 1 < argc)
            runtime = argv[++i];
        else
            a.push_back(argv[i]);
    }
    if (a.empty()) return Usage();
    const std::string& cmd = a[0];
    if (cmd == "release" && a.size() == 3) return Release(a[1], a[2], runtime, opt);
    PackStats st;
    if (cmd == "pack" && a.size() == 3) {
        if (!PackFolder(a[1], a[2], opt, &st)) {
            std::printf("Fehler: %s\n", st.error.c_str());
            return 1;
        }
        std::printf("geschrieben: %s\n", a[2].c_str());
        Report(st);
        return 0;
    }
    if (cmd == "install" && (a.size() == 4 || a.size() == 5)) {
        const char kind = a.size() == 5 && a[4] == "n" ? 'n' : 'm';
        std::string data = a[2];
        if (GetFileAttributesA((data + "\\Data").c_str()) != INVALID_FILE_ATTRIBUTES)
            data += "\\Data";   // Spielordner statt Data-Ordner angegeben
        const std::string out = PackMod(a[1], data, a[3], kind, opt, &st);
        if (out.empty()) {
            std::printf("Fehler: %s\n", st.error.c_str());
            return 1;
        }
        std::printf("installiert: %s\n", out.c_str());
        Report(st);
        return 0;
    }
    if (cmd == "copy" &&
        (a.size() == 4 ||
         a.size() == 5)) {   // fertiges Archiv (z. B. ModFiles\Quests.m0x) als naechste Generation einspielen, wie der Union Starter
        const char kind = a.size() == 5 && a[4] == "n" ? 'n' : 'm';
        std::string data = a[2];
        if (GetFileAttributesA((data + "\\Data").c_str()) != INVALID_FILE_ATTRIBUTES) data += "\\Data";
        std::vector<ArchiveEntry> e;
        std::string err;
        if (!ListArchive(a[1], &e, &err)) {
            std::printf("Fehler: kein gueltiges Archiv: %s\n", err.c_str());
            return 1;
        }   // nichts Unbrauchbares ins Spiel kopieren
        const std::string out = NextGenerationFile(data, a[3], kind);
        if (out.empty()) {
            std::printf("Fehler: kein freier Platz fuer '%s'\n", a[3].c_str());
            return 1;
        }
        if (!CopyFileA(a[1].c_str(), out.c_str(), TRUE)) {
            std::printf("Fehler: Kopieren nach %s fehlgeschlagen (%lu)\n", out.c_str(), GetLastError());
            return 1;
        }
        std::printf("kopiert: %s\n", out.c_str());
        return 0;
    }
    if (cmd == "next" && (a.size() == 3 || a.size() == 4)) {
        const std::string out = NextGenerationFile(a[1], a[2], a.size() == 4 && a[3] == "n" ? 'n' : 'm');
        if (out.empty()) {
            std::printf("kein freier Platz\n");
            return 1;
        }
        std::printf("%s\n", out.c_str());
        return 0;
    }
    if (cmd == "list" && a.size() == 2) {
        std::vector<ArchiveEntry> e;
        std::string err;
        if (!ListArchive(a[1], &e, &err)) {
            std::printf("Fehler: %s\n", err.c_str());
            return 1;
        }
        int files = 0, dirs = 0, del = 0;
        for (const ArchiveEntry& x : e) {
            if (x.isDir) {
                ++dirs;
                std::printf("  [Ordner] %s\n", x.name.c_str());
            } else if (x.deleted) {
                ++del;
                std::printf("  [geloescht] %s\n", x.name.c_str());
            } else {
                ++files;
                std::printf("  %10llu  %s%s\n", (unsigned long long)x.size, x.name.c_str(), x.compressed ? "  (gepackt)" : "");
            }
        }
        std::printf("%d Dateien, %d Verzeichnisse, %d geloescht\n", files, dirs, del);
        return 0;
    }
    return Usage();
}
