// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 OG3-Patch authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// Eingriffe mitten in Spielskripten der Script_Game.dll (Skript-Schnittstelle ab Version 42). Anders als die uebrigen Hooks haengen sie nicht an einem
// Export, sondern an einer festen Stelle: Vor dem Umleiten werden die Originalbytes Byte fuer Byte verglichen. Passt eine Stelle nicht (andere Version der
// Script_Game.dll, oder eine Script-DLL wie Script_MonsterDamage.dll hat sie schon geaendert), bleibt sie unberuehrt und das Protokoll sagt es.
//
// Monsterschaden (AssessHit, Script_Game.dll RVA 0x2C05F, gemessen am Assembler 2026-11-10):
//   Trifft ein Nicht-Humanoider (Monster, Tier) einen Humanoiden und ist keiner der beiden der Held, teilt AssessHit den Schaden durch 10, bei beschworenen
//   Angreifern (PSParty::PartyMemberType == 5) durch 2 (eingefuehrt mit Patch 1.12). Danach rechnet AssessHit weiter (Verdopplung, Ruestung, Untergrenzen).
//   An der Stelle steht der Schaden in esi, der Typ des Angreifers in eax; die Script.dll-Entities liegen im Stapelrahmen: Opfer [esp+30h], Angreifer [esp+198h]
//   (erstes Feld je eCEntity*). Ersetzt werden die 31 Byte von "cmp eax,5" bis vor "mov esi,eax" (RVA 0x2C07E), dort geht es mit dem Ergebnis in eax weiter.
//   bool Override_MonsterDamage(Entity victim, Entity attacker, int damage, int divisor): Super() teilt damage durch divisor (Argumente mit SetOverrideArg
//   aenderbar, divisor <= 1 = keine Teilung); ohne Super() bekommt das Spiel damage ungeteilt.
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "script_host.h"

