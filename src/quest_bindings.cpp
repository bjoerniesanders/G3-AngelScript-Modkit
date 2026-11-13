// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 OG3-Patch authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// Quest-Bindings des OG3-Patches (Skript-Schnittstelle ab Version 6): Questsystem des Spiels steuern.
// Quellen im Spiel: Game.dll gCQuestManager_PS (Singleton, GetInstance) und gCQuest_PS. Quests liegen als ini-Dateien im Quest-Verzeichnis
// (Data\Quests.pak bzw. Ordner Quests); der Manager laedt sie beim Start (LoadQuestsFromInis).
// Jeder Aufruf laeuft unter SEH-Schutz; ohne Spiel/ohne Manager liefern die Funktionen false bzw. -1.
#include <windows.h>

#include <angelscript.h>

#include <cstdio>
#include <cstdint>
#include <map>
#include <string>
#include <cstring>

#include <MinHook.h>

#include "sb_api.h"
#include "pack_mod.h"
#include "script_host.h"

namespace og3p {
namespace {

typedef void*(__stdcall* InstFn)();
typedef void*(__thiscall* QuestByNameFn)(void*, const void*);
typedef bool(__thiscall* ByNameBoolFn)(void*, const void*);
typedef bool(__thiscall* NoArgBoolFn)(void*);
typedef bool(__thiscall* SetStatusFn)(void*, const int*);
typedef const void*(__thiscall* GetRefFn)(const void*);
typedef void(__thiscall* SetU32Fn)(void*, const unsigned*);
typedef void(__thiscall* LocCtorFn)(void*, const void*);
typedef void(__thiscall* LocDtorFn)(void*);
typedef void(__thiscall* SetLocFn)(void*, const void*);
typedef bool(__thiscall* FileFn)(void*, const void*);

struct QuestApi {
    bool tried = false, ok = false;
    InstFn instance = nullptr;
    QuestByNameFn add = nullptr, get = nullptr;
    ByNameBoolFn run = nullptr, succeed = nullptr, fail = nullptr, close = nullptr, remove = nullptr;
    NoArgBoolFn reloadInis = nullptr, isFinished = nullptr, isSucceeded = nullptr;
    SetStatusFn setStatus = nullptr;
    GetRefFn getStatus = nullptr, getXp = nullptr;
    SetU32Fn setXp = nullptr;
    LocCtorFn locCtor = nullptr;
    LocDtorFn locDtor = nullptr;
    SetLocFn setTopic = nullptr, setDesc = nullptr;
    FileFn readFile = nullptr;   // Schreiben (gCQuest_PS::Write) entfernt mit API 46: schrieb nie eine Datei
    GetRefFn getName = nullptr;
    GetRefFn getAttrib = nullptr, getAttribAmount = nullptr;
    SetLocFn setAttrib = nullptr;   // SetAttribSuccess(const bCString&)
    void(__thiscall* setAttribAmount)(void*, const long*) = nullptr;
} g_q;

bool Resolve() {
    if (g_q.ok) return true;
    HMODULE game = GetModuleHandleA("Game.dll");
    if (!game) return false;
    auto G = [&](const char* n) { return reinterpret_cast<void*>(GetProcAddress(game, n)); };
    g_q.instance = reinterpret_cast<InstFn>(G("?GetInstance@gCQuestManager_PS@@SGPAV1@XZ"));
    g_q.add = reinterpret_cast<QuestByNameFn>(G("?AddQuest@gCQuestManager_PS@@QAEPAVgCQuest_PS@@ABVbCString@@@Z"));
    g_q.get = reinterpret_cast<QuestByNameFn>(G("?GetQuest@gCQuestManager_PS@@QAEPAVgCQuest_PS@@ABVbCString@@@Z"));
    g_q.run = reinterpret_cast<ByNameBoolFn>(G("?RunQuest@gCQuestManager_PS@@QAE_NABVbCString@@@Z"));
    g_q.succeed = reinterpret_cast<ByNameBoolFn>(G("?SucceedQuest@gCQuestManager_PS@@QAE_NABVbCString@@@Z"));
    g_q.fail = reinterpret_cast<ByNameBoolFn>(G("?FailQuest@gCQuestManager_PS@@QAE_NABVbCString@@@Z"));
    g_q.close = reinterpret_cast<ByNameBoolFn>(G("?CloseQuest@gCQuestManager_PS@@QAE_NABVbCString@@@Z"));
    g_q.remove = reinterpret_cast<ByNameBoolFn>(G("?RemoveQuest@gCQuestManager_PS@@QAE_NABVbCString@@@Z"));
    g_q.reloadInis = reinterpret_cast<NoArgBoolFn>(G("?LoadQuestsFromInis@gCQuestManager_PS@@QAE_NXZ"));
    g_q.isFinished = reinterpret_cast<NoArgBoolFn>(G("?IsFinished@gCQuest_PS@@QBE_NXZ"));
    g_q.isSucceeded = reinterpret_cast<NoArgBoolFn>(G("?IsSucceeded@gCQuest_PS@@QBE_NXZ"));
    g_q.setStatus = reinterpret_cast<SetStatusFn>(G("?SetStatus@gCQuest_PS@@QAE_NABW4gEQuestStatus@@@Z"));
    g_q.getStatus = reinterpret_cast<GetRefFn>(G("?GetStatus@gCQuest_PS@@QBEABV?$bTPropertyContainer@W4gEQuestStatus@@@@XZ"));
    g_q.getXp = reinterpret_cast<GetRefFn>(G("?GetExperiencePoints@gCQuest_PS@@QBEABKXZ"));
    g_q.setXp = reinterpret_cast<SetU32Fn>(G("?SetExperiencePoints@gCQuest_PS@@QAEXABK@Z"));
    if (HMODULE eng = GetModuleHandleA("Engine.dll")) {
        g_q.locCtor = reinterpret_cast<LocCtorFn>(GetProcAddress(eng, "??0eCLocString@@QAE@ABVbCString@@@Z"));
        g_q.locDtor = reinterpret_cast<LocDtorFn>(GetProcAddress(eng, "??1eCLocString@@QAE@XZ"));
    }
    g_q.setTopic = reinterpret_cast<SetLocFn>(G("?SetLogTopic@gCQuest_PS@@QAEXABVeCLocString@@@Z"));
    g_q.setDesc = reinterpret_cast<SetLocFn>(G("?SetQuestDesc@gCQuest_PS@@QAEXABVeCLocString@@@Z"));
    g_q.readFile = reinterpret_cast<FileFn>(G("?Read@gCQuest_PS@@QAE_NABVbCString@@@Z"));
    g_q.getName = reinterpret_cast<GetRefFn>(G("?GetName@gCQuest_PS@@QBEABVbCString@@XZ"));
    g_q.getAttrib = reinterpret_cast<GetRefFn>(G("?GetAttribSuccess@gCQuest_PS@@QBEABVbCString@@XZ"));
    g_q.getAttribAmount = reinterpret_cast<GetRefFn>(G("?GetAttribSuccessAmount@gCQuest_PS@@QBEABJXZ"));
    g_q.setAttrib = reinterpret_cast<SetLocFn>(G("?SetAttribSuccess@gCQuest_PS@@QAEXABVbCString@@@Z"));
    g_q.setAttribAmount = reinterpret_cast<decltype(g_q.setAttribAmount)>(G("?SetAttribSuccessAmount@gCQuest_PS@@QAEXABJ@Z"));
    g_q.ok = g_q.instance && g_q.add && g_q.get && g_q.run && g_q.succeed && g_q.fail && g_q.close && g_q.remove;
    if (!g_q.tried && ScriptHost::s_current)
        ScriptHost::s_current->Log(g_q.ok ? "Quest-Bindings bereit" : "Quest-Bindings: Exporte fehlen");
    g_q.tried = true;
    return g_q.ok;
}

// ---- rohe, SEH-geschuetzte Aufrufe (kein C++-Objekt mit Destruktor in diesen Funktionen)
void* g_mgrOverride =
    nullptr;   // waehrend LoadQuestsFromInis: der Manager, der gerade aufgebaut wird (GetInstance() liefert dann noch den alten)
void* Manager() {
    if (g_mgrOverride) return g_mgrOverride;
    __try {
        return g_q.instance();
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
void* SafeQuest(QuestByNameFn f, void* mgr, const void* name) {
    __try {
        return f(mgr, name);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool SafeBool(ByNameBoolFn f, void* mgr, const void* name, bool* out) {
    __try {
        *out = f(mgr, name);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeNoArg(NoArgBoolFn f, void* self, bool* out) {
    __try {
        *out = f(self);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeStatus(void* q, int* out) {
    __try {
        const unsigned char* c = static_cast<const unsigned char*>(g_q.getStatus(q));   // bTPropertyContainer: vtable, dann der Wert
        *out = *reinterpret_cast<const int*>(c + 4);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeSetStatus(void* q, int v, bool* out) {
    __try {
        *out = g_q.setStatus(q, &v);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeGetXp(void* q, unsigned* out) {
    __try {
        *out = *static_cast<const unsigned*>(g_q.getXp(q));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeSetXp(void* q, unsigned v) {
    __try {
        g_q.setXp(q, &v);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void* FindQuest(const SStr& name) {
    if (!Resolve()) return nullptr;
    void* m = Manager();
    return m ? SafeQuest(g_q.get, m, &name) : nullptr;
}

// ---- Skript-Funktionen
bool QuestExists(const SStr& name) { return FindQuest(name) != nullptr; }
bool CreateQuest(const SStr& name) {
    if (!Resolve()) return false;
    void* m = Manager();
    if (!m) return false;
    if (SafeQuest(g_q.get, m, &name)) return true;
    return SafeQuest(g_q.add, m, &name) != nullptr;
}
bool Control(ByNameBoolFn f, const SStr& name) {
    if (!Resolve() || !f) return false;
    void* m = Manager();
    bool r = false;
    return m && SafeBool(f, m, &name, &r) && r;
}
bool RunQuest(const SStr& n) { return Resolve() && Control(g_q.run, n); }
bool SucceedQuest(const SStr& n) { return Resolve() && Control(g_q.succeed, n); }
bool FailQuest(const SStr& n) { return Resolve() && Control(g_q.fail, n); }
bool CloseQuest(const SStr& n) { return Resolve() && Control(g_q.close, n); }
bool RemoveQuest(const SStr& n) { return Resolve() && Control(g_q.remove, n); }
int GetQuestStatus(
    const SStr& name) {   // gEQuestStatus (0 Open, 1 Running, 2 Success, 3 Failed, 4 Obsolete, 5 Cancelled, 6 Lost, 7 Won), -1 unbekannt
    void* q = FindQuest(name);
    int v = -1;
    return q && g_q.getStatus && SafeStatus(q, &v) && v >= 0 && v <= 7 ? v : -1;
}
bool SetQuestStatus(const SStr& name, int status) {
    void* q = FindQuest(name);
    bool r = false;
    return q && g_q.setStatus && status >= 0 && status <= 7 && SafeSetStatus(q, status, &r) && r;
}
bool IsQuestFinished(const SStr& name) {
    void* q = FindQuest(name);
    bool r = false;
    return q && g_q.isFinished && SafeNoArg(g_q.isFinished, q, &r) && r;
}
bool IsQuestSucceeded(const SStr& name) {
    void* q = FindQuest(name);
    bool r = false;
    return q && g_q.isSucceeded && SafeNoArg(g_q.isSucceeded, q, &r) && r;
}
int GetQuestXP(const SStr& name) {
    void* q = FindQuest(name);
    unsigned v = 0;
    return q && g_q.getXp && SafeGetXp(q, &v) ? int(v) : -1;
}
bool SetQuestXP(const SStr& name, int xp) {
    void* q = FindQuest(name);
    return q && g_q.setXp && xp >= 0 && SafeSetXp(q, unsigned(xp));
}
bool ReloadQuestFiles() {
    if (!Resolve() || !g_q.reloadInis) return false;
    void* m = Manager();
    bool r = false;
    return m && SafeNoArg(g_q.reloadInis, m, &r) && r;
}

// ---- Texte, Dateien, Liste
bool SafeSetLoc(SetLocFn f, void* q, const void* text) {   // eCLocString (4 Byte) aus bCString bauen, setzen, freigeben
    __try {
        void* loc = nullptr;
        g_q.locCtor(&loc, text);
        f(q, &loc);
        g_q.locDtor(&loc);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeFile(FileFn f, void* q, const void* path, bool* out) {
    __try {
        *out = f(q, path);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void RegisterText(const SStr& id, const SStr& text);
bool SetQuestText(SetLocFn fn, const SStr& name, const char* prefix, const SStr& text) {
    void* q = FindQuest(name);
    if (!q || !fn || !g_q.locCtor || !g_q.locDtor) return false;
    SStr id((std::string(prefix) + name.c_str()).c_str());   // eigene Text-ID, z. B. OG3_QTOPIC_<Quest>
    RegisterText(id, text);
    return SafeSetLoc(fn, q, &id);
}
bool SetQuestTopic(const SStr& name, const SStr& text) { return SetQuestText(g_q.setTopic, name, "OG3_QTOPIC_", text); }
bool SetQuestDescription(const SStr& name, const SStr& text) { return SetQuestText(g_q.setDesc, name, "OG3_QDESC_", text); }
bool LoadQuestFile(const SStr& name, const SStr& path) {   // gCQuest_PS::Read(path): Eigenschaften der Quest aus ini-Datei lesen
    void* q = FindQuest(name);
    bool r = false;
    return q && g_q.readFile && SafeFile(g_q.readFile, q, &path, &r) && r;
}

// Questliste: gCQuestManager_PS::m_arrQuests (bTObjArray<bTAutoPOSmartPtr<gCQuest_PS>>) bei Manager+0x14, 12 Byte. Feldreihenfolge und
// Elementzeiger sind nicht dokumentiert -> QuestDebug() protokolliert die Rohwerte, GetQuestCount/NameAt versuchen die wahrscheinlichste Lesart.
bool SafeRaw(const void* p, void* out, size_t n) {
    if (!ReadableMem(p, n)) return false;
    __try {
        std::memcpy(out, p, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeQuestName(void* q, const char** out) {
    __try {
        *out = g_sb.strGetText(g_q.getName(q));
        return *out != nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// Name in einen Puffer kopieren (Zeichen werden unter SEH gelesen, nicht erst spaeter ausserhalb)
bool SafeQuestNameCopy(void* q, char* buf, size_t n) {
    __try {
        const char* t = g_sb.strGetText(g_q.getName(q));
        if (!t) return false;
        size_t i = 0;
        for (; i + 1 < n && t[i]; ++i) buf[i] = t[i];
        buf[i] = 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// Element der Questliste -> gCQuest_PS*. Die Liste ist ein bTObjArray<bTAutoPOSmartPtr<gCQuest_PS>>; ein Element (4 Byte) zeigt laut SDK auf ein
// bTPOPureSmartPtr (0x0C Byte: vtable, Zaehler, m_pObject), das Quest-Objekt steht also bei Element -> +8. Frueher wurde das Element selbst als Quest gelesen:
// GetName lieferte dann irgendeinen lesbaren Text ("3"), die Pruefung auf "Name vorhanden" liess das durch. Jetzt gilt ein Treffer nur, wenn der gelesene Name
// druckbar ist UND GetQuest(Name) wieder dasselbe Objekt liefert. Zur Sicherheit werden +8, +4 und das Element selbst probiert.
bool PrintableName(const char* s) {
    if (!s || !*s) return false;
    for (; *s; ++s)
        if (static_cast<unsigned char>(*s) < 0x20) return false;
    return true;
}
void* QuestFromElement(void* elem, char* nm, size_t n) {
    static const int offs[3] = {8, 4, -1};
    for (int k = 0; k < 3; ++k) {
        void* q = elem;
        if (offs[k] >= 0 && (!SafeRaw(static_cast<unsigned char*>(elem) + offs[k], &q, sizeof q) || !q)) continue;
        if (!SafeQuestNameCopy(q, nm, n) || !PrintableName(nm)) continue;
        if (FindQuest(SStr(nm)) == q) return q;
    }
    return nullptr;
}
void* QuestAtIndex(const unsigned char* d, int i, char* nm, size_t n) {
    void* elem = nullptr;
    return SafeRaw(d + 4 * i, &elem, sizeof elem) && elem ? QuestFromElement(elem, nm, n) : nullptr;
}
bool ArrayView(unsigned char** data, int* count) {
    if (!Resolve()) return false;
    void* m = Manager();
    unsigned w[3];
    if (!m || !SafeRaw(static_cast<unsigned char*>(m) + 0x14, w, sizeof w)) return false;
    // Lesart A: {Daten, Anzahl, Kapazitaet}; B: {Anzahl, Kapazitaet, Daten}; C: {Kapazitaet, Anzahl, Daten}
    const int cand[3][2] = {{0, 1}, {2, 0}, {2, 1}};
    for (auto& c : cand) {
        unsigned char* d = reinterpret_cast<unsigned char*>(static_cast<uintptr_t>(w[c[0]]));
        const int n = int(w[c[1]]);
        if (!d || n <= 0 || n > 100000) continue;
        char nm[96];
        if (QuestAtIndex(d, 0, nm, sizeof nm)) {
            *data = d;
            *count = n;
            return true;
        }
    }
    return false;
}
int GetQuestCount() {
    unsigned char* d;
    int n;
    return ArrayView(&d, &n) ? n : -1;
}
SStr GetQuestNameAt(int i) {
    unsigned char* d;
    int n;
    char nm[128];
    if (i >= 0 && ArrayView(&d, &n) && i < n && QuestAtIndex(d, i, nm, sizeof nm)) return SStr(nm);
    return SStr();
}
void QuestDebug() {
    ScriptHost* h = ScriptHost::s_current;
    if (!h || !Resolve()) return;
    void* m = Manager();
    unsigned w[3] = {0, 0, 0};
    char b[200];
    if (!m || !SafeRaw(static_cast<unsigned char*>(m) + 0x14, w, sizeof w)) {
        h->Log("QuestDebug: Manager nicht lesbar");
        return;
    }
    std::snprintf(b, sizeof b, "QuestDebug: Manager=%p Array-Rohwerte: %08x %08x %08x", m, w[0], w[1], w[2]);
    h->Log(b);
    unsigned char* d;
    int n;
    if (ArrayView(&d, &n)) {
        std::snprintf(b, sizeof b, "QuestDebug: %d Quests lesbar (Daten %p)", n, d);
        h->Log(b);
    } else
        h->Log("QuestDebug: Array nicht deutbar");
}

// ---- Eigene Texte fuer Text-IDs (eCLocString): Quest-Titel/-Beschreibung sind IDs der Stringtabelle des Spiels. Unbekannte IDs liefern
// einen leeren Text -> GetString()/GetString(bCString)/operator bCUnicodeString der Engine werden abgefangen; fuer registrierte IDs
// kommt unser Text, sonst das Original. Rueckgabe bCUnicodeString per Wert = versteckter Zeiger (erstes Stapelargument nach this).
typedef void*(__thiscall* LocGet0Fn)(const void*, void*);
typedef void*(__thiscall* LocGet1Fn)(const void*, void*, const void*);
typedef void(__thiscall* UniCtorFn)(void*, const wchar_t*);
LocGet0Fn o_locGet0 = nullptr, o_locConv = nullptr;
LocGet1Fn o_locGet1 = nullptr;
UniCtorFn g_uniCtor = nullptr;
std::map<std::string, std::wstring> g_texts;
bool g_textHooks = false;

const char* SafeLocId(const void* self) {
    __try {
        return g_sb.strGetText(self);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }   // eCLocString = ein bCString (4 Byte)
}
const wchar_t* FindText(const void* self) {
    if (g_texts.empty() || !self) return nullptr;
    const char* id = SafeLocId(self);
    if (!id || !*id) return nullptr;
    auto it = g_texts.find(id);
    return it == g_texts.end() ? nullptr : it->second.c_str();
}
void* __fastcall HLocGet0(const void* self, void*, void* ret) {
    if (const wchar_t* w = FindText(self)) {
        g_uniCtor(ret, w);
        return ret;
    }
    return o_locGet0(self, ret);
}
void* __fastcall HLocConv(const void* self, void*, void* ret) {
    if (const wchar_t* w = FindText(self)) {
        g_uniCtor(ret, w);
        return ret;
    }
    return o_locConv(self, ret);
}
void* __fastcall HLocGet1(const void* self, void*, void* ret, const void* arg) {
    if (const wchar_t* w = FindText(self)) {
        g_uniCtor(ret, w);
        return ret;
    }
    return o_locGet1(self, ret, arg);
}

bool InstallTextHooks() {
    if (g_textHooks) return true;
    HMODULE eng = GetModuleHandleA("Engine.dll"), sb = GetModuleHandleA("SharedBase.dll");
    if (!eng || !sb) return false;
    g_uniCtor = reinterpret_cast<UniCtorFn>(GetProcAddress(sb, "??0bCUnicodeString@@QAE@PB_W@Z"));
    void* t0 = reinterpret_cast<void*>(GetProcAddress(eng, "?GetString@eCLocString@@QBE?BVbCUnicodeString@@XZ"));
    void* t1 = reinterpret_cast<void*>(GetProcAddress(eng, "?GetString@eCLocString@@QBE?BVbCUnicodeString@@ABVbCString@@@Z"));
    void* tc = reinterpret_cast<void*>(GetProcAddress(eng, "??BeCLocString@@QBE?BVbCUnicodeString@@XZ"));
    ScriptHost* h = ScriptHost::s_current;
    if (!g_uniCtor || !t0 || !t1 || !tc) {
        if (h) h->Log("Texte: Exporte fehlen");
        return false;
    }
    const MH_STATUS st = MH_Initialize();
    if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) {
        if (h) h->Log("Texte: MinHook nicht startbar");
        return false;
    }
    int n = 0;
    n += MH_CreateHook(t0, reinterpret_cast<void*>(&HLocGet0), reinterpret_cast<void**>(&o_locGet0)) == MH_OK && MH_EnableHook(t0) == MH_OK;
    n += MH_CreateHook(t1, reinterpret_cast<void*>(&HLocGet1), reinterpret_cast<void**>(&o_locGet1)) == MH_OK && MH_EnableHook(t1) == MH_OK;
    n += MH_CreateHook(tc, reinterpret_cast<void*>(&HLocConv), reinterpret_cast<void**>(&o_locConv)) == MH_OK && MH_EnableHook(tc) == MH_OK;
    if (h) {
        char b[96];
        std::snprintf(b, sizeof b, "Texte: %d von 3 eCLocString-Hooks aktiv", n);
        h->Log(b);
    }
    g_textHooks = n == 3;
    return g_textHooks;
}

void RegisterText(const SStr& id, const SStr& text) {
    const char* t = text.c_str();
    const int len = MultiByteToWideChar(1252, 0, t, -1, nullptr, 0);
    std::wstring w(len > 0 ? len - 1 : 0, L'\0');
    if (len > 1) MultiByteToWideChar(1252, 0, t, -1, &w[0], len);
    g_texts[id.c_str()] = w;
    InstallTextHooks();   // ohne Spiel (Test) bleibt der Text gespeichert, wirkt aber nicht
}
// Unicode-Fassung fuer mod-eigene Stringtabellen. Speichert immer; die Hooks werden bei Bedarf spaeter nachgeholt (EnsureTextHooks).
void RegisterTextWideImpl(const std::string& id, const std::wstring& text) {
    g_texts[id] = text;
    InstallTextHooks();
}
bool EnsureTextHooksImpl() { return g_textHooks || InstallTextHooks(); }
// Name der aktuellen Spielsprache ("English", "German", ...); leer, solange Engine.dll nicht bereit ist
typedef void*(__stdcall* LocInstFn)();
typedef const void*(__thiscall* LocLangFn)(const void*);
const char* SafeLangName(LocInstFn inst, LocLangFn lang) {
    __try {
        void* a = inst();
        const void* s = a ? lang(a) : nullptr;
        return s ? g_sb.strGetText(s) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
std::string CurrentGameLanguageImpl() {
    HMODULE eng = GetModuleHandleA("Engine.dll");
    if (!eng) return std::string();
    LocInstFn inst = reinterpret_cast<LocInstFn>(GetProcAddress(eng, "?GetInstance@eCLocAdmin@@SGAAV1@XZ"));
    LocLangFn lang = reinterpret_cast<LocLangFn>(GetProcAddress(eng, "?GetCurrentLanguage@eCLocAdmin@@QBEABVbCString@@XZ"));
    const char* t = inst && lang ? SafeLangName(inst, lang) : nullptr;
    return t ? std::string(t) : std::string();
}

// Pfade
typedef void*(__stdcall* PathFn)(void*);
typedef void*(__stdcall* QuestPathFn)(void*, const void*);
SStr FromRaw(void*& raw) {
    SStr r(raw ? g_sb.strGetText(&raw) : "");
    if (raw) g_sb.strDtor(&raw);
    return r;
}
void* SafePath0(PathFn f) {
    void* raw = nullptr;
    __try {
        f(&raw);
    } __except (EXCEPTION_EXECUTE_HANDLER) { raw = nullptr; }
    return raw;
}
void* SafePath1(QuestPathFn f, void* q) {
    void* raw = nullptr;
    __try {
        f(&raw, q);
    } __except (EXCEPTION_EXECUTE_HANDLER) { raw = nullptr; }
    return raw;
}
SStr GetQuestDir() {
    HMODULE game = GetModuleHandleA("Game.dll");
    PathFn f = game ? reinterpret_cast<PathFn>(GetProcAddress(game, "?GetQuestDir@gCQuestManager_PS@@SG?AVbCString@@XZ")) : nullptr;
    void* raw = f ? SafePath0(f) : nullptr;
    return FromRaw(raw);
}
SStr GetQuestFilePath(const SStr& name) {
    HMODULE game = GetModuleHandleA("Game.dll");
    QuestPathFn f =
        game ? reinterpret_cast<QuestPathFn>(GetProcAddress(game, "?GetQuestFilePath@gCQuestManager_PS@@SG?AVbCString@@PBVgCQuest_PS@@@Z"))
             : nullptr;
    void* q = FindQuest(name);
    void* raw = f && q ? SafePath1(f, q) : nullptr;
    return FromRaw(raw);
}
SStr GetGameDir() {
    char b[MAX_PATH];
    GetModuleFileNameA(nullptr, b, MAX_PATH);
    std::string d(b);
    return SStr(d.substr(0, d.find_last_of("\\/") + 1));
}

// ---- Mod-Archive (pack_mod.cpp): Ordner als naechste freie Generation <Volume>.mod / .m00 / ... in <Spiel>\Data ablegen, bzw. Archiv an beliebigem Ort schreiben
std::string g_packError;
std::string GameDataDir() { return std::string(GetGameDir().c_str()) + "Data"; }
SStr ScriptPackMod(const SStr& folder, const SStr& volume, bool alternativeBalancing) {
    PackStats st;
    const std::string out = PackMod(folder.c_str(), GameDataDir(), volume.c_str(), alternativeBalancing ? 'n' : 'm', PackOptions(), &st);
    g_packError = out.empty() ? st.error : std::string();
    return SStr(out);
}
bool ScriptPackFolder(const SStr& folder, const SStr& file) {
    PackStats st;
    const bool ok = PackFolder(folder.c_str(), file.c_str(), PackOptions(), &st);
    g_packError = ok ? std::string() : st.error;
    return ok;
}
SStr ScriptNextModFile(const SStr& volume, bool alternativeBalancing) {
    return SStr(NextGenerationFile(GameDataDir(), volume.c_str(), alternativeBalancing ? 'n' : 'm'));
}
SStr ScriptPackError() { return SStr(g_packError); }

// ---- Attribut-Belohnung der Quest (wird beim Erfolg im Spiel angezeigt und vergeben)
bool SafeSetReward(void* q, const void* tag, long amount) {
    __try {
        g_q.setAttrib(q, tag);
        g_q.setAttribAmount(q, &amount);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeGetReward(void* q, char* tag, size_t n, long* amount) {   // Text und Zahl unter SEH in Puffer kopieren
    __try {
        const char* t = g_sb.strGetText(g_q.getAttrib(q));
        size_t i = 0;
        if (t)
            for (; i + 1 < n && t[i]; ++i) tag[i] = t[i];
        tag[i] = 0;
        *amount = *static_cast<const long*>(g_q.getAttribAmount(q));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SetQuestReward(const SStr& name, const SStr& attribute, int amount) {
    void* q = FindQuest(name);
    return q && g_q.setAttrib && g_q.setAttribAmount && SafeSetReward(q, &attribute, amount);
}
SStr GetQuestReward(const SStr& name) {   // "TAG=Menge" oder leer
    void* q = FindQuest(name);
    char tag[64];
    long amount = 0;
    if (!q || !g_q.getAttrib || !g_q.getAttribAmount || !SafeGetReward(q, tag, sizeof tag, &amount) || !*tag) return SStr();
    return SStr(std::string(tag) + "=" + std::to_string(amount));
}
// Alle Quests des Spiels: welche Attribut-Belohnungen kommen vor (Kennung, Anzahl, Beispiel)?
SStr DescribeQuestRewards() {
    unsigned char* d;
    int n;
    if (!ArrayView(&d, &n)) return SStr("(Questliste nicht lesbar)");
    std::map<std::string, std::pair<int, std::string>> tags;
    for (int i = 0; i < n; ++i) {
        char tag[64], nm[96];
        long amount = 0;
        void* q = QuestAtIndex(d, i, nm, sizeof nm);
        if (!q || !g_q.getAttrib || !g_q.getAttribAmount || !SafeGetReward(q, tag, sizeof tag, &amount) || !*tag) continue;
        auto& e = tags[tag];
        if (e.first++ == 0) { e.second = std::string(nm) + " (+" + std::to_string(amount) + ")"; }
    }
    std::string r = "Quests mit Attribut-Belohnung: " + std::to_string(tags.size()) + " verschiedene Kennungen";
    for (auto& kv : tags) r += "\\n  " + kv.first + " (" + std::to_string(kv.second.first) + "x), z. B. " + kv.second.second;
    return SStr(r);
}

const int kStatus[] = {0, 1, 2, 3, 4, 5, 6, 7};
const char* kStatusNames[] = {"QUEST_OPEN",     "QUEST_RUNNING",   "QUEST_SUCCESS", "QUEST_FAILED",
                              "QUEST_OBSOLETE", "QUEST_CANCELLED", "QUEST_LOST",    "QUEST_WON"};

}   // namespace

void RegisterTextWide(const std::string& id, const std::wstring& text) { RegisterTextWideImpl(id, text); }
bool EnsureTextHooks() { return EnsureTextHooksImpl(); }
std::string CurrentGameLanguage() { return CurrentGameLanguageImpl(); }
void RegisterTextForId(const std::string& id, const std::string& text) { RegisterText(SStr(id), SStr(text)); }

void SetQuestManagerOverride(void* mgr) { g_mgrOverride = mgr; }

bool RegisterQuestBindings(asIScriptEngine* e) {
    bool ok = true;
    OG3_REG(e->RegisterGlobalFunction("bool QuestExists(const bCString &in)", asFUNCTION(QuestExists), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool CreateQuest(const bCString &in)", asFUNCTION(CreateQuest), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool RunQuest(const bCString &in)", asFUNCTION(RunQuest), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool SucceedQuest(const bCString &in)", asFUNCTION(SucceedQuest), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool FailQuest(const bCString &in)", asFUNCTION(FailQuest), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool CloseQuest(const bCString &in)", asFUNCTION(CloseQuest), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool RemoveQuest(const bCString &in)", asFUNCTION(RemoveQuest), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("int GetQuestStatus(const bCString &in)", asFUNCTION(GetQuestStatus), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool SetQuestStatus(const bCString &in, int)", asFUNCTION(SetQuestStatus), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool IsQuestFinished(const bCString &in)", asFUNCTION(IsQuestFinished), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool IsQuestSucceeded(const bCString &in)", asFUNCTION(IsQuestSucceeded), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("int GetQuestXP(const bCString &in)", asFUNCTION(GetQuestXP), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool SetQuestXP(const bCString &in, int)", asFUNCTION(SetQuestXP), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool ReloadQuestFiles()", asFUNCTION(ReloadQuestFiles), asCALL_CDECL));
    OG3_REG(
        e->RegisterGlobalFunction("bool SetQuestTopic(const bCString &in, const bCString &in)", asFUNCTION(SetQuestTopic), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool SetQuestDescription(const bCString &in, const bCString &in)", asFUNCTION(SetQuestDescription),
                                      asCALL_CDECL));
    OG3_REG(
        e->RegisterGlobalFunction("bool LoadQuestFile(const bCString &in, const bCString &in)", asFUNCTION(LoadQuestFile), asCALL_CDECL));   // funktioniert nicht
    OG3_REG(e->RegisterGlobalFunction("int GetQuestCount()", asFUNCTION(GetQuestCount), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bCString GetQuestNameAt(int)", asFUNCTION(GetQuestNameAt), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool SetQuestReward(const bCString &in, const bCString &in, int)", asFUNCTION(SetQuestReward),
                                      asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bCString GetQuestReward(const bCString &in)", asFUNCTION(GetQuestReward), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bCString DescribeQuestRewards()", asFUNCTION(DescribeQuestRewards), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("void QuestDebug()", asFUNCTION(QuestDebug), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("void RegisterText(const bCString &in, const bCString &in)", asFUNCTION(RegisterText), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bCString GetQuestDir()", asFUNCTION(GetQuestDir), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bCString GetQuestFilePath(const bCString &in)", asFUNCTION(GetQuestFilePath), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bCString GetGameDir()", asFUNCTION(GetGameDir), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bCString PackMod(const bCString &in, const bCString &in, bool = false)", asFUNCTION(ScriptPackMod),
                                      asCALL_CDECL));
    OG3_REG(
        e->RegisterGlobalFunction("bool PackFolder(const bCString &in, const bCString &in)", asFUNCTION(ScriptPackFolder), asCALL_CDECL));
    OG3_REG(
        e->RegisterGlobalFunction("bCString NextModFile(const bCString &in, bool = false)", asFUNCTION(ScriptNextModFile), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bCString GetPackError()", asFUNCTION(ScriptPackError), asCALL_CDECL));
    for (int i = 0; i < 8; ++i) {
        char d[48];
        std::snprintf(d, sizeof d, "const int %s", kStatusNames[i]);
        OG3_REG(e->RegisterGlobalProperty(d, const_cast<int*>(&kStatus[i])));
    }
    return ok;
}

}   // namespace og3p
