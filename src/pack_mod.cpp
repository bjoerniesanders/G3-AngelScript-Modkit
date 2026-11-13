// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// PackMod: Ordner -> G3V0-Archiv. Format und Verhalten folgen G3PakDir (Quellcode im Union Starter, G3PakDef.pas/G3PakDir.pas) und den Archiven der Community:
//   Kopf (0x30 Byte): Version 0, 'G3V0', Revision 0, Verschluesselung 0, Komprimierung, 'G3PD', OffsetToFiles = OffsetToFolders = Beginn des Verzeichnisses,
//   OffsetToVolume = Ende des Verzeichnisses (= Dateigroesse - 4); danach Nutzdaten, Verzeichnis, 4 Nullbytes.
//   Eintrag: Erstellt/Zugriff/Geschrieben (FILETIME), 8 Nullbytes, Attribute (immer mit "Packed" 0x20000); Verzeichnis: Name, Zahl+Verzeichnisse, Zahl+Dateien;
//   Datei: Versatz, gepackte Groesse, Groesse (je u64), Verschluesselung 0, Komprimierung (0 gespeichert, 2 zlib), Name, Kommentar.
//   Namen sind Pfade relativ zum Archivwurzelordner mit '/', Verzeichnisse enden auf '/'. Je Ebene zuerst die Verzeichnisse, dann die Dateien, ohne Beachtung der Gross-/Kleinschreibung sortiert.
//   "<name>.deleted" -> Eintrag mit Attribut "Deleted" (0x8000), Zeiten 0, ohne Daten. .dds/.ogg/.wav tragen das Attribut "Stream" (0x40000).
// Hier wird nicht komprimiert (gespeichert; wie der "Neu verpacken"-Modus des Union Starters). Das Lesen komprimierter Eintraege liefert ReadArchiveEntry nicht.
#include "pack_mod.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace g3asm {
namespace {

constexpr uint32_t kProduct = 0x30563347;        // 'G3V0'
constexpr uint32_t kReservedG3PD = 0x44503347;   // 'G3PD' (G3PakDir >= 0.0.0.9)
constexpr uint32_t kAttrDir = 0x10, kAttrDeleted = 0x8000, kAttrPacked = 0x20000, kAttrStream = 0x40000, kAttrCompressed = 0x800;
constexpr uint32_t kSettable = 0x1 | 0x2 | 0x4 | 0x20;   // ReadOnly, Hidden, System, Archive: werden aus dem Dateisystem uebernommen

struct Node {
    std::string name;   // Name im Archiv
    std::string path;   // physischer Pfad (Dateien)
    FILETIME c{}, a{}, w{};
    uint32_t attr = 0;
    uint64_t size = 0;
    bool deleted = false;
    std::vector<Node> dirs, files;
};

std::string Lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = char(c + 32);
    return s;
}
bool EndsWithNoCase(const std::string& s, const char* suffix) {
    const size_t n = std::strlen(suffix);
    return s.size() >= n && Lower(s.substr(s.size() - n)) == suffix;
}
std::string WinPath(std::string s) {
    for (char& c : s)
        if (c == '/') c = '\\';
    return s;
}
bool IsDirectory(const std::string& p) {
    const DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
bool FileExists(const std::string& p) {
    const DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// rel = Pfad des Ordners im Archiv ("" oder "A/B/")
bool Scan(const std::string& root, const std::string& rel, const PackOptions& o, Node* parent, std::string* err, int depth) {
    if (depth > 64) {
        *err = "Ordner zu tief verschachtelt: " + rel;
        return false;
    }
    const std::string dir = rel.empty() ? root : root + "\\" + WinPath(rel.substr(0, rel.size() - 1));
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) return true;
        *err = "Ordner nicht lesbar: " + dir;
        return false;
    }
    bool ok = true;
    do {
        const std::string n = fd.cFileName;
        if (n == "." || n == "..") continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;   // keine Verknuepfungen (Schleifen)
            Node d;
            d.name = rel + n + "/";
            d.attr = kAttrDir;
            d.c = fd.ftCreationTime;
            d.a = fd.ftLastAccessTime;
            d.w = fd.ftLastWriteTime;
            if (!Scan(root, d.name, o, &d, err, depth + 1)) {
                ok = false;
                break;
            }
            parent->dirs.push_back(std::move(d));
        } else {
            if (Lower(n) == "desktop.ini") continue;
            Node f;
            f.path = dir + "\\" + n;
            f.c = fd.ftCreationTime;
            f.a = fd.ftLastAccessTime;
            f.w = fd.ftLastWriteTime;
            f.size = (uint64_t(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
            f.attr = fd.dwFileAttributes & kSettable;
            if (o.deletions && EndsWithNoCase(n, ".deleted") && n.size() > 8) {
                f.name = rel + n.substr(0, n.size() - 8);
                f.deleted = true;
                f.c = f.a = f.w = FILETIME{};
                f.attr = kAttrDeleted;
                f.size = 0;
            } else {
                f.name = rel + n;
                if (EndsWithNoCase(n, ".dds") || EndsWithNoCase(n, ".ogg") || EndsWithNoCase(n, ".wav")) f.attr |= kAttrStream;
            }
            parent->files.push_back(std::move(f));
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (ok) {
        auto less = [](const Node& x, const Node& y) { return _stricmp(x.name.c_str(), y.name.c_str()) < 0; };
        std::stable_sort(parent->dirs.begin(), parent->dirs.end(), less);
        std::stable_sort(parent->files.begin(), parent->files.end(), less);
    }
    return ok;
}

struct Writer {
    FILE* data = nullptr;
    uint64_t pos = 0;
    std::string info, err;
    const PackOptions* opt = nullptr;
    PackStats* st = nullptr;

    void Raw(const void* p, size_t n) { info.append(static_cast<const char*>(p), n); }
    void U32(uint32_t v) { Raw(&v, 4); }
    void U64(uint64_t v) { Raw(&v, 8); }
    void Str(const std::string& s) {
        U32(uint32_t(s.size()));
        if (!s.empty()) {
            info.append(s);
            info.push_back('\0');
        }
    }
    void Header(const Node& n, uint32_t attr) {
        Raw(&n.c, 8);
        Raw(&n.a, 8);
        Raw(&n.w, 8);
        U64(0);   // FileSizeHigh/Low (G3PakDir schreibt Null)
        U32(attr | kAttrPacked);
    }
    bool CopyFile(const Node& f) {
        FILE* in = nullptr;
        if (fopen_s(&in, f.path.c_str(), "rb") != 0 || !in) {
            err = "Datei nicht lesbar: " + f.path;
            return false;
        }
        char buf[1 << 16];
        uint64_t left = f.size;
        bool ok = true;
        while (left > 0) {
            const size_t want = size_t(std::min<uint64_t>(left, sizeof buf));
            const size_t got = std::fread(buf, 1, want, in);
            if (got != want || std::fwrite(buf, 1, got, data) != got) {
                ok = false;
                break;
            }
            left -= got;
        }
        std::fclose(in);
        if (!ok) err = "Datei beim Schreiben veraendert oder Schreibfehler: " + f.path;
        pos += f.size - left;
        return ok;
    }
    bool Save(const Node& n, bool root) {
        if (root || (n.attr & kAttrDir)) {
            Header(n, kAttrDir);
            Str(root ? std::string() : n.name);
            U32(uint32_t(n.dirs.size()));
            for (const Node& d : n.dirs) {
                if (!Save(d, false)) return false;
                ++st->dirs;
            }
            U32(uint32_t(n.files.size()));
            for (const Node& f : n.files)
                if (!Save(f, false)) return false;
            return true;
        }
        Header(n, n.attr);
        uint64_t off = 0, bytes = 0, size = 0;
        if (n.deleted) {
            ++st->deleted;
        } else {
            off = pos;
            if (n.size > 0 && !CopyFile(n)) return false;
            bytes = size = n.size;
            st->bytes += n.size;
            ++st->files;
        }
        U64(off);
        U64(bytes);
        U64(size);
        U32(0);   // Verschluesselung: keine
        U32(0);   // Komprimierung: gespeichert
        Str(n.name);
        Str(opt->comments && !n.deleted ? n.path : std::string());
        return true;
    }
};

bool Fail(PackStats* st, const std::string& msg) {
    if (st) st->error = msg;
    return false;
}

}   // namespace

bool PackFolder(const std::string& srcDirIn, const std::string& outFile, const PackOptions& opt, PackStats* statsOut) {
    PackStats local;
    PackStats* st = statsOut ? statsOut : &local;
    *st = PackStats();
    std::string src = srcDirIn;
    while (src.size() > 1 && (src.back() == '\\' || src.back() == '/')) src.pop_back();
    if (!IsDirectory(src)) return Fail(st, "Ordner nicht gefunden: " + src);
    Node root;
    root.attr = kAttrDir;
    SYSTEMTIME stNow;
    FILETIME now;
    GetSystemTime(&stNow);
    SystemTimeToFileTime(&stNow, &now);
    root.c = root.a = root.w = now;
    std::string err;
    if (!Scan(src, std::string(), opt, &root, &err, 0)) return Fail(st, err);

    // leerer Baum: nichts zu packen
    struct Count {
        static size_t Files(const Node& n) {
            size_t c = n.files.size();
            for (const Node& d : n.dirs) c += Files(d);
            return c;
        }
    };
    if (Count::Files(root) == 0) return Fail(st, "Der Ordner enthaelt keine Dateien: " + src);

    const std::string tmp = outFile + ".tmp";
    FILE* f = nullptr;
    if (fopen_s(&f, tmp.c_str(), "wb") != 0 || !f) return Fail(st, "Zieldatei nicht schreibbar: " + tmp);
    uint32_t hdr[12] = {};
    bool ok = std::fwrite(hdr, 4, 12, f) == 12;
    Writer w;
    w.data = f;
    w.pos = 0x30;
    w.opt = &opt;
    w.st = st;
    if (ok) ok = w.Save(root, true);
    if (!ok) err = w.err.empty() ? "Schreibfehler" : w.err;
    if (ok) {
        const uint64_t infoOff = w.pos;
        const uint64_t volOff = infoOff + w.info.size();
        const uint32_t zero = 0;
        ok = std::fwrite(w.info.data(), 1, w.info.size(), f) == w.info.size() && std::fwrite(&zero, 4, 1, f) == 1;
        if (ok) {
            hdr[0] = 0;
            hdr[1] = kProduct;
            hdr[2] = 0;
            hdr[3] = 0;
            hdr[4] = 0;   // Komprimierung: keine
            hdr[5] = kReservedG3PD;
            hdr[6] = uint32_t(infoOff);
            hdr[7] = uint32_t(infoOff >> 32);
            hdr[8] = uint32_t(infoOff);
            hdr[9] = uint32_t(infoOff >> 32);
            hdr[10] = uint32_t(volOff);
            hdr[11] = uint32_t(volOff >> 32);
            ok = std::fseek(f, 0, SEEK_SET) == 0 && std::fwrite(hdr, 4, 12, f) == 12;
        }
        if (!ok) err = "Schreibfehler";
    }
    if (std::fclose(f) != 0 && ok) {
        ok = false;
        err = "Schreibfehler beim Schliessen";
    }
    if (!ok) {
        DeleteFileA(tmp.c_str());
        return Fail(st, err);
    }
    if (!MoveFileExA(tmp.c_str(), outFile.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmp.c_str());
        return Fail(st, "Zieldatei nicht ersetzbar: " + outFile);
    }
    return true;
}

std::string NextGenerationFile(const std::string& dataDir, const std::string& volume, char kind) {
    if (volume.empty() || volume.find_first_of("\\/:") != std::string::npos) return std::string();
    std::string base = dataDir;
    if (!base.empty() && base.back() != '\\' && base.back() != '/') base += '\\';
    base += volume;
    const char k = kind == 'n' ? 'n' : 'm';
    const std::string first = base + (k == 'n' ? ".nod" : ".mod");
    if (!FileExists(first)) return first;
    for (int i = 0; i < 100; ++i) {
        char ext[8];
        std::snprintf(ext, sizeof ext, ".%c%02d", k, i);
        if (!FileExists(base + ext)) return base + ext;
    }
    return std::string();
}

std::string PackMod(const std::string& srcDir, const std::string& dataDir, const std::string& volume, char kind, const PackOptions& opt,
                    PackStats* stats) {
    PackStats local;
    PackStats* st = stats ? stats : &local;
    *st = PackStats();
    if (!IsDirectory(dataDir)) {
        st->error = "Data-Ordner nicht gefunden: " + dataDir;
        return std::string();
    }
    const std::string out = NextGenerationFile(dataDir, volume, kind);
    if (out.empty()) {
        st->error = "Kein freier Platz fuer '" + volume + "' (ungueltiger Name oder alle Generationen belegt)";
        return std::string();
    }
    return PackFolder(srcDir, out, opt, st) ? out : std::string();
}

// ---------------------------------------------------------------- Lesen
namespace {
struct Meta {
    std::string m;
    size_t pos = 0;
    bool bad = false;
    uint32_t U32() {
        uint32_t v = 0;
        if (pos + 4 <= m.size())
            std::memcpy(&v, &m[pos], 4);
        else
            bad = true;
        pos += 4;
        return v;
    }
    uint64_t U64() {
        uint64_t v = 0;
        if (pos + 8 <= m.size())
            std::memcpy(&v, &m[pos], 8);
        else
            bad = true;
        pos += 8;
        return v;
    }
    std::string Str() {
        const uint32_t n = U32();
        std::string s;
        if (pos + n <= m.size() && n < (1u << 20))
            s.assign(&m[pos], n);
        else
            bad = true;
        pos += n;
        if (n) pos += 1;
        return s;
    }
    void Rec(int depth, std::vector<ArchiveEntry>* out) {
        if (depth > 64 || bad || pos + 0x24 > m.size()) {
            bad = true;
            return;
        }
        uint32_t h[9];
        std::memcpy(h, &m[pos], 0x24);
        pos += 0x24;
        ArchiveEntry e;
        e.attrs = h[8];
        if (!(h[8] & kAttrDir)) {
            e.offset = U64();
            e.stored = U64();
            e.size = U64();
            U32();
            const uint32_t comp = U32();
            e.compressed = comp != 0;
            if (!e.compressed && e.size == 0) e.size = e.stored;
            e.deleted = (h[8] & kAttrDeleted) != 0;
            e.name = Str();
            e.comment = Str();
            out->push_back(e);
            return;
        }
        e.isDir = true;
        e.name = Str();
        for (int pass = 0; pass < 2; ++pass) {
            if (pass == 0) out->push_back(e);
            const uint32_t c = U32();
            for (uint32_t i = 0; i < c && !bad; ++i) Rec(depth + 1, out);
        }
    }
};
}   // namespace

bool ListArchive(const std::string& file, std::vector<ArchiveEntry>* out, std::string* error) {
    out->clear();
    FILE* f = nullptr;
    if (fopen_s(&f, file.c_str(), "rb") != 0 || !f) {
        if (error) *error = "Datei nicht lesbar: " + file;
        return false;
    }
    uint32_t h[12];
    bool ok = std::fread(h, 4, 12, f) == 12 && h[1] == kProduct;
    int64_t size = 0;
    Meta meta;
    if (ok) {
        _fseeki64(f, 0, SEEK_END);
        size = _ftelli64(f);
        const int64_t moff = int64_t(uint64_t(h[6]) | (uint64_t(h[7]) << 32));
        ok = moff >= 0x30 && moff < size;
        if (ok) {
            meta.m.resize(size_t(size - moff));
            _fseeki64(f, moff, SEEK_SET);
            ok = std::fread(&meta.m[0], 1, meta.m.size(), f) == meta.m.size();
        }
    }
    std::fclose(f);
    if (!ok) {
        if (error) *error = "Kein G3V0-Archiv: " + file;
        return false;
    }
    meta.Rec(0, out);
    if (meta.bad) {
        if (error) *error = "Verzeichnis beschaedigt: " + file;
        return false;
    }
    if (!out->empty()) out->erase(out->begin());   // Wurzel (leerer Name)
    return true;
}

bool ReadArchiveEntry(const std::string& file, const ArchiveEntry& e, std::string* data) {
    if (e.isDir || e.deleted || e.compressed) return false;
    FILE* f = nullptr;
    if (fopen_s(&f, file.c_str(), "rb") != 0 || !f) return false;
    data->resize(size_t(e.size));
    bool ok = _fseeki64(f, int64_t(e.offset), SEEK_SET) == 0;
    if (ok && e.size) ok = std::fread(&(*data)[0], 1, data->size(), f) == data->size();
    std::fclose(f);
    return ok;
}

}   // namespace g3asm
