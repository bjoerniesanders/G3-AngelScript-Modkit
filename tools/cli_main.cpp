// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// g3asm: command line for modders (from version 47). Each command also writes JSON with --json (for scripts and agents).
//   g3asm new     <mod folder>                          make a mod folder with main.as and mod.ini
//   g3asm check   <mod folder|file.as> [--sharedbase <SharedBase.dll>]
//                                                       compile the script without the game (errors with line and column)
//   g3asm pack    <folder> <archive> ...                as g3asm_packmod pack
//   g3asm release <mod folder> <target folder> ...      as g3asm_packmod release
//   g3asm log     [<game folder>] [-n <count>] [--all]  the last errors and warnings from g3asm.log
// check loads g3asm.dll (next to g3asm.exe) with the replacement SharedBase.dll from sbstub\ (no game files needed).
// Exit code: 0 = ok, 1 = errors in the mod (or a failed pack/release), 2 = wrong call or the tool cannot work.
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <string>
#include <vector>

#include "../src/script_host.h"   // G3ASM_SCRIPT_API_VERSION for the mod.ini of a new mod

namespace {

bool g_json = false;

// ---------------------------------------------------------------- text helpers
std::string Narrow(const std::wstring& w, UINT cp) {
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(cp, 0, w.c_str(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0) WideCharToMultiByte(cp, 0, w.c_str(), int(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}
std::wstring Widen(const std::string& s, UINT cp) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(cp, 0, s.c_str(), int(s.size()), nullptr, 0);
    std::wstring w(n > 0 ? n : 0, L'\0');
    if (n > 0) MultiByteToWideChar(cp, 0, s.c_str(), int(s.size()), &w[0], n);
    return w;
}
std::string Utf8(const std::wstring& w) { return Narrow(w, CP_UTF8); }
// the log and the compiler messages are bytes: script texts are UTF-8, paths from the modkit are in the ANSI code page
std::string ToUtf8(const std::string& s) {
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), int(s.size()), nullptr, 0) > 0 || s.empty()) return s;
    return Narrow(Widen(s, CP_ACP), CP_UTF8);
}
std::string JsonStr(const std::string& raw) {
    const std::string s = ToUtf8(raw);
    std::string o = "\"";
    for (const char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if (c < 0x20) {
            char b[8];
            std::snprintf(b, sizeof b, "\\u%04x", c);
            o += b;
        } else o += ch;
    }
    return o + "\"";
}
void Out(const std::string& s) { std::fwrite(s.data(), 1, s.size(), stdout); }

// error before the command could work (exit code 2)
int Fail(const std::string& msg) {
    if (g_json) Out("{\"ok\":false,\"error\":" + JsonStr(msg) + "}\n");
    else std::fprintf(stderr, "%s\n", msg.c_str());
    return 2;
}

int Usage() {
    return Fail("Aufruf:\n"
                "  g3asm new     <Mod-Ordner>\n"
                "  g3asm check   <Mod-Ordner|Datei.as> [--sharedbase <SharedBase.dll>]\n"
                "  g3asm pack    <Ordner> <Archiv>                    (wie g3asm_packmod pack)\n"
                "  g3asm release <Mod-Ordner> <Ziel-Ordner> [...]     (wie g3asm_packmod release)\n"
                "  g3asm log     [<Spielordner>] [-n <Zahl>] [--all]\n"
                "Option fuer alle Befehle: --json");
}

