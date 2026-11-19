// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// g3asm_dev: client for the dev pipe (src/dev_pipe.cpp). Sends AngelScript code to the running game and writes the JSON answer to stdout.
//   g3asm_dev [-m <mod>] [-w <seconds>] "<code>"      code as an argument (body of a function, for example: print(GetHero().GetName());)
//   g3asm_dev [-m <mod>] [-w <seconds>] -f <file.as>   code from a file (UTF-8)
// -m: run the code in the module of a loaded mod (globals of the mod are available). -w: wait for the pipe (default 5 s).
// Exit code: 0 = ok, 1 = script error, 2 = no connection or wrong call.
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

std::string Utf8(const wchar_t* w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    return s;
}

int Usage() {
    std::fprintf(stderr, "Aufruf: g3asm_dev [-m <Mod>] [-w <Sekunden>] \"<Code>\"  |  g3asm_dev [-m <Mod>] [-w <Sekunden>] -f <Datei.as>\n");
    return 2;
}

}   // namespace

int wmain(int argc, wchar_t** argv) {
    std::string mod, code;
    int waitSec = 5;
    bool haveCode = false;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (a == L"-m" && i + 1 < argc) {
            mod = Utf8(argv[++i]);
        } else if (a == L"-w" && i + 1 < argc) {
            waitSec = _wtoi(argv[++i]);
        } else if (a == L"-f" && i + 1 < argc) {
            FILE* f = _wfopen(argv[++i], L"rb");
            if (!f) {
                std::fprintf(stderr, "Datei nicht lesbar\n");
                return 2;
            }
            char buf[4096];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) code.append(buf, n);
            std::fclose(f);
            if (code.compare(0, 3, "\xEF\xBB\xBF") == 0) code.erase(0, 3);   // UTF-8 BOM
            haveCode = true;
        } else if (!haveCode) {
            code = Utf8(argv[i]);
            haveCode = true;
        } else {
            return Usage();
        }
    }
    if (!haveCode) return Usage();
    const std::string msg = mod.empty() ? code : "#mod " + mod + "\n" + code;

    const char* name = "\\\\.\\pipe\\g3asm_dev";
    HANDLE pipe = INVALID_HANDLE_VALUE;
    const DWORD end = GetTickCount() + DWORD(waitSec > 0 ? waitSec : 0) * 1000u;
    for (;;) {
        pipe = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) break;
        const DWORD e = GetLastError();
        if (int(GetTickCount() - end) >= 0) {
            std::printf("{\"ok\":false,\"error\":\"keine Verbindung zur Dev-Pipe (%lu) - laeuft das Spiel mit [dev] pipe=1?\"}\n", e);
            return 2;
        }
        if (e == ERROR_PIPE_BUSY) WaitNamedPipeA(name, 1000);
        else Sleep(200);
    }
    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);
    DWORD n = 0;
    if (!WriteFile(pipe, msg.data(), DWORD(msg.size()), &n, nullptr)) {
        std::printf("{\"ok\":false,\"error\":\"Senden fehlgeschlagen (%lu)\"}\n", GetLastError());
        CloseHandle(pipe);
        return 2;
    }
    std::string answer;
    char buf[4096];
    for (;;) {
        n = 0;
        const BOOL ok = ReadFile(pipe, buf, sizeof buf, &n, nullptr);
        answer.append(buf, n);
        if (ok) break;
        if (GetLastError() != ERROR_MORE_DATA) {
            std::printf("{\"ok\":false,\"error\":\"Antwort nicht lesbar (%lu)\"}\n", GetLastError());
            CloseHandle(pipe);
            return 2;
        }
    }
    CloseHandle(pipe);
    std::fwrite(answer.data(), 1, answer.size(), stdout);
    std::fputc('\n', stdout);
    return answer.compare(0, 10, "{\"ok\":true") == 0 ? 0 : 1;
}
