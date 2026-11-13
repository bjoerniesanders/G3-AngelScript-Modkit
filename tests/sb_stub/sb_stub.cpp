// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
//
// Ersatz-SharedBase.dll fuer Tests ohne Spieldateien (CI): exportiert unter den dekorierten Namen der Original-SharedBase genau die Funktionen,
// die g3asm.dll ueber src/sb_api.cpp aufloest (bCString, Vektoren, Quaternion, Matrizen, GE_MESSAGEF_INFO). Die Rechenregeln folgen dem
// Original (Zeilenvektoren, Produktreihenfolge, Drehrichtung), die Rundung auf dem x87-Stapel ist nur angenaehert. Fuer Messungen am Verhalten
// des Originals immer die echte SharedBase.dll nehmen.
// Wie das Original importiert die DLL dbghelp.dll (MiniDumpWriteDump), damit auch der Loader-Test (tests/run_loader_test.ps1) damit laeuft.
#include <windows.h>
#include <dbghelp.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#define SB_API __declspec(dllexport)

// ---------------------------------------------------------------- bCString (4 Byte: Zeiger auf Text, nullptr = leer)
class SB_API bCString {
public:
    bCString() : p_(nullptr) {}
    bCString(const char* s) : p_(Dup(s, s ? std::strlen(s) : 0)) {}
    bCString(const bCString& o) : p_(Dup(o.p_, o.p_ ? std::strlen(o.p_) : 0)) {}
    ~bCString() { std::free(p_); }
    bCString& operator=(const bCString& o) {
        if (this != &o) {
            char* n = Dup(o.p_, o.p_ ? std::strlen(o.p_) : 0);
            std::free(p_);
            p_ = n;
        }
        return *this;
    }
    bCString& operator+=(const bCString& o) {
        const size_t a = p_ ? std::strlen(p_) : 0, b = o.p_ ? std::strlen(o.p_) : 0;
        if (b == 0) return *this;
        char* n = static_cast<char*>(std::malloc(a + b + 1));
        if (a) std::memcpy(n, p_, a);
        std::memcpy(n + a, o.p_, b);
        n[a + b] = 0;
        std::free(p_);
        p_ = n;
        return *this;
    }
    const char* GetText() const { return p_ ? p_ : ""; }
    int GetLength() const { return p_ ? int(std::strlen(p_)) : 0; }
    bool IsEmpty() const { return GetLength() == 0; }
    int Compare(const bCString& o) const {
        if (!p_) return o.p_ ? -1 : 0;
        if (!o.p_) return 1;
        const int r = std::strcmp(p_, o.p_);
        return r < 0 ? -1 : r > 0 ? 1 : 0;
    }
    int CompareNoCase(const bCString& o) const {
        if (!p_) return o.p_ ? -1 : 0;
        if (!o.p_) return 1;
        const int r = _stricmp(p_, o.p_);
        return r < 0 ? -1 : r > 0 ? 1 : 0;
    }
    int Find(const char* s, int start, bool) const {
        if (GetLength() <= start || !s) return -1;
        const char* r = std::strstr(p_ + start, s);
        return r ? int(r - p_) : -1;
    }
    bCString Left(int n) const {
        const int len = GetLength();
        if (n < 0 || n >= len) return *this;
        return Part(0, n);
    }
    bCString Right(int n) const {
        const int len = GetLength();
        if (n < 0 || n >= len) return *this;
        return Part(len - n, n);
    }
    bCString Mid(int first, int count) const {
        const int len = GetLength();
        if (len == 0 || first < 0 || count < 0) return *this;
        if (len < first) return bCString();
        if (len < first + count) count = len - first;
        return Part(first, count);
    }
    void ToUpper() {
        if (p_) _strupr(p_);
    }
    void ToLower() {
        if (p_) _strlwr(p_);
    }

private:
    static char* Dup(const char* s, size_t n) {
        if (!s || n == 0) return nullptr;
        char* r = static_cast<char*>(std::malloc(n + 1));
        std::memcpy(r, s, n);
        r[n] = 0;
        return r;
    }
    bCString Part(int first, int count) const {
        bCString r;
        r.p_ = Dup(p_ + first, size_t(count));
        return r;
    }
    char* p_;
};
static_assert(sizeof(bCString) == 4, "bCString-Layout");

