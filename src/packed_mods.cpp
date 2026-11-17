// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Additional permissions: LICENSE-EXCEPTIONS.md
// Packed mods: players get a mod as the archive <game>\Data\G3ASM.mod, .m00, .m01, ... (Union Starter: ModFiles\G3ASM.m0x). The game itself
// never opens these archives (unknown name, measured 2026-11-16). At the start, the modkit unpacks each mod in them to <mods>\<mod>\ and puts the
// marker .g3asm_packed there (source archive, file list). Then the mod loads as a loose folder (main.as, #include, mod.ini, stringtable.ini, load_order.txt).
//
// Layout of an archive: top level = one folder for each mod (<mod>/main.as, <mod>/mod.ini, ...). Files directly on the top level are ignored.
// Rules:
//   - The modkit never touches a folder WITHOUT a marker (loose mod, development). It has priority over the packed version with the same name.
//   - The modkit unpacks a marked folder again if the source archive (name, size, time) changed. If not, the folder stays.
//   - The modkit removes a marked folder whose mod is not in an archive anymore (cleared in the Starter): only the listed files, then empty folders.
//   - If the same mod is in two archives, the first one applies (.mod before .m00 before .m01 ...).
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "pack_mod.h"
#include "script_host.h"

namespace g3asm {
namespace {

using LogFn = std::function<void(const std::string&)>;
const char kMarker[] = ".g3asm_packed";
const char kMarkerHead[] = "G3ASM packed 1";

std::string Lower(std::string s) {
    for (char& c : s) c = char(tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string Win(std::string s) {
    for (char& c : s)
        if (c == '/') c = '\\';
    return s;
}
bool IsDir(const std::string& p) {
    const DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
bool IsFile(const std::string& p) {
    const DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
// makes all missing folders of a path (without the file name)
bool MakeDirs(const std::string& dir) {
    if (dir.empty() || IsDir(dir)) return true;
    const size_t s = dir.find_last_of('\\');
    if (s != std::string::npos && s > 2 && !MakeDirs(dir.substr(0, s))) return false;
    return CreateDirectoryA(dir.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}
// one part of a path in the archive: no empty part, no "." or "..", no drive characters or control characters
bool SafePart(const std::string& p) {
    if (p.empty() || p == "." || p == "..") return false;
    for (char c : p)
        if (c == ':' || c == '\\' || static_cast<unsigned char>(c) < 32 || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' ||
            c == '|')
            return false;
    return true;
}
bool SplitSafe(const std::string& name, std::vector<std::string>* parts) {
    parts->clear();
    size_t b = 0;
    while (b <= name.size()) {
        size_t e = name.find('/', b);
        if (e == std::string::npos) e = name.size();
        const std::string p = name.substr(b, e - b);
        if (!SafePart(p)) return false;
        parts->push_back(p);
        b = e + 1;
    }
    return !parts->empty();
}

struct Archive {
    std::string path, file;   // full path, file name (G3ASM.m00)
    uint64_t size = 0, time = 0;
};
std::string SourceLine(const Archive& a) { return a.file + "\t" + std::to_string(a.size) + "\t" + std::to_string(a.time); }
// archives in the sequence .mod, .m00 ... .m99
std::vector<Archive> FindArchives(const std::string& dataDir) {
    std::vector<Archive> out;
    for (int i = -1; i < 100; ++i) {
        char ext[8];
        if (i < 0)
            std::snprintf(ext, sizeof ext, ".mod");
        else
            std::snprintf(ext, sizeof ext, ".m%02d", i);
        Archive a;
        a.file = std::string("G3ASM") + ext;
        a.path = dataDir + "\\" + a.file;
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (!GetFileAttributesExA(a.path.c_str(), GetFileExInfoStandard, &fa) || (fa.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        a.size = (uint64_t(fa.nFileSizeHigh) << 32) | fa.nFileSizeLow;
        a.time = (uint64_t(fa.ftLastWriteTime.dwHighDateTime) << 32) | fa.ftLastWriteTime.dwLowDateTime;
        out.push_back(a);
    }
    return out;
}

struct Marker {
    bool valid = false;
    std::string source;               // line as SourceLine
    std::vector<std::string> files;   // relative to the mod folder, separated by '\'
};
Marker ReadMarker(const std::string& modDir) {
    Marker m;
    FILE* f = std::fopen((modDir + "\\" + kMarker).c_str(), "rb");
    if (!f) return m;
    char line[1024];
    bool first = true;
    while (std::fgets(line, sizeof line, f)) {
        std::string t(line);
        while (!t.empty() && (t.back() == '\n' || t.back() == '\r')) t.pop_back();
        if (first) {
            first = false;
            if (t != kMarkerHead) break;
            m.valid = true;
            continue;
        }
        if (t.compare(0, 7, "source\t") == 0)
            m.source = t.substr(7);
        else if (t.compare(0, 5, "file\t") == 0) {
            const std::string rel = t.substr(5);
            // delete only safe relative paths (a marker that was changed by hand must not affect anything outside)
            std::vector<std::string> parts;
            std::string s = rel;
            for (char& c : s)
                if (c == '\\') c = '/';
            if (SplitSafe(s, &parts)) m.files.push_back(rel);
        }
    }
    std::fclose(f);
    return m;
}
// delete the listed files and the marker, then empty subfolders and the mod folder itself (if empty)
void RemoveExtracted(const std::string& modDir, const Marker& m) {
    std::set<std::string> dirs;
    for (const std::string& rel : m.files) {
        DeleteFileA((modDir + "\\" + rel).c_str());
        for (size_t s = rel.find_last_of('\\'); s != std::string::npos && s > 0; s = rel.find_last_of('\\', s - 1))
            dirs.insert(rel.substr(0, s));
    }
    DeleteFileA((modDir + "\\" + kMarker).c_str());
    for (auto it = dirs.rbegin(); it != dirs.rend(); ++it)
        RemoveDirectoryA((modDir + "\\" + *it).c_str());   // longest first, only if empty
    RemoveDirectoryA(modDir.c_str());
}
bool WriteAll(const std::string& path, const std::string& data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = data.empty() || std::fwrite(data.data(), 1, data.size(), f) == data.size();
    return std::fclose(f) == 0 && ok;
}

struct PackedMod {
    std::string name;   // spelling in the archive
    const Archive* src = nullptr;
    std::vector<const ArchiveEntry*> files;   // only files (no folders, no deleted entries)
    std::vector<std::string> rel;             // agrees with files: path relative to the mod folder, '\'
    std::string error;
};

}   // namespace

int ExtractPackedMods(const std::string& gameDir, const std::string& modsDir, const std::function<void(const std::string&)>& log) {
    const std::vector<Archive> archives = FindArchives(gameDir + "Data");
    std::map<std::string, std::vector<ArchiveEntry>> lists;   // archive path -> entries (they stay for the pointers in PackedMod)
    std::map<std::string, PackedMod> mods;                    // name in lower case -> mod
    std::vector<std::string> order;                           // names in lower case, in the sequence in which they were found
    for (const Archive& a : archives) {
        std::string err;
        std::vector<ArchiveEntry>& entries = lists[a.path];
        if (!ListArchive(a.path, &entries, &err)) {
            log("Gepackte Mods: " + a.file + " nicht lesbar: " + err);
            continue;
        }
        std::set<std::string> here;   // Mods dieses Archivs
        int loose = 0;
        for (const ArchiveEntry& e : entries) {
            if (e.isDir || e.deleted) continue;
            std::vector<std::string> parts;
            if (!SplitSafe(e.name, &parts)) {
                log("Gepackte Mods: " + a.file + ": unzulaessiger Pfad '" + e.name + "' uebersprungen");
                continue;
            }
            if (parts.size() < 2) {   // file on the top level: belongs to no mod
                ++loose;
                continue;
            }
            const std::string key = Lower(parts[0]);
            auto it = mods.find(key);
            if (it != mods.end() && it->second.src != &a) {
                if (here.insert(key).second)
                    log("Gepackte Mods: '" + parts[0] + "' steht schon in " + it->second.src->file + "; Fassung in " + a.file +
                        " ignoriert");
                continue;
            }
            if (it == mods.end()) {
                PackedMod m;
                m.name = parts[0];
                m.src = &a;
                it = mods.emplace(key, m).first;
                order.push_back(key);
                here.insert(key);
            }
            PackedMod& m = it->second;
            if (e.compressed) {
                if (m.error.empty())
                    m.error = "komprimierter Eintrag '" + e.name + "' (nur unkomprimierte Archive, z. B. von g3asm_packmod)";
                continue;
            }
            std::string rel;
            for (size_t i = 1; i < parts.size(); ++i) rel += (i > 1 ? "\\" : "") + parts[i];
            if (Lower(rel) == kMarker) continue;
            m.files.push_back(&e);
            m.rel.push_back(rel);
        }
        if (loose)
            log("Gepackte Mods: " + a.file + ": " + std::to_string(loose) +
                " Datei(en) ausserhalb eines Mod-Ordners ignoriert (erwartet: <Mod>/main.as ...)");
    }

    int ready = 0;
    if (!mods.empty() && !MakeDirs(modsDir)) {
        log("Gepackte Mods: Mod-Ordner nicht anlegbar: " + modsDir);
        return 0;
    }
    for (const std::string& key : order) {
        PackedMod& m = mods[key];
        const std::string dir = modsDir + "\\" + m.name;
        const std::string from = " (aus " + m.src->file + ")";
        if (!m.error.empty()) {
            log("Gepackte Mod '" + m.name + "'" + from + " nicht entpackt: " + m.error);
            continue;
        }
        if (IsDir(dir) || IsFile(dir)) {
            const Marker old = ReadMarker(dir);
            if (!old.valid) {
                log("Gepackte Mod '" + m.name + "'" + from +
                    ": loser Ordner gleichen Namens vorhanden, er gewinnt (gepackte Fassung nicht entpackt)");
                continue;
            }
            if (old.source == SourceLine(*m.src)) {
                ++ready;
                continue;   // no change
            }
            RemoveExtracted(dir, old);
        }
        std::string fail;
        for (size_t i = 0; i < m.files.size() && fail.empty(); ++i) {
            const std::string target = dir + "\\" + m.rel[i];
            std::string data;
            if (!ReadArchiveEntry(m.src->path, *m.files[i], &data))
                fail = "Eintrag nicht lesbar: " + m.files[i]->name;
            else if (!MakeDirs(target.substr(0, target.find_last_of('\\'))))
                fail = "Ordner nicht anlegbar: " + target;
            else if (!WriteAll(target, data))
                fail = "Datei nicht schreibbar: " + target;
        }
        // write the marker also if there is an error, so that partial results are removed cleanly at the next start
        std::string marker =
            std::string(kMarkerHead) + "\r\nsource\t" + (fail.empty() ? SourceLine(*m.src) : std::string("unvollstaendig")) + "\r\n";
        for (const std::string& r : m.rel) marker += "file\t" + r + "\r\n";
        if (!MakeDirs(dir) || !WriteAll(dir + "\\" + kMarker, marker)) {
            log("Gepackte Mod '" + m.name + "'" + from + ": Markierung nicht schreibbar in " + dir);
            continue;
        }
        if (!fail.empty()) {
            log("Gepackte Mod '" + m.name + "'" + from + " nicht vollstaendig entpackt: " + fail);
            continue;
        }
        log("Gepackte Mod '" + m.name + "' entpackt" + from + ": " + std::to_string(m.files.size()) + " Datei(en)");
        ++ready;
    }

    // remove unpacked mods that are not in an archive anymore
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((modsDir + "\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        std::vector<std::string> gone;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.') continue;
            if (mods.count(Lower(fd.cFileName))) continue;
            if (IsFile(modsDir + "\\" + fd.cFileName + "\\" + kMarker)) gone.push_back(fd.cFileName);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
        for (const std::string& n : gone) {
            const std::string dir = modsDir + "\\" + n;
            const Marker m = ReadMarker(dir);
            if (!m.valid) continue;
            RemoveExtracted(dir, m);
            log("Gepackte Mod '" + n + "' entfernt (steht in keinem Data\\G3ASM.*-Archiv mehr)" +
                (IsDir(dir) ? std::string(", fremde Dateien im Ordner bleiben") : std::string()));
        }
    }
    if (!archives.empty())
        log("Gepackte Mods: " + std::to_string(archives.size()) + " Archiv(e), " + std::to_string(ready) + " Mod(s) bereit");
    return ready;
}

}   // namespace g3asm
