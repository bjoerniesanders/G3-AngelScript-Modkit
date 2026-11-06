#include "script_host.h"

#include <angelscript.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>

#include "sb_api.h"
#include "scriptarray/scriptarray.h"
#include "scriptbuilder/scriptbuilder.h"
#include "scriptmath/scriptmath.h"

namespace og3p {

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

// Literale: konstante bCString-Objekte, nach Inhalt zusammengelegt
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
        }   // erase gibt das SStr ueber unique_ptr frei
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
// bCMatrix4 (= bCMatrix der Engine, 4x4): Zeilenvektor-Konvention v' = v * M, Zeilen 0..2 = Achsen, Zeile 3 = Verschiebung
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
}   // nur Zeile 3, der Rest bleibt
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

// ---------------------------------------------------------------- globale Funktionen
bool g_printToGameLog = true;   // ini [patch] printgame=0: print() schreibt nur ins eigene Protokoll, nicht zusaetzlich ins Spielprotokoll (geInfo)
void Print(const SStr& s) {
    if (ScriptHost::s_current) ScriptHost::s_current->Log(std::string("[") + ScriptHost::s_current->CurrentMod() + "] " + s.c_str());
    if (g_printToGameLog && g_sb.geInfo) g_sb.geInfo("og3-Skript: %s", s.c_str());
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
int GetApiVersion() { return OG3_SCRIPT_API_VERSION; }

// Fenster/Eingabe/Ton (Windows): das Spielfenster = groesstes sichtbares Hauptfenster dieses Prozesses
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
    if (pid != GetCurrentProcessId()) return false;   // nur, wenn das Spiel im Vordergrund ist
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

// Schutz gegen Endlosschleifen: ein Aufruf darf hoechstens 5 s laufen
struct Timeout {
    unsigned startTick;
};
void LineCallback(asIScriptContext* ctx, Timeout* t) {
    if (GetTickCount() - t->startTick > 5000) ctx->Abort();
}

bool RegisterAll(asIScriptEngine* e) {
    bool ok = true;
    // bCString
    OG3_REG(e->RegisterObjectType("bCString", sizeof(SStr), asOBJ_VALUE | asGetTypeTraits<SStr>()));
    OG3_REG(e->RegisterStringFactory("bCString", &g_strFactory));
    OG3_REG(e->RegisterObjectBehaviour("bCString", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(StrConstruct), asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectBehaviour("bCString", asBEHAVE_CONSTRUCT, "void f(const bCString &in)", asFUNCTION(StrCopyConstruct),
                                       asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectBehaviour("bCString", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(StrDestruct), asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bCString &opAssign(const bCString &in)", asMETHODPR(SStr, operator=, (const SStr&), SStr&),
                                    asCALL_THISCALL));
    OG3_REG(e->RegisterObjectMethod("bCString", "bCString &opAddAssign(const bCString &in)",
                                    asMETHODPR(SStr, operator+=, (const SStr&), SStr&), asCALL_THISCALL));
    OG3_REG(e->RegisterObjectMethod("bCString", "bCString opAdd(const bCString &in) const", asFUNCTION(StrAdd), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bCString opAdd(int) const", asFUNCTION(StrAddInt), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bCString opAdd(uint) const", asFUNCTION(StrAddUInt), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bCString opAdd(float) const", asFUNCTION(StrAddFloat), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bCString opAdd(double) const", asFUNCTION(StrAddDouble), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bCString opAdd(bool) const", asFUNCTION(StrAddBool), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bCString opAdd_r(int) const", asFUNCTION(StrAddRInt), asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bCString opAdd_r(float) const", asFUNCTION(StrAddRFloat), asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bool opEquals(const bCString &in) const", asFUNCTION(StrEquals), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "int opCmp(const bCString &in) const", asFUNCTION(StrCompare), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "int GetLength() const", asFUNCTION(StrLength), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bool IsEmpty() const", asFUNCTION(StrIsEmpty), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "int Compare(const bCString &in) const", asFUNCTION(StrCompare), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "int CompareNoCase(const bCString &in) const", asFUNCTION(StrCompareNoCase),
                                    asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "int Find(const bCString &in, int start = 0) const", asFUNCTION(StrFind),
                                    asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bool Contains(const bCString &in) const", asFUNCTION(StrContains), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bCString Left(int) const", asFUNCTION(StrLeft), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bCString Right(int) const", asFUNCTION(StrRight), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bCString Mid(int, int) const", asFUNCTION(StrMid), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "void ToUpper()", asFUNCTION(StrToUpper), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "void ToLower()", asFUNCTION(StrToLower), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bCString GetUpper() const", asFUNCTION(StrUpper), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCString", "bCString GetLower() const", asFUNCTION(StrLower), asCALL_CDECL_OBJFIRST));

    // bCVector
    OG3_REG(
        e->RegisterObjectType("bCVector", sizeof(SVec3), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<SVec3>()));
    OG3_REG(e->RegisterObjectProperty("bCVector", "float x", asOFFSET(SVec3, x)));
    OG3_REG(e->RegisterObjectProperty("bCVector", "float y", asOFFSET(SVec3, y)));
    OG3_REG(e->RegisterObjectProperty("bCVector", "float z", asOFFSET(SVec3, z)));
    OG3_REG(e->RegisterObjectBehaviour("bCVector", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(V3Construct), asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectBehaviour("bCVector", asBEHAVE_CONSTRUCT, "void f(float, float, float)", asFUNCTION(V3ConstructXYZ),
                                       asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectMethod("bCVector", "float GetMagnitude() const", asFUNCTION(V3Mag), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector", "float GetSquareMagnitude() const", asFUNCTION(V3SqMag), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector", "void Normalize()", asFUNCTION(V3Normalize), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector", "bCVector GetNormalized() const", asFUNCTION(V3GetNormalized), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector", "float GetDotProduct(const bCVector &in) const", asFUNCTION(V3Dot), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector", "bCVector GetCrossProduct(const bCVector &in) const", asFUNCTION(V3Cross),
                                    asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector", "bCVector opAdd(const bCVector &in) const", asFUNCTION(V3Add), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector", "bCVector opSub(const bCVector &in) const", asFUNCTION(V3Sub), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector", "bCVector opMul(float) const", asFUNCTION(V3Mul), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector", "bCVector opMul_r(float) const", asFUNCTION(V3Mul), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector", "bCVector opDiv(float) const", asFUNCTION(V3Div), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector", "bCVector opNeg() const", asFUNCTION(V3Neg), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector", "bool opEquals(const bCVector &in) const", asFUNCTION(V3Equals), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector", "bCString ToString() const", asFUNCTION(V3ToString), asCALL_CDECL_OBJFIRST));

    // bCVector2 / bCVector4
    OG3_REG(
        e->RegisterObjectType("bCVector2", sizeof(SVec2), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<SVec2>()));
    OG3_REG(e->RegisterObjectProperty("bCVector2", "float x", asOFFSET(SVec2, x)));
    OG3_REG(e->RegisterObjectProperty("bCVector2", "float y", asOFFSET(SVec2, y)));
    OG3_REG(e->RegisterObjectBehaviour("bCVector2", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(V2Construct), asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectBehaviour("bCVector2", asBEHAVE_CONSTRUCT, "void f(float, float)", asFUNCTION(V2ConstructXY),
                                       asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectMethod("bCVector2", "float GetMagnitude() const", asFUNCTION(V2Mag), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector2", "void Normalize()", asFUNCTION(V2Normalize), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector2", "bCVector2 opAdd(const bCVector2 &in) const", asFUNCTION(V2Add), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector2", "bCVector2 opSub(const bCVector2 &in) const", asFUNCTION(V2Sub), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector2", "bCVector2 opMul(float) const", asFUNCTION(V2Mul), asCALL_CDECL_OBJFIRST));
    OG3_REG(
        e->RegisterObjectType("bCVector4", sizeof(SVec4), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<SVec4>()));
    OG3_REG(e->RegisterObjectProperty("bCVector4", "float x", asOFFSET(SVec4, x)));
    OG3_REG(e->RegisterObjectProperty("bCVector4", "float y", asOFFSET(SVec4, y)));
    OG3_REG(e->RegisterObjectProperty("bCVector4", "float z", asOFFSET(SVec4, z)));
    OG3_REG(e->RegisterObjectProperty("bCVector4", "float w", asOFFSET(SVec4, w)));
    OG3_REG(e->RegisterObjectBehaviour("bCVector4", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(V4Construct), asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectBehaviour("bCVector4", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)", asFUNCTION(V4ConstructXYZW),
                                       asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectMethod("bCVector4", "float GetMagnitude() const", asFUNCTION(V4Mag), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCVector4", "void Normalize()", asFUNCTION(V4Normalize), asCALL_CDECL_OBJFIRST));

    // bCQuaternion / bCMatrix3 (Matrix zuerst deklarieren, beide verweisen aufeinander)
    OG3_REG(e->RegisterObjectType("bCQuaternion", sizeof(SQuat),
                                  asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<SQuat>()));
    OG3_REG(
        e->RegisterObjectType("bCMatrix3", sizeof(SMat3), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<SMat3>()));
    OG3_REG(e->RegisterObjectProperty("bCQuaternion", "float x", asOFFSET(SQuat, x)));
    OG3_REG(e->RegisterObjectProperty("bCQuaternion", "float y", asOFFSET(SQuat, y)));
    OG3_REG(e->RegisterObjectProperty("bCQuaternion", "float z", asOFFSET(SQuat, z)));
    OG3_REG(e->RegisterObjectProperty("bCQuaternion", "float w", asOFFSET(SQuat, w)));
    OG3_REG(e->RegisterObjectBehaviour("bCQuaternion", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(QConstruct), asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectBehaviour("bCQuaternion", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)", asFUNCTION(QConstructXYZW),
                                       asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectMethod("bCQuaternion", "void Clear()", asFUNCTION(QClear), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCQuaternion", "void Normalize()", asFUNCTION(QNormalize), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCQuaternion", "void Invert()", asFUNCTION(QInvert), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCQuaternion", "float GetMagnitude() const", asFUNCTION(QMag), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCQuaternion", "bCQuaternion opMul(const bCQuaternion &in) const", asFUNCTION(QMul),
                                    asCALL_CDECL_OBJFIRST));
    OG3_REG(
        e->RegisterObjectMethod("bCQuaternion", "void SetQuaternion(const bCMatrix3 &in)", asFUNCTION(QFromMatrix), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectBehaviour("bCMatrix3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(M3Construct), asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix3", "float GetElement(int row, int col) const", asFUNCTION(M3Get), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix3", "void SetElement(int row, int col, float)", asFUNCTION(M3Set), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix3", "void SetIdentity()", asFUNCTION(M3Identity), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix3", "void Transpose()", asFUNCTION(M3Transpose), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix3", "void Invert()", asFUNCTION(M3Invert), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix3", "bCMatrix3 opMul(const bCMatrix3 &in) const", asFUNCTION(M3MulM), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix3", "bCVector opMul(const bCVector &in) const", asFUNCTION(M3MulV), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix3", "void SetToRotationAxisX(float)", asFUNCTION(M3RotX), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix3", "void SetToRotationAxisY(float)", asFUNCTION(M3RotY), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix3", "void SetToRotationAxisZ(float)", asFUNCTION(M3RotZ), asCALL_CDECL_OBJFIRST));
    OG3_REG(
        e->RegisterObjectMethod("bCMatrix3", "void SetToRotation(const bCQuaternion &in)", asFUNCTION(M3FromQuat), asCALL_CDECL_OBJFIRST));
    // bCMatrix4 (API 19)
    OG3_REG(
        e->RegisterObjectType("bCMatrix4", sizeof(SMat4), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<SMat4>()));
    OG3_REG(e->RegisterObjectBehaviour("bCMatrix4", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(M4Construct), asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "float GetElement(int row, int col) const", asFUNCTION(M4Get), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "void SetElement(int row, int col, float)", asFUNCTION(M4Set), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "void SetIdentity()", asFUNCTION(M4Identity), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "void Transpose()", asFUNCTION(M4Transpose), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "void Invert()", asFUNCTION(M4Invert), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "bCMatrix4 GetInverted() const", asFUNCTION(M4GetInverted), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "float GetDeterminant() const", asFUNCTION(M4Det), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "bCMatrix4 opMul(const bCMatrix4 &in) const", asFUNCTION(M4MulM), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "bCVector opMul(const bCVector &in) const", asFUNCTION(M4MulV), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "bCVector GetTranslation() const", asFUNCTION(M4GetTranslation), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "void SetTranslation(const bCVector &in)", asFUNCTION(M4SetTranslation),
                                    asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "bCVector GetXAxis() const", asFUNCTION(M4GetX), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "bCVector GetYAxis() const", asFUNCTION(M4GetY), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "bCVector GetZAxis() const", asFUNCTION(M4GetZ), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "void SetToTranslation(const bCVector &in)", asFUNCTION(M4ToTranslation),
                                    asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "void SetToScaling(const bCVector &in)", asFUNCTION(M4ToScaling), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "void SetToRotation(const bCQuaternion &in)", asFUNCTION(M4ToRotation),
                                    asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "void SetToRotationAxisX(float)", asFUNCTION(M4RotX), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "void SetToRotationAxisY(float)", asFUNCTION(M4RotY), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "void SetToRotationAxisZ(float)", asFUNCTION(M4RotZ), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "bCMatrix3 GetRotationMatrix() const", asFUNCTION(M4RotationPart), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("bCMatrix4", "bCQuaternion GetRotation() const", asFUNCTION(M4Quaternion), asCALL_CDECL_OBJFIRST));

    // globale Funktionen
    OG3_REG(e->RegisterGlobalFunction("void print(const bCString &in)", asFUNCTION(Print), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool Super()", asFUNCTION(SuperCall), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool SetOverrideArg(int index, uint value)", asFUNCTION(SetOverrideArgScript), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("uint GetOverrideArg(int index)", asFUNCTION(GetOverrideArgScript), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bCString ToString(int)", asFUNCTION(ToStringInt), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bCString ToString(uint)", asFUNCTION(ToStringUInt), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bCString ToString(float)", asFUNCTION(ToStringFloat), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bCString ToString(double)", asFUNCTION(ToStringDouble), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bCString ToString(bool)", asFUNCTION(ToStringBool), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("void check(bool, const bCString &in)", asFUNCTION(Check), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("float GetFrameTime()", asFUNCTION(GetFrameTime), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("uint GetTimeStamp()", asFUNCTION(GetTimeStamp), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("int GetApiVersion()", asFUNCTION(GetApiVersion), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("void SetWindowTitle(const bCString &in)", asFUNCTION(SetWindowTitle), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool IsKeyDown(int)", asFUNCTION(IsKeyDown), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("void PlaySystemSound()", asFUNCTION(PlaySystemSound), asCALL_CDECL));
    static const int* keys[] = {&kKeyF1, &kKeyF2, &kKeyF3, &kKeyF4,  &kKeyF5,  &kKeyF6,
                                &kKeyF7, &kKeyF8, &kKeyF9, &kKeyF10, &kKeyF11, &kKeyF12};
    for (int i = 0; i < 12; ++i) {
        char d[32];
        std::snprintf(d, sizeof d, "const int KEY_F%d", i + 1);
        OG3_REG(e->RegisterGlobalProperty(d, const_cast<int*>(keys[i])));
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

bool ReadableMem(const void* p, size_t n) {
    const uintptr_t start = reinterpret_cast<uintptr_t>(p);
    if (start < 0x10000 || n == 0) return false;   // Nullzeiger-Bereich: Zahlen, Flags, Zaehler - keine Zeiger
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
    const bool ok = RegisterAll(engine_) && RegisterEngineBindings(engine_);
    ctx_ = engine_->CreateContext();
    char b[160];
    std::snprintf(b, sizeof b, "AngelScript %s bereit (Skript-Schnittstelle Version %d)%s", asGetLibraryVersion(), OG3_SCRIPT_API_VERSION,
                  ok ? "" : " - mit Registrierungsfehlern");
    Log(b);
    return ok;
}

void ScriptHost::Shutdown() {
    for (Mod& m : mods_) Unload(m);
    mods_.clear();
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

void ScriptHost::Unload(Mod& m) {
    if (!m.module) return;
    if (asIScriptFunction* f = m.module->GetFunctionByDecl("void OnShutdown()")) {
        curMod_ = m.name;
        Execute(f, nullptr);
    }
    m.module->Discard();
    m.module = nullptr;
    m.fMain = m.fFrame = m.fKey = nullptr;
    m.fnCache.clear();
    m.mainDone = false;
}

bool ScriptHost::Build(Mod& m) {
    Unload(m);
    m.mtime = FileTime(m.path);   // auch bei Fehlern merken: erst bei neuer Aenderung wieder versuchen
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
    return AddScript("main", path);
}

// Gleitkomma-Zustand der Engine schuetzen (gefunden 2026-11-06): Genome rechnet mit x87-Steuerwort 007F (24 Bit Genauigkeit, bCFPUAdmin). Beim ersten Skriptaufruf,
// der in C++ springt, setzt etwas im Patch die x87-Einheit auf den Windows-Standard 037F (64 Bit) zurueck. Ohne Schutz rechnete die Engine danach die ganze Sitzung
// mit falscher Genauigkeit weiter: Gebaeude-Texturen blieben nach dem Laden verwaschen. Jeder Skriptaufruf stellt deshalb x87-Steuerwort und MXCSR danach wieder her.
namespace {
unsigned short FpuControlWord() {
    unsigned short cw;
    __asm fnstcw cw
    return cw;
}
void FpuLoadControlWord(unsigned short cw) { __asm fldcw cw }
unsigned FpuMxcsr() {
    unsigned v;
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
        // MXCSR-Bits 0-5 sind Merkbits fuer Ausnahmen, die normale Rechnungen setzen (z. B. "ungenau"): nur die Steuerbits vergleichen und zuruecksetzen
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

bool ScriptHost::Execute(void* fp, float* argFloat, const int* argInt, const bool* argBool, const EvArg* ev, int nev, bool* retBool) {
    asIScriptFunction* f = static_cast<asIScriptFunction*>(fp);
    FpuRestore fpu(this, f);
    const bool nested = ctx_->GetState() == asEXECUTION_ACTIVE;   // Ereignis feuert, waehrend ein Skript laeuft
    if (nested && ctx_->PushState() < 0) return false;
    struct Pop {
        asIScriptContext* c;
        bool on;
        ~Pop() {
            if (on) c->PopState();
        }
    } pop{ctx_, nested};
    if (ctx_->Prepare(f) < 0) return false;
    if (argFloat) ctx_->SetArgFloat(0, *argFloat);
    if (argInt) ctx_->SetArgDWord(0, static_cast<asDWORD>(*argInt));
    if (argBool) ctx_->SetArgByte(1, *argBool ? 1 : 0);
    for (int i = 0; i < nev; ++i) {
        if (ev[i].kind == EvArg::Entity) {
            struct {
                void* p;
            } e{const_cast<void*>(ev[i].ptr)};
            ctx_->SetArgObject(i, &e);
        } else if (ev[i].kind == EvArg::Text)
            ctx_->SetArgObject(i, const_cast<void*>(ev[i].ptr));
        else
            ctx_->SetArgDWord(i, ev[i].value);
    }
    Timeout t{GetTickCount()};
    ctx_->SetLineCallback(asFUNCTION(LineCallback), &t, asCALL_CDECL);
    const int r = ctx_->Execute();
    ctx_->ClearLineCallback();
    if (r == asEXECUTION_FINISHED) {
        if (retBool) *retBool = ctx_->GetReturnByte() != 0;
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

void ScriptHost::CallIn(Mod& m, const char* decl) {
    if (!m.module) return;
    if (asIScriptFunction* f = FindFn(m, decl)) {
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

bool ScriptHost::HasHandler(const char* decl) {
    for (Mod& m : mods_)
        if (FindFn(m, decl)) return true;
    return false;
}

bool ScriptHost::CallOverride(const char* decl, const EvArg* args, int n, std::function<bool(const EvArg*)> original, bool* result,
                              bool* originalCalled) {
    if (!HasHandler(decl)) return false;
    chain_.emplace_back();
    const size_t idx = chain_.size() - 1;
    Chain& c0 = chain_[idx];
    c0.decl = decl;
    c0.n = n > 8 ? 8 : n;
    for (int i = 0; i < c0.n; ++i) c0.args[i] = args[i];
    c0.original = std::move(original);
    bool ret = false;
    bool ok = false;
    for (size_t i = 0; i < mods_.size() && !ok; ++i) {
        asIScriptFunction* f = FindFn(mods_[i], decl);
        if (!f) continue;
        chain_[idx].next = i + 1;
        curMod_ = mods_[i].name;
        ok = Execute(f, nullptr, nullptr, nullptr, chain_[idx].args, chain_[idx].n, &ret);
        if (!ok) break;   // Fehler im Skript: Rueckfall unten
    }
    if (!ok) {   // Skriptfehler: das Spiel soll sein Original bekommen
        if (!chain_[idx].originalCalled) {
            chain_[idx].originalCalled = true;
            ret = chain_[idx].original(chain_[idx].args);
        }
    }
    if (result) *result = ret;
    if (originalCalled) *originalCalled = chain_[idx].originalCalled;
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
    chain_[idx].originalCalled = true;   // Ende der Kette: das Original der Spielfunktion
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

void ScriptHost::CallEvent(const char* decl, const EvArg* args, int n) {
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
    if (busyKey_) return;   // Nachrichten, die waehrend eines Skriptaufrufs eintreffen, nicht verschachteln
    busyKey_ = true;
    for (size_t i = 0; i < mods_.size(); ++i)
        if (mods_[i].module && mods_[i].fKey) {
            curMod_ = mods_[i].name;
            Execute(mods_[i].fKey, nullptr, &vk, &down);
        }
    busyKey_ = false;
}

// stringtable.ini eines Mods: im Mod-Ordner oder, wie beim Union Starter, unter ModFiles\ (leer, wenn keine vorhanden ist)
static std::string ModStringTablePath(const std::string& modBase) {
    const std::string a = modBase + "\\stringtable.ini", b = modBase + "\\ModFiles\\stringtable.ini";
    if (GetFileAttributesA(a.c_str()) != INVALID_FILE_ATTRIBUTES) return a;
    if (GetFileAttributesA(b.c_str()) != INVALID_FILE_ATTRIBUTES) return b;
    return std::string();
}

// Mod-Namen in Ladereihenfolge (load_order.txt, dann alphabetisch), abgeschaltete weggelassen
// Beispiel- und Test-Mods sind standardmaessig aus: <Mod>\mod.ini mit [mod] enabled=0. Sie werden nur geladen, wenn load_order.txt sie mit "+name"
// zuschaltet. Mods ohne mod.ini (oder mit enabled=1) laden wie bisher, ausser "-name" schaltet sie ab. Das Testprogramm laedt alle (SetLoadDefaultOffMods).
static bool g_loadDefaultOff = false;
void SetLoadDefaultOffMods(bool on) { g_loadDefaultOff = on; }
static std::vector<std::string> DiscoverMods(const std::string& dir, bool withPatchOnly = false, bool withStrings = false) {
    std::vector<std::string> found, order, off, on, defaultOff, result;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.') continue;
            const std::string base = dir + "\\" + fd.cFileName;
            const DWORD pa = withPatchOnly ? GetFileAttributesA((base + "\\ScriptPatches").c_str())
                                           : INVALID_FILE_ATTRIBUTES;   // Mod nur mit Datei-Patches, ohne Skript
            if (FileTime(base + "\\main.as") || (pa != INVALID_FILE_ATTRIBUTES && (pa & FILE_ATTRIBUTE_DIRECTORY)) ||
                (withStrings && !ModStringTablePath(base).empty())) {
                found.push_back(fd.cFileName);
                if (GetPrivateProfileIntA("mod", "enabled", 1, (base + "\\mod.ini").c_str()) == 0) defaultOff.push_back(fd.cFileName);
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    std::sort(found.begin(), found.end(), [](const std::string& a, const std::string& b) { return _stricmp(a.c_str(), b.c_str()) < 0; });
    if (FILE* f = std::fopen((dir + "\\load_order.txt").c_str(), "r")) {
        char line[512];
        while (std::fgets(line, sizeof line, f)) {
            std::string t(line);
            size_t c = t.find('#');
            if (c != std::string::npos) t.erase(c);
            while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' ' || t.back() == '\t')) t.pop_back();
            size_t b = t.find_first_not_of(" \t");
            if (b == std::string::npos) continue;
            t = t.substr(b);
            if (t[0] == '-')
                off.push_back(t.substr(1));
            else if (t[0] == '+') {
                on.push_back(t.substr(1));
                order.push_back(t.substr(1));
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
    auto enabled = [&](const std::string& n) { return !has(off, n) && (g_loadDefaultOff || !has(defaultOff, n) || has(on, n)); };
    for (const std::string& n : order)
        if (has(found, n) && enabled(n) && !has(result, n)) result.push_back(n);
    for (const std::string& n : found)
        if (enabled(n) && !has(result, n)) result.push_back(n);
    return result;
}

// ---------------------------------------------------------------- Mod-Stringtabellen (<Mod>\stringtable.ini)
// Format wie die stringtable.ini des Spiels: "Schluessel=Text;;Text;;..." je Zeile. Steht oben eine Zeile "Languages=English;German;...", sind die Spalten
// in dieser Reihenfolge und die Spalte der aktuellen Spielsprache wird genommen (fehlt/leer: erste Spalte). Ohne diese Zeile gilt der ganze Wert als Text.
// Kommentarzeilen beginnen mit ; # oder [ . Dateikodierung: UTF-16 (BOM), UTF-8 (BOM) oder ANSI (cp1252). Spaetere Mods ueberschreiben fruehere.
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
// Traegt alle geladenen Tabellen in die Text-Hooks ein, sobald Sprache und Hooks verfuegbar sind (sonst spaeter, siehe CheckReload)
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
        mods_.erase(mods_.begin() + i);
    }
    for (const std::string& n : want) {   // neue Mods bauen, geaenderte neu laden
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
    ScanStringTables(modsDir_, this);                                                // <Mod>\stringtable.ini (auch fuer Mods ohne Skript)
    std::stable_sort(mods_.begin(), mods_.end(), [&](const Mod& a, const Mod& b) {   // "main" zuerst, dann Ladereihenfolge
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
    ApplyStringTables(this);   // Tabellen, die vor dem Spielstart gelesen wurden (Sprache/Hooks noch nicht bereit)
    return any;
}

void ScriptHost::Check(bool ok, const char* msg) {
    ++checks_;
    if (!ok) {
        ++failures_;
        Log(std::string("PRUEFUNG FEHLGESCHLAGEN: ") + msg);
    }
}

}   // namespace og3p