std::wstring ExeDir() {
    wchar_t p[MAX_PATH];
    GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::wstring s(p);
    return s.substr(0, s.find_last_of(L'\\'));
}
std::wstring FullPath(const std::wstring& p) {
    wchar_t b[MAX_PATH];
    const DWORD n = GetFullPathNameW(p.c_str(), MAX_PATH, b, nullptr);
    std::wstring s = n > 0 && n < MAX_PATH ? std::wstring(b) : p;
    while (s.size() > 3 && (s.back() == L'\\' || s.back() == L'/')) s.pop_back();
    return s;
}
bool IsDir(const std::wstring& p) {
    const DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
bool IsFile(const std::wstring& p) {
    const DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
std::wstring LastPart(const std::wstring& p) {
    const size_t s = p.find_last_of(L"\\/");
    return s == std::wstring::npos ? p : p.substr(s + 1);
}
std::wstring Parent(const std::wstring& p) {
    const size_t s = p.find_last_of(L"\\/");
    return s == std::wstring::npos ? std::wstring(L".") : p.substr(0, s);
}
bool ReadFileBytes(const std::wstring& p, std::string& out) {
    FILE* f = _wfsopen(p.c_str(), L"rb", _SH_DENYNO);   // the game writes g3asm.log during the session
    if (!f) return false;
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}
bool WriteText(const std::wstring& p, const std::string& text) {   // text with \n, written with \r\n (as the examples)
    FILE* f = _wfopen(p.c_str(), L"wb");
    if (!f) return false;
    std::string t;
    for (const char c : text) {
        if (c == '\n') t += '\r';
        t += c;
    }
    const bool ok = std::fwrite(t.data(), 1, t.size(), f) == t.size();
    return std::fclose(f) == 0 && ok;
}
std::vector<std::string> Lines(const std::string& s) {
    std::vector<std::string> v;
    size_t a = 0;
    while (a < s.size()) {
        size_t e = s.find('\n', a);
        if (e == std::string::npos) e = s.size();
        std::string l = s.substr(a, e - a);
        if (!l.empty() && l.back() == '\r') l.pop_back();
        v.push_back(l);
        a = e + 1;
    }
    return v;
}
std::string Trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return std::string();
    return s.substr(b, s.find_last_not_of(" \t") - b + 1);
}
// value of a key in the section [mod] of a mod.ini (empty if there is none)
std::string ModIniValue(const std::wstring& ini, const char* key) {
    std::string t;
    if (!ReadFileBytes(ini, t)) return std::string();
    if (t.compare(0, 3, "\xEF\xBB\xBF") == 0) t.erase(0, 3);
    bool inMod = false;
    for (std::string l : Lines(t)) {
        const size_t sc = l.find(';');
        if (sc != std::string::npos) l = l.substr(0, sc);
        l = Trim(l);
        if (l.empty()) continue;
        if (l[0] == '[') {
            inMod = _stricmp(l.c_str(), "[mod]") == 0;
            continue;
        }
        const size_t eq = l.find('=');
        if (inMod && eq != std::string::npos && _stricmp(Trim(l.substr(0, eq)).c_str(), key) == 0) return Trim(l.substr(eq + 1));
    }
    return std::string();
}

// ---------------------------------------------------------------- new
bool ValidModName(const std::wstring& n) {
    if (n.empty() || n.size() > 64) return false;
    for (const wchar_t c : n)
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'_' || c == L'-')) return false;
    return true;
}

int CmdNew(const std::vector<std::wstring>& a, int api) {
    if (a.size() != 1) return Usage();
    const std::wstring dir = FullPath(a[0]);
    const std::wstring name = LastPart(dir);
    if (!ValidModName(name)) return Fail("Ungueltiger Mod-Name '" + Utf8(name) + "': nur Buchstaben a-z, Ziffern, _ und -, hoechstens 64 Zeichen");
    if (GetFileAttributesW(dir.c_str()) != INVALID_FILE_ATTRIBUTES) return Fail("Den Ordner gibt es schon: " + Utf8(dir));
    if (!IsDir(Parent(dir))) return Fail("Den uebergeordneten Ordner gibt es nicht: " + Utf8(Parent(dir)));
    if (!CreateDirectoryW(dir.c_str(), nullptr)) return Fail("Ordner nicht anlegbar: " + Utf8(dir));
    const std::string n = Utf8(name);
    const std::string mainAs = "// " + n +
                               ": G3ASM mod. Documentation: docs\\modkit\\index.md (German), docs\\en\\modkit\\index.md (English)\n"
                               "\n"
                               "void main() {\n"
                               "    print(\"" + n + " geladen\");\n"
                               "}\n";
    const std::string modIni = "[mod]\n"
                               "version=1.0.0\n"
                               "api=" + std::to_string(api) + "\n"
                               "title=" + n + "\n";
    if (!WriteText(dir + L"\\main.as", mainAs) || !WriteText(dir + L"\\mod.ini", modIni)) return Fail("Dateien nicht schreibbar in " + Utf8(dir));
    if (g_json) {
        Out("{\"ok\":true,\"mod\":" + JsonStr(n) + ",\"path\":" + JsonStr(Utf8(dir)) + ",\"files\":[\"main.as\",\"mod.ini\"]}\n");
    } else {
        Out("Mod angelegt: " + Utf8(dir) + "\n  main.as\n  mod.ini (api=" + std::to_string(api) + ")\n");
    }
    return 0;
}

