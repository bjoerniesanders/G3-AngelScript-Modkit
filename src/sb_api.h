// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
#pragma once
// Bruecke zur geladenen SharedBase.dll (Original, Classic oder og3-Nachbau): Funktionszeiger werden ueber die dekorierten Exportnamen
// aufgeloest, die Huellklassen haben exakt das Speicherlayout der Genome-Typen. Damit arbeiten Skripte auf echten Original-Objekten
// und mit dem Originalverhalten (z. B. x87-Rundung der Vektorlaenge); nur + - * / der Vektoren rechnet die Huelle selbst.
// Aufrufkonvention: Memberfunktionen = thiscall; Rueckgabe einer Klasse per Wert ueber einen versteckten Zeiger (erstes Stapelargument
// nach this) - unter MSVC fuer ALLE Klassen-Rueckgaben von Memberfunktionen, unabhaengig von der Groesse.
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
    // bCMatrix (4x4, 16 floats zeilenweise, Zeilenvektor-Konvention v' = v * M, Zeilen 0..2 = Achsen X/Y/Z, Zeile 3 = Verschiebung)
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
    // Protokoll des Spiels (cdecl, printf-artig)
    void(__cdecl* geInfo)(const char*, ...) = nullptr;

    HMODULE module = nullptr;
    std::string missing;                // nicht gefundene Exporte (Leerzeichen-getrennt)
    bool Resolve(HMODULE sharedBase);   // false, wenn bCString-Grundfunktionen fehlen
};
extern SbApi g_sb;

// ---------------------------------------------------------------- Huellklassen (Layout = Genome-Typ)
class SStr {   // bCString: 4 Byte, Zeiger auf Zeichen mit Kopf (Referenzzaehler) davor
public:
    SStr() { g_sb.strCtor(this); }
    SStr(const char* s) { g_sb.strCtorText(this, s); }
    SStr(const std::string& s) { g_sb.strCtorText(this, s.c_str()); }
    SStr(const SStr& o) { g_sb.strCopy(this, &o); }
    ~SStr() { g_sb.strDtor(this); }
    SStr& operator=(const SStr& o) {   // NOLINT(bugprone-unhandled-self-assignment): bCString::operator= der Engine prueft das selbst
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
};   // bCMatrix: zeilenweise (Element i = Zeile i/4, Spalte i%4)
struct SMat3 {
    float m[9];
};   // zeilenweise (Element i = Zeile i/3, Spalte i%3)
static_assert(sizeof(SVec3) == 12 && sizeof(SQuat) == 16 && sizeof(SMat3) == 36 && sizeof(SMat4) == 64, "Layouts");

}   // namespace g3asm