// ---------------------------------------------------------------- Vektoren
class SB_API bCVector {
public:
    float x, y, z;
    float GetSquareMagnitude() const { return float((double(x) * x + double(y) * y) + double(z) * z); }
    float GetMagnitude() const { return float(std::sqrt(double(GetSquareMagnitude()))); }
    const bCVector& Normalize() {   // Nullvektor bleibt unveraendert
        const float len = GetMagnitude();
        if (len != 0.0f) {
            const float inv = 1.0f / len;
            x *= inv, y *= inv, z *= inv;
        }
        return *this;
    }
    bCVector GetNormalized() const {
        bCVector r = *this;
        r.Normalize();
        return r;
    }
    float GetDotProduct(const bCVector& o) const { return float(double(x) * o.x + double(y) * o.y + double(z) * o.z); }
    bCVector GetCrossProduct(const bCVector& o) const {
        bCVector r;
        r.x = y * o.z - z * o.y, r.y = z * o.x - x * o.z, r.z = x * o.y - y * o.x;
        return r;
    }
};
class SB_API bCVector2 {
public:
    float x, y;
    float GetMagnitude() const { return float(std::sqrt(double(x) * x + double(y) * y)); }
    const bCVector2& Normalize() {
        const float len = GetMagnitude();
        if (len != 0.0f) x /= len, y /= len;
        return *this;
    }
};
class SB_API bCVector4 {
public:
    float x, y, z, w;
    float GetMagnitude() const { return float(std::sqrt(double(x) * x + double(y) * y + double(z) * z + double(w) * w)); }
    const bCVector4& Normalize() {
        const float len = GetMagnitude();
        if (len != 0.0f) x /= len, y /= len, z /= len, w /= len;
        return *this;
    }
};

class bCMatrix3;
class bCQuaternion;
static void Mat3FromQuat(float r[3][3], const bCQuaternion& q);

// ---------------------------------------------------------------- bCQuaternion (x, y, z, w)
class SB_API bCQuaternion {
public:
    float x, y, z, w;
    void Clear() { x = y = z = 0.0f, w = 1.0f; }
    float GetMagnitude() const { return float(std::sqrt(double(x) * x + double(y) * y + double(z) * z + double(w) * w)); }
    void Normalize() {
        const float sq = x * x + y * y + z * z + w * w;
        if (sq == 0.0f) return;
        const float inv = 1.0f / std::sqrt(sq);
        x *= inv, y *= inv, z *= inv, w *= inv;
    }
    void Invert() {
        const float inv = 1.0f / (x * x + y * y + z * z + w * w);
        x *= -inv, y *= -inv, z *= -inv, w *= inv;
    }
    bCQuaternion operator*(const bCQuaternion& b) const {   // wie Original SetToProduct(*this, b)
        const bCQuaternion& a = *this;
        bCQuaternion r;
        r.x = (a.y * b.z + a.w * b.x + b.w * a.x) - a.z * b.y;
        r.y = (a.z * b.x + a.w * b.y + a.y * b.w) - b.z * a.x;
        r.z = (a.x * b.y + b.w * a.z + b.z * a.w) - a.y * b.x;
        r.w = ((a.w * b.w - a.x * b.x) - a.y * b.y) - b.z * a.z;
        return r;
    }
    void SetQuaternion(const bCMatrix3& m);
};

