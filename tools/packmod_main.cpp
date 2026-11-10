// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 OG3-Patch authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// og3_packmod: Kommandozeilenwerkzeug zum Packen und Pruefen von Gothic-3-Mod-Archiven (G3V0), Gegenstueck zu G3PakDir.
//   og3_packmod pack    <Ordner> <Archiv>                  Ordner als Archiv schreiben (z. B. Quests.mod)
//   og3_packmod install <Ordner> <Data-Ordner> <Volume> [m|n]   als naechste freie Generation ablegen (Quests.mod, Quests.m00, ...; n = .nod/.n00)
//   og3_packmod copy    <Archiv> <Spielordner|Data-Ordner> <Volume> [m|n]   fertiges Archiv (z. B. ModFiles\Quests.m0x) als naechste Generation einspielen
//   og3_packmod next    <Data-Ordner> <Volume> [m|n]       naechsten freien Dateinamen anzeigen
//   og3_packmod list    <Archiv>                           Inhalt anzeigen
// Optionen (vor den Argumenten): --comments (Quellpfade speichern), --no-deleted (".deleted"-Dateien als normale Dateien packen)
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/pack_mod.h"

using namespace og3p;

static int Usage() {
    std::printf("og3_packmod pack    <Ordner> <Archiv>\n"
                "og3_packmod install <Ordner> <Data-Ordner> <Volume> [m|n]\n"
                "og3_packmod next    <Data-Ordner> <Volume> [m|n]\n"
                "og3_packmod list    <Archiv>\n"
                "Optionen: --comments, --no-deleted\n");
    return 2;
}

static void Report(const PackStats& s) {
    std::printf("%d Dateien, %d Verzeichnisse, %d als geloescht markiert, %llu Byte Nutzdaten\n", s.files, s.dirs, s.deleted,
                (unsigned long long)s.bytes);
}

int main(int argc, char** argv) {
    PackOptions opt;
    std::vector<std::string> a;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--comments"))
            opt.comments = true;
        else if (!std::strcmp(argv[i], "--no-deleted"))
            opt.deletions = false;
        else
            a.push_back(argv[i]);
    }
    if (a.empty()) return Usage();
    const std::string& cmd = a[0];
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
