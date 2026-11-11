// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 OG3-Patch authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// Einstellungen je Mod (Skript-Schnittstelle ab Version 44). Jeder Mod liest und schreibt nur seine eigenen Werte (Mod = der gerade laufende Mod).
//   Vorgaben:  <Mod>\mod.ini, Abschnitt [settings] (liefert der Mod-Autor mit, wird nie geschrieben)
//   Benutzer:  <Spiel>\og3\settings\<Mod>.ini, Abschnitt [settings] (schreibt SetModSetting; bleibt bei einem Update des Mods erhalten)
// Lesen: Benutzerwert, sonst Vorgabe, sonst der Standard aus dem Aufruf. Schluessel ohne Beachtung von Gross-/Kleinschreibung.
// Die Dateien werden einmal je Mod gelesen und zwischengespeichert; beim Neuladen des Mods (Hot-Reload) wird neu gelesen. Von Hand geaenderte
// Dateien gelten also nach dem Neuladen des Mods oder einem Neustart. Ohne Mod-Ordner (Einzelskript) gibt es nur Vorgaben, Schreiben schlaegt fehl.
// Testprogramm: Umgebungsvariable OG3_SETTINGS_DIR ersetzt den Ordner fuer Benutzerwerte.
#include <windows.h>

#include <angelscript.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>

#include "sb_api.h"
#include "script_host.h"

namespace og3p {
namespace {

struct Entry {
    std::string key, value;   // key in der Schreibweise des Mods
};
struct ModSettings {
    bool loaded = false;
    std::map<std::string, Entry> user, defaults;   // Schluessel klein geschrieben
};
std::mutex g_mu;
std::map<std::string, ModSettings> g_mods;   // Modname klein geschrieben

std::string Lower(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// Abschnitt [settings] einer ini-Datei lesen (eigener Leser: ohne die 64-KB-Grenze und Eigenheiten von GetPrivateProfileString)
void ReadSection(const std::string& path, std::map<std::string, Entry>& out) {
    FILE* f = nullptr;
    if (path.empty() || fopen_s(&f, path.c_str(), "rb") != 0 || !f) return;
    std::string data;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0 && data.size() < (1u << 20)) data.append(buf, n);
    std::fclose(f);
    if (data.size() >= 3 && static_cast<unsigned char>(data[0]) == 0xEF && static_cast<unsigned char>(data[1]) == 0xBB &&
        static_cast<unsigned char>(data[2]) == 0xBF)
        data.erase(0, 3);
    bool inSection = false;
    size_t pos = 0;
    while (pos <= data.size()) {
        size_t end = data.find('\n', pos);
        if (end == std::string::npos) end = data.size();
        std::string line = Trim(data.substr(pos, end - pos));
        pos = end + 1;
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') {
            inSection = Lower(line) == "[settings]";
            continue;
        }
        if (!inSection) continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, eq));
        if (key.empty()) continue;
        out[Lower(key)] = Entry{key, Trim(line.substr(eq + 1))};
    }
}

std::string UserDir() {
    // Windows-Umgebung statt getenv: das Testprogramm setzt die Variable mit seiner eigenen C-Laufzeit, die DLL hat eine andere
    char env[MAX_PATH];
    const DWORD n = GetEnvironmentVariableA("OG3_SETTINGS_DIR", env, sizeof env);
    if (n > 0 && n < sizeof env) return std::string(env);
    if (!ScriptHost::s_current) return std::string();
    std::string mods = ScriptHost::s_current->ModsDir();
    while (!mods.empty() && (mods.back() == '\\' || mods.back() == '/')) mods.pop_back();
    const size_t s = mods.find_last_of("\\/");
    if (s == std::string::npos) return std::string();
    return mods.substr(0, s) + "\\settings";
}
std::string UserFile(const std::string& mod) {
    const std::string d = UserDir();
    return d.empty() ? std::string() : d + "\\" + mod + ".ini";
}

// Einstellungen des laufenden Mods (geladen bei Bedarf); nullptr ohne laufenden Mod. Aufrufer haelt g_mu.
ModSettings* Current(std::string* modOut) {
    if (!ScriptHost::s_current) return nullptr;
    const std::string mod = ScriptHost::s_current->CurrentMod();
    if (mod.empty()) return nullptr;
    ModSettings& s = g_mods[Lower(mod)];
    if (!s.loaded) {
        s.loaded = true;
        const std::string dir = ScriptHost::s_current->ModDir(mod);
        if (!dir.empty()) ReadSection(dir + "\\mod.ini", s.defaults);
        ReadSection(UserFile(mod), s.user);
    }
    if (modOut) *modOut = mod;
    return &s;
}

