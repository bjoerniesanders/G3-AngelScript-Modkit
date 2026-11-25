// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
#include "script_host.h"

#include <angelscript.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <set>
#include <string>
#include <unordered_map>

#include "sb_api.h"
#include "scriptarray/scriptarray.h"
#include "scriptbuilder/scriptbuilder.h"
#include "scriptdictionary/scriptdictionary.h"
#include "scriptmath/scriptmath.h"
#include "scriptstdstring/scriptstdstring.h"

namespace g3asm {

bool RegisterEngineBindings(asIScriptEngine* e);   // engine_bindings.cpp
ScriptHost* ScriptHost::s_current = nullptr;

namespace {

// ---------------------------------------------------------------- bCString
void StrConstruct(void* mem) { new (mem) SStr(); }
void StrCopyConstruct(const SStr& o, void* mem) { new (mem) SStr(o); }
void StrDestruct(void* mem) { static_cast<SStr*>(mem)->~SStr(); }
SStr StrAdd(const SStr& self, const SStr& o) {
    SStr r(self);
    r += o;
    return r;
}
SStr StrFromStd(const std::string& s) { return SStr(s); }
SStr StrAddText(const SStr& self, const char* t) {
    SStr r(self);
    r += SStr(t);
    return r;
}
SStr StrAddInt(const SStr& self, int v) {
    char b[32];
    std::snprintf(b, sizeof b, "%d", v);
    return StrAddText(self, b);
}
SStr StrAddUInt(const SStr& self, unsigned v) {
    char b[32];
    std::snprintf(b, sizeof b, "%u", v);
    return StrAddText(self, b);
}
SStr StrAddFloat(const SStr& self, float v) {
    char b[64];
    std::snprintf(b, sizeof b, "%g", v);
    return StrAddText(self, b);
}
SStr StrAddDouble(const SStr& self, double v) {
    char b[64];
    std::snprintf(b, sizeof b, "%g", v);
    return StrAddText(self, b);
}
SStr StrAddBool(const SStr& self, bool v) { return StrAddText(self, v ? "true" : "false"); }
SStr StrAddRInt(int v, const SStr& self) {
    char b[32];
    std::snprintf(b, sizeof b, "%d", v);
    SStr r(b);
    r += self;
    return r;
}
SStr StrAddRFloat(float v, const SStr& self) {
    char b[64];
    std::snprintf(b, sizeof b, "%g", v);
    SStr r(b);
    r += self;
    return r;
}
int StrLength(const SStr& s) { return g_sb.strGetLength(&s); }
bool StrIsEmpty(const SStr& s) { return g_sb.strIsEmpty ? g_sb.strIsEmpty(&s) : StrLength(s) == 0; }
int StrCompare(const SStr& a, const SStr& b) { return g_sb.strCompare ? g_sb.strCompare(&a, &b) : std::strcmp(a.c_str(), b.c_str()); }
int StrCompareNoCase(const SStr& a, const SStr& b) {
    return g_sb.strCompareNoCase ? g_sb.strCompareNoCase(&a, &b) : _stricmp(a.c_str(), b.c_str());
}
bool StrEquals(const SStr& a, const SStr& b) { return StrCompare(a, b) == 0; }
int StrFind(const SStr& s, const SStr& what, int start) {
    if (g_sb.strFind) return g_sb.strFind(&s, what.c_str(), start, false);
    const char* t = s.c_str();
    const int n = s.length();
    if (start < 0 || start > n) return -1;
    const char* f = std::strstr(t + start, what.c_str());
    return f ? int(f - t) : -1;
}
bool StrContains(const SStr& s, const SStr& what) { return StrFind(s, what, 0) != -1; }
SStr StrLeft(const SStr& s, int n) {
    SStr r;
    r.~SStr();
    g_sb.strLeft(&s, &r, n);
    return r;
}
SStr StrRight(const SStr& s, int n) {
    SStr r;
    r.~SStr();
    g_sb.strRight(&s, &r, n);
    return r;
}
SStr StrMid(const SStr& s, int a, int n) {
    SStr r;
    r.~SStr();
    g_sb.strMid(&s, &r, a, n);
    return r;
}
void StrToUpper(SStr& s) { g_sb.strToUpper(&s); }
void StrToLower(SStr& s) { g_sb.strToLower(&s); }
SStr StrUpper(const SStr& s) {
    SStr r(s);
    g_sb.strToUpper(&r);
    return r;
}
SStr StrLower(const SStr& s) {
    SStr r(s);
    g_sb.strToLower(&r);
    return r;
}

// literals: constant bCString objects, merged by contents
class StrFactory : public asIStringFactory {
public:
    const void* GetStringConstant(const char* data, asUINT length) override {
        std::lock_guard<std::mutex> g(lock_);
        Entry& e = byText_[std::string(data, length)];
        if (!e.str) {
            e.str = std::make_unique<SStr>(std::string(data, length));
            byPtr_[e.str.get()] = std::string(data, length);
        }
        ++e.refs;
        return e.str.get();
    }
    int ReleaseStringConstant(const void* str) override {
        std::lock_guard<std::mutex> g(lock_);
        auto p = byPtr_.find(str);
        if (p == byPtr_.end()) return asERROR;
        auto t = byText_.find(p->second);
        if (--t->second.refs == 0) {
            byText_.erase(t);
            byPtr_.erase(p);
        }   // erase frees the SStr through unique_ptr
        return asSUCCESS;
    }
    int GetRawStringData(const void* str, char* data, asUINT* length) const override {
        const SStr* s = static_cast<const SStr*>(str);
        if (length) *length = asUINT(s->length());
        if (data) std::memcpy(data, s->c_str(), size_t(s->length()));
        return asSUCCESS;
    }

private:
    struct Entry {
        std::unique_ptr<SStr> str;
        int refs = 0;
    };
    std::mutex lock_;
    std::unordered_map<std::string, Entry> byText_;
    std::unordered_map<const void*, std::string> byPtr_;
};
StrFactory g_strFactory;

// ---------------------------------------------------------------- Vektoren, Quaternion, Matrix
void V3Construct(void* m) { new (m) SVec3{0, 0, 0}; }
void V3ConstructXYZ(float x, float y, float z, void* m) { new (m) SVec3{x, y, z}; }
float V3Mag(const SVec3& v) { return g_sb.vecMagnitude(&v); }
float V3SqMag(const SVec3& v) { return g_sb.vecSquareMagnitude(&v); }
void V3Normalize(SVec3& v) { g_sb.vecNormalize(&v); }
SVec3 V3GetNormalized(const SVec3& v) {
    SVec3 r;
    g_sb.vecGetNormalized(&v, &r);
    return r;
}
float V3Dot(const SVec3& a, const SVec3& b) { return g_sb.vecDot(&a, &b); }
SVec3 V3Cross(const SVec3& a, const SVec3& b) {
    SVec3 r;
    g_sb.vecCross(&a, &r, &b);
    return r;
}
SVec3 V3Add(const SVec3& a, const SVec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
SVec3 V3Sub(const SVec3& a, const SVec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
SVec3 V3Mul(const SVec3& a, float f) { return {a.x * f, a.y * f, a.z * f}; }
SVec3 V3Div(const SVec3& a, float f) { return {a.x / f, a.y / f, a.z / f}; }
SVec3 V3Neg(const SVec3& a) { return {-a.x, -a.y, -a.z}; }
bool V3Equals(const SVec3& a, const SVec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
SStr V3ToString(const SVec3& v) {
    char b[96];
    std::snprintf(b, sizeof b, "(%g, %g, %g)", v.x, v.y, v.z);
    return SStr(b);
}

void V2Construct(void* m) { new (m) SVec2{0, 0}; }
void V2ConstructXY(float x, float y, void* m) { new (m) SVec2{x, y}; }
float V2Mag(const SVec2& v) { return g_sb.vec2Magnitude(&v); }
void V2Normalize(SVec2& v) { g_sb.vec2Normalize(&v); }
SVec2 V2Add(const SVec2& a, const SVec2& b) { return {a.x + b.x, a.y + b.y}; }
SVec2 V2Sub(const SVec2& a, const SVec2& b) { return {a.x - b.x, a.y - b.y}; }
SVec2 V2Mul(const SVec2& a, float f) { return {a.x * f, a.y * f}; }

void V4Construct(void* m) { new (m) SVec4{0, 0, 0, 0}; }
void V4ConstructXYZW(float x, float y, float z, float w, void* m) { new (m) SVec4{x, y, z, w}; }
float V4Mag(const SVec4& v) { return g_sb.vec4Magnitude(&v); }
void V4Normalize(SVec4& v) { g_sb.vec4Normalize(&v); }

void QConstruct(void* m) { new (m) SQuat{0, 0, 0, 1}; }   // Einheitsquaternion
void QConstructXYZW(float x, float y, float z, float w, void* m) { new (m) SQuat{x, y, z, w}; }
void QClear(SQuat& q) { g_sb.quatClear(&q); }
void QNormalize(SQuat& q) { g_sb.quatNormalize(&q); }
void QInvert(SQuat& q) { g_sb.quatInvert(&q); }
float QMag(const SQuat& q) { return g_sb.quatMagnitude(&q); }
SQuat QMul(const SQuat& a, const SQuat& b) {
    SQuat r;
    g_sb.quatMul(&a, &r, &b);
    return r;
}
void QFromMatrix(SQuat& q, const SMat3& m) { g_sb.quatFromMatrix3(&q, &m); }

void M3Construct(void* m) {
    SMat3* p = new (m) SMat3;
    g_sb.m3Identity(p);
}   // Einheitsmatrix
float M3Get(const SMat3& m, int r, int c) { return (r >= 0 && r < 3 && c >= 0 && c < 3) ? m.m[r * 3 + c] : 0.0f; }
void M3Set(SMat3& m, int r, int c, float v) {
    if (r >= 0 && r < 3 && c >= 0 && c < 3) m.m[r * 3 + c] = v;
}
void M3Identity(SMat3& m) { g_sb.m3Identity(&m); }
void M3Transpose(SMat3& m) { g_sb.m3Transpose(&m); }
void M3Invert(SMat3& m) { g_sb.m3Invert(&m); }
SMat3 M3MulM(const SMat3& a, const SMat3& b) {
    SMat3 r;
    g_sb.m3MulM3(&a, &r, &b);
    return r;
}
SVec3 M3MulV(const SMat3& a, const SVec3& v) {
    SVec3 r;
    g_sb.m3MulVec(&a, &r, &v);
    return r;
}
void M3RotX(SMat3& m, float a) { g_sb.m3RotX(&m, a); }
void M3RotY(SMat3& m, float a) { g_sb.m3RotY(&m, a); }
void M3RotZ(SMat3& m, float a) { g_sb.m3RotZ(&m, a); }
void M3FromQuat(SMat3& m, const SQuat& q) { g_sb.m3FromQuat(&m, &q); }
// bCMatrix4 (= bCMatrix of the engine, 4x4): row vector convention v' = v * M, rows 0..2 = axes, row 3 = translation
void M4Construct(void* m) {
    SMat4* p = new (m) SMat4;
    g_sb.m4Identity(p);
}
float M4Get(const SMat4& m, int r, int c) { return (r >= 0 && r < 4 && c >= 0 && c < 4) ? m.m[r * 4 + c] : 0.0f; }
void M4Set(SMat4& m, int r, int c, float v) {
    if (r >= 0 && r < 4 && c >= 0 && c < 4) m.m[r * 4 + c] = v;
}
void M4Identity(SMat4& m) { g_sb.m4Identity(&m); }
void M4Transpose(SMat4& m) { g_sb.m4Transpose(&m); }
void M4Invert(SMat4& m) { g_sb.m4Invert(&m); }
SMat4 M4GetInverted(const SMat4& m) {
    SMat4 r = m;
    g_sb.m4Invert(&r);
    return r;
}
float M4Det(const SMat4& m) { return g_sb.m4Det(&m); }
SMat4 M4MulM(const SMat4& a, const SMat4& b) {
    SMat4 r;
    g_sb.m4MulM4(&a, &r, &b);
    return r;
}
SVec3 M4MulV(const SMat4& a, const SVec3& v) {
    SVec3 r;
    g_sb.m4MulVec(&a, &r, &v);
    return r;
}
SVec3 M4Row3(const SMat4& m, int r) {
    SVec3 v{0, 0, 0};
    if (r >= 0 && r < 4) v = {m.m[r * 4], m.m[r * 4 + 1], m.m[r * 4 + 2]};
    return v;
}
SVec3 M4GetTranslation(const SMat4& m) { return M4Row3(m, 3); }
SVec3 M4GetX(const SMat4& m) { return M4Row3(m, 0); }
SVec3 M4GetY(const SMat4& m) { return M4Row3(m, 1); }
SVec3 M4GetZ(const SMat4& m) { return M4Row3(m, 2); }
void M4SetTranslation(SMat4& m, const SVec3& v) {
    m.m[12] = v.x;
    m.m[13] = v.y;
    m.m[14] = v.z;
}   // only row 3, the rest stays
void M4ToTranslation(SMat4& m, const SVec3& v) { g_sb.m4Translation(&m, &v); }
void M4ToScaling(SMat4& m, const SVec3& v) { g_sb.m4Scaling(&m, &v); }
void M4ToRotation(SMat4& m, const SQuat& q) { g_sb.m4FromQuat(&m, &q); }
void M4RotX(SMat4& m, float a) { g_sb.m4RotX(&m, a); }
void M4RotY(SMat4& m, float a) { g_sb.m4RotY(&m, a); }
void M4RotZ(SMat4& m, float a) { g_sb.m4RotZ(&m, a); }
SMat3 M4RotationPart(const SMat4& m) {
    SMat3 r;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) r.m[i * 3 + j] = m.m[i * 4 + j];
    return r;
}
SQuat M4Quaternion(const SMat4& m) {
    SQuat q{0, 0, 0, 1};
    const SMat3 r = M4RotationPart(m);
    g_sb.quatFromMatrix3(&q, &r);
    return q;
}

// ---------------------------------------------------------------- global functions
bool g_printToGameLog = true;   // ini [g3asm] printgame=0: print() writes only to the modkit log, not also to the log of the game (geInfo)
void Print(const SStr& s) {
    if (ScriptHost::s_current) ScriptHost::s_current->Log(std::string("[") + ScriptHost::s_current->CurrentMod() + "] " + s.c_str());
    if (g_printToGameLog && g_sb.geInfo) g_sb.geInfo("g3asm-Skript: %s", s.c_str());
}
SStr ToStringInt(int v) {
    char b[32];
    std::snprintf(b, sizeof b, "%d", v);
    return SStr(b);
}
SStr ToStringUInt(unsigned v) {
    char b[32];
    std::snprintf(b, sizeof b, "%u", v);
    return SStr(b);
}
SStr ToStringFloat(float v) {
    char b[64];
    std::snprintf(b, sizeof b, "%g", v);
    return SStr(b);
}
SStr ToStringDouble(double v) {
    char b[64];
    std::snprintf(b, sizeof b, "%g", v);
    return SStr(b);
}
SStr ToStringBool(bool v) { return SStr(v ? "true" : "false"); }
bool SuperCall() { return ScriptHost::s_current && ScriptHost::s_current->ChainSuper(); }
bool SetOverrideArgScript(int index, unsigned value) {
    return ScriptHost::s_current && ScriptHost::s_current->SetOverrideArg(index, value);
}
unsigned GetOverrideArgScript(int index) {
    unsigned v = 0;
    if (ScriptHost::s_current) ScriptHost::s_current->GetOverrideArg(index, &v);
    return v;
}
void Check(bool ok, const SStr& msg) {
    if (ScriptHost::s_current) ScriptHost::s_current->Check(ok, msg.c_str());
}
float GetFrameTime() { return ScriptHost::s_current && ScriptHost::s_current->frameTime ? ScriptHost::s_current->frameTime() : 0.0f; }
unsigned GetTimeStamp() {
    typedef unsigned long(__stdcall * Fn)();
    static Fn f = g_sb.module ? reinterpret_cast<Fn>(GetProcAddress(g_sb.module, "?GetTimeStamp@bCTimer@@SGKXZ")) : nullptr;
    return f ? unsigned(f()) : GetTickCount();
}
int GetApiVersion() { return G3ASM_SCRIPT_API_VERSION; }
bool IsModActive(const SStr& name);   // mod dependencies, below at DiscoverMods
SStr CallModScript(const SStr& mod, const SStr& func, const SStr& arg) {
    SStr out;
    if (ScriptHost::s_current) ScriptHost::s_current->CallModText(mod.c_str(), func.c_str(), &arg, &out);
    return out;
}
bool ModFunctionExistsScript(const SStr& mod, const SStr& func) {
    return ScriptHost::s_current && ScriptHost::s_current->HasModTextFunction(mod.c_str(), func.c_str());
}
int SendModMessageScript(const SStr& topic, const SStr& data) { return ScriptHost::s_current ? ScriptHost::s_current->SendModMessage(&topic, &data) : 0; }
SStr GetModVersion(const SStr& name);

// window, input, sound (Windows): the game window = largest visible main window of this process
HWND FindGameWindow() {
    static HWND cached = nullptr;
    if (cached && IsWindow(cached)) return cached;
    struct Ctx {
        HWND best;
        long area;
    } c{nullptr, 0};
    EnumWindows(
        [](HWND h, LPARAM l) -> BOOL {
            DWORD pid = 0;
            GetWindowThreadProcessId(h, &pid);
            if (pid != GetCurrentProcessId() || !IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
            RECT r;
            GetWindowRect(h, &r);
            Ctx* c = reinterpret_cast<Ctx*>(l);
            const long a = (r.right - r.left) * (r.bottom - r.top);
            if (a > c->area) {
                c->area = a;
                c->best = h;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&c));
    cached = c.best;
    return cached;
}
void SetWindowTitle(const SStr& s) {
    if (HWND h = FindGameWindow()) SetWindowTextA(h, s.c_str());
}
bool IsKeyDown(int vk) {
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (pid != GetCurrentProcessId()) return false;   // only if the game is in the foreground
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}
void PlaySystemSound() { MessageBeep(MB_ICONASTERISK); }
const int kKeyF1 = VK_F1, kKeyF2 = VK_F2, kKeyF3 = VK_F3, kKeyF4 = VK_F4, kKeyF5 = VK_F5, kKeyF6 = VK_F6, kKeyF7 = VK_F7, kKeyF8 = VK_F8,
          kKeyF9 = VK_F9, kKeyF10 = VK_F10, kKeyF11 = VK_F11, kKeyF12 = VK_F12;

void MessageCallback(const asSMessageInfo* msg, void*) {
    const char* type = msg->type == asMSGTYPE_ERROR ? "FEHLER" : msg->type == asMSGTYPE_WARNING ? "Warnung" : "Info";
    char b[1024];
    std::snprintf(b, sizeof b, "%s (%d, %d): %s: %s", msg->section, msg->row, msg->col, type, msg->message);
    if (ScriptHost::s_current) ScriptHost::s_current->Log(b);
}

// protection against endless loops: a call can run for a maximum of 5 s
struct Timeout {
    unsigned startTick;
};
void LineCallback(asIScriptContext* ctx, Timeout* t) {
    if (GetTickCount() - t->startTick > 5000) ctx->Abort();
}

// ---- string and dictionary (API 47): conversion in both directions between bCString and string (std::string, add-on scriptstdstring)
std::string StrToStd(const SStr& s) { return std::string(s.c_str()); }
SStr StdToStr(const std::string& s) { return SStr(s); }
void StdConstructFromStr(const SStr& s, std::string* self) { new (self) std::string(s.c_str()); }
void StrConstructFromStd(const std::string& s, SStr* self) { new (self) SStr(s); }
bool RegisterStringConversions(asIScriptEngine* e) {
    bool ok = true;
    G3ASM_REG(e->RegisterObjectMethod("bCString", "string opImplConv() const", asFUNCTION(StrToStd), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("string", "bCString opImplConv() const", asFUNCTION(StdToStr), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectBehaviour("string", asBEHAVE_CONSTRUCT, "void f(const bCString &in)", asFUNCTION(StdConstructFromStr),
                                       asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectBehaviour("bCString", asBEHAVE_CONSTRUCT, "void f(const string &in)", asFUNCTION(StrConstructFromStd),
                                       asCALL_CDECL_OBJLAST));
    return ok;
}

// ---- coroutines (API 47): the work is in ScriptHost (StartCoroutine ... TickCoroutines)
int StartCoroutineScript(asIScriptFunction* f) { return ScriptHost::s_current ? ScriptHost::s_current->StartCoroutine(f, nullptr) : -1; }
int StartCoroutineArgsScript(asIScriptFunction* f, CScriptDictionary* args) {
    return ScriptHost::s_current ? ScriptHost::s_current->StartCoroutine(f, args) : -1;
}
void WaitScript(float seconds) {
    if (ScriptHost::s_current) ScriptHost::s_current->WaitCoroutine(seconds);
}
void YieldScript() {
    if (ScriptHost::s_current) ScriptHost::s_current->WaitCoroutine(0.0f);
}
bool StopCoroutineScript(int id) { return ScriptHost::s_current && ScriptHost::s_current->StopCoroutine(id); }
int StopAllCoroutinesScript() { return ScriptHost::s_current ? ScriptHost::s_current->StopAllCoroutines() : 0; }
bool IsCoroutineRunningScript(int id) { return ScriptHost::s_current && ScriptHost::s_current->IsCoroutineRunning(id); }
bool RegisterCoroutines(asIScriptEngine* e) {   // after dictionary
    bool ok = true;
    G3ASM_REG(e->RegisterFuncdef("void Coroutine()"));
    G3ASM_REG(e->RegisterFuncdef("void CoroutineArgs(dictionary@ args)"));
    // @+: AngelScript handles the reference counters of the handles (the function only uses them during the call)
    G3ASM_REG(e->RegisterGlobalFunction("int StartCoroutine(Coroutine@+ f)", asFUNCTION(StartCoroutineScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("int StartCoroutine(CoroutineArgs@+ f, dictionary@+ args)", asFUNCTION(StartCoroutineArgsScript),
                                        asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("void Wait(float seconds)", asFUNCTION(WaitScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("void Yield()", asFUNCTION(YieldScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool StopCoroutine(int id)", asFUNCTION(StopCoroutineScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("int StopAllCoroutines()", asFUNCTION(StopAllCoroutinesScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool IsCoroutineRunning(int id)", asFUNCTION(IsCoroutineRunningScript), asCALL_CDECL));
    return ok;
}

bool RegisterAll(asIScriptEngine* e) {
    bool ok = true;
    // bCString
    G3ASM_REG(e->RegisterObjectType("bCString", sizeof(SStr), asOBJ_VALUE | asGetTypeTraits<SStr>()));
    G3ASM_REG(e->RegisterStringFactory("bCString", &g_strFactory));
    G3ASM_REG(e->RegisterObjectBehaviour("bCString", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(StrConstruct), asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectBehaviour("bCString", asBEHAVE_CONSTRUCT, "void f(const bCString &in)", asFUNCTION(StrCopyConstruct),
                                       asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectBehaviour("bCString", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(StrDestruct), asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bCString &opAssign(const bCString &in)", asMETHODPR(SStr, operator=, (const SStr&), SStr&),
                                    asCALL_THISCALL));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bCString &opAddAssign(const bCString &in)",
                                    asMETHODPR(SStr, operator+=, (const SStr&), SStr&), asCALL_THISCALL));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bCString opAdd(const bCString &in) const", asFUNCTION(StrAdd), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bCString opAdd(int) const", asFUNCTION(StrAddInt), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bCString opAdd(uint) const", asFUNCTION(StrAddUInt), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bCString opAdd(float) const", asFUNCTION(StrAddFloat), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bCString opAdd(double) const", asFUNCTION(StrAddDouble), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bCString opAdd(bool) const", asFUNCTION(StrAddBool), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bCString opAdd_r(int) const", asFUNCTION(StrAddRInt), asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bCString opAdd_r(float) const", asFUNCTION(StrAddRFloat), asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bool opEquals(const bCString &in) const", asFUNCTION(StrEquals), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "int opCmp(const bCString &in) const", asFUNCTION(StrCompare), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "int GetLength() const", asFUNCTION(StrLength), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bool IsEmpty() const", asFUNCTION(StrIsEmpty), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "int Compare(const bCString &in) const", asFUNCTION(StrCompare), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "int CompareNoCase(const bCString &in) const", asFUNCTION(StrCompareNoCase),
                                    asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "int Find(const bCString &in, int start = 0) const", asFUNCTION(StrFind),
                                    asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bool Contains(const bCString &in) const", asFUNCTION(StrContains), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bCString Left(int) const", asFUNCTION(StrLeft), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bCString Right(int) const", asFUNCTION(StrRight), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bCString Mid(int, int) const", asFUNCTION(StrMid), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "void ToUpper()", asFUNCTION(StrToUpper), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "void ToLower()", asFUNCTION(StrToLower), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bCString GetUpper() const", asFUNCTION(StrUpper), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCString", "bCString GetLower() const", asFUNCTION(StrLower), asCALL_CDECL_OBJFIRST));

    // bCVector
    G3ASM_REG(
        e->RegisterObjectType("bCVector", sizeof(SVec3), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<SVec3>()));
    G3ASM_REG(e->RegisterObjectProperty("bCVector", "float x", asOFFSET(SVec3, x)));
    G3ASM_REG(e->RegisterObjectProperty("bCVector", "float y", asOFFSET(SVec3, y)));
    G3ASM_REG(e->RegisterObjectProperty("bCVector", "float z", asOFFSET(SVec3, z)));
    G3ASM_REG(e->RegisterObjectBehaviour("bCVector", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(V3Construct), asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectBehaviour("bCVector", asBEHAVE_CONSTRUCT, "void f(float, float, float)", asFUNCTION(V3ConstructXYZ),
                                       asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector", "float GetMagnitude() const", asFUNCTION(V3Mag), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector", "float GetSquareMagnitude() const", asFUNCTION(V3SqMag), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector", "void Normalize()", asFUNCTION(V3Normalize), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector", "bCVector GetNormalized() const", asFUNCTION(V3GetNormalized), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector", "float GetDotProduct(const bCVector &in) const", asFUNCTION(V3Dot), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector", "bCVector GetCrossProduct(const bCVector &in) const", asFUNCTION(V3Cross),
                                    asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector", "bCVector opAdd(const bCVector &in) const", asFUNCTION(V3Add), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector", "bCVector opSub(const bCVector &in) const", asFUNCTION(V3Sub), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector", "bCVector opMul(float) const", asFUNCTION(V3Mul), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector", "bCVector opMul_r(float) const", asFUNCTION(V3Mul), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector", "bCVector opDiv(float) const", asFUNCTION(V3Div), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector", "bCVector opNeg() const", asFUNCTION(V3Neg), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector", "bool opEquals(const bCVector &in) const", asFUNCTION(V3Equals), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector", "bCString ToString() const", asFUNCTION(V3ToString), asCALL_CDECL_OBJFIRST));

    // bCVector2 / bCVector4
    G3ASM_REG(
        e->RegisterObjectType("bCVector2", sizeof(SVec2), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<SVec2>()));
    G3ASM_REG(e->RegisterObjectProperty("bCVector2", "float x", asOFFSET(SVec2, x)));
    G3ASM_REG(e->RegisterObjectProperty("bCVector2", "float y", asOFFSET(SVec2, y)));
    G3ASM_REG(e->RegisterObjectBehaviour("bCVector2", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(V2Construct), asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectBehaviour("bCVector2", asBEHAVE_CONSTRUCT, "void f(float, float)", asFUNCTION(V2ConstructXY),
                                       asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector2", "float GetMagnitude() const", asFUNCTION(V2Mag), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector2", "void Normalize()", asFUNCTION(V2Normalize), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector2", "bCVector2 opAdd(const bCVector2 &in) const", asFUNCTION(V2Add), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector2", "bCVector2 opSub(const bCVector2 &in) const", asFUNCTION(V2Sub), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector2", "bCVector2 opMul(float) const", asFUNCTION(V2Mul), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(
        e->RegisterObjectType("bCVector4", sizeof(SVec4), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<SVec4>()));
    G3ASM_REG(e->RegisterObjectProperty("bCVector4", "float x", asOFFSET(SVec4, x)));
    G3ASM_REG(e->RegisterObjectProperty("bCVector4", "float y", asOFFSET(SVec4, y)));
    G3ASM_REG(e->RegisterObjectProperty("bCVector4", "float z", asOFFSET(SVec4, z)));
    G3ASM_REG(e->RegisterObjectProperty("bCVector4", "float w", asOFFSET(SVec4, w)));
    G3ASM_REG(e->RegisterObjectBehaviour("bCVector4", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(V4Construct), asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectBehaviour("bCVector4", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)", asFUNCTION(V4ConstructXYZW),
                                       asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector4", "float GetMagnitude() const", asFUNCTION(V4Mag), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCVector4", "void Normalize()", asFUNCTION(V4Normalize), asCALL_CDECL_OBJFIRST));

    // bCQuaternion / bCMatrix3 (declare the matrix first, the two refer to each other)
    G3ASM_REG(e->RegisterObjectType("bCQuaternion", sizeof(SQuat),
                                  asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<SQuat>()));
    G3ASM_REG(
        e->RegisterObjectType("bCMatrix3", sizeof(SMat3), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<SMat3>()));
    G3ASM_REG(e->RegisterObjectProperty("bCQuaternion", "float x", asOFFSET(SQuat, x)));
    G3ASM_REG(e->RegisterObjectProperty("bCQuaternion", "float y", asOFFSET(SQuat, y)));
    G3ASM_REG(e->RegisterObjectProperty("bCQuaternion", "float z", asOFFSET(SQuat, z)));
    G3ASM_REG(e->RegisterObjectProperty("bCQuaternion", "float w", asOFFSET(SQuat, w)));
    G3ASM_REG(e->RegisterObjectBehaviour("bCQuaternion", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(QConstruct), asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectBehaviour("bCQuaternion", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)", asFUNCTION(QConstructXYZW),
                                       asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectMethod("bCQuaternion", "void Clear()", asFUNCTION(QClear), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCQuaternion", "void Normalize()", asFUNCTION(QNormalize), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCQuaternion", "void Invert()", asFUNCTION(QInvert), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCQuaternion", "float GetMagnitude() const", asFUNCTION(QMag), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCQuaternion", "bCQuaternion opMul(const bCQuaternion &in) const", asFUNCTION(QMul),
                                    asCALL_CDECL_OBJFIRST));
    G3ASM_REG(
        e->RegisterObjectMethod("bCQuaternion", "void SetQuaternion(const bCMatrix3 &in)", asFUNCTION(QFromMatrix), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectBehaviour("bCMatrix3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(M3Construct), asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix3", "float GetElement(int row, int col) const", asFUNCTION(M3Get), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix3", "void SetElement(int row, int col, float)", asFUNCTION(M3Set), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix3", "void SetIdentity()", asFUNCTION(M3Identity), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix3", "void Transpose()", asFUNCTION(M3Transpose), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix3", "void Invert()", asFUNCTION(M3Invert), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix3", "bCMatrix3 opMul(const bCMatrix3 &in) const", asFUNCTION(M3MulM), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix3", "bCVector opMul(const bCVector &in) const", asFUNCTION(M3MulV), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix3", "void SetToRotationAxisX(float)", asFUNCTION(M3RotX), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix3", "void SetToRotationAxisY(float)", asFUNCTION(M3RotY), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix3", "void SetToRotationAxisZ(float)", asFUNCTION(M3RotZ), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(
        e->RegisterObjectMethod("bCMatrix3", "void SetToRotation(const bCQuaternion &in)", asFUNCTION(M3FromQuat), asCALL_CDECL_OBJFIRST));
    // bCMatrix4 (API 19)
    G3ASM_REG(
        e->RegisterObjectType("bCMatrix4", sizeof(SMat4), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<SMat4>()));
    G3ASM_REG(e->RegisterObjectBehaviour("bCMatrix4", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(M4Construct), asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "float GetElement(int row, int col) const", asFUNCTION(M4Get), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "void SetElement(int row, int col, float)", asFUNCTION(M4Set), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "void SetIdentity()", asFUNCTION(M4Identity), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "void Transpose()", asFUNCTION(M4Transpose), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "void Invert()", asFUNCTION(M4Invert), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "bCMatrix4 GetInverted() const", asFUNCTION(M4GetInverted), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "float GetDeterminant() const", asFUNCTION(M4Det), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "bCMatrix4 opMul(const bCMatrix4 &in) const", asFUNCTION(M4MulM), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "bCVector opMul(const bCVector &in) const", asFUNCTION(M4MulV), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "bCVector GetTranslation() const", asFUNCTION(M4GetTranslation), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "void SetTranslation(const bCVector &in)", asFUNCTION(M4SetTranslation),
                                    asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "bCVector GetXAxis() const", asFUNCTION(M4GetX), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "bCVector GetYAxis() const", asFUNCTION(M4GetY), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "bCVector GetZAxis() const", asFUNCTION(M4GetZ), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "void SetToTranslation(const bCVector &in)", asFUNCTION(M4ToTranslation),
                                    asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "void SetToScaling(const bCVector &in)", asFUNCTION(M4ToScaling), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "void SetToRotation(const bCQuaternion &in)", asFUNCTION(M4ToRotation),
                                    asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "void SetToRotationAxisX(float)", asFUNCTION(M4RotX), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "void SetToRotationAxisY(float)", asFUNCTION(M4RotY), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "void SetToRotationAxisZ(float)", asFUNCTION(M4RotZ), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "bCMatrix3 GetRotationMatrix() const", asFUNCTION(M4RotationPart), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("bCMatrix4", "bCQuaternion GetRotation() const", asFUNCTION(M4Quaternion), asCALL_CDECL_OBJFIRST));

    // globale Funktionen
    G3ASM_REG(e->RegisterGlobalFunction("void print(const bCString &in)", asFUNCTION(Print), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool Super()", asFUNCTION(SuperCall), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool SetOverrideArg(int index, uint value)", asFUNCTION(SetOverrideArgScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("uint GetOverrideArg(int index)", asFUNCTION(GetOverrideArgScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bCString ToString(int)", asFUNCTION(ToStringInt), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bCString ToString(uint)", asFUNCTION(ToStringUInt), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bCString ToString(float)", asFUNCTION(ToStringFloat), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bCString ToString(double)", asFUNCTION(ToStringDouble), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bCString ToString(bool)", asFUNCTION(ToStringBool), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("void check(bool, const bCString &in)", asFUNCTION(Check), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("float GetFrameTime()", asFUNCTION(GetFrameTime), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("uint GetTimeStamp()", asFUNCTION(GetTimeStamp), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("int GetApiVersion()", asFUNCTION(GetApiVersion), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool IsModActive(const bCString &in)", asFUNCTION(IsModActive), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bCString GetModVersion(const bCString &in)", asFUNCTION(GetModVersion), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bCString CallMod(const bCString &in mod, const bCString &in func, const bCString &in arg)", asFUNCTION(CallModScript),
                                      asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool ModFunctionExists(const bCString &in mod, const bCString &in func)", asFUNCTION(ModFunctionExistsScript),
                                      asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("int SendModMessage(const bCString &in topic, const bCString &in data)", asFUNCTION(SendModMessageScript),
                                      asCALL_CDECL));
    if (!RegisterModSettingsBindings(e)) ok = false;
    G3ASM_REG(e->RegisterGlobalFunction("void SetWindowTitle(const bCString &in)", asFUNCTION(SetWindowTitle), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool IsKeyDown(int)", asFUNCTION(IsKeyDown), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("void PlaySystemSound()", asFUNCTION(PlaySystemSound), asCALL_CDECL));
    static const int* keys[] = {&kKeyF1, &kKeyF2, &kKeyF3, &kKeyF4,  &kKeyF5,  &kKeyF6,
                                &kKeyF7, &kKeyF8, &kKeyF9, &kKeyF10, &kKeyF11, &kKeyF12};
    for (int i = 0; i < 12; ++i) {
        char d[32];
        std::snprintf(d, sizeof d, "const int KEY_F%d", i + 1);
        G3ASM_REG(e->RegisterGlobalProperty(d, const_cast<int*>(keys[i])));
    }
    return ok;
}

unsigned long long FileTime(const std::string& p) {
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExA(p.c_str(), GetFileExInfoStandard, &a)) return 0;
    return (static_cast<unsigned long long>(a.ftLastWriteTime.dwHighDateTime) << 32) | a.ftLastWriteTime.dwLowDateTime;
}

}   // namespace

void SetPrintToGameLog(bool on) { g_printToGameLog = on; }

HWND GameWindow() { return FindGameWindow(); }

void NoteNull(const char* where) {
    static std::mutex mu;
    static const char* seen[64] = {};
    static int count = 0;
    {
        std::lock_guard<std::mutex> lock(mu);
        for (int i = 0; i < count; ++i)
            if (seen[i] == where) return;
        if (count < 64) seen[count++] = where;
    }
    if (ScriptHost::s_current) ScriptHost::s_current->Log(std::string("Hook: unerwarteter Nullzeiger (nur einmal gemeldet): ") + where);
}

const char* SafeStrText(const void* s, const char* fallback) {
    if (!s || !ReadableMem(s, sizeof(void*))) return fallback;
    __try {
        const char* t = g_sb.strGetText(s);
        return t ? t : fallback;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return fallback; }
}

bool ReadableMem(const void* p, size_t n) {
    const uintptr_t start = reinterpret_cast<uintptr_t>(p);
    if (start < 0x10000 || n == 0) return false;   // null pointer area: numbers, flags, counters - no pointers
    uintptr_t addr = start;
    const uintptr_t end = start + n;
    if (end < start) return false;
    while (addr < end) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(reinterpret_cast<const void*>(addr), &mbi, sizeof mbi) != sizeof mbi) return false;
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        const DWORD prot = mbi.Protect & 0xFF;
        if (prot != PAGE_READONLY && prot != PAGE_READWRITE && prot != PAGE_WRITECOPY && prot != PAGE_EXECUTE_READ &&
            prot != PAGE_EXECUTE_READWRITE && prot != PAGE_EXECUTE_WRITECOPY)
            return false;
        addr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return true;
}

bool ScriptHost::Init(LogFn log) {
    log_ = std::move(log);
    s_current = this;
    engine_ = asCreateScriptEngine();
    if (!engine_) {
        Log("AngelScript: Engine konnte nicht erzeugt werden");
        return false;
    }
    engine_->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);
    engine_->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, false);
    RegisterScriptArray(engine_, true);
    RegisterScriptMath(engine_);
    // string BEFORE bCString: the engine uses the string factory that was registered last, so string literals stay bCString
    RegisterStdString(engine_);
    RegisterStdStringUtils(engine_);
    const bool ok = RegisterAll(engine_) && RegisterStringConversions(engine_) && RegisterEngineBindings(engine_);
    RegisterScriptDictionary(engine_);   // needs array and string
    const bool coOk = RegisterCoroutines(engine_);
    ctx_ = engine_->CreateContext();
    char b[160];
    std::snprintf(b, sizeof b, "AngelScript %s bereit (Skript-Schnittstelle Version %d)%s", asGetLibraryVersion(), G3ASM_SCRIPT_API_VERSION,
                  ok && coOk ? "" : " - mit Registrierungsfehlern");
    Log(b);
    return ok && coOk;
}

void ForgetKeptGlobals(const std::string& mod);   // below, at Unload

void ScriptHost::Shutdown() {
    for (Mod& m : mods_) Unload(m);
    mods_.clear();
    StopCoroutinesOf(std::string());   // the rest (for example from the dev pipe), before the engine goes
    ForgetKeptGlobals(std::string());
    devModule_ = nullptr;   // the engine releases the module
    if (ctx_) {
        ctx_->Release();
        ctx_ = nullptr;
    }
    if (engine_) {
        engine_->ShutDownAndRelease();
        engine_ = nullptr;
    }
}

ScriptHost::Mod* ScriptHost::Find(const std::string& name) {
    for (Mod& m : mods_)
        if (m.name == name) return &m;
    return nullptr;
}

bool ScriptHost::Loaded() const {
    for (const Mod& m : mods_)
        if (m.module) return true;
    return false;
}

// kept globals for each mod name until the next successful Build (also across a build with compile errors)
std::map<std::string, SavedGlobals*> g_keptGlobals;
void ForgetKeptGlobals(const std::string& mod) {   // empty = all
    for (auto it = g_keptGlobals.begin(); it != g_keptGlobals.end();)
        if (mod.empty() || it->first == mod) {
            FreeSavedGlobals(it->second);
            it = g_keptGlobals.erase(it);
        } else {
            ++it;
        }
}
// mod.ini [mod] keepglobals=0 turns it off for a mod (default: on)
bool KeepGlobalsWanted(const std::string& scriptPath) {
    const size_t s = scriptPath.find_last_of("\\/");
    if (s == std::string::npos) return true;
    const std::map<std::string, std::string> ini = ReadIniSection(scriptPath.substr(0, s) + "\\mod.ini", "mod");
    const auto it = ini.find("keepglobals");
    return it == ini.end() || it->second != "0";
}

void ScriptHost::Unload(Mod& m, bool keepGlobals) {
    if (!m.module) return;
    if (asIScriptFunction* f = m.module->GetFunctionByDecl("void OnShutdown()")) {
        curMod_ = m.name;
        Execute(f, nullptr);
    }
    if (keepGlobals) {   // after OnShutdown: what the mod resets there is not kept
        if (SavedGlobals* g = SaveGlobals(m.module)) {
            ForgetKeptGlobals(m.name);
            g_keptGlobals[m.name] = g;
        } else {
            Log("[" + m.name + "] Hot-Reload: globale Variablen konnten nicht gesichert werden");
        }
    }
    StopCoroutinesOf(m.name);   // before Discard: the contexts refer to functions of the module
    m.module->Discard();
    m.module = nullptr;
    ModSettingsForget(m.name);
    m.fMain = m.fFrame = m.fKey = nullptr;
    m.fnCache.clear();
    m.mainDone = false;
}

bool ScriptHost::Build(Mod& m) {
    Unload(m, m.module && KeepGlobalsWanted(m.path));
    m.mtime = FileTime(m.path);   // keep it also for errors: try again only at a new change
    if (!m.mtime) {
        Log("[" + m.name + "] Skript nicht gefunden: " + m.path);
        return false;
    }
    const std::string modName = "mod_" + m.name;
    CScriptBuilder builder;
    if (builder.StartNewModule(engine_, modName.c_str()) < 0) {
        Log("[" + m.name + "] Modul konnte nicht angelegt werden");
        return false;
    }
    if (builder.AddSectionFromFile(m.path.c_str()) < 0) {
        Log("[" + m.name + "] Skript nicht lesbar: " + m.path);
        engine_->DiscardModule(modName.c_str());
        return false;
    }
    if (builder.BuildModule() < 0) {
        Log("[" + m.name + "] Skript enthaelt Fehler, nicht geladen: " + m.path);
        engine_->DiscardModule(modName.c_str());
        return false;
    }
    m.module = engine_->GetModule(modName.c_str());
    const auto kept = g_keptGlobals.find(m.name);
    if (kept != g_keptGlobals.end()) {   // hot reload: the globals get their values from before (main() runs after this)
        const int n = RestoreGlobals(kept->second, m.module);
        Log(n >= 0 ? "[" + m.name + "] Hot-Reload: " + std::to_string(n) + " globale Variablen uebernommen"
                   : "[" + m.name + "] Hot-Reload: Fehler beim Uebernehmen der globalen Variablen");
        ForgetKeptGlobals(m.name);
    }
    m.fMain = m.module->GetFunctionByDecl("void main()");
    m.fFrame = m.module->GetFunctionByDecl("void OnFrame(float)");
    m.fKey = m.module->GetFunctionByDecl("void OnKey(int, bool)");
    Log("[" + m.name + "] Skript geladen: " + m.path);
    return true;
}

bool ScriptHost::AddScript(const std::string& name, const std::string& path) {
    Mod* m = Find(name);
    if (!m) {
        mods_.push_back(Mod());
        m = &mods_.back();
        m->name = name;
    }
    m->path = path;
    return Build(*m);
}

bool ScriptHost::Load(const std::string& path) {
    for (Mod& m : mods_) Unload(m);
    mods_.clear();
    ForgetKeptGlobals(std::string());
    return AddScript("main", path);
}

// Protect the floating-point state of the engine (found 2026-11-06): Genome calculates with the x87 control word 007F (24-bit precision, bCFPUAdmin). At the first script call
// that jumps into C++, something in the modkit resets the x87 unit to the Windows default 037F (64 bit). Without protection, the engine then calculated for the full session
// with an incorrect precision: building textures stayed blurred after a load. Thus each script call restores the x87 control word and MXCSR after it.
namespace {
unsigned short FpuControlWord() {
    unsigned short cw = 0;
    __asm fnstcw cw
    return cw;
}
void FpuLoadControlWord(unsigned short cw) { __asm fldcw cw }
unsigned FpuMxcsr() {
    unsigned v = 0;
    __asm stmxcsr v
    return v;
}
void FpuLoadMxcsr(unsigned v) { __asm ldmxcsr v }
struct FpuRestore {
    ScriptHost* host;
    const asIScriptFunction* fn;
    unsigned short cw;
    unsigned mx;
    FpuRestore(ScriptHost* h, const asIScriptFunction* f) : host(h), fn(f), cw(FpuControlWord()), mx(FpuMxcsr()) {}
    ~FpuRestore() {
        // MXCSR bits 0-5 are flag bits for exceptions that normal calculations set (for example "inexact"): compare and reset only the control bits
        const unsigned kFlags = 0x3F;
        const unsigned short cw2 = FpuControlWord();
        const unsigned mx2 = FpuMxcsr();
        const bool cwChanged = cw2 != cw, mxChanged = (mx2 & ~kFlags) != (mx & ~kFlags);
        if (!cwChanged && !mxChanged) return;
        if (cwChanged) FpuLoadControlWord(cw);
        if (mxChanged) FpuLoadMxcsr((mx & ~kFlags) | (mx2 & kFlags));
        static int reported = 0;
        if (reported < 8 && host) {
            ++reported;
            char b[300];
            std::snprintf(b, sizeof b, "Gleitkomma-Zustand nach %s wiederhergestellt (x87 %04X -> %04X, MXCSR %08X -> %08X)", fn ? fn->GetDeclaration() : "?", cw, cw2, mx,
                          mx2);
            host->Log(b);
        }
    }
};
}   // namespace

bool ScriptHost::Execute(void* fp, float* argFloat, const int* argInt, const bool* argBool, const EvArg* ev, int nev, bool* retBool, void* retText) {
    asIScriptFunction* f = static_cast<asIScriptFunction*>(fp);
    FpuRestore fpu(this, f);
    const bool nested = ctx_->GetState() == asEXECUTION_ACTIVE;   // an event fires while a script runs
    auto failOnce = [&](const char* what, int code) {   // if not, silent failures: report 1 time for each function
        static std::set<const void*> seen;
        if (seen.insert(f).second) Log(std::string(what) + " fehlgeschlagen (" + std::to_string(code) + ") fuer " + f->GetDeclaration() + " in [" + curMod_ + "]");
        return false;
    };
    if (nested) {
        const int rp = ctx_->PushState();
        if (rp < 0) return failOnce("Verschachtelter Skriptaufruf (PushState)", rp);
    }
    struct Pop {
        asIScriptContext* c;
        bool on;
        ~Pop() {
            if (on) c->PopState();
        }
    } pop{ctx_, nested};
    const int rprep = ctx_->Prepare(f);
    if (rprep < 0) return failOnce("Skriptaufruf vorbereiten (Prepare)", rprep);
    if (argFloat) ctx_->SetArgFloat(0, *argFloat);
    if (argInt) ctx_->SetArgDWord(0, static_cast<asDWORD>(*argInt));
    if (argBool) ctx_->SetArgByte(1, *argBool ? 1 : 0);
    for (int i = 0; i < nev; ++i) {
        if (ev[i].kind == EvArg::Entity) {
            struct {
                void* p;
            } e{const_cast<void*>(ev[i].ptr)};
            ctx_->SetArgObject(i, &e);
        } else if (ev[i].kind == EvArg::Text) {
            // const bCString &in must never be null (the script crashed at the first access): then an empty text
            static SStr* empty = nullptr;
            const void* s = ev[i].ptr;
            if (!s) {
                if (!empty) empty = new SStr("");
                s = empty;
                NoteNull("Text-Parameter eines Ereignisses (leer weitergegeben)");
            }
            ctx_->SetArgObject(i, const_cast<void*>(s));
        } else if (ev[i].kind == EvArg::Bool)
            ctx_->SetArgByte(i, ev[i].value ? 1 : 0);
        else
            ctx_->SetArgDWord(i, ev[i].value);
    }
    // The time guard belongs to the context, not to the saved state: a nested call leaves the guard of the outer call
    // (its 5 s continue). Previously, the inner call deleted it, and the outer script then ran without loop protection.
    Timeout t{GetTickCount()};
    if (!nested) ctx_->SetLineCallback(asFUNCTION(LineCallback), &t, asCALL_CDECL);
    const int r = ctx_->Execute();
    if (!nested) ctx_->ClearLineCallback();
    if (r == asEXECUTION_FINISHED) {
        if (retBool) *retBool = ctx_->GetReturnByte() != 0;
        if (retText)
            if (void* o = ctx_->GetReturnObject()) *static_cast<SStr*>(retText) = *static_cast<const SStr*>(o);
        return true;
    }
    char b[1024];
    if (r == asEXECUTION_EXCEPTION) {
        const asIScriptFunction* ef = ctx_->GetExceptionFunction();
        std::snprintf(b, sizeof b, "Skript-Ausnahme in %s (Zeile %d): %s", ef ? ef->GetDeclaration() : "?", ctx_->GetExceptionLineNumber(),
                      ctx_->GetExceptionString());
    } else if (r == asEXECUTION_ABORTED) {
        std::snprintf(b, sizeof b, "Skript abgebrochen (laenger als 5 s in %s - Endlosschleife?)", f->GetDeclaration());
    } else {
        std::snprintf(b, sizeof b, "Skript-Ausfuehrung beendet mit Code %d in %s", r, f->GetDeclaration());
    }
    Log(b);
    return false;
}

// Events and overrides can come during a script call (for example HurtHitPoints in a mod -> OnDamage of all mods). The caller then must
// keep its own mod name (log prefix, owner of coroutines, settings): each dispatch restores curMod_ when it ends.
struct KeepCurMod {
    std::string& ref;
    std::string saved;
    explicit KeepCurMod(std::string& r) : ref(r), saved(r) {}
    ~KeepCurMod() { ref = saved; }
    KeepCurMod(const KeepCurMod&) = delete;
    KeepCurMod& operator=(const KeepCurMod&) = delete;
};

void ScriptHost::CallIn(Mod& m, const char* decl) {
    if (!m.module) return;
    if (asIScriptFunction* f = FindFn(m, decl)) {
        const KeepCurMod keep(curMod_);
        curMod_ = m.name;
        Execute(f, nullptr);
    }
}

void ScriptHost::CallMain() {
    for (size_t i = 0; i < mods_.size(); ++i) {
        Mod& m = mods_[i];
        if (!m.module || m.mainDone) continue;
        m.mainDone = true;
        if (m.fMain) {
            curMod_ = m.name;
            Execute(m.fMain, nullptr);
        }
    }
}

asIScriptFunction* ScriptHost::FindFn(Mod& m, const char* decl) {
    if (!m.module) return nullptr;
    auto it = m.fnCache.find(decl);
    if (it != m.fnCache.end()) return it->second;
    asIScriptFunction* f = m.module->GetFunctionByDecl(decl);
    m.fnCache.emplace(decl, f);
    return f;
}

static bool DropForeignThread(ScriptHost* host, const char* decl);   // below, at CallEvent

bool ScriptHost::HasHandler(const char* decl) {
    if (DropForeignThread(this, decl)) return false;   // FindFn writes into the cache of the mod: only in the game thread
    for (Mod& m : mods_)
        if (FindFn(m, decl)) return true;
    return false;
}

bool ScriptHost::CallOverride(const char* decl, const EvArg* args, int n, std::function<bool(const EvArg*)> original, bool* result,
                              bool* originalCalled, EvArg* finalArgs) {
    if (!HasHandler(decl)) return false;   // also from a different thread: the caller then uses its original
    chain_.emplace_back();
    const size_t idx = chain_.size() - 1;
    Chain& c0 = chain_[idx];
    c0.decl = decl;
    c0.n = n > 8 ? 8 : n;
    for (int i = 0; i < c0.n; ++i) c0.args[i] = args[i];
    c0.original = std::move(original);
    bool ret = false;
    bool ok = false;
    const KeepCurMod keep(curMod_);
    for (size_t i = 0; i < mods_.size() && !ok; ++i) {
        asIScriptFunction* f = FindFn(mods_[i], decl);
        if (!f) continue;
        chain_[idx].next = i + 1;
        curMod_ = mods_[i].name;
        ok = Execute(f, nullptr, nullptr, nullptr, chain_[idx].args, chain_[idx].n, &ret);
        if (!ok) break;   // error in the script: fallback below
    }
    if (!ok) {   // script error: the game must get its original
        if (!chain_[idx].originalCalled) {
            chain_[idx].originalCalled = true;
            ret = chain_[idx].original(chain_[idx].args);
        }
    }
    if (result) *result = ret;
    if (originalCalled) *originalCalled = chain_[idx].originalCalled;
    if (finalArgs)
        for (int i = 0; i < chain_[idx].n; ++i) finalArgs[i] = chain_[idx].args[i];
    chain_.pop_back();
    return true;
}

bool ScriptHost::ChainSuper() {
    if (chain_.empty()) return false;
    const size_t idx = chain_.size() - 1;
    const char* decl = chain_[idx].decl;
    for (size_t i = chain_[idx].next; i < mods_.size(); ++i) {
        asIScriptFunction* f = FindFn(mods_[i], decl);
        if (!f) continue;
        chain_[idx].next = i + 1;
        bool ret = false;
        const std::string saved = curMod_;
        curMod_ = mods_[i].name;
        const bool ok = Execute(f, nullptr, nullptr, nullptr, chain_[idx].args, chain_[idx].n, &ret);
        curMod_ = saved;
        return ok ? ret : false;
    }
    chain_[idx].originalCalled = true;   // end of the chain: the original of the game function
    return chain_[idx].original(chain_[idx].args);
}

bool ScriptHost::SetOverrideArg(int index, unsigned value) {
    if (chain_.empty()) {
        Log("SetOverrideArg: nur innerhalb einer Override_...-Funktion");
        return false;
    }
    Chain& c = chain_.back();
    if (index < 0 || index >= c.n) {
        Log(std::string("SetOverrideArg: Argument ") + std::to_string(index) + " gibt es in " + c.decl + " nicht");
        return false;
    }
    if (c.args[index].kind != EvArg::UInt) {
        Log(std::string("SetOverrideArg: Argument ") + std::to_string(index) + " von " + c.decl +
            " ist keine Zahl (nur Zahlen sind aenderbar)");
        return false;
    }
    c.args[index].value = value;
    return true;
}
bool ScriptHost::GetOverrideArg(int index, unsigned* value) const {
    if (chain_.empty() || index < 0 || index >= chain_.back().n || chain_.back().args[index].kind != EvArg::UInt) return false;
    *value = chain_.back().args[index].value;
    return true;
}

// Scripts run only in the game thread: there is one AngelScript context and one override chain, both without a lock. If the game calls a
// hooked function from a different thread, the event is not reported to the scripts (overrides: the game gets its original).
// The log shows the first cases. The code does not check if a mod would have handled the event (the search for it already
// writes into the cache of the mod). Hooks on frequent paths check the thread themselves (OnGameThread) before they come here.
static bool DropForeignThread(ScriptHost* host, const char* decl) {
    if (!OnForeignThread()) return false;
    static volatile long logged = 0;
    if (InterlockedIncrement(&logged) <= 10) host->Log(std::string("Aufruf aus fremdem Thread, nicht an Skripte gemeldet: ") + decl);
    return true;
}

void ScriptHost::CallEvent(const char* decl, const EvArg* args, int n) {
    if (DropForeignThread(this, decl)) return;
    const KeepCurMod keep(curMod_);
    for (size_t i = 0; i < mods_.size(); ++i) {
        Mod& m = mods_[i];
        if (!m.module) continue;
        if (asIScriptFunction* f = FindFn(m, decl)) {
            curMod_ = m.name;
            Execute(f, nullptr, nullptr, nullptr, args, n);
        }
    }
}

void ScriptHost::CallSimple(const char* decl) {
    if (DropForeignThread(this, decl)) return;
    for (size_t i = 0; i < mods_.size(); ++i) CallIn(mods_[i], decl);
}

void ScriptHost::CallFrame(float dt) {
    for (size_t i = 0; i < mods_.size(); ++i)
        if (mods_[i].module && mods_[i].fFrame) {
            curMod_ = mods_[i].name;
            Execute(mods_[i].fFrame, &dt);
        }
}

void ScriptHost::CallKey(int vk, bool down) {
    if (busyKey_) return;   // do not nest messages that come during a script call
    busyKey_ = true;
    for (size_t i = 0; i < mods_.size(); ++i)
        if (mods_[i].module && mods_[i].fKey) {
            curMod_ = mods_[i].name;
            Execute(mods_[i].fKey, nullptr, &vk, &down);
        }
    busyKey_ = false;
}

// ---------------------------------------------------------------- coroutines (from version 47)
namespace {
const int kMaxCoroutinesPerMod = 256;   // protection: StartCoroutine in OnFrame without an end
}

int ScriptHost::StartCoroutine(asIScriptFunction* f, void* args) {
    if (!f || !engine_) return -1;
    int own = 0;
    for (const Coroutine& c : coroutines_)
        if (!c.stop && c.mod == curMod_) ++own;
    if (own >= kMaxCoroutinesPerMod) {
        static std::set<std::string> told;
        if (told.insert(curMod_).second)
            Log("[" + curMod_ + "] StartCoroutine abgelehnt: schon " + std::to_string(kMaxCoroutinesPerMod) + " Koroutinen aktiv");
        return -1;
    }
    asIScriptContext* c = engine_->CreateContext();
    if (!c) return -1;
    if (c->Prepare(f) < 0) {
        c->Release();
        return -1;
    }
    if (args) c->SetArgObject(0, args);   // handle parameter: the context keeps its own reference
    const int id = nextCoId_++;
    // the first part runs at the next tick (also if StartCoroutine itself runs in a coroutine during a tick)
    coroutines_.push_back({id, curMod_, c, coClock_, coTick_, false});
    return id;
}

void ScriptHost::WaitCoroutine(float seconds) {
    asIScriptContext* active = asGetActiveContext();
    for (Coroutine& c : coroutines_)
        if (c.id == runningCo_ && c.ctx == active) {
            c.wakeAt = coClock_ + (seconds > 0.0f ? seconds : 0.0f);
            active->Suspend();   // takes effect when this function returns to the script
            return;
        }
    if (active) active->SetException("Wait() und Yield() gehen nur in einer Koroutine (StartCoroutine)");
}

bool ScriptHost::StopCoroutine(int id) {
    for (Coroutine& c : coroutines_)
        if (c.id == id && !c.stop && c.mod == curMod_) {
            c.stop = true;
            c.ctx->Abort();   // also for the running coroutine itself: it stops at the next possible point
            if (!coTicking_) SweepCoroutines();
            return true;
        }
    return false;
}

int ScriptHost::StopAllCoroutines() {
    int n = 0;
    for (Coroutine& c : coroutines_)
        if (!c.stop && c.mod == curMod_) {
            c.stop = true;
            c.ctx->Abort();
            ++n;
        }
    if (!coTicking_) SweepCoroutines();
    return n;
}

bool ScriptHost::IsCoroutineRunning(int id) const {
    for (const Coroutine& c : coroutines_)
        if (c.id == id) return !c.stop;
    return false;
}

void ScriptHost::SweepCoroutines() {
    for (size_t i = 0; i < coroutines_.size();)
        if (coroutines_[i].stop) {
            coroutines_[i].ctx->Release();
            coroutines_.erase(coroutines_.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
}

void ScriptHost::StopCoroutinesOf(const std::string& mod) {   // empty = all
    for (Coroutine& c : coroutines_)
        if (mod.empty() || c.mod == mod) {
            if (!c.stop) c.ctx->Abort();
            c.stop = true;
        }
    if (!coTicking_) SweepCoroutines();
}

void ScriptHost::TickCoroutines(float dt) {
    if (dt > 0.0f) coClock_ += dt;
    ++coTick_;
    if (coroutines_.empty() || coTicking_) return;
    coTicking_ = true;
    const std::string savedMod = curMod_;
    const size_t n = coroutines_.size();   // coroutines that start now run at the next tick
    for (size_t i = 0; i < n; ++i) {
        if (coroutines_[i].stop || coroutines_[i].lastTick == coTick_ || coroutines_[i].wakeAt > coClock_) continue;
        coroutines_[i].lastTick = coTick_;
        asIScriptContext* c = coroutines_[i].ctx;
        curMod_ = coroutines_[i].mod;
        runningCo_ = coroutines_[i].id;
        int r;
        {
            FpuRestore fpu(this, c->GetFunction());
            Timeout t{GetTickCount()};
            c->SetLineCallback(asFUNCTION(LineCallback), &t, asCALL_CDECL);
            r = c->Execute();
            c->ClearLineCallback();
        }
        runningCo_ = 0;
        Coroutine& co = coroutines_[i];   // the vector can be larger now (StartCoroutine in the coroutine)
        if (r == asEXECUTION_SUSPENDED && !co.stop) continue;
        if (!co.stop) {
            char b[1024];
            const asIScriptFunction* f = c->GetFunction();
            if (r == asEXECUTION_EXCEPTION) {
                const asIScriptFunction* ef = c->GetExceptionFunction();
                std::snprintf(b, sizeof b, "[%s] Skript-Ausnahme in Koroutine %s (Zeile %d): %s", co.mod.c_str(), ef ? ef->GetDeclaration() : "?",
                              c->GetExceptionLineNumber(), c->GetExceptionString());
                Log(b);
            } else if (r == asEXECUTION_ABORTED) {
                std::snprintf(b, sizeof b, "[%s] Koroutine abgebrochen (laenger als 5 s ohne Wait in %s - Endlosschleife?)", co.mod.c_str(),
                              f ? f->GetDeclaration() : "?");
                Log(b);
            } else if (r != asEXECUTION_FINISHED) {
                std::snprintf(b, sizeof b, "[%s] Koroutine beendet mit Code %d in %s", co.mod.c_str(), r, f ? f->GetDeclaration() : "?");
                Log(b);
            }
        }
        co.stop = true;
    }
    curMod_ = savedMod;
    coTicking_ = false;
    SweepCoroutines();
}

// ---------------------------------------------------------------- dev pipe (dev_pipe.cpp)
bool ScriptHost::RunSnippet(const std::string& code, const std::string& mod, std::string* out) {
    if (!engine_ || !ctx_) return false;
    if (ctx_->GetState() == asEXECUTION_ACTIVE) {   // only between script calls, never nested
        if (out) *out += "Skript laeuft gerade - Anfrage nicht ausgefuehrt\n";
        return false;
    }
    asIScriptModule* module = nullptr;
    if (!mod.empty()) {
        Mod* m = Find(mod);
        if (!m || !m->module) {
            if (out) *out += "Mod nicht geladen: " + mod + "\n";
            return false;
        }
        module = m->module;
    } else {
        if (!devModule_) {
            devModule_ = engine_->GetModule("g3asm_dev", asGM_ALWAYS_CREATE);
            if (devModule_) devModule_->Build();   // empty module, so that CompileFunction has a built module
        }
        module = devModule_;
    }
    if (!module) return false;
    struct Restore {   // also on early return: capture off, previous mod name back
        ScriptHost* h;
        std::string savedMod;
        ~Restore() {
            h->capture_ = nullptr;
            h->curMod_ = savedMod;
        }
    } restore{this, curMod_};
    capture_ = out;
    captureThread_ = std::this_thread::get_id();
    curMod_ = mod.empty() ? "dev" : mod;
    asIScriptFunction* f = nullptr;
    // line offset -1: the line "void dev() {" is line 0, the first line of the code is line 1 in the error messages
    const std::string src = "void dev() {\n" + code + "\n}";
    const int r = module->CompileFunction("dev", src.c_str(), -1, 0, &f);
    if (r < 0 || !f) {
        if (f) f->Release();
        return false;
    }
    const bool ok = Execute(f, nullptr);
    f->Release();
    return ok;
}

// ---------------------------------------------------------------- between mods (from version 44)
std::string ScriptHost::ModDir(const std::string& mod) const {
    for (const Mod& m : mods_)
        if (_stricmp(m.name.c_str(), mod.c_str()) == 0) {
            const size_t s = m.path.find_last_of("\\/");
            return s == std::string::npos ? std::string() : m.path.substr(0, s);
        }
    return std::string();
}

static bool ValidFuncName(const std::string& f) {
    if (f.empty() || f.size() > 100 || std::isdigit(static_cast<unsigned char>(f[0]))) return false;
    for (char c : f)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    return true;
}

bool ScriptHost::HasModTextFunction(const std::string& mod, const std::string& func) {
    if (!ValidFuncName(func)) return false;
    const std::string decl = "bCString " + func + "(const bCString &in)";
    for (Mod& m : mods_)
        if (_stricmp(m.name.c_str(), mod.c_str()) == 0) return FindFn(m, decl.c_str()) != nullptr;
    return false;
}

bool ScriptHost::CallModText(const std::string& mod, const std::string& func, const void* arg, void* out) {
    if (!ValidFuncName(func)) {
        Log("CallMod: ungueltiger Funktionsname '" + func + "' (nur Buchstaben, Ziffern, _)");
        return false;
    }
    if (modCallDepth_ >= 8) {
        Log("CallMod: zu tief verschachtelt (mehr als 8 Aufrufe zwischen Mods), abgebrochen bei " + mod + "::" + func);
        return false;
    }
    const std::string decl = "bCString " + func + "(const bCString &in)";
    for (Mod& m : mods_) {
        if (_stricmp(m.name.c_str(), mod.c_str()) != 0) continue;
        asIScriptFunction* f = FindFn(m, decl.c_str());
        if (!f) return false;
        const EvArg a = EvArg::Str(arg);
        const std::string saved = curMod_;
        curMod_ = m.name;
        ++modCallDepth_;
        const bool ok = Execute(f, nullptr, nullptr, nullptr, &a, 1, nullptr, out);
        --modCallDepth_;
        curMod_ = saved;
        return ok;
    }
    return false;
}

int ScriptHost::SendModMessage(const void* topic, const void* data) {
    static const char* const kDecl = "void OnModMessage(const bCString &in, const bCString &in, const bCString &in)";
    if (modCallDepth_ >= 8) {
        Log("SendModMessage: zu tief verschachtelt (mehr als 8 Aufrufe zwischen Mods), Nachricht verworfen");
        return 0;
    }
    const std::string sender = curMod_;
    const SStr senderText(sender.c_str());
    const EvArg a[3] = {EvArg::Str(&senderText), EvArg::Str(topic), EvArg::Str(data)};
    int n = 0;
    ++modCallDepth_;
    for (size_t i = 0; i < mods_.size(); ++i) {
        Mod& m = mods_[i];
        if (!m.module || m.name == sender) continue;
        if (asIScriptFunction* f = FindFn(m, kDecl)) {
            curMod_ = m.name;
            if (Execute(f, nullptr, nullptr, nullptr, a, 3)) ++n;
        }
    }
    --modCallDepth_;
    curMod_ = sender;
    return n;
}

// stringtable.ini of a mod: in the mod folder or, as for the Union Starter, in ModFiles\ (empty if there is none)
static std::string ModStringTablePath(const std::string& modBase) {
    const std::string a = modBase + "\\stringtable.ini", b = modBase + "\\ModFiles\\stringtable.ini";
    if (GetFileAttributesA(a.c_str()) != INVALID_FILE_ATTRIBUTES) return a;
    if (GetFileAttributesA(b.c_str()) != INVALID_FILE_ATTRIBUTES) return b;
    return std::string();
}

// Mod names in the load order (load_order.txt, then alphabetical), without the mods that are off
// Example and test mods are off by default: <mod>\mod.ini with [mod] enabled=0. They load only if load_order.txt turns them on with "+name".
// Mods without mod.ini (or with enabled=1) load as before, but "-name" turns them off. The test program loads all (SetLoadDefaultOffMods).
// Dependencies (API 40), also in [mod]: version=1.2.0 (own version), api=39 (minimum necessary script interface),
// requires=other_mod >= 1.0, third_mod (separated by commas, version optional). A mod whose conditions are not met does not load
// (also not its file patches and string tables). This also stops dependent mods. Needed mods load before the mod that needs them.
// Each change of the state is logged 1 time.
static bool g_loadDefaultOff = false;
void SetLoadDefaultOffMods(bool on) { g_loadDefaultOff = on; }

namespace {
struct ModDep {
    std::string name, minVersion;
};
struct ModMeta {
    std::string version;
    int api = 0;
    std::vector<ModDep> deps;
    unsigned kinds = 0;    // 1 script, 2 file patches, 4 string table
    bool enabled = true;   // [mod] enabled (Standard 1)
};
std::mutex g_modStateMu;
std::map<std::string, std::string> g_modVersions;   // active mods (all kinds) -> version, keys in lower case
std::map<std::string, std::string> g_modBlockLog;   // mod -> last reported reason why it does not load
std::map<std::string, std::string> g_modOrderLog;   // mod -> last reported change of the sequence

std::string LowerName(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string TrimText(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return std::string();
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}
// one number for each part ("1.10.2"); text after the digits of a part (for example "0-beta") is ignored, missing parts count as 0
std::vector<unsigned long> VersionParts(const std::string& v) {
    std::vector<unsigned long> parts;
    size_t i = 0;
    while (i < v.size()) {
        unsigned long x = 0;
        while (i < v.size() && std::isdigit(static_cast<unsigned char>(v[i]))) {
            if (x < 100000000) x = x * 10 + unsigned(v[i] - '0');
            ++i;
        }
        parts.push_back(x);
        while (i < v.size() && v[i] != '.') ++i;
        if (i < v.size()) ++i;
    }
    return parts;
}
int CompareVersions(const std::string& a, const std::string& b) {
    const std::vector<unsigned long> x = VersionParts(a), y = VersionParts(b);
    for (size_t i = 0; i < x.size() || i < y.size(); ++i) {
        const unsigned long p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
        if (p != q) return p < q ? -1 : 1;
    }
    return 0;
}
ModMeta ReadModMeta(const std::string& base) {
    ModMeta m;
    // own reader in place of GetPrivateProfile*: that function did not see [mod] in files with UTF-8 BOM (then a mod with enabled=0 loaded)
    const std::map<std::string, std::string> sec = ReadIniSection(base + "\\mod.ini", "mod");
    const auto get = [&sec](const char* key) {
        const auto it = sec.find(key);
        return it == sec.end() ? std::string() : it->second;
    };
    m.version = get("version");
    m.api = int(std::strtol(get("api").c_str(), nullptr, 10));
    m.enabled = sec.find("enabled") == sec.end() || std::strtol(get("enabled").c_str(), nullptr, 10) != 0;
    const std::string req = get("requires");
    size_t pos = 0;
    while (pos < req.size()) {
        size_t c = req.find(',', pos);
        if (c == std::string::npos) c = req.size();
        const std::string item = TrimText(req.substr(pos, c - pos));
        pos = c + 1;
        if (item.empty()) continue;
        ModDep d;
        const size_t ge = item.find(">=");
        if (ge != std::string::npos) {
            d.name = TrimText(item.substr(0, ge));
            d.minVersion = TrimText(item.substr(ge + 2));
        } else
            d.name = item;
        if (!d.name.empty()) m.deps.push_back(d);
    }
    return m;
}
// reports a change of the state of a mod exactly 1 time; empty text = state cleared
void ModStateLog(std::map<std::string, std::string>& seen, const std::string& mod, const std::string& msg, const char* cleared) {
    if (!ScriptHost::s_current) return;   // before the host starts: report at the first scan after that
    auto it = seen.find(mod);
    if (msg.empty()) {
        if (it == seen.end()) return;
        seen.erase(it);
        if (cleared) ScriptHost::s_current->Log("[" + mod + "] " + cleared);
        return;
    }
    if (it != seen.end() && it->second == msg) return;
    seen[mod] = msg;
    ScriptHost::s_current->Log("[" + mod + "] " + msg);
}
}   // namespace

// kinds: which mods are returned (1 script, 2 file patches, 4 string table). Dependencies are always checked across all kinds.
static std::vector<std::string> DiscoverModsOfKind(const std::string& dir, unsigned kinds) {
    std::vector<std::string> found, order, off, on, defaultOff, result;
    std::map<std::string, ModMeta> meta;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.') continue;
            const std::string base = dir + "\\" + fd.cFileName;
            const DWORD pa = GetFileAttributesA((base + "\\ScriptPatches").c_str());   // mod only with file patches, without a script
            unsigned k = 0;
            if (FileTime(base + "\\main.as")) k |= 1;
            if (pa != INVALID_FILE_ATTRIBUTES && (pa & FILE_ATTRIBUTE_DIRECTORY)) k |= 2;
            if (!ModStringTablePath(base).empty()) k |= 4;
            if (!k) continue;
            found.push_back(fd.cFileName);
            ModMeta m = ReadModMeta(base);
            m.kinds = k;
            if (!m.enabled) defaultOff.push_back(fd.cFileName);
            meta[fd.cFileName] = m;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    std::sort(found.begin(), found.end(), [](const std::string& a, const std::string& b) { return _stricmp(a.c_str(), b.c_str()) < 0; });
    if (FILE* f = std::fopen((dir + "\\load_order.txt").c_str(), "r")) {
        char line[512];
        bool first = true;
        auto name = [](const std::string& s) {   // name after + or -, spaces between are permitted ("+ my_mod")
            const size_t b = s.find_first_not_of(" \t", 1);
            return b == std::string::npos ? std::string() : s.substr(b);
        };
        while (std::fgets(line, sizeof line, f)) {
            std::string t(line);
            // UTF-8 signature at the start of the file (editor "UTF-8 with BOM", PowerShell 5 Set-Content -Encoding utf8): if not removed, the first line did not count
            if (first && t.compare(0, 3, "\xEF\xBB\xBF") == 0) t.erase(0, 3);
            first = false;
            size_t c = t.find('#');
            if (c != std::string::npos) t.erase(c);
            while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' ' || t.back() == '\t')) t.pop_back();
            size_t b = t.find_first_not_of(" \t");
            if (b == std::string::npos) continue;
            t = t.substr(b);
            if (t[0] == '-')
                off.push_back(name(t));
            else if (t[0] == '+') {
                on.push_back(name(t));
                order.push_back(name(t));
            } else
                order.push_back(t);
        }
        std::fclose(f);
    }
    auto has = [](const std::vector<std::string>& v, const std::string& n) {
        for (const std::string& x : v)
            if (_stricmp(x.c_str(), n.c_str()) == 0) return true;
        return false;
    };
    auto spelled = [](const std::vector<std::string>& v, const std::string& n) {   // spelling of the folder, empty if not included
        for (const std::string& x : v)
            if (_stricmp(x.c_str(), n.c_str()) == 0) return x;
        return std::string();
    };
    auto enabled = [&](const std::string& n) { return !has(off, n) && (g_loadDefaultOff || !has(defaultOff, n) || has(on, n)); };
    for (const std::string& n : order) {
        const std::string e = spelled(found, n);
        if (!e.empty() && enabled(e) && !has(result, e)) result.push_back(e);
    }
    for (const std::string& n : found)
        if (enabled(n) && !has(result, n)) result.push_back(n);

    // check the conditions until nothing changes (a blocked mod blocks the mods that need it)
    std::map<std::string, std::string> blocked;
    for (bool changed = true; changed;) {
        changed = false;
        for (size_t i = 0; i < result.size() && !changed; ++i) {
            const ModMeta& m = meta[result[i]];
            std::string why;
            if (m.api > G3ASM_SCRIPT_API_VERSION)
                why = "braucht Skript-Schnittstelle " + std::to_string(m.api) + ", vorhanden ist " + std::to_string(G3ASM_SCRIPT_API_VERSION) +
                      " (G3ASM aktualisieren)";
            for (size_t j = 0; j < m.deps.size() && why.empty(); ++j) {
                const ModDep& d = m.deps[j];
                const std::string dep = spelled(result, d.name);
                if (dep.empty()) {
                    const std::string f = spelled(found, d.name);
                    if (f.empty())
                        why = "braucht die Mod '" + d.name + "', die nicht installiert ist";
                    else if (blocked.count(f))
                        why = "braucht die Mod '" + f + "', die selbst nicht geladen wird";
                    else
                        why = "braucht die Mod '" + f + "', die abgeschaltet ist (in load_order.txt mit +" + f + " zuschalten)";
                } else if (!d.minVersion.empty() && CompareVersions(meta[dep].version, d.minVersion) < 0)
                    why = "braucht die Mod '" + dep + "' ab Version " + d.minVersion + ", vorhanden ist " +
                          (meta[dep].version.empty() ? std::string("eine ohne Versionsangabe") : "Version " + meta[dep].version);
            }
            if (why.empty()) continue;
            blocked[result[i]] = why;
            result.erase(result.begin() + i);
            changed = true;
        }
    }

    // put needed mods before the mod that needs them (if not necessary, the sequence stays); circles are reported and not resolved
    std::vector<std::string> sorted, stack;
    std::map<std::string, std::string> moved, cycles;
    std::function<void(const std::string&)> place = [&](const std::string& n) {
        if (has(sorted, n)) return;
        stack.push_back(n);
        for (const ModDep& d : meta[n].deps) {
            const std::string dep = spelled(result, d.name);
            if (dep.empty() || has(sorted, dep)) continue;
            if (has(stack, dep)) {
                cycles[n] = "Abhaengigkeiten im Kreis mit '" + dep + "': Reihenfolge zwischen beiden bleibt wie in load_order.txt";
                continue;
            }
            moved[n] = "laedt nach '" + dep + "', weil sie diese Mod braucht";
            place(dep);
        }
        stack.pop_back();
        sorted.push_back(n);
    };
    for (const std::string& n : result) place(n);

    {
        std::lock_guard<std::mutex> lock(g_modStateMu);
        g_modVersions.clear();
        for (const std::string& n : sorted) g_modVersions[LowerName(n)] = meta[n].version;
        for (const std::string& n : found) {
            auto b = blocked.find(n);
            ModStateLog(g_modBlockLog, n, b == blocked.end() ? std::string() : "wird nicht geladen: " + b->second, "Abhaengigkeiten jetzt erfuellt");
            auto c = cycles.find(n);
            auto mv = moved.find(n);
            ModStateLog(g_modOrderLog, n, c != cycles.end() ? c->second : mv != moved.end() ? mv->second : std::string(), nullptr);
        }
    }
    std::vector<std::string> out;
    for (const std::string& n : sorted)
        if (meta[n].kinds & kinds) out.push_back(n);
    return out;
}
static std::vector<std::string> DiscoverMods(const std::string& dir, bool withPatchOnly = false, bool withStrings = false) {
    return DiscoverModsOfKind(dir, 1u | (withPatchOnly ? 2u : 0u) | (withStrings ? 4u : 0u));
}

// script functions (API 40): is a mod active (conditions met, not off), and which version does its mod.ini give
namespace {
bool IsModActive(const SStr& name) {
    std::lock_guard<std::mutex> lock(g_modStateMu);
    return g_modVersions.count(LowerName(name.c_str())) != 0;
}
SStr GetModVersion(const SStr& name) {
    std::lock_guard<std::mutex> lock(g_modStateMu);
    auto it = g_modVersions.find(LowerName(name.c_str()));
    return SStr(it == g_modVersions.end() ? "" : it->second.c_str());
}
}   // namespace

// ---------------------------------------------------------------- mod string tables (<mod>\stringtable.ini)
// Format as the stringtable.ini of the game: "key=text;;text;;..." on each line. If a line "Languages=English;German;..." is at the top, the columns are
// in this sequence, and the code uses the column of the current game language (missing or empty: first column). Without this line, the full value is the text.
// Comment lines start with ; # or [ . File encoding: UTF-16 (BOM), UTF-8 (BOM) or ANSI (cp1252). Later mods overwrite earlier mods.
struct StrTable {
    std::vector<std::wstring> langs;
    std::vector<std::pair<std::string, std::vector<std::wstring>>> rows;
};
static std::map<std::string, StrTable> g_strTables;
bool g_strDirty = false;
std::map<std::string, unsigned long long> g_strMtime;

std::wstring DecodeText(const std::string& raw) {
    if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF && static_cast<unsigned char>(raw[1]) == 0xFE)
        return std::wstring(reinterpret_cast<const wchar_t*>(raw.data() + 2), (raw.size() - 2) / 2);
    const bool utf8 = raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF && static_cast<unsigned char>(raw[1]) == 0xBB &&
                      static_cast<unsigned char>(raw[2]) == 0xBF;
    const char* p = raw.data() + (utf8 ? 3 : 0);
    const int n = int(raw.size()) - (utf8 ? 3 : 0);
    if (n <= 0) return std::wstring();
    const int len = MultiByteToWideChar(utf8 ? CP_UTF8 : 1252, 0, p, n, nullptr, 0);
    std::wstring w(len > 0 ? len : 0, L'\0');
    if (len > 0) MultiByteToWideChar(utf8 ? CP_UTF8 : 1252, 0, p, n, &w[0], len);
    return w;
}
std::vector<std::wstring> SplitW(const std::wstring& s, const std::wstring& sep) {
    std::vector<std::wstring> r;
    size_t pos = 0;
    for (;;) {
        const size_t f = s.find(sep, pos);
        if (f == std::wstring::npos) {
            r.push_back(s.substr(pos));
            break;
        }
        r.push_back(s.substr(pos, f - pos));
        pos = f + sep.size();
    }
    return r;
}
bool ParseStringTable(const std::string& path, StrTable* out, std::string* err) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        *err = "nicht lesbar";
        return false;
    }
    std::string raw;
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) raw.append(buf, n);
    std::fclose(f);
    const std::wstring text = DecodeText(raw);
    size_t pos = 0;
    while (pos < text.size()) {
        size_t e = text.find_first_of(L"\r\n", pos);
        if (e == std::wstring::npos) e = text.size();
        std::wstring line = text.substr(pos, e - pos);
        pos = e + 1;
        if (line.empty() || line[0] == L';' || line[0] == L'#' || line[0] == L'[') continue;
        const size_t eq = line.find(L'=');
        if (eq == std::wstring::npos || eq == 0) continue;
        std::wstring key = line.substr(0, eq), val = line.substr(eq + 1);
        if (key == L"Languages") {
            out->langs = SplitW(val, L";");
            continue;
        }
        std::string k;
        for (wchar_t c : key) k += c < 128 ? char(c) : '?';
        out->rows.push_back({k, out->langs.empty() ? std::vector<std::wstring>{val} : SplitW(val, L";;")});
    }
    return true;
}
// puts all loaded tables into the text hooks when the language and the hooks are available (if not, later, see CheckReload)
void ApplyStringTables(ScriptHost* host) {
    if (!g_strDirty) return;
    const std::string lang = CurrentGameLanguage();
    if (lang.empty() || !EnsureTextHooks()) return;
    int total = 0;
    for (const auto& t : g_strTables) {
        size_t idx = 0;
        for (size_t i = 0; i < t.second.langs.size(); ++i) {
            std::string l;
            for (wchar_t c : t.second.langs[i]) l += c < 128 ? char(c) : '?';
            if (_stricmp(l.c_str(), lang.c_str()) == 0) {
                idx = i;
                break;
            }
        }
        for (const auto& row : t.second.rows) {
            const std::wstring& w = idx < row.second.size() && !row.second[idx].empty() ? row.second[idx] : row.second[0];
            RegisterTextWide(row.first, w);
            ++total;
        }
    }
    g_strDirty = false;
    host->Log("Stringtabellen angewendet: " + std::to_string(total) + " Eintraege, Sprache " + lang);
}
void ScanStringTables(const std::string& modsDir, ScriptHost* host) {
    if (modsDir.empty()) return;
    for (const std::string& n : DiscoverMods(modsDir, false, true)) {
        const std::string path = ModStringTablePath(modsDir + "\\" + n);
        const unsigned long long t = path.empty() ? 0 : FileTime(path);
        if (!t) {
            if (g_strTables.erase(n)) {
                g_strDirty = true;
                g_strMtime.erase(n);
            }
            continue;
        }
        auto it = g_strMtime.find(n);
        if (it != g_strMtime.end() && it->second == t) continue;
        g_strMtime[n] = t;
        StrTable tab;
        std::string err;
        if (!ParseStringTable(path, &tab, &err)) {
            host->Log("[" + n + "] stringtable.ini: " + err);
            continue;
        }
        host->Log("[" + n + "] stringtable.ini gelesen: " + std::to_string(tab.rows.size()) + " Eintraege" +
                  (tab.langs.empty() ? " (eine Sprache)" : ", " + std::to_string(tab.langs.size()) + " Sprachen"));
        g_strTables[n] = std::move(tab);
        g_strDirty = true;
    }
    ApplyStringTables(host);
}

std::vector<std::string> DiscoverModNames(const std::string& modsDir) { return DiscoverMods(modsDir, true); }

int ScriptHost::ScanMods() {
    if (modsDir_.empty()) return 0;
    const std::vector<std::string> want = DiscoverMods(modsDir_);
    int loaded = 0;
    for (size_t i = 0; i < mods_.size();) {   // entfernte/abgeschaltete Mods entladen
        Mod& m = mods_[i];
        bool keep = m.name == "main";
        for (const std::string& n : want)
            if (n == m.name) keep = true;
        if (keep) {
            ++i;
            continue;
        }
        Log("[" + m.name + "] Mod entfernt/abgeschaltet - entladen");
        Unload(m);
        ForgetKeptGlobals(m.name);
        mods_.erase(mods_.begin() + i);
    }
    for (const std::string& n : want) {   // build new mods, reload changed mods
        if (n == "main") {
            Log("Mod-Ordner 'main' ignoriert (Name fuer das Einzelskript reserviert)");
            continue;
        }
        const std::string path = modsDir_ + "\\" + n + "\\main.as";
        Mod* m = Find(n);
        if (m && m->mtime == FileTime(path)) continue;
        if (!m) {
            mods_.push_back(Mod());
            m = &mods_.back();
            m->name = n;
        } else
            Log("[" + n + "] Skript geaendert - lade neu");
        m->path = path;
        if (Build(*m)) ++loaded;
    }
    ScanStringTables(modsDir_, this);                                                // <mod>\stringtable.ini (also for mods without a script)
    std::stable_sort(mods_.begin(), mods_.end(), [&](const Mod& a, const Mod& b) {   // "main" first, then the load order
        auto rank = [&](const Mod& m) {
            if (m.name == "main") return -1;
            for (size_t i = 0; i < want.size(); ++i)
                if (want[i] == m.name) return int(i);
            return int(want.size());
        };
        return rank(a) < rank(b);
    });
    return loaded;
}

bool ScriptHost::CheckReload() {
    const unsigned now = GetTickCount();
    if (now - lastCheckTick_ < 1000) return false;
    lastCheckTick_ = now;
    bool any = false;
    for (size_t i = 0; i < mods_.size(); ++i) {   // Einzelskript ("main")
        Mod& m = mods_[i];
        if (m.name != "main") continue;
        const unsigned long long t = FileTime(m.path);
        if (!t || t == m.mtime) continue;
        Log("[main] Skript geaendert - lade neu: " + m.path);
        if (Build(m)) {
            CallMain();
            any = true;
        }
    }
    if (ScanMods() > 0) {
        CallMain();
        any = true;
    }
    ApplyStringTables(this);   // tables that were read before the game start (language and hooks not ready yet)
    return any;
}

void ScriptHost::Check(bool ok, const char* msg) {
    ++checks_;
    if (!ok) {
        ++failures_;
        Log(std::string("PRUEFUNG FEHLGESCHLAGEN: ") + msg);
    }
}

}   // namespace g3asm