// ---------------------------------------------------------------- check
std::vector<std::string> g_hostLines;
void HostLog(const char* s) {   // the modkit puts "g3asm: " in front of each line for the host
    std::string l(s ? s : "");
    if (l.compare(0, 7, "g3asm: ") == 0) l.erase(0, 7);
    g_hostLines.push_back(l);
}

struct Message {
    std::string file, type, text;
    int line = 0, col = 0;
};

int CmdCheck(const std::vector<std::wstring>& args) {
    std::wstring target, sbPath;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == L"--sharedbase" && i + 1 < args.size()) sbPath = FullPath(args[++i]);
        else if (target.empty()) target = args[i];
        else return Usage();
    }
    if (target.empty()) return Usage();
    target = FullPath(target);
    std::wstring script, modDir;
    if (IsDir(target)) {
        modDir = target;
        script = target + L"\\main.as";
    } else {
        script = target;
        modDir = Parent(target);
    }
    if (!IsFile(script)) return Fail("Skript nicht gefunden: " + Utf8(script));
    const std::wstring name = LastPart(modDir);

    const std::wstring exeDir = ExeDir();
    if (sbPath.empty()) {   // release: bin\g3asm\sbstub\SharedBase.dll; build folder: build\sbstub\SharedBase.dll next to build\g3asm
        for (const std::wstring& c : {exeDir + L"\\sbstub\\SharedBase.dll", Parent(exeDir) + L"\\sbstub\\SharedBase.dll"})
            if (IsFile(c)) {
                sbPath = c;
                break;
            }
    }
    if (sbPath.empty() || !IsFile(sbPath)) return Fail("SharedBase.dll fuer die Pruefung fehlt (erwartet: " + Utf8(exeDir) + "\\sbstub\\SharedBase.dll)");
    SetDllDirectoryW(Parent(sbPath).c_str());
    HMODULE sb = LoadLibraryW(sbPath.c_str());
    if (!sb) return Fail("SharedBase.dll nicht ladbar: " + Utf8(sbPath));
    HMODULE dll = LoadLibraryW((exeDir + L"\\g3asm.dll").c_str());
    using InitFn = int (*)(HMODULE, const char*, void (*)(const char*));
    using CheckFn = int (*)(const char*, const char*);
    using ApiFn = int (*)();
    auto init = dll ? reinterpret_cast<InitFn>(GetProcAddress(dll, "g3asm_init")) : nullptr;
    auto check = dll ? reinterpret_cast<CheckFn>(GetProcAddress(dll, "g3asm_check_script")) : nullptr;
    auto apiFn = dll ? reinterpret_cast<ApiFn>(GetProcAddress(dll, "g3asm_api_version")) : nullptr;
    if (!init || !check || !apiFn) return Fail("g3asm.dll neben g3asm.exe fehlt oder ist zu alt");

    // the modkit writes its g3asm.log and the settings of the mods into the temp folder, not into the mod folder
    wchar_t tmpW[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpW);
    const std::wstring work = std::wstring(tmpW) + L"g3asm_check";
    CreateDirectoryW(work.c_str(), nullptr);
    _wputenv_s(L"G3ASM_SETTINGS_DIR", (work + L"\\settings").c_str());
    if (!init(sb, Narrow(work, CP_ACP).c_str(), HostLog)) {
        std::string why;
        for (const std::string& l : g_hostLines) why += "\n" + l;
        return Fail("Modkit nicht startbar:" + why);
    }
    const int api = apiFn();
    g_hostLines.clear();
    const bool compiled = check(Narrow(name, CP_ACP).c_str(), Narrow(script, CP_ACP).c_str()) != 0;

    std::vector<Message> msgs;
    const std::regex re(R"(^(.*) \((\d+), (\d+)\): (FEHLER|Warnung|Info): (.*)$)");
    std::smatch m;
    for (const std::string& l : g_hostLines)
        if (std::regex_match(l, m, re)) msgs.push_back({m[1].str(), m[4].str() == "FEHLER" ? "error" : m[4].str() == "Warnung" ? "warning" : "info",
                                                        m[5].str(), std::atoi(m[2].str().c_str()), std::atoi(m[3].str().c_str())});
    const std::string prefix = "[" + Narrow(name, CP_ACP) + "] ";
    bool anyError = false;
    for (const Message& x : msgs) anyError |= x.type == "error";
    if (!compiled && !anyError)   // no compiler message: for example the file is not readable
        for (const std::string& l : g_hostLines)
            if (l.compare(0, prefix.size(), prefix) == 0) msgs.push_back({Narrow(script, CP_ACP), "error", l.substr(prefix.size()), 0, 0});
    const std::string needApi = ModIniValue(modDir + L"\\mod.ini", "api");
    if (!needApi.empty() && std::atoi(needApi.c_str()) > api)
        msgs.push_back({Narrow(modDir, CP_ACP) + "\\mod.ini", "error",
                        "mod.ini verlangt api=" + needApi + ", dieses Modkit hat die Skript-Schnittstelle " + std::to_string(api), 0, 0});

    int errors = 0, warnings = 0;
    for (const Message& x : msgs) {
        errors += x.type == "error";
        warnings += x.type == "warning";
    }
    const bool ok = compiled && errors == 0;
    if (g_json) {
        std::string j = "{\"ok\":" + std::string(ok ? "true" : "false") + ",\"mod\":" + JsonStr(Utf8(name)) + ",\"file\":" + JsonStr(Utf8(script)) +
                        ",\"api\":" + std::to_string(api) + ",\"errors\":" + std::to_string(errors) + ",\"warnings\":" + std::to_string(warnings) +
                        ",\"messages\":[";
        for (size_t i = 0; i < msgs.size(); ++i) {
            const Message& x = msgs[i];
            j += std::string(i ? "," : "") + "{\"type\":\"" + x.type + "\",\"file\":" + JsonStr(x.file) + ",\"line\":" + std::to_string(x.line) +
                 ",\"col\":" + std::to_string(x.col) + ",\"text\":" + JsonStr(x.text) + "}";
        }
        Out(j + "]}\n");
    } else {
        for (const Message& x : msgs) {
            const char* t = x.type == "error" ? "FEHLER" : x.type == "warning" ? "Warnung" : "Info";
            if (x.line > 0) Out(ToUtf8(x.file) + " (" + std::to_string(x.line) + ", " + std::to_string(x.col) + "): " + t + ": " + ToUtf8(x.text) + "\n");
            else Out(ToUtf8(x.file) + ": " + t + ": " + ToUtf8(x.text) + "\n");
        }
        Out(ok ? "OK: " + Utf8(name) + " ohne Fehler (Skript-Schnittstelle " + std::to_string(api) + ")" +
                     (warnings ? ", " + std::to_string(warnings) + " Warnung(en)" : std::string()) + "\n"
               : Utf8(name) + ": " + std::to_string(errors) + " Fehler, " + std::to_string(warnings) + " Warnung(en)\n");
    }
    std::fflush(stdout);
    // do not let the modkit clean up (as in tests/test_runner.cpp)
    TerminateProcess(GetCurrentProcess(), ok ? 0 : 1);
    return ok ? 0 : 1;
}

