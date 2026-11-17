// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
#pragma once
// Bridge to the loaded SharedBase.dll (original, Classic or og3 rebuild): the function pointers are resolved through the decorated export names,
// and the wrapper classes have exactly the memory layout of the Genome types. Thus scripts work on real original objects
// and with the original behavior (for example x87 rounding of the vector length). Only + - * / of the vectors are calculated by the wrapper itself.
// Calling convention: member functions = thiscall. A class return value by value goes through a hidden pointer (first stack argument
// after this) - in MSVC for ALL class return values of member functions, independent of the size.
#include <windows.h>

#include <cstdint>
#include <string>

namespace g3asm {

struct SbApi {
    // bCString
    void(__thiscall* strCtor)(void*) = nullptr;
    void(__thiscall* strCtorText)(void*, const char*) = nullptr;
    void(__thiscall* strCopy)(void*, const void*) = nullptr;
    void(__thiscall* strDtor)(void*) = nullptr;
    void*(__thiscall* strAssign)(void*, const void*) = nullptr;
    void*(__thiscall* strAddAssign)(void*, const void*) = nullptr;
    const char*(__thiscall* strGetText)(const void*) = nullptr;
    int(__thiscall* strGetLength)(const void*) = nullptr;
    bool(__thiscall* strIsEmpty)(const void*) = nullptr;
    int(__thiscall* strCompare)(const void*, const void*) = nullptr;
    int(__thiscall* strCompareNoCase)(const void*, const void*) = nullptr;
    int(__thiscall* strFind)(const void*, const char*, int, bool) = nullptr;
    void*(__thiscall* strLeft)(const void*, void*, int) = nullptr;
    void*(__thiscall* strRight)(const void*, void*, int) = nullptr;
    void*(__thiscall* strMid)(const void*, void*, int, int) = nullptr;
    void(__thiscall* strToUpper)(void*) = nullptr;
    void(__thiscall* strToLower)(void*) = nullptr;
    // bCVector
    float(__thiscall* vecMagnitude)(const void*) = nullptr;
    float(__thiscall* vecSquareMagnitude)(const void*) = nullptr;
    const void*(__thiscall* vecNormalize)(void*) = nullptr;
    void*(__thiscall* vecGetNormalized)(const void*, void*) = nullptr;
    float(__thiscall* vecDot)(const void*, const void*) = nullptr;
    void*(__thiscall* vecCross)(const void*, void*, const void*) = nullptr;
    // bCVector2 / bCVector4
    float(__thiscall* vec2Magnitude)(const void*) = nullptr;
    const void*(__thiscall* vec2Normalize)(void*) = nullptr;
    float(__thiscall* vec4Magnitude)(const void*) = nullptr;
    const void*(__thiscall* vec4Normalize)(void*) = nullptr;
    // bCQuaternion
    void(__thiscall* quatClear)(void*) = nullptr;
    void(__thiscall* quatNormalize)(void*) = nullptr;
    void(__thiscall* quatInvert)(void*) = nullptr;
    float(__thiscall* quatMagnitude)(const void*) = nullptr;
    void*(__thiscall* quatMul)(const void*, void*, const void*) = nullptr;
    void(__thiscall* quatFromMatrix3)(void*, const void*) = nullptr;
    // bCMatrix3
    void(__thiscall* m3Identity)(void*) = nullptr;
    void(__thiscall* m3Transpose)(void*) = nullptr;
    void(__thiscall* m3Invert)(void*) = nullptr;
    void*(__thiscall* m3MulM3)(const void*, void*, const void*) = nullptr;
    void*(__thiscall* m3MulVec)(const void*, void*, const void*) = nullptr;
    // bCMatrix (4x4, 16 floats row by row, row vector convention v' = v * M, rows 0..2 = axes X/Y/Z, row 3 = translation)
    void(__thiscall* m4Identity)(void*) = nullptr;
    void(__thiscall* m4Transpose)(void*) = nullptr;
    void(__thiscall* m4Invert)(void*) = nullptr;
    float(__thiscall* m4Det)(const void*) = nullptr;
    void*(__thiscall* m4MulM4)(const void*, void*, const void*) = nullptr;
    void*(__thiscall* m4MulVec)(const void*, void*, const void*) = nullptr;
    void(__thiscall* m4Translation)(void*, const void*) = nullptr;
    void(__thiscall* m4Scaling)(void*, const void*) = nullptr;
    void(__thiscall* m4FromQuat)(void*, const void*) = nullptr;
    void(__thiscall* m4RotX)(void*, float) = nullptr;
    void(__thiscall* m4RotY)(void*, float) = nullptr;
    void(__thiscall* m4RotZ)(void*, float) = nullptr;
    void(__thiscall* m3RotX)(void*, float) = nullptr;
    void(__thiscall* m3RotY)(void*, float) = nullptr;
    void(__thiscall* m3RotZ)(void*, float) = nullptr;
    void(__thiscall* m3FromQuat)(void*, const void*) = nullptr;
    // log of the game (cdecl, printf style)
    void(__cdecl* geInfo)(const char*, ...) = nullptr;

    HMODULE module = nullptr;
    std::string missing;                // exports that were not found (separated by spaces)
    bool Resolve(HMODULE sharedBase);   // false if basic bCString functions are missing
};
extern SbApi g_sb;

// ---------------------------------------------------------------- wrapper classes (layout = Genome type)
class SStr {   // bCString: 4 bytes, pointer to the characters, with a header (reference counter) before them
public:
    SStr() { g_sb.strCtor(this); }
    SStr(const char* s) { g_sb.strCtorText(this, s); }
    SStr(const std::string& s) { g_sb.strCtorText(this, s.c_str()); }
    SStr(const SStr& o) { g_sb.strCopy(this, &o); }
    ~SStr() { g_sb.strDtor(this); }
    SStr& operator=(const SStr& o) {   // NOLINT(bugprone-unhandled-self-assignment): bCString::operator= of the engine checks this itself
        g_sb.strAssign(this, &o);
        return *this;
    }
    SStr& operator+=(const SStr& o) {
        g_sb.strAddAssign(this, &o);
        return *this;
    }
    const char* c_str() const {
        const char* t = g_sb.strGetText(this);
        return t ? t : "";
    }
    int length() const { return g_sb.strGetLength(this); }

private:
    void* p_;
};
static_assert(sizeof(SStr) == 4 || sizeof(void*) == 8, "bCString-Layout");

struct SVec2 {
    float x, y;
};
struct SVec3 {
    float x, y, z;
};
struct SVec4 {
    float x, y, z, w;
};
struct SQuat {
    float x, y, z, w;
};
struct SMat4 {
    float m[16];
};   // bCMatrix: row by row (element i = row i/4, column i%4)
struct SMat3 {
    float m[9];
};   // row by row (element i = row i/3, column i%3)
static_assert(sizeof(SVec3) == 12 && sizeof(SQuat) == 16 && sizeof(SMat3) == 36 && sizeof(SMat4) == 64, "Layouts");

}   // namespace g3asm