namespace og3p {
namespace {

ScriptHost* g_host = nullptr;

constexpr unsigned kMdRva = 0x2C05F;
constexpr unsigned kMdBackRva = 0x2C07E;
const unsigned char kMdOrig[31] = {
    0x83, 0xF8, 0x05,                     // cmp eax,5
    0x75, 0x09,                           // jne +9
    0x8B, 0xC6,                           // mov eax,esi
    0x99,                                 // cdq
    0x2B, 0xC2,                           // sub eax,edx
    0xD1, 0xF8,                           // sar eax,1
    0xEB, 0x11,                           // jmp 2C07E
    0xB8, 0x67, 0x66, 0x66, 0x66,         // mov eax,66666667h
    0xF7, 0xEE,                           // imul esi
    0xC1, 0xFA, 0x02,                     // sar edx,2
    0x8B, 0xC2,                           // mov eax,edx
    0xC1, 0xE8, 0x1F,                     // shr eax,1Fh
    0x03, 0xC2,                           // add eax,edx
};
static_assert(sizeof(kMdOrig) == kMdBackRva - kMdRva, "Laenge der Stelle");

void* g_mdBack = nullptr;
int g_mdLogged = 0;
const char* const kMdDecl = "bool Override_MonsterDamage(Entity, Entity, int, int)";

void* EntityAt(const char* frame, unsigned off) {
    const void* slot = frame + off;
    if (!ReadableMem(slot, sizeof(void*))) return nullptr;
    void* e = *static_cast<void* const*>(slot);
    return e && ReadableMem(e, sizeof(void*)) ? e : nullptr;
}

int Divide(int damage, int divisor) {
    return divisor > 1 ? damage / divisor : damage;   // wie das Original: Teilung mit Abschneiden Richtung 0
}

// Aufruf aus dem Stub: raw = Schaden vor der Teilung, partyType = Typ des Angreifers, frame = esp an der Stelle
int __cdecl MonsterDamage(int raw, int partyType, const char* frame) {
    const int divisor = partyType == 5 ? 2 : 10;
    if (!g_host || !OnGameThread() || !g_host->HasHandler(kMdDecl)) return Divide(raw, divisor);
    void* victim = EntityAt(frame, 0x30);
    void* attacker = EntityAt(frame, 0x198);
    EvArg a[4] = {EvArg::Ent(victim), EvArg::Ent(attacker), EvArg::U(static_cast<unsigned>(raw)), EvArg::U(static_cast<unsigned>(divisor))};
    int out = raw;
    bool ran = false;
    bool res = false;
    EvArg fin[4] = {a[0], a[1], a[2], a[3]};
    const bool taken = g_host->CallOverride(
        kMdDecl, a, 4,
        [&](const EvArg* x) {
            out = Divide(static_cast<int>(x[2].value), static_cast<int>(x[3].value));
            return true;
        },
        &res, &ran, fin);
    int result = Divide(raw, divisor);
    if (taken) result = ran ? out : static_cast<int>(fin[2].value);
    if (g_mdLogged < 10) {
        ++g_mdLogged;
        char b[200];
        std::snprintf(b, sizeof b, "Monsterschaden: roh %d, Teiler %d, an das Spiel %d (Override %s, Original %s)", raw, divisor, result, taken ? "ja" : "nein",
                      ran ? "ja" : "nein");
        g_host->Log(b);
    }
    return result;
}

__declspec(naked) void MonsterDamageStub() {
    __asm {
        push ecx
        push edx
        lea edx, [esp + 8]   // esp an der Stelle
        push edx
        push eax
        push esi
        call MonsterDamage
        add esp, 12
        pop edx
        pop ecx
        jmp dword ptr [g_mdBack]
    }
}

bool BytesMatch(const unsigned char* at, const unsigned char* orig, size_t n) {
    return ReadableMem(at, n) && std::memcmp(at, orig, n) == 0;
}
bool PatchAt(unsigned char* at, const unsigned char* orig, size_t n, void* stub, const char* what) {
    if (!BytesMatch(at, orig, n)) {
        g_host->Log(std::string(what) + ": Stelle passt nicht (andere Version oder schon von einer Script-DLL geaendert, z. B. "
                    "Script_MonsterDamage.dll oder Script_LearningThresholdTweaks.dll); Hook aus");
        return false;
    }
    DWORD old = 0;
    if (!VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old)) return false;
    at[0] = 0xE9;
    const long rel = static_cast<long>(reinterpret_cast<const char*>(stub) - reinterpret_cast<const char*>(at + 5));
    std::memcpy(at + 1, &rel, 4);
    std::memset(at + 5, 0x90, n - 5);
    VirtualProtect(at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, n);
    return true;
}

// ---------------------------------------------------------------- Lernkosten (Game.dll, gCInfo_PS, gemessen am Assembler 2026-11-10)
// Eine Lehr-Info (ConditionType Teach = 16, Feld +0x28) mit TeachAttrib (+0x88, z. B. "STR") erhoeht das Attribut um TeachAttribValue (+0x8C) und kostet
// ebenso viele Lernpunkte, dazu GoldCost (+0x34) Gold. Mit dem Schalter "Alternative Balance" der Community-Patches (eSSetupEngine +0x1EA) verdoppeln sich
// Lernpunkte und Gold, wenn STR/DEX/INT schon 250 erreicht haben. Die Werte stehen nicht an einer Stelle, sondern werden dreimal ausgerechnet:
//   Execute (Pruefung LP: [esp+18h] an 0x1C0C2B, ebp = Info; Pruefung und Bezahlung Gold: [esp+18h] an 0x1C0F35),
//   OnEndInfo (Bezahlung LP: LP = [ebp+58h] - [ebx+8Ch] an 0x1C1B48, bei Verdopplung vorher ein zweites Mal an 0x1C1B2F; ebx = Info),
//   GetDialogDescription (Anzeige LP: [esp+10h] an 0x1C637C, Info [esp+18h], Verdopplung Byte [esp+3Ch]; Anzeige Gold an 0x1C647B verdoppelt bzw. 0x1C64CF).
// An allen sieben Stellen fragt der Hook dieselbe Kette: bool Override_LearnCost(info, attrib, amount, lp, gold) - lp und gold sind die Werte des Spiels
// (mit Verdopplung), SetOverrideArg(3/4, ..) aendert sie. Nur Lehr-Infos fuer Attribute (TeachAttrib gesetzt, keine TeachSkill-Vorlage). Die Stellen werden
// nur alle zusammen umgeleitet (sonst zeigte der Dialog einen anderen Preis als bezahlt wird).
constexpr unsigned kLcCount = 7;
struct LcSite {
    unsigned rva, len;
    unsigned char orig[13];
};
const LcSite kLc[kLcCount] = {
    {0x1C0C2B, 8, {0x8B, 0x54, 0x24, 0x1C, 0x39, 0x54, 0x24, 0x18}},                                // mov edx,[esp+1Ch]; cmp [esp+18h],edx
    {0x1C0F35, 5, {0x33, 0xF6, 0x39, 0x77, 0x34}},                                                  // xor esi,esi; cmp [edi+34h],esi
    {0x1C1B2F, 9, {0x8B, 0x45, 0x58, 0x2B, 0x83, 0x8C, 0x00, 0x00, 0x00}},                          // mov eax,[ebp+58h]; sub eax,[ebx+8Ch]
    {0x1C1B48, 9, {0x8B, 0x55, 0x58, 0x2B, 0x93, 0x8C, 0x00, 0x00, 0x00}},                          // mov edx,[ebp+58h]; sub edx,[ebx+8Ch]
    {0x1C637C, 5, {0x83, 0x7C, 0x24, 0x10, 0x00}},                                                  // cmp dword ptr [esp+10h],0
    {0x1C647B, 13, {0x8B, 0x44, 0x24, 0x24, 0x8B, 0x48, 0x34, 0x8D, 0x54, 0x24, 0x38, 0x03, 0xC9}},  // mov eax,[esp+24h]; mov ecx,[eax+34h]; lea edx,[esp+38h]; add ecx,ecx
    {0x1C64CF, 7, {0x8B, 0x4C, 0x24, 0x20, 0x8B, 0x51, 0x34}},                                      // mov ecx,[esp+20h]; mov edx,[ecx+34h]
};
void* g_lcBack[kLcCount] = {};
const char* const kLcDecl = "bool Override_LearnCost(const bCString &in, const bCString &in, int, int, int)";
typedef bool(__thiscall* ProxyValidFn)(const void*);
ProxyValidFn g_proxyValid = nullptr;
int g_lcLogged = 0;
bool g_lcPayDouble = false;   // OnEndInfo: erste (zusaetzliche) Zahlung ausgelassen, die zweite zahlt den verdoppelten Preis

int ReadInt(const char* p, unsigned off) {
    return ReadableMem(p + off, 4) ? *reinterpret_cast<const int*>(p + off) : 0;
}
// Lehr-Info fuer ein Attribut?
bool IsAttribTeach(const char* info) {
    if (!info || !ReadableMem(info, 0x90)) return false;
    if (ReadInt(info, 0x28) != 16) return false;
    const char* a = SafeStrText(info + 0x88, nullptr);
    if (!a || !*a) return false;
    return !(g_proxyValid && g_proxyValid(info + 0x7C));
}
// which: 0 = Lernpunkte, 1 = Gold; doubled: Verdopplung des Spiels aktiv. Rueckgabe: Wert fuer das Spiel
int LearnCost(const char* info, int which, bool doubled, int site) {
    const int amount = ReadInt(info, 0x8C);
    const int gold = ReadInt(info, 0x34);
    const int m = doubled ? 2 : 1;
    const int orig = which ? gold * m : amount * m;
    if (!g_host || !OnGameThread() || !g_host->HasHandler(kLcDecl) || !IsAttribTeach(info)) return orig;
    EvArg a[5] = {EvArg::Str(info + 0x0C), EvArg::Str(info + 0x88), EvArg::U(static_cast<unsigned>(amount)), EvArg::U(static_cast<unsigned>(amount * m)),
                  EvArg::U(static_cast<unsigned>(gold * m))};
    EvArg fin[5] = {a[0], a[1], a[2], a[3], a[4]};
    bool res = false, ran = false;
    if (!g_host->CallOverride(kLcDecl, a, 5, [](const EvArg*) { return true; }, &res, &ran, fin)) return orig;
    int v = static_cast<int>(fin[which ? 4 : 3].value);
    if (v < 0) v = 0;
    if (g_lcLogged < 20) {
        ++g_lcLogged;
        char b[220];
        std::snprintf(b, sizeof b, "Lernkosten: Stelle %d, Info '%s', %s +%d, %s %d -> %d%s", site, SafeStrText(info + 0x0C, "?"), SafeStrText(info + 0x88, "?"),
                      amount, which ? "Gold" : "LP", orig, v, doubled ? " (verdoppelt)" : "");
        g_host->Log(b);
    }
    return v;
}
// Execute: Wert im Stapel (LP oder Gold, evtl. verdoppelt); Verdopplung erkennt man am Verhaeltnis zum Feld
void __cdecl LcExecute(const char* info, int which, int* slot) {
    const int base = ReadInt(info, which ? 0x34 : 0x8C);
    const bool doubled = base != 0 && *slot == 2 * base;
    if (!doubled && *slot != base) return;   // nicht der Attributweg (z. B. Talent): unveraendert
    *slot = LearnCost(info, which, doubled, which ? 2 : 1);
}
// OnEndInfo: Betrag, der von den LP abgezogen wird; first = zusaetzliche Zahlung der Verdopplung
int __cdecl LcPay(const char* info, int first) {
    const int amount = ReadInt(info, 0x8C);
    if (!g_host || !g_host->HasHandler(kLcDecl) || !IsAttribTeach(info)) {
        g_lcPayDouble = false;
        return amount;
    }
    if (first) {
        g_lcPayDouble = true;
        return 0;
    }
    const bool d = g_lcPayDouble;
    g_lcPayDouble = false;
    return LearnCost(info, 0, d, 3);
}
void __cdecl LcDescLp(const char* info, int* slot, int doubledFlag) {
    if (!IsAttribTeach(info)) return;
    *slot = LearnCost(info, 0, (doubledFlag & 0xFF) != 0, 4);
}
int __cdecl LcDescGold(const char* info, int doubled) {
    return LearnCost(info, 1, doubled != 0, 5);
}

// Stubs: jeweils die ueberschriebenen Befehle nachbilden und hinter die Stelle zurueckspringen
__declspec(naked) void LcStub0() {   // Execute LP: [esp+18h] Kosten, ebp Info
    __asm {
        push eax
        push ecx
        push edx
        lea eax, [esp + 12 + 18h]
        push eax
        push 0
        push ebp
        call LcExecute
        add esp, 12
        pop edx
        pop ecx
        pop eax
        mov edx, [esp + 1Ch]
        cmp [esp + 18h], edx
        jmp dword ptr [g_lcBack + 0]
    }
}
__declspec(naked) void LcStub1() {   // Execute Gold: [esp+18h] Gold, ebp Info
    __asm {
        push eax
        push ecx
        push edx
        lea eax, [esp + 12 + 18h]
        push eax
        push 1
        push ebp
        call LcExecute
        add esp, 12
        pop edx
        pop ecx
        pop eax
        xor esi, esi
        cmp [edi + 34h], esi
        jmp dword ptr [g_lcBack + 4]
    }
}
__declspec(naked) void LcStub2() {   // OnEndInfo zusaetzliche Zahlung: eax = LP - Betrag, ebx Info, ebp Spielergedaechtnis
    __asm {
        push ecx
        push edx
        push 1
        push ebx
        call LcPay
        add esp, 8
        mov ecx, eax
        mov eax, [ebp + 58h]
        sub eax, ecx
        pop edx
        pop ecx
        jmp dword ptr [g_lcBack + 8]
    }
}
__declspec(naked) void LcStub3() {   // OnEndInfo Zahlung: edx = LP - Betrag
    __asm {
        push eax
        push ecx
        push 0
        push ebx
        call LcPay
        add esp, 8
        mov edx, [ebp + 58h]
        sub edx, eax
        pop ecx
        pop eax
        jmp dword ptr [g_lcBack + 12]
    }
}
__declspec(naked) void LcStub4() {   // Beschreibung LP: [esp+10h] Kosten, [esp+18h] Info, Byte [esp+3Ch] verdoppelt
    __asm {
        push eax
        push ecx
        push edx
        movzx eax, byte ptr [esp + 12 + 3Ch]
        push eax
        lea eax, [esp + 16 + 10h]
        push eax
        push dword ptr [esp + 20 + 18h]
        call LcDescLp
        add esp, 12
        pop edx
        pop ecx
        pop eax
        cmp dword ptr [esp + 10h], 0
        jmp dword ptr [g_lcBack + 16]
    }
}
__declspec(naked) void LcStub5() {   // Beschreibung Gold verdoppelt: ecx = Anzeigewert, eax = Info, edx = esp+38h
    __asm {
        mov eax, [esp + 24h]
        push eax
        push 1
        push eax
        call LcDescGold
        add esp, 8
        mov ecx, eax
        pop eax
        lea edx, [esp + 38h]
        jmp dword ptr [g_lcBack + 20]
    }
}
__declspec(naked) void LcStub6() {   // Beschreibung Gold: edx = Anzeigewert, ecx = Info, eax erhalten
    __asm {
        push eax
        mov ecx, [esp + 4 + 20h]
        push 0
        push ecx
        call LcDescGold
        add esp, 8
        mov edx, eax
        mov ecx, [esp + 4 + 20h]
        pop eax
        jmp dword ptr [g_lcBack + 24]
    }
}
void* const kLcStubs[kLcCount] = {reinterpret_cast<void*>(&LcStub0), reinterpret_cast<void*>(&LcStub1), reinterpret_cast<void*>(&LcStub2),
                                  reinterpret_cast<void*>(&LcStub3), reinterpret_cast<void*>(&LcStub4), reinterpret_cast<void*>(&LcStub5),
                                  reinterpret_cast<void*>(&LcStub6)};

int InstallLearnCost(ScriptHost& host) {
    HMODULE game = GetModuleHandleA("Game.dll");
    HMODULE engine = GetModuleHandleA("Engine.dll");
    if (!game || !engine) return 0;
    g_proxyValid = reinterpret_cast<ProxyValidFn>(GetProcAddress(engine, "?IsEntityIDValid@eCTemplateEntityProxy@@QBE_NXZ"));
    unsigned char* base = reinterpret_cast<unsigned char*>(game);
    for (unsigned i = 0; i < kLcCount; ++i)
        if (!BytesMatch(base + kLc[i].rva, kLc[i].orig, kLc[i].len)) {
            char b[200];
            std::snprintf(b, sizeof b, "Lernkosten: Stelle %u (Game.dll RVA 0x%X) passt nicht (andere Version oder Script_LearningThresholdTweaks.dll); Hook aus", i,
                          kLc[i].rva);
            host.Log(b);
            return 0;
        }
    if (!g_proxyValid) {
        host.Log("Lernkosten: eCTemplateEntityProxy::IsEntityIDValid fehlt; Hook aus");
        return 0;
    }
    int ok = 0;
    for (unsigned i = 0; i < kLcCount; ++i) {
        g_lcBack[i] = base + kLc[i].rva + kLc[i].len;
        if (PatchAt(base + kLc[i].rva, kLc[i].orig, kLc[i].len, kLcStubs[i], "Lernkosten")) ++ok;
    }
    return ok == int(kLcCount) ? 1 : 0;
}

}   // namespace

int InstallScriptGamePatches(ScriptHost& host) {
    static bool done = false;
    if (done) return 0;
    HMODULE sg = GetModuleHandleA("Script_Game.dll");
    if (!sg) return 0;   // laedt erst nach main(); naechste Weltaktivierung erneut
    done = true;
    g_host = &host;
    unsigned char* base = reinterpret_cast<unsigned char*>(sg);
    int ok = 0;
    g_mdBack = base + kMdBackRva;
    if (PatchAt(base + kMdRva, kMdOrig, sizeof kMdOrig, reinterpret_cast<void*>(&MonsterDamageStub), "Monsterschaden")) ++ok;
    const int lc = InstallLearnCost(host);
    ok += lc;
    char b[160];
    std::snprintf(b, sizeof b, "Feste Stellen: %d von 2 aktiv (Monsterschaden in Script_Game.dll, Lernkosten in Game.dll: %s)", ok, lc ? "7 von 7" : "aus");
    host.Log(b);
    return ok;
}

}   // namespace og3p