// ---------------------------------------------------------------- bCMatrix3 (zeilenweise, Zeilenvektor-Konvention v' = v * M)
class SB_API bCMatrix3 {
public:
    float m[3][3];
    void SetIdentity() {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) m[i][j] = i == j ? 1.0f : 0.0f;
    }
    void Transpose() {
        std::swap(m[0][1], m[1][0]);
        std::swap(m[0][2], m[2][0]);
        std::swap(m[1][2], m[2][1]);
    }
    void Invert() {
        const double det = double(m[0][0]) * (double(m[1][1]) * m[2][2] - double(m[1][2]) * m[2][1]) -
                           double(m[0][1]) * (double(m[1][0]) * m[2][2] - double(m[1][2]) * m[2][0]) +
                           double(m[0][2]) * (double(m[1][0]) * m[2][1] - double(m[1][1]) * m[2][0]);
        if (det == 0.0) {
            SetIdentity();
            return;
        }
        float r[3][3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                const int r0 = (i + 1) % 3, r1 = (i + 2) % 3, c0 = (j + 1) % 3, c1 = (j + 2) % 3;
                r[j][i] = float((double(m[r0][c0]) * m[r1][c1] - double(m[r0][c1]) * m[r1][c0]) / det);
            }
        std::memcpy(m, r, sizeof m);
    }
    bCMatrix3 operator*(const bCMatrix3& b) const {   // Original: Ergebnis = b * this (mathematisch)
        bCMatrix3 r;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) r.m[i][j] = b.m[i][0] * m[0][j] + b.m[i][1] * m[1][j] + b.m[i][2] * m[2][j];
        return r;
    }
    bCVector operator*(const bCVector& v) const {
        bCVector r;
        r.x = v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0];
        r.y = v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1];
        r.z = v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2];
        return r;
    }
    void SetToRotationAxisX(float a) {
        const float c = std::cos(a), s = std::sin(a);
        SetIdentity();
        m[1][1] = c, m[1][2] = s, m[2][1] = -s, m[2][2] = c;
    }
    void SetToRotationAxisY(float a) {
        const float c = std::cos(a), s = std::sin(a);
        SetIdentity();
        m[0][0] = c, m[0][2] = -s, m[2][0] = s, m[2][2] = c;
    }
    void SetToRotationAxisZ(float a) {
        const float c = std::cos(a), s = std::sin(a);
        SetIdentity();
        m[0][0] = c, m[0][1] = s, m[1][0] = -s, m[1][1] = c;
    }
    void SetToRotation(const bCQuaternion& q) { Mat3FromQuat(m, q); }
};

static void Mat3FromQuat(float r[3][3], const bCQuaternion& q) {
    const float x = q.x, y = q.y, z = q.z, w = q.w;
    const float x2 = x + x, y2 = y + y, z2 = z + z;
    const float xx = x * x2, yy = y * y2, zz = z * z2, xy = x * y2, xz = x * z2, yz = y * z2, wx = x2 * w, wy = y2 * w, wz = z2 * w;
    r[0][0] = 1.0f - (zz + yy), r[0][1] = wz + xy, r[0][2] = xz - wy;
    r[1][0] = xy - wz, r[1][1] = 1.0f - (zz + xx), r[1][2] = wx + yz;
    r[2][0] = xz + wy, r[2][1] = yz - wx, r[2][2] = 1.0f - (xx + yy);
}

void bCQuaternion::SetQuaternion(const bCMatrix3& mat) {
    const float(*m)[3] = mat.m;
    const double m00 = m[0][0], m11 = m[1][1], m22 = m[2][2];
    const float trace1 = float(m00 + m11 + m22 + 1.0);
    if (1e-5f < trace1) {
        const double s = float(0.5 / std::sqrt(double(trace1)));
        x = float((double(m[1][2]) - m[2][1]) * s);
        y = float((double(m[2][0]) - m[0][2]) * s);
        z = float((double(m[0][1]) - m[1][0]) * s);
        w = float(0.25 / s);
        return;
    }
    if (m11 < m00 && m22 < m00) {
        const double t = 2.0 * std::sqrt(1.0 + m00 - m11 - m22);
        x = float(0.25 * t), y = float((double(m[1][0]) + m[0][1]) / t), z = float((double(m[2][0]) + m[0][2]) / t),
        w = float((double(m[2][1]) - m[1][2]) / t);
    } else if (m22 < m11) {
        const double t = 2.0 * std::sqrt(1.0 + m11 - m00 - m22);
        x = float((double(m[1][0]) + m[0][1]) / t), y = float(0.25 * t), z = float((double(m[2][1]) + m[1][2]) / t),
        w = float((double(m[2][0]) - m[0][2]) / t);
    } else {
        const double t = 2.0 * std::sqrt(1.0 + m22 - m00 - m11);
        x = float((double(m[2][0]) + m[0][2]) / t), y = float((double(m[2][1]) + m[1][2]) / t), z = float(0.25 * t),
        w = float((double(m[1][0]) - m[0][1]) / t);
    }
}

