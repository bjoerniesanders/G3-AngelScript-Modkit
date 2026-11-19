// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// Dev pipe: developer channel into the running game (ini [dev] pipe=1, default off). Only g3asm.dll (loader path) contains this file. The
// script DLL for players does not (CMakeLists.txt), so player packages cannot open the channel.
//
// Server: a separate thread with the named pipe \\.\pipe\g3asm_dev. Only local clients (PIPE_REJECT_REMOTE_CLIENTS) and only the current
// user (DACL) can connect. One request = one message with AngelScript code (UTF-8, as the .as files of the mods). Optional first line
// "#mod <name>": run the code in the module of this mod. The thread puts the request into a queue. The game thread runs it in the next frame
// (DevPipeFrame from HProcess) with ScriptHost::RunSnippet, with the usual 5-s stop. Answer: one message with JSON
//   {"ok":true,"out":"...","ms":1.25}    or    {"ok":false,"out":"<log lines with the error>","ms":...}
// If no frame runs for 15 s (for example the game hangs), the answer is {"ok":false,"error":"..."} and the request is discarded.
// Client: tools/dev_main.cpp (g3asm_dev.exe).
#include <windows.h>
#include <sddl.h>

#include <atomic>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

#include "script_host.h"

using namespace g3asm;

namespace {

const char kPipeName[] = "\\\\.\\pipe\\g3asm_dev";
const DWORD kMaxRequest = 1u << 20;     // 1 MiB code is more than sufficient
const DWORD kFrameWaitMs = 15000;       // wait for the game thread

struct Request {
    std::string code, mod, answer;
    HANDLE done = nullptr;
    std::atomic<bool> cancelled{false};
    ~Request() {
        if (done) CloseHandle(done);
    }
};

ScriptHost* g_host = nullptr;
std::mutex g_lock;
std::deque<std::shared_ptr<Request>> g_queue;
std::atomic<bool> g_started{false};

std::string JsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 16);
    for (unsigned char c : s) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if (c < 0x20) {
                char b[8];
                std::snprintf(b, sizeof b, "\\u%04x", c);
                o += b;
            } else
                o += char(c);   // bytes >= 0x80 unchanged (UTF-8 from the scripts)
        }
    }
    return o;
}

// security descriptor: full access only for the user of the game process
bool MakeSecurity(SECURITY_ATTRIBUTES* sa, PSECURITY_DESCRIPTOR* sd) {
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return false;
    char buf[256];
    DWORD len = 0;
    const bool got = GetTokenInformation(tok, TokenUser, buf, sizeof buf, &len) != 0;
    CloseHandle(tok);
    if (!got) return false;
    char* sid = nullptr;
    if (!ConvertSidToStringSidA(reinterpret_cast<TOKEN_USER*>(buf)->User.Sid, &sid)) return false;
    const std::string sddl = std::string("D:P(A;;GA;;;") + sid + ")";
    LocalFree(sid);
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl.c_str(), SDDL_REVISION_1, sd, nullptr)) return false;
    sa->nLength = sizeof *sa;
    sa->lpSecurityDescriptor = *sd;
    sa->bInheritHandle = FALSE;
    return true;
}

bool ReadMessage(HANDLE pipe, std::string* msg) {
    char buf[4096];
    for (;;) {
        DWORD n = 0;
        const BOOL ok = ReadFile(pipe, buf, sizeof buf, &n, nullptr);
        msg->append(buf, n);
        if (msg->size() > kMaxRequest) return false;
        if (ok) return true;
        if (GetLastError() != ERROR_MORE_DATA) return false;
    }
}

