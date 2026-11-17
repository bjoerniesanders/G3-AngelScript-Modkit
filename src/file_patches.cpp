// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Additional permissions: LICENSE-EXCEPTIONS.md
// File patches ("Script Patches"): mods give unified diff files <mod>\ScriptPatches\*.patch that the modkit applies to the data files of the game (quests, infos, ...)
// BEFORE the game reads them. The game reads its text data (for example Data\Quests.pak -> G3_World_01\<name>_quest_G3_World_01.quest) through the file system module, in which
// physical files have priority over archive entries. Thus the modkit makes the patched version as a physical file in <game>\Data\<archive>\<path>
// (overlay). The archive itself does not change. At each start, the modkit removes the overlay files that it made the last time (list g3asm\patched_files.txt) and calculates everything
// again from the original - thus you can remove mods without traces, and more than one mod stacks its patches in the load order.
// If a loose file of another source is already at the target (another mod, your own change), it is the base: the modkit makes a backup of it in g3asm\patch_backup\<path> and
// restores it at the next start. The list keeps the size and the CRC32 of the written data for each file. A file that changed since then does not belong to the modkit anymore
// and stays without changes.
//
// Paths in the diff: a/Data/<archive>/<path in the archive> or b/..., for example b/Data/Quests/G3_World_01/Abbas_Artefakt_quest_G3_World_01.quest. An entry --- /dev/null makes a new file.
// The code reads archives directly (G3V0 format, entries that are not compressed). Compressed entries (Strings.pak\stringtable.ini) are not supported yet.
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "script_host.h"