// ---------------------------------------------------------------- bCMatrix (4x4, zeilenweise, Zeile 3 = Verschiebung)
class SB_API bCMatrix {
public:
    float m[4][4];
    void SetIdentity() {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) m[i][j] = i == j ? 1.0f : 0.0f;
    }
    void Transpose() {
        for (int r = 0; r < 4; ++r)
            for (int c = r + 1; c < 4; ++c) std::swap(m[r][c], m[c][r]);
    }
    float GetDeterminant() const {
        double a[4][4];
        Load(a);
        return float(Det4(a));
    }
    void Invert() {   // allgemeine Inverse (Adjunkte / Determinante), Determinante 0 -> Einheitsmatrix
        double a[4][4];
        Load(a);
        const double det = Det4(a);
        if (det == 0.0) {
            SetIdentity();
            return;
        }
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) {
                double sub[3][3];
                for (int r = 0, rr = 0; r < 4; ++r) {
                    if (r == j) continue;
                    for (int c = 0, cc = 0; c < 4; ++c) {
                        if (c == i) continue;
                        sub[rr][cc++] = a[r][c];
                    }
                    ++rr;
                }
                const double cof = Det3(sub) * (((i + j) & 1) ? -1.0 : 1.0);
                m[i][j] = float(cof / det);
            }
    }
    bCMatrix operator*(const bCMatrix& b) const {   // Original: Ergebnis = b * this (mathematisch)
        bCMatrix r;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) {
                double acc = 0.0;
                for (int k = 0; k < 4; ++k) acc += double(b.m[i][k]) * m[k][j];
                r.m[i][j] = float(acc);
            }
        return r;
    }
    bCVector operator*(const bCVector& v) const {   // Punkt: v * Drehung + Verschiebung
        bCVector r;
        r.x = v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + m[3][0];
        r.y = v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + m[3][1];
        r.z = v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + m[3][2];
        return r;
    }
    void SetToTranslation(const bCVector& t) {
        SetIdentity();
        m[3][0] = t.x, m[3][1] = t.y, m[3][2] = t.z;
    }
    void SetToScaling(const bCVector& s) {
        SetIdentity();
        m[0][0] = s.x, m[1][1] = s.y, m[2][2] = s.z;
    }
    void SetToRotation(const bCQuaternion& q) {
        SetIdentity();
        float r[3][3];
        Mat3FromQuat(r, q);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) m[i][j] = r[i][j];
    }
    void SetToRotationAxisX(float a) {
        bCMatrix3 r;
        r.SetToRotationAxisX(a);
        From3(r);
    }
    void SetToRotationAxisY(float a) {
        bCMatrix3 r;
        r.SetToRotationAxisY(a);
        From3(r);
    }
    void SetToRotationAxisZ(float a) {
        bCMatrix3 r;
        r.SetToRotationAxisZ(a);
        From3(r);
    }

private:
    void Load(double a[4][4]) const {
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c) a[r][c] = m[r][c];
    }
    void From3(const bCMatrix3& r) {
        SetIdentity();
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) m[i][j] = r.m[i][j];
    }
    static double Det3(const double s[3][3]) {
        return s[0][0] * (s[1][1] * s[2][2] - s[1][2] * s[2][1]) - s[0][1] * (s[1][0] * s[2][2] - s[1][2] * s[2][0]) +
               s[0][2] * (s[1][0] * s[2][1] - s[1][1] * s[2][0]);
    }
    static double Det4(const double a[4][4]) {
        double d = 0.0;
        for (int c = 0; c < 4; ++c) {
            double sub[3][3];
            for (int r = 1; r < 4; ++r)
                for (int j = 0, cc = 0; j < 4; ++j)
                    if (j != c) sub[r - 1][cc++] = a[r][j];
            d += ((c & 1) ? -1.0 : 1.0) * a[0][c] * Det3(sub);
        }
        return d;
    }
};

// ---------------------------------------------------------------- Protokoll des Spiels
extern "C" SB_API void __cdecl GE_MESSAGEF_INFO(const char* fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(b, sizeof b, fmt ? fmt : "", ap);
    va_end(ap);
    OutputDebugStringA(b);
}

// Haelt den Import von dbghelp.dll (MiniDumpWriteDump) wie beim Original fest; wird nie aufgerufen.
extern "C" SB_API void* g3asm_sb_stub_dbghelp() { return reinterpret_cast<void*>(&MiniDumpWriteDump); }