bool Lookup(const std::string& key, std::string* value) {
    std::lock_guard<std::mutex> lock(g_mu);
    ModSettings* s = Current(nullptr);
    if (!s) return false;
    const std::string k = Lower(Trim(key));
    auto it = s->user.find(k);
    if (it != s->user.end()) return *value = it->second.value, true;
    it = s->defaults.find(k);
    if (it != s->defaults.end()) return *value = it->second.value, true;
    return false;
}

bool ValidKey(const std::string& k) {
    if (k.empty() || k.size() > 100) return false;
    for (char c : k)
        if (c == '=' || c == '[' || c == ']' || c == ';' || c == '#' || c == '\r' || c == '\n') return false;
    return true;
}

bool WriteUser(const std::string& mod, const ModSettings& s) {
    const std::string dir = UserDir();
    if (dir.empty()) return false;
    CreateDirectoryA(dir.c_str(), nullptr);
    const std::string path = dir + "\\" + mod + ".ini", tmp = path + ".tmp";
    FILE* f = nullptr;
    if (fopen_s(&f, tmp.c_str(), "wb") != 0 || !f) return false;
    std::fprintf(f, "; Einstellungen des Mods %s (vom OG3-Patch geschrieben; Vorgaben stehen in der mod.ini des Mods)\r\n[settings]\r\n", mod.c_str());
    for (const auto& kv : s.user) std::fprintf(f, "%s=%s\r\n", kv.second.key.c_str(), kv.second.value.c_str());
    const bool ok = std::fflush(f) == 0;
    std::fclose(f);
    if (!ok || !MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmp.c_str());
        return false;
    }
    return true;
}

// ---- Skriptfunktionen
SStr GetSetting(const SStr& key, const SStr& def) {
    std::string v;
    return Lookup(key.c_str(), &v) ? SStr(v.c_str()) : def;
}
int GetSettingInt(const SStr& key, int def) {
    std::string v;
    if (!Lookup(key.c_str(), &v) || v.empty()) return def;
    char* end = nullptr;
    const long r = std::strtol(v.c_str(), &end, 0);
    return end && *end == 0 ? int(r) : def;
}
float GetSettingFloat(const SStr& key, float def) {
    std::string v;
    if (!Lookup(key.c_str(), &v) || v.empty()) return def;
    for (char& c : v)
        if (c == ',') c = '.';
    char* end = nullptr;
    const double r = std::strtod(v.c_str(), &end);
    return end && *end == 0 ? float(r) : def;
}
bool GetSettingBool(const SStr& key, bool def) {
    std::string v;
    if (!Lookup(key.c_str(), &v)) return def;
    v = Lower(v);
    if (v == "1" || v == "true" || v == "yes" || v == "ja" || v == "on") return true;
    if (v == "0" || v == "false" || v == "no" || v == "nein" || v == "off") return false;
    return def;
}
bool HasSetting(const SStr& key) {
    std::string v;
    return Lookup(key.c_str(), &v);
}
bool SetSetting(const SStr& key, const SStr& value) {
    const std::string k = Trim(key.c_str()), v = value.c_str();
    if (!ValidKey(k) || v.size() > 2000 || v.find_first_of("\r\n") != std::string::npos) {
        if (ScriptHost::s_current) ScriptHost::s_current->Log("SetModSetting: ungueltiger Schluessel oder Wert ('" + k + "'; kein = [ ] ; # und keine Zeilenumbrueche)");
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    std::string mod;
    ModSettings* s = Current(&mod);
    if (!s) return false;
    s->user[Lower(k)] = Entry{k, Trim(v)};
    return WriteUser(mod, *s);
}
bool ResetSetting(const SStr& key) {
    std::lock_guard<std::mutex> lock(g_mu);
    std::string mod;
    ModSettings* s = Current(&mod);
    if (!s) return false;
    if (s->user.erase(Lower(Trim(key.c_str()))) == 0) return true;
    return WriteUser(mod, *s);
}

}   // namespace

void ModSettingsForget(const std::string& mod) {
    std::lock_guard<std::mutex> lock(g_mu);
    g_mods.erase(Lower(mod));
}

bool RegisterModSettingsBindings(asIScriptEngine* e) {
    bool ok = true;
    OG3_REG(e->RegisterGlobalFunction("bCString GetModSetting(const bCString &in key, const bCString &in def = \"\")", asFUNCTION(GetSetting), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("int GetModSettingInt(const bCString &in key, int def = 0)", asFUNCTION(GetSettingInt), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("float GetModSettingFloat(const bCString &in key, float def = 0)", asFUNCTION(GetSettingFloat), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool GetModSettingBool(const bCString &in key, bool def = false)", asFUNCTION(GetSettingBool), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool HasModSetting(const bCString &in key)", asFUNCTION(HasSetting), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool SetModSetting(const bCString &in key, const bCString &in value)", asFUNCTION(SetSetting), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool ResetModSetting(const bCString &in key)", asFUNCTION(ResetSetting), asCALL_CDECL));
    return ok;
}

}   // namespace og3p