namespace g3asm {
namespace {

using Log = std::function<void(const std::string&)>;

// ---------------------------------------------------------------- Hilfen
std::string Lower(std::string s) {
    for (char& c : s) c = char(tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string Slash(std::string s) {
    for (char& c : s)
        if (c == '\\') c = '/';
    return s;
}
std::string Win(std::string s) {
    for (char& c : s)
        if (c == '/') c = '\\';
    return s;
}

bool ReadFile(const std::string& path, std::string* out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out->resize(n > 0 ? size_t(n) : 0);
    const bool ok = n <= 0 || std::fread(&(*out)[0], 1, size_t(n), f) == size_t(n);
    std::fclose(f);
    return ok;
}
void MakeDirs(const std::string& dir) {   // Windows path, makes all levels
    for (size_t i = 0; i < dir.size(); ++i)
        if (dir[i] == '\\' && i > 2) CreateDirectoryA(dir.substr(0, i).c_str(), nullptr);
    CreateDirectoryA(dir.c_str(), nullptr);
}
bool WriteFileBinary(const std::string& path, const std::string& data) {
    const size_t cut = path.find_last_of('\\');
    if (cut != std::string::npos) MakeDirs(path.substr(0, cut));
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = data.empty() || std::fwrite(data.data(), 1, data.size(), f) == data.size();
    std::fclose(f);
    return ok;
}

bool FileExists(const std::string& path) {
    const DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
uint32_t Crc32(const std::string& d) {   // zlib polynomial; the files are small, thus without a table
    uint32_t c = 0xFFFFFFFFu;
    for (const char ch : d) {
        c ^= static_cast<unsigned char>(ch);
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}
// target path relative to the game folder: no drive, no leading separator, no ".." (patches and the list must not leave the game folder)
bool SafeRelPath(const std::string& p) {
    return !p.empty() && p[0] != '/' && p[0] != '\\' && p.find(':') == std::string::npos && p.find("..") == std::string::npos;
}

// ---------------------------------------------------------------- read a G3V0 archive
struct PakReader {
    std::string path;
    std::string meta;
    FILE* f = nullptr;
    size_t pos = 0;
    struct Entry {
        std::string name;
        uint64_t off = 0, a = 0, b = 0;
        uint32_t type = 0;
    };
    std::vector<Entry> entries;
    bool ok = false;

    uint32_t U32() {
        uint32_t v = 0;
        if (pos + 4 <= meta.size()) std::memcpy(&v, &meta[pos], 4);
        pos += 4;
        return v;
    }
    uint64_t U64() {
        uint64_t v = 0;
        if (pos + 8 <= meta.size()) std::memcpy(&v, &meta[pos], 8);
        pos += 8;
        return v;
    }
    std::string Str() {
        const uint32_t n = U32();
        std::string s;
        if (pos > meta.size() || n > meta.size() - pos) {   // damaged directory: to the end, so that all loops end
            pos = meta.size();
            return s;
        }
        s.assign(meta.data() + pos, n);
        pos += n;
        if (n) pos += 1;
        return s;
    }
    void Rec(int depth) {
        if (depth > 32 || pos + 0x24 > meta.size()) return;
        uint32_t hdr[9];
        std::memcpy(hdr, &meta[pos], 0x24);
        pos += 0x24;
        if (!(hdr[8] & 0x10)) {
            Entry e;
            e.off = U64();
            e.a = U64();
            e.b = U64();
            U32();
            e.type = U32();
            e.name = Str();
            Str();
            entries.push_back(e);
            return;
        }
        Str();
        for (int pass = 0; pass < 2; ++pass) {
            const uint32_t c = U32();
            for (uint32_t i = 0; i < c && pos < meta.size(); ++i) Rec(depth + 1);
        }
    }
    bool Open(const std::string& p) {
        path = p;
        f = std::fopen(p.c_str(), "rb");
        if (!f) return false;
        uint32_t h[12];
        if (std::fread(h, 4, 12, f) != 12 || h[1] != 0x30563347) return false;
        const uint64_t moff = uint64_t(h[6]) | (uint64_t(h[7]) << 32);
        _fseeki64(f, 0, SEEK_END);
        const int64_t size = _ftelli64(f);
        // directory before the end of the header or above 256 MB: not a valid archive
        if (int64_t(moff) < 0x30 || int64_t(moff) >= size || size - int64_t(moff) > (int64_t(1) << 28)) return false;
        meta.resize(size_t(size - int64_t(moff)));
        _fseeki64(f, int64_t(moff), SEEK_SET);
        if (std::fread(&meta[0], 1, meta.size(), f) != meta.size()) return false;
        pos = 0;
        Rec(0);
        ok = true;
        return true;
    }
    ~PakReader() {
        if (f) std::fclose(f);
    }
    // return value: 1 ok, 0 not found, -1 compressed or not readable
    int Read(const std::string& inner, std::string* out) {
        const std::string want = Lower(Slash(inner));
        for (const Entry& e : entries) {
            if (Lower(Slash(e.name)) != want) continue;
            if (e.type != 0 || e.a > (uint64_t(64) << 20)) return -1;   // compressed, or not a text file (above 64 MB)
            out->resize(size_t(e.a));
            _fseeki64(f, int64_t(e.off), SEEK_SET);
            return std::fread(out->empty() ? nullptr : &(*out)[0], 1, out->size(), f) == out->size() ? 1 : -1;
        }
        return 0;
    }
};

// ---------------------------------------------------------------- Unified Diff
struct Hunk {
    int oldStart = 0, oldCount = 0;
    std::vector<std::string> oldLines, newLines;
};
struct FilePatch {
    std::string target;
    bool isNew = false;
    std::vector<Hunk> hunks;
    std::string source;
};

std::vector<std::string> SplitLines(const std::string& text, bool* crlf, bool* finalNewline) {
    std::vector<std::string> lines;
    size_t i = 0, crlfCount = 0;
    while (i < text.size()) {
        size_t j = text.find('\n', i);
        if (j == std::string::npos) {
            lines.push_back(text.substr(i));
            break;
        }
        std::string l = text.substr(i, j - i);
        if (!l.empty() && l.back() == '\r') {
            l.pop_back();
            ++crlfCount;
        }
        lines.push_back(l);
        i = j + 1;
    }
    if (crlf) *crlf = !lines.empty() && crlfCount * 2 >= lines.size();
    if (finalNewline) *finalNewline = !text.empty() && text.back() == '\n';
    return lines;
}
std::string StripPrefix(std::string p) {   // "a/Data/..." -> "Data/...", Zeitstempel hinter Tab entfernen
    const size_t tab = p.find('\t');
    if (tab != std::string::npos) p.resize(tab);
    while (!p.empty() && (p.back() == ' ' || p.back() == '\r')) p.pop_back();
    p = Slash(p);
    if (p.size() > 2 && (p.compare(0, 2, "a/") == 0 || p.compare(0, 2, "b/") == 0)) p = p.substr(2);
    return p;
}
bool ParsePatch(const std::string& text, const std::string& source, std::vector<FilePatch>* out, std::string* err) {
    bool crlf, fin;
    std::vector<std::string> L = SplitLines(text, &crlf, &fin);
    size_t i = 0;
    while (i < L.size()) {
        if (L[i].compare(0, 4, "--- ") != 0) {
            ++i;
            continue;
        }
        if (i + 1 >= L.size() || L[i + 1].compare(0, 4, "+++ ") != 0) {
            ++i;
            continue;
        }
        FilePatch fp;
        fp.source = source;
        const std::string from = L[i].substr(4);
        fp.isNew = from.compare(0, 9, "/dev/null") == 0;
        fp.target = StripPrefix(L[i + 1].substr(4));
        if (!SafeRelPath(fp.target)) {
            *err = source + ": Zielpfad nicht erlaubt (relativ zum Spielordner, ohne '..'): '" + fp.target + "'";
            return false;
        }
        i += 2;
        while (i < L.size() && L[i].compare(0, 3, "@@ ") == 0) {
            Hunk h;
            int os = 0, oc = 1, ns = 0, nc = 1;
            if (std::sscanf(L[i].c_str(), "@@ -%d,%d +%d,%d @@", &os, &oc, &ns, &nc) < 4) {
                if (std::sscanf(L[i].c_str(), "@@ -%d +%d,%d @@", &os, &ns, &nc) == 3)
                    oc = 1;
                else if (std::sscanf(L[i].c_str(), "@@ -%d,%d +%d @@", &os, &oc, &ns) == 3)
                    nc = 1;
                else if (std::sscanf(L[i].c_str(), "@@ -%d +%d @@", &os, &ns) == 2) {
                    oc = 1;
                    nc = 1;
                } else {
                    *err = source + ": ungueltiger Hunk-Kopf '" + L[i] + "'";
                    return false;
                }
            }
            h.oldStart = os;
            h.oldCount = oc;
            ++i;
            int oldSeen = 0, newSeen = 0;
            while (i < L.size() && (oldSeen < oc || newSeen < nc)) {
                const std::string& l = L[i];
                if (!l.empty() && l[0] == '\\') {
                    ++i;
                    continue;
                }   // "\ No newline at end of file"
                const char c = l.empty() ? ' ' : l[0];
                const std::string body = l.empty() ? std::string() : l.substr(1);
                if (c == ' ') {
                    h.oldLines.push_back(body);
                    h.newLines.push_back(body);
                    ++oldSeen;
                    ++newSeen;
                } else if (c == '-') {
                    h.oldLines.push_back(body);
                    ++oldSeen;
                } else if (c == '+') {
                    h.newLines.push_back(body);
                    ++newSeen;
                } else
                    break;
                ++i;
            }
            while (i < L.size() && !L[i].empty() && L[i][0] == '\\') ++i;
            fp.hunks.push_back(std::move(h));
        }
        if (fp.hunks.empty()) {
            *err = source + ": keine Hunks fuer " + fp.target;
            return false;
        }
        out->push_back(std::move(fp));
    }
    if (out->empty()) {
        *err = source + ": keine Dateipatches gefunden (Unified-Diff erwartet)";
        return false;
    }
    return true;
}

bool SameLine(const std::string& a, const std::string& b) {
    size_t ea = a.size(), eb = b.size();
    while (ea && (a[ea - 1] == ' ' || a[ea - 1] == '\t' || a[ea - 1] == '\r')) --ea;
    while (eb && (b[eb - 1] == ' ' || b[eb - 1] == '\t' || b[eb - 1] == '\r')) --eb;
    return ea == eb && a.compare(0, ea, b, 0, eb) == 0;
}
bool MatchAt(const std::vector<std::string>& file, size_t at, const std::vector<std::string>& old) {
    if (at + old.size() > file.size()) return false;
    for (size_t k = 0; k < old.size(); ++k)
        if (!SameLine(file[at + k], old[k])) return false;
    return true;
}
// Applies all hunks to the line list. Hunks can have an offset from the expected location (+-300 lines).
bool ApplyHunks(std::vector<std::string>* lines, const std::vector<Hunk>& hunks, std::string* err) {
    long delta = 0;
    int n = 0;
    for (const Hunk& h : hunks) {
        ++n;
        const long expect = long(h.oldStart) - 1 + delta + (h.oldLines.empty() ? 1 : 0);
        long found = -1;
        for (long d = 0; d <= 300 && found < 0; ++d) {
            for (int sign = 0; sign < 2 && found < 0; ++sign) {
                const long at = expect + (sign ? -d : d);
                if (at < 0 || at > long(lines->size())) continue;
                if (MatchAt(*lines, size_t(at), h.oldLines)) found = at;
            }
        }
        if (found < 0) {
            char b[128];
            std::snprintf(b, sizeof b, "Hunk %d (ab Zeile %d) passt nicht: der Inhalt der Datei weicht ab", n, h.oldStart);
            *err = b;
            return false;
        }
        lines->erase(lines->begin() + found, lines->begin() + found + long(h.oldLines.size()));
        lines->insert(lines->begin() + found, h.newLines.begin(), h.newLines.end());
        delta += long(h.newLines.size()) - long(h.oldLines.size());
        delta += found - expect;   // keep the real offset
    }
    return true;
}

std::string Join(const std::vector<std::string>& lines, bool crlf, bool finalNewline) {
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        out += lines[i];
        if (i + 1 < lines.size() || finalNewline) out += crlf ? "\r\n" : "\n";
    }
    return out;
}

// original of the target file: archive (Data/<archive>/<inner>) or - if an own physical file exists - that file
bool LoadBase(const std::string& gameDir, const std::string& target, std::string* content, std::string* how, std::string* err) {
    if (ReadFile(gameDir + Win(target), content)) {
        *how = "physische Datei";
        return true;
    }
    const std::string t = Slash(target);
    if (t.size() > 5 && Lower(t.substr(0, 5)) == "data/") {
        const size_t sl = t.find('/', 5);
        if (sl != std::string::npos) {
            const std::string pak = t.substr(5, sl - 5), inner = t.substr(sl + 1);
            PakReader r;
            const std::string pakPath = gameDir + "Data\\" + pak + ".pak";
            if (!r.Open(pakPath)) {
                *err = "Archiv nicht lesbar: " + pakPath;
                return false;
            }
            const int rc = r.Read(inner, content);
            if (rc == 1) {
                *how = "Archiv " + pak + ".pak";
                return true;
            }
            *err = rc == 0 ? ("nicht im Archiv " + pak + ".pak gefunden: " + inner)
                           : ("Eintrag in " + pak + ".pak ist komprimiert (noch nicht unterstuetzt): " + inner);
            return false;
        }
    }
    *err = "Zieldatei nicht gefunden: " + target;
    return false;
}

}   // namespace

// ---------------------------------------------------------------- Einstieg
int ApplyScriptPatches(const std::string& gameDirIn, const std::string& modsDir, const std::function<void(const std::string&)>& log) {
    std::string gameDir = gameDirIn;
    if (!gameDir.empty() && gameDir.back() != '\\' && gameDir.back() != '/') gameDir += '\\';
    gameDir = Win(gameDir);
    const std::string manifest = gameDir + "g3asm\\patched_files.txt";
    const std::string backupDir = gameDir + "g3asm\\patch_backup\\";

    // 1) Remove the overlay files of the last run, or restore the backed-up file of another source. Line: path [tab size tab CRC32]; older lists have
    //    only the path. If a file is not the same as the written data anymore, another person replaced it: it stays, and its backup is discarded.
    std::string old;
    int removed = 0;
    if (ReadFile(manifest, &old)) {
        for (const std::string& l : SplitLines(old, nullptr, nullptr)) {
            const size_t t1 = l.find('\t'), t2 = t1 == std::string::npos ? t1 : l.find('\t', t1 + 1);
            const std::string rel = l.substr(0, t1);
            if (!SafeRelPath(rel)) continue;
            const std::string path = gameDir + Win(rel), backup = backupDir + Win(rel);
            if (t2 != std::string::npos) {
                std::string now;
                const bool ours = ReadFile(path, &now) && now.size() == std::strtoull(l.c_str() + t1 + 1, nullptr, 10) &&
                                  Crc32(now) == std::strtoul(l.c_str() + t2 + 1, nullptr, 16);
                if (!ours) {
                    if (FileExists(path)) log("Datei-Patches: " + rel + " wurde seit dem letzten Start geaendert und bleibt, wie sie ist");
                    DeleteFileA(backup.c_str());
                    continue;
                }
            }
            if (FileExists(backup)) {
                if (MoveFileExA(backup.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
                    ++removed;
                else
                    log("Datei-Patches: Sicherung von " + rel + " liess sich nicht zurueckspielen (g3asm\\patch_backup)");
            } else if (DeleteFileA(path.c_str()))
                ++removed;
        }
        DeleteFileA(manifest.c_str());
    }

    // 2) Patches einsammeln (Mods in Ladereihenfolge, Patchdateien alphabetisch)
    std::vector<FilePatch> patches;
    int patchFiles = 0;
    for (const std::string& mod : DiscoverModNames(modsDir)) {
        const std::string dir = modsDir + "\\" + mod + "\\ScriptPatches";
        std::vector<std::string> names;
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA((dir + "\\*.patch").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) names.push_back(fd.cFileName);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
        std::sort(names.begin(), names.end(),
                  [](const std::string& a, const std::string& b) { return _stricmp(a.c_str(), b.c_str()) < 0; });
        for (const std::string& n : names) {
            std::string text, err;
            const std::string src = mod + "\\ScriptPatches\\" + n;
            if (!ReadFile(dir + "\\" + n, &text)) {
                log("Patch nicht lesbar: " + src);
                continue;
            }
            std::vector<FilePatch> parsed;
            if (!ParsePatch(text, src, &parsed, &err)) {
                log("Patch ungueltig: " + err);
                continue;
            }
            ++patchFiles;
            for (FilePatch& p : parsed) patches.push_back(std::move(p));
        }
    }
    if (patches.empty()) {
        if (removed) log("Datei-Patches: keine Patches mehr, " + std::to_string(removed) + " alte Overlay-Dateien entfernt oder zurueckgespielt");
        return 0;
    }

    // 3) apply for each target file, in sequence
    struct Work {
        std::vector<std::string> lines;
        bool crlf = true, finalNewline = true;
        std::string how;
        bool loaded = false, failed = false;
        int applied = 0;
    };
    std::map<std::string, Work> work;
    std::vector<std::string> order;
    for (const FilePatch& p : patches) {
        const std::string key = Lower(p.target);
        Work& w = work[key];
        if (!w.loaded && !w.failed) {
            if (p.isNew) {
                w.lines.clear();
                w.how = "neu";
                w.loaded = true;
            } else {
                std::string content, how, err;
                if (!LoadBase(gameDir, p.target, &content, &how, &err)) {
                    log("Patch " + p.source + ": " + err);
                    w.failed = true;
                    continue;
                }
                w.lines = SplitLines(content, &w.crlf, &w.finalNewline);
                w.how = how;
                w.loaded = true;
            }
            order.push_back(key);
        }
        if (w.failed) continue;
        std::vector<std::string> trial = w.lines;
        std::string err;
        if (!ApplyHunks(&trial, p.hunks, &err)) {
            log("Patch " + p.source + " (" + p.target + "): " + err + " - dieser Patch wird uebersprungen");
            continue;
        }
        w.lines = std::move(trial);
        ++w.applied;
    }

    // 4) write the result as physical files and keep a list
    std::string newManifest;
    int written = 0;
    for (const std::string& key : order) {
        Work& w = work[key];
        if (w.failed || w.applied == 0) continue;
        std::string target;
        for (const FilePatch& p : patches)
            if (Lower(p.target) == key) {
                target = p.target;
                break;
            }
        const std::string data = Join(w.lines, w.crlf, w.finalNewline);
        const std::string path = gameDir + Win(target), backup = backupDir + Win(target);
        const bool foreign = FileExists(path);   // own files were removed in step 1: a file that is there now belongs to another source
        if (foreign) {
            MakeDirs(backup.substr(0, backup.find_last_of('\\')));
            if (!CopyFileA(path.c_str(), backup.c_str(), FALSE)) {
                log("Datei-Patch uebersprungen: vorhandene Datei laesst sich nicht sichern: " + target);
                continue;
            }
        }
        if (!WriteFileBinary(path, data)) {
            log("Datei nicht schreibbar: " + target);
            if (foreign) MoveFileExA(backup.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
            continue;
        }
        char stamp[48];
        std::snprintf(stamp, sizeof stamp, "\t%llu\t%08lX", static_cast<unsigned long long>(data.size()), static_cast<unsigned long>(Crc32(data)));
        newManifest += target + stamp + "\n";
        ++written;
        log("Datei-Patch angewendet: " + target + " (Basis: " + w.how + (foreign ? ", gesichert" : "") + ", " + std::to_string(w.applied) + " Patch(es))");
    }
    if (written) WriteFileBinary(manifest, newManifest);
    log("Datei-Patches: " + std::to_string(patchFiles) + " Patchdatei(en), " + std::to_string(written) + " Datei(en) gepatcht");
    return written;
}

}   // namespace g3asm