// ---------------------------------------------------------------- pack, release: g3asm_packmod does the work
std::wstring QuoteArg(const std::wstring& a) {   // rules of CommandLineToArgvW
    if (!a.empty() && a.find_first_of(L" \t\"") == std::wstring::npos) return a;
    std::wstring o = L"\"";
    size_t bs = 0;
    for (const wchar_t c : a) {
        if (c == L'\\') {
            ++bs;
            continue;
        }
        if (c == L'"') o.append(bs * 2 + 1, L'\\');
        else o.append(bs, L'\\');
        bs = 0;
        o += c;
    }
    o.append(bs * 2, L'\\');
    return o + L"\"";
}

int CmdPackmod(const std::wstring& cmd, const std::vector<std::wstring>& args) {
    const std::wstring tool = ExeDir() + L"\\g3asm_packmod.exe";
    if (!IsFile(tool)) return Fail("g3asm_packmod.exe neben g3asm.exe fehlt");
    std::wstring line = QuoteArg(tool) + L" " + cmd;
    for (const std::wstring& a : args) line += L" " + QuoteArg(a);

    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return Fail("Pipe nicht anlegbar");
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = wr;
    si.hStdError = wr;
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(line.begin(), line.end());
    buf.push_back(L'\0');
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(rd);
        CloseHandle(wr);
        return Fail("g3asm_packmod.exe nicht startbar");
    }
    CloseHandle(wr);
    std::string out;
    char b[4096];
    DWORD n = 0;
    while (ReadFile(rd, b, sizeof b, &n, nullptr) && n > 0) {
        out.append(b, n);
        if (!g_json) std::fwrite(b, 1, n, stdout);
    }
    CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD rc = 2;
    GetExitCodeProcess(pi.hProcess, &rc);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (g_json) {
        std::string o;
        for (const char c : out)
            if (c != '\r') o += c;
        Out("{\"ok\":" + std::string(rc == 0 ? "true" : "false") + ",\"exit\":" + std::to_string(rc) + ",\"out\":" + JsonStr(o) + "}\n");
    }
    return rc == 0 ? 0 : rc == 2 ? 2 : 1;
}

