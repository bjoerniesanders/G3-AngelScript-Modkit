// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
#pragma once
// Lesen von g3asm.ini (Loader und DLL). GetPrivateProfile* uebersieht den ersten Abschnitt, wenn die Datei mit einem UTF-8-BOM beginnt
// (Editor "UTF-8 mit BOM", PowerShell 5 Set-Content -Encoding utf8): dann galten alle Standardwerte, enable=0 wirkte nicht.
// Ohne BOM bleibt alles bei GetPrivateProfile* (gleiches Verhalten wie bisher, auch fuer UTF-16-Dateien). Mit BOM liest diese Klasse
// die Datei selbst, mit denselben Regeln: Abschnitt und Schluessel ohne Gross-/Kleinschreibung, Leerraum getrimmt, erster Treffer gilt,
// ein umschliessendes Paar "" wird entfernt, Kommentarzeilen beginnen mit ';'. Nur kernel32 und C-Laufzeit (auch in DllMain nutzbar).
#include <windows.h>

#include <cstdlib>
#include <cstring>

namespace g3asm {

class IniFile {
public:
    explicit IniFile(const char* path) : path_(path) {
        HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) return;
        unsigned char head[3] = {};
        DWORD got = 0;
        if (ReadFile(f, head, 3, &got, nullptr) && got == 3 && head[0] == 0xEF && head[1] == 0xBB && head[2] == 0xBF) {
            const DWORD size = GetFileSize(f, nullptr);
            if (size != INVALID_FILE_SIZE && size > 3 && size <= (1u << 20)) {
                data_ = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, size - 3 + 1));
                if (data_ && ReadFile(f, data_, size - 3, &got, nullptr)) {
                    len_ = got;
                    data_[len_] = 0;
                }
            }
            bom_ = true;   // auch bei leerer Datei mit BOM: eigener Leser (findet dann nichts)
        }
        CloseHandle(f);
    }
    ~IniFile() {
        if (data_) HeapFree(GetProcessHeap(), 0, data_);
    }
    IniFile(const IniFile&) = delete;
    IniFile& operator=(const IniFile&) = delete;

    bool Bom() const { return bom_; }

    // wie GetPrivateProfileSectionA(...) > 0: Abschnitt vorhanden und nicht leer
    bool HasSection(const char* sec) const {
        if (!bom_) {
            char probe[8];
            return GetPrivateProfileSectionA(sec, probe, sizeof probe, path_) > 0;
        }
        bool in = false;
        for (size_t pos = 0; pos < len_;) {
            const char *a, *b;
            pos = NextLine(pos, a, b);
            if (a == b || *a == ';') continue;
            if (*a == '[') {
                in = SectionIs(a, b, sec);
                continue;
            }
            if (in) return true;
        }
        return false;
    }

    int Int(const char* sec, const char* key, int def) const {
        if (!bom_) return static_cast<int>(GetPrivateProfileIntA(sec, key, def, path_));
        char buf[64];
        if (!Find(sec, key, buf, sizeof buf)) return def;
        return static_cast<int>(std::strtol(buf, nullptr, 10));
    }

    // def darf auf out zeigen (wie bei GetPrivateProfileStringA)
    void Str(const char* sec, const char* key, const char* def, char* out, DWORD n) const {
        if (!n) return;
        if (!bom_) {
            GetPrivateProfileStringA(sec, key, def, out, n, path_);
            return;
        }
        char buf[MAX_PATH * 2];
        if (Find(sec, key, buf, sizeof buf)) strncpy_s(out, n, buf, _TRUNCATE);
        else if (def != out) strncpy_s(out, n, def ? def : "", _TRUNCATE);
    }

private:
    // Zeile ab pos: [a, b) getrimmt; liefert die Position der naechsten Zeile
    size_t NextLine(size_t pos, const char*& a, const char*& b) const {
        size_t end = pos;
        while (end < len_ && data_[end] != '\n') ++end;
        a = data_ + pos;
        b = data_ + end;
        while (a < b && (*a == ' ' || *a == '\t' || *a == '\r')) ++a;
        while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r')) --b;
        return end + 1;
    }
    static bool SectionIs(const char* a, const char* b, const char* sec) {
        const char* close = static_cast<const char*>(std::memchr(a, ']', size_t(b - a)));
        if (!close) return false;
        const char *s = a + 1, *e = close;
        while (s < e && (*s == ' ' || *s == '\t')) ++s;
        while (e > s && (e[-1] == ' ' || e[-1] == '\t')) --e;
        const size_t n = std::strlen(sec);
        return size_t(e - s) == n && _strnicmp(s, sec, n) == 0;
    }
    bool Find(const char* sec, const char* key, char* out, size_t n) const {
        bool in = false;
        const size_t kn = std::strlen(key);
        for (size_t pos = 0; pos < len_;) {
            const char *a, *b;
            pos = NextLine(pos, a, b);
            if (a == b || *a == ';') continue;
            if (*a == '[') {
                in = SectionIs(a, b, sec);
                continue;
            }
            if (!in) continue;
            const char* eq = static_cast<const char*>(std::memchr(a, '=', size_t(b - a)));
            if (!eq) continue;
            const char* ke = eq;
            while (ke > a && (ke[-1] == ' ' || ke[-1] == '\t')) --ke;
            if (size_t(ke - a) != kn || _strnicmp(a, key, kn) != 0) continue;
            const char* v = eq + 1;
            const char* ve = b;
            while (v < ve && (*v == ' ' || *v == '\t')) ++v;
            if (ve - v >= 2 && *v == '"' && ve[-1] == '"') {
                ++v;
                --ve;
            }
            const size_t vn = size_t(ve - v) < n - 1 ? size_t(ve - v) : n - 1;
            std::memcpy(out, v, vn);
            out[vn] = 0;
            return true;
        }
        return false;
    }

    const char* path_;
    char* data_ = nullptr;
    size_t len_ = 0;
    bool bom_ = false;
};

}   // namespace g3asm