std::string Handle(const std::string& msg) {
    auto req = std::make_shared<Request>();
    req->code = msg;
    if (msg.compare(0, 5, "#mod ") == 0) {   // first line "#mod <name>"
        const size_t nl = msg.find('\n');
        req->mod = msg.substr(5, nl == std::string::npos ? std::string::npos : nl - 5);
        while (!req->mod.empty() && (req->mod.back() == '\r' || req->mod.back() == ' ')) req->mod.pop_back();
        req->code = nl == std::string::npos ? std::string() : msg.substr(nl + 1);
    }
    req->done = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!req->done) return "{\"ok\":false,\"error\":\"CreateEvent fehlgeschlagen\"}";
    {
        std::lock_guard<std::mutex> l(g_lock);
        g_queue.push_back(req);
    }
    if (WaitForSingleObject(req->done, kFrameWaitMs) != WAIT_OBJECT_0) {
        req->cancelled = true;   // the game thread skips it if it comes later
        return "{\"ok\":false,\"error\":\"Kein Spielbild innerhalb von 15 s - Anfrage verworfen\"}";
    }
    return req->answer;
}

DWORD WINAPI ServerThread(void*) {
    SECURITY_ATTRIBUTES sa{};
    PSECURITY_DESCRIPTOR sd = nullptr;
    const bool secured = MakeSecurity(&sa, &sd);
    if (!secured) g_host->Log("Dev-Pipe: Zugriffsschutz nicht erzeugt - Pipe nicht gestartet");
    bool first = true;
    while (secured) {
        // FILE_FLAG_FIRST_PIPE_INSTANCE: if another process already has the name, do not share it
        HANDLE pipe = CreateNamedPipeA(kPipeName, PIPE_ACCESS_DUPLEX | (first ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0),
                                       PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 1u << 16, 1u << 16, 0,
                                       &sa);
        if (pipe == INVALID_HANDLE_VALUE) {
            char b[160];
            std::snprintf(b, sizeof b, "Dev-Pipe: CreateNamedPipe fehlgeschlagen (%lu)%s", GetLastError(),
                          first ? " - laeuft schon ein zweites Spiel?" : "");
            g_host->Log(b);
            break;
        }
        if (first) g_host->Log(std::string("Dev-Pipe bereit: ") + kPipeName + " (nur lokal, nur dieser Benutzer)");
        first = false;
        if (ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) {
            std::string msg;
            const std::string answer = ReadMessage(pipe, &msg) ? Handle(msg) : "{\"ok\":false,\"error\":\"Anfrage nicht lesbar oder zu gross\"}";
            DWORD n = 0;
            WriteFile(pipe, answer.data(), DWORD(answer.size()), &n, nullptr);
            FlushFileBuffers(pipe);
            DisconnectNamedPipe(pipe);
        }
        CloseHandle(pipe);
    }
    if (sd) LocalFree(sd);
    g_started = false;
    return 0;
}

bool DevPipeStart(ScriptHost& host) {
    if (g_started.exchange(true)) return true;
    g_host = &host;
    HANDLE t = CreateThread(nullptr, 0, ServerThread, nullptr, 0, nullptr);
    if (!t) {
        g_started = false;
        host.Log("Dev-Pipe: Thread nicht gestartet");
        return false;
    }
    CloseHandle(t);
    return true;
}

void DevPipeFrame() {
    if (!g_host) return;
    std::deque<std::shared_ptr<Request>> work;
    {
        std::lock_guard<std::mutex> l(g_lock);
        if (g_queue.empty()) return;
        work.swap(g_queue);
    }
    for (auto& r : work) {
        if (r->cancelled) continue;
        LARGE_INTEGER f, a, b;
        QueryPerformanceFrequency(&f);
        QueryPerformanceCounter(&a);
        std::string out;
        const bool ok = g_host->RunSnippet(r->code, r->mod, &out);
        QueryPerformanceCounter(&b);
        char ms[32];
        std::snprintf(ms, sizeof ms, "%.2f", double(b.QuadPart - a.QuadPart) * 1000.0 / double(f.QuadPart));
        r->answer = std::string("{\"ok\":") + (ok ? "true" : "false") + ",\"out\":\"" + JsonEscape(out) + "\",\"ms\":" + ms + "}";
        SetEvent(r->done);
    }
}

// set the pointers of the core (script_host.h) when g3asm.dll loads
struct Register {
    Register() {
        g_devPipeStart = &DevPipeStart;
        g_devPipeFrame = &DevPipeFrame;
    }
} g_register;

}   // namespace