// ---------------------------------------------------------------- log
bool IsProblem(const std::string& l) {
    static const char* const kWords[] = {"FEHLER", "Fehler", "Warnung", "WARNUNG", "VERALTET", "Ausnahme", "abgebrochen (laenger", "nicht geladen",
                                         "nicht gefunden", "nicht lesbar"};
    for (const char* w : kWords)
        if (l.find(w) != std::string::npos) return true;
    return false;
}

int CmdLog(const std::vector<std::wstring>& args) {
    std::wstring dir = L".";
    int count = 20;
    bool all = false, haveDir = false;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == L"-n" && i + 1 < args.size()) count = _wtoi(args[++i].c_str());
        else if (args[i] == L"--all") all = true;
        else if (!haveDir) {
            dir = args[i];
            haveDir = true;
        } else return Usage();
    }
    if (count <= 0) count = 20;
    dir = FullPath(dir);
    const std::wstring file = dir + L"\\g3asm.log";
    std::string text;
    if (!ReadFileBytes(file, text)) return Fail("Kein g3asm.log in " + Utf8(dir) + " (Spielordner angeben)");
    std::vector<std::string> pick;
    for (const std::string& l : Lines(text))
        if (!l.empty() && (all || IsProblem(l))) pick.push_back(l);
    if (pick.size() > size_t(count)) pick.erase(pick.begin(), pick.end() - count);

    if (g_json) {
        // a line: "12:34:56.789 [mod] text"; time and mod are optional
        const std::regex re(R"(^(\d\d:\d\d:\d\d\.\d\d\d) (?:\[([^\]]+)\] )?(.*)$)");
        std::string j = "{\"ok\":true,\"file\":" + JsonStr(Utf8(file)) + ",\"count\":" + std::to_string(pick.size()) + ",\"lines\":[";
        for (size_t i = 0; i < pick.size(); ++i) {
            std::smatch m;
            std::string time, mod, t = pick[i];
            if (std::regex_match(pick[i], m, re)) {
                time = m[1].str();
                mod = m[2].str();
                t = m[3].str();
            }
            j += std::string(i ? "," : "") + "{\"time\":" + JsonStr(time) + ",\"mod\":" + JsonStr(mod) + ",\"text\":" + JsonStr(t) + "}";
        }
        Out(j + "]}\n");
    } else if (pick.empty()) {
        Out("Keine Fehler und Warnungen in " + Utf8(file) + "\n");
    } else {
        for (const std::string& l : pick) Out(ToUtf8(l) + "\n");
    }
    return 0;
}

}   // namespace

int wmain(int argc, wchar_t** argv) {
    std::vector<std::wstring> args;
    for (int i = 1; i < argc; ++i) {
        if (std::wcscmp(argv[i], L"--json") == 0) g_json = true;
        else args.emplace_back(argv[i]);
    }
    if (args.empty()) return Usage();
    const std::wstring cmd = args[0];
    args.erase(args.begin());
    if (cmd == L"new") return CmdNew(args, G3ASM_SCRIPT_API_VERSION);
    if (cmd == L"check") return CmdCheck(args);
    if (cmd == L"pack" || cmd == L"release") return CmdPackmod(cmd, args);
    if (cmd == L"log") return CmdLog(args);
    return Usage();
}
