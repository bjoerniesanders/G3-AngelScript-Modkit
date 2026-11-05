// Dialog-Bindings (Skript-Schnittstelle ab Version 10), Teil 1: die Infos des Spiels LESEN.
// Dialoge in Gothic 3 sind "Infos" (gCInfo_PS) im gCInfoManager_PS: jede Info hat Besitzer (NPC), Eltern-Info, Quest, Bedingungsart,
// Typ und ein Info-Skript aus Zeilen (gCInfoScriptLine_PS: Befehl, Entity1/2, ID1/2, Text als Text-ID). Bevor eigene Dialoge angelegt werden,
// laesst sich hier ansehen, wie die echten aufgebaut sind (DescribeInfo).
// Alles unter SEH-Schutz; ohne Spiel liefern die Funktionen -1 bzw. leeren Text.
#include <windows.h>

#include <angelscript.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include <MinHook.h>

#include "sb_api.h"
#include "script_host.h"

namespace og3p {
namespace {

typedef void*(__stdcall* InstFn)();
typedef void*(__thiscall* GetArrFn)(void*);
typedef const void*(__thiscall* RefFn)(const void*);
typedef void*(__thiscall* GetCmdFn)(void*, int);
typedef int(__thiscall* CountFn)(const void*);
typedef void*(__thiscall* HiddenRetFn)(const void*, void*);        // Rueckgabe per Wert: versteckter Zeiger nach this
typedef const wchar_t*(__thiscall* UniTextFn)(const void*);
typedef void(__thiscall* DtorFn)(void*);

struct DialogApi {
    bool tried = false, ok = false;
    InstFn instance = nullptr;
    GetArrFn accessInfos = nullptr;
    RefFn getName = nullptr, getOwner = nullptr, getParent = nullptr, getQuest = nullptr, getFolder = nullptr, getSort = nullptr;
    RefFn getType = nullptr, getCondType = nullptr, getGold = nullptr, getGiven = nullptr, getPermanent = nullptr, getInfoScript = nullptr;
    HiddenRetFn getTitle = nullptr, locGetString = nullptr;
    CountFn cmdCount = nullptr;
    GetCmdFn getCmd = nullptr;
    RefFn lineCommand = nullptr, lineE1 = nullptr, lineE2 = nullptr, lineId1 = nullptr, lineId2 = nullptr, lineText = nullptr;
    UniTextFn uniText = nullptr;
    DtorFn uniDtor = nullptr;
    const void* scriptVtable = nullptr;
    // Schreiben
    typedef void*(__thiscall* AddInfoFn)(void*, const void*);
    typedef bool(__thiscall* InfoByNameFn)(void*, const void*);
    typedef void(__thiscall* VoidFn)(void*);
    typedef void(__thiscall* SetStrFn)(void*, const void*);
    typedef void*(__thiscall* AccessFn)(void*);
    typedef void(__thiscall* AddCmdFn)(void*, const void*, const void*, const void*, const void*, const void*, const void*);
    typedef void(__thiscall* LocCtorFn)(void*, const void*);
    typedef void(__thiscall* LocDtorFn)(void*);
    AddInfoFn addInfo = nullptr;
    InfoByNameFn removeInfo = nullptr;
    VoidFn buildMap = nullptr;
    SetStrFn setOwner = nullptr, setQuest = nullptr, setParent = nullptr;
    AccessFn accessType = nullptr, accessCond = nullptr, accessGiven = nullptr;
    typedef bool(__thiscall* ExecInfoFn)(void*, void*);
    ExecInfoFn execInfo = nullptr;
    AddCmdFn addCmd = nullptr;
    LocCtorFn locCtor = nullptr;
    LocDtorFn locDtor = nullptr;
} g_d;

bool Resolve() {
    if (g_d.ok) return true;
    HMODULE game = GetModuleHandleA("Game.dll"), eng = GetModuleHandleA("Engine.dll"), sb = GetModuleHandleA("SharedBase.dll");
    if (!game || !eng || !sb) return false;
    auto G = [&](const char* n) { return reinterpret_cast<void*>(GetProcAddress(game, n)); };
    g_d.instance = reinterpret_cast<InstFn>(G("?GetInstance@gCInfoManager_PS@@SGPAV1@XZ"));
    g_d.accessInfos = reinterpret_cast<GetArrFn>(G("?AccessInfos@gCInfoManager_PS@@QAEAAV?$bTPtrArray@PAVgCInfo_PS@@@@XZ"));
    g_d.getName = reinterpret_cast<RefFn>(G("?GetName@gCInfo_PS@@QBEABVbCString@@XZ"));
    g_d.getOwner = reinterpret_cast<RefFn>(G("?GetOwner@gCInfo_PS@@QBEABVbCString@@XZ"));
    g_d.getParent = reinterpret_cast<RefFn>(G("?GetParent@gCInfo_PS@@QBEABVbCString@@XZ"));
    g_d.getQuest = reinterpret_cast<RefFn>(G("?GetQuest@gCInfo_PS@@QBEABVbCString@@XZ"));
    g_d.getFolder = reinterpret_cast<RefFn>(G("?GetFolder@gCInfo_PS@@QBEABVbCString@@XZ"));
    g_d.getSort = reinterpret_cast<RefFn>(G("?GetSortID@gCInfo_PS@@QBEABKXZ"));
    g_d.getType = reinterpret_cast<RefFn>(G("?GetType@gCInfo_PS@@QBEABV?$bTPropertyContainer@W4gEInfoType@@@@XZ"));
    g_d.getCondType = reinterpret_cast<RefFn>(G("?GetConditionType@gCInfo_PS@@QBEABV?$bTPropertyContainer@W4gEInfoCondType@@@@XZ"));
    g_d.getGold = reinterpret_cast<RefFn>(G("?GetGoldCost@gCInfo_PS@@QBEABJXZ"));
    g_d.getGiven = reinterpret_cast<RefFn>(G("?GetInfoGiven@gCInfo_PS@@QBEAB_NXZ"));
    g_d.getPermanent = reinterpret_cast<RefFn>(G("?GetPermanent@gCInfo_PS@@QBEAB_NXZ"));
    g_d.getInfoScript = reinterpret_cast<RefFn>(G("?GetInfoScript@gCInfo_PS@@QBEABV?$bTPropertyObject@VgCInfoScript_PS@@VbCObjectRefBase@@@@XZ"));
    g_d.getTitle = reinterpret_cast<HiddenRetFn>(G("?GetTitle@gCInfo_PS@@QBE?AVbCUnicodeString@@XZ"));
    g_d.cmdCount = reinterpret_cast<CountFn>(G("?GetCommandCount@gCInfoScript_PS@@QBEHXZ"));
    g_d.getCmd = reinterpret_cast<GetCmdFn>(G("?GetCommand@gCInfoScript_PS@@QAEPAVgCInfoScriptLine_PS@@H@Z"));
    g_d.lineCommand = reinterpret_cast<RefFn>(G("?GetCommand@gCInfoScriptLine_PS@@QBEABVbCString@@XZ"));
    g_d.lineE1 = reinterpret_cast<RefFn>(G("?GetEntity1@gCInfoScriptLine_PS@@QBEABVbCString@@XZ"));
    g_d.lineE2 = reinterpret_cast<RefFn>(G("?GetEntity2@gCInfoScriptLine_PS@@QBEABVbCString@@XZ"));
    g_d.lineId1 = reinterpret_cast<RefFn>(G("?GetID1@gCInfoScriptLine_PS@@QBEABVbCString@@XZ"));
    g_d.lineId2 = reinterpret_cast<RefFn>(G("?GetID2@gCInfoScriptLine_PS@@QBEABVbCString@@XZ"));
    g_d.lineText = reinterpret_cast<RefFn>(G("?GetText@gCInfoScriptLine_PS@@QBEABVeCLocString@@XZ"));
    g_d.scriptVtable = G("??_7gCInfoScript_PS@@6B@");
    g_d.locGetString = reinterpret_cast<HiddenRetFn>(GetProcAddress(eng, "?GetString@eCLocString@@QBE?BVbCUnicodeString@@XZ"));
    g_d.uniText = reinterpret_cast<UniTextFn>(GetProcAddress(sb, "?GetText@bCUnicodeString@@QBEPB_WXZ"));
    g_d.uniDtor = reinterpret_cast<DtorFn>(GetProcAddress(sb, "??1bCUnicodeString@@QAE@XZ"));
    g_d.addInfo = reinterpret_cast<DialogApi::AddInfoFn>(G("?AddInfo@gCInfoManager_PS@@QAEPAVgCInfo_PS@@ABVbCString@@@Z"));
    g_d.removeInfo = reinterpret_cast<DialogApi::InfoByNameFn>(G("?RemoveInfo@gCInfoManager_PS@@QAE_NABVbCString@@@Z"));
    g_d.buildMap = reinterpret_cast<DialogApi::VoidFn>(G("?BuildMapInfosByOwners@gCInfoManager_PS@@AAEXXZ"));
    g_d.setOwner = reinterpret_cast<DialogApi::SetStrFn>(G("?SetOwner@gCInfo_PS@@QAEXABVbCString@@@Z"));
    g_d.setQuest = reinterpret_cast<DialogApi::SetStrFn>(G("?SetQuest@gCInfo_PS@@QAEXABVbCString@@@Z"));
    g_d.setParent = reinterpret_cast<DialogApi::SetStrFn>(G("?SetParent@gCInfo_PS@@QAEXABVbCString@@@Z"));
    g_d.accessType = reinterpret_cast<DialogApi::AccessFn>(G("?AccessType@gCInfo_PS@@QAEAAV?$bTPropertyContainer@W4gEInfoType@@@@XZ"));
    g_d.accessCond = reinterpret_cast<DialogApi::AccessFn>(G("?AccessConditionType@gCInfo_PS@@QAEAAV?$bTPropertyContainer@W4gEInfoCondType@@@@XZ"));
    g_d.execInfo = reinterpret_cast<DialogApi::ExecInfoFn>(G("?Execute@gCInfo_PS@@QAE_NPAVgCInfoManager_PS@@@Z"));
    g_d.accessGiven = reinterpret_cast<DialogApi::AccessFn>(G("?AccessInfoGiven@gCInfo_PS@@QAEAA_NXZ"));
    g_d.addCmd = reinterpret_cast<DialogApi::AddCmdFn>(G("?AddCommand@gCInfoScript_PS@@QAEXABVbCString@@0000ABVeCLocString@@@Z"));
    g_d.locCtor = reinterpret_cast<DialogApi::LocCtorFn>(GetProcAddress(eng, "??0eCLocString@@QAE@ABVbCString@@@Z"));
    g_d.locDtor = reinterpret_cast<DialogApi::LocDtorFn>(GetProcAddress(eng, "??1eCLocString@@QAE@XZ"));
    g_d.ok = g_d.instance && g_d.accessInfos && g_d.getName && g_d.getOwner && g_d.getParent && g_d.getQuest && g_d.getType && g_d.getCondType &&
             g_d.getInfoScript && g_d.cmdCount && g_d.getCmd && g_d.lineCommand && g_d.lineText && g_d.locGetString && g_d.uniText && g_d.uniDtor;
    if (!g_d.tried && ScriptHost::s_current) ScriptHost::s_current->Log(g_d.ok ? "Dialog-Bindings bereit" : "Dialog-Bindings: Exporte fehlen");
    g_d.tried = true;
    return g_d.ok;
}

// ---- rohe, SEH-geschuetzte Zugriffe
void* g_imOverride = nullptr;   // waehrend des Aufbaus eines neuen Info-Managers (Laden)
void* SafeInstance() {
    if (g_imOverride) return g_imOverride;
    __try { return g_d.instance(); } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool SafeRaw(const void* p, void* out, size_t n) { if (!ReadableMem(p, n)) return false; __try { std::memcpy(out, p, n); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
const void* SafeRef(RefFn f, const void* self) { __try { return f(self); } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; } }
const char* SafeStr(RefFn f, const void* self) {   // Getter, der eine bCString& liefert
    __try { const void* r = f(self); return r ? g_sb.strGetText(r) : nullptr; } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool SafeInt(RefFn f, const void* self, int* out, int off) {   // Referenz auf Wert (off 4 = bTPropertyContainer: vtable, Wert)
    __try { const unsigned char* r = static_cast<const unsigned char*>(f(self)); *out = *reinterpret_cast<const int*>(r + off); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void* SafeArrayRaw(void* mgr, unsigned w[3]) {
    __try { void* a = g_d.accessInfos(mgr); return SafeRaw(a, w, 12) ? a : nullptr; } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
void* SafeCmd(void* script, int i) { __try { return g_d.getCmd(script, i); } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; } }
int SafeCmdCount(void* script) { __try { return g_d.cmdCount(script); } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; } }
bool SafeVtable(const void* obj, const void** vt) { if (!ReadableMem(obj, sizeof(void*))) return false; __try { *vt = *static_cast<const void* const*>(obj); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }

// bCUnicodeString per Wert (4 Byte, versteckter Rueckgabezeiger) -> UTF-8-freier Windows-1252-Text
bool SafeUnicode(HiddenRetFn f, const void* self, std::wstring* out) {
    void* raw = nullptr;
    wchar_t buf[1024];
    size_t n = 0;
    __try {
        f(self, &raw);
        const wchar_t* t = raw ? g_d.uniText(&raw) : nullptr;
        if (t) { while (t[n] && n < 1023) { buf[n] = t[n]; ++n; } }
        buf[n] = 0;
        if (raw) g_d.uniDtor(&raw);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    out->assign(buf, n);
    return true;
}
std::string Narrow(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int len = WideCharToMultiByte(1252, 0, w.c_str(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(len > 0 ? len : 0, '\0');
    if (len > 0) WideCharToMultiByte(1252, 0, w.c_str(), int(w.size()), &s[0], len, nullptr, nullptr);
    return s;
}

// Info-Array (bTPtrArray<gCInfo_PS*>): {Daten, Anzahl, Kapazitaet} wie beim Quest-Array, Lesart wird am ersten Element geprueft
bool InfoArray(unsigned char** data, int* count) {
    if (!Resolve()) return false;
    void* mgr = SafeInstance();
    unsigned w[3];
    if (!mgr || !SafeArrayRaw(mgr, w)) return false;
    const int cand[3][2] = {{0, 1}, {2, 0}, {2, 1}};
    for (auto& c : cand) {
        unsigned char* d = reinterpret_cast<unsigned char*>(static_cast<uintptr_t>(w[c[0]]));
        const int n = int(w[c[1]]);
        if (!d || n <= 0 || n > 1000000) continue;
        void* first = nullptr;
        if (SafeRaw(d, &first, sizeof first) && first && SafeStr(g_d.getName, first)) { *data = d; *count = n; return true; }
    }
    return false;
}
void* InfoAt(int i) {
    unsigned char* d; int n;
    void* p = nullptr;
    return i >= 0 && InfoArray(&d, &n) && i < n && SafeRaw(d + 4 * i, &p, sizeof p) ? p : nullptr;
}
void* FindInfoByName(const char* name) {
    unsigned char* d; int n;
    if (!InfoArray(&d, &n)) return nullptr;
    for (int i = 0; i < n; ++i) {
        void* p = nullptr;
        if (!SafeRaw(d + 4 * i, &p, sizeof p) || !p) continue;
        const char* nm = SafeStr(g_d.getName, p);
        if (nm && std::strcmp(nm, name) == 0) return p;
    }
    return nullptr;
}

// gCInfoScript_PS* aus dem bTPropertyObject-Huellobjekt des Info finden (Layout unbekannt): Zeiger bis Tiefe 2 mit passender vtable suchen
void* FindScript(void* info, unsigned raw[4]) {
    const unsigned char* w = static_cast<const unsigned char*>(SafeRef(g_d.getInfoScript, info));
    std::memset(raw, 0, 16);
    if (!w || !SafeRaw(w, raw, 16)) return nullptr;
    for (int i = 0; i < 4; ++i) {
        void* p1 = reinterpret_cast<void*>(static_cast<uintptr_t>(raw[i]));
        const void* vt = nullptr;
        if (p1 && SafeVtable(p1, &vt) && vt == g_d.scriptVtable) return p1;
    }
    for (int i = 0; i < 4; ++i) {
        void* p1 = reinterpret_cast<void*>(static_cast<uintptr_t>(raw[i]));
        unsigned inner[4];
        if (!p1 || !SafeRaw(p1, inner, 16)) continue;
        for (int j = 0; j < 4; ++j) {
            void* p2 = reinterpret_cast<void*>(static_cast<uintptr_t>(inner[j]));
            const void* vt = nullptr;
            if (p2 && SafeVtable(p2, &vt) && vt == g_d.scriptVtable) return p2;
        }
    }
    return nullptr;
}

std::string S(const char* s) { return s ? s : ""; }
std::string LocText(const void* loc) {
    std::wstring w;
    return loc && SafeUnicode(g_d.locGetString, loc, &w) ? Narrow(w) : std::string("?");
}

// ---- Skript-Funktionen
int GetInfoCount() { unsigned char* d; int n; return InfoArray(&d, &n) ? n : -1; }
SStr GetInfoName(int i) { void* p = InfoAt(i); return SStr(p ? S(SafeStr(g_d.getName, p)) : std::string()); }
SStr GetInfoOwner(int i) { void* p = InfoAt(i); return SStr(p ? S(SafeStr(g_d.getOwner, p)) : std::string()); }

SStr DescribeInfo(const SStr& name) {
    if (!Resolve()) return SStr("(Dialog-Bindings nicht bereit)");
    void* info = FindInfoByName(name.c_str());
    if (!info) return SStr("(Info nicht gefunden)");
    std::string r;
    char b[256];
    int type = -1, cond = -1, gold = 0;
    SafeInt(g_d.getType, info, &type, 4);
    SafeInt(g_d.getCondType, info, &cond, 4);
    if (g_d.getGold) SafeInt(g_d.getGold, info, &gold, 0);
    std::wstring title;
    if (g_d.getTitle) SafeUnicode(g_d.getTitle, info, &title);
    r += "Name=" + S(SafeStr(g_d.getName, info)) + " Owner=" + S(SafeStr(g_d.getOwner, info)) + " Parent=" + S(SafeStr(g_d.getParent, info)) +
         " Quest=" + S(SafeStr(g_d.getQuest, info));
    if (g_d.getFolder) r += " Folder=" + S(SafeStr(g_d.getFolder, info));
    std::snprintf(b, sizeof b, " CondType=%d Type=%d Gold=%d Titel=\"", cond, type, gold);
    r += b + Narrow(title) + "\"";
    unsigned raw[4];
    void* script = FindScript(info, raw);
    if (!script) {
        std::snprintf(b, sizeof b, "\n  (Info-Skript nicht gefunden, Huellobjekt: %08x %08x %08x %08x)", raw[0], raw[1], raw[2], raw[3]);
        return SStr(r + b);
    }
    const int n = SafeCmdCount(script);
    std::snprintf(b, sizeof b, "\n  Skriptzeilen: %d", n);
    r += b;
    for (int i = 0; i < n && i < 64; ++i) {
        void* line = SafeCmd(script, i);
        if (!line) { r += "\n  [" + std::to_string(i) + "] (nicht lesbar)"; continue; }
        r += "\n  [" + std::to_string(i) + "] " + S(SafeStr(g_d.lineCommand, line)) + " | " + S(SafeStr(g_d.lineE1, line)) + " | " +
             S(SafeStr(g_d.lineE2, line)) + " | " + S(SafeStr(g_d.lineId1, line)) + " | " + S(SafeStr(g_d.lineId2, line)) + " | \"" +
             LocText(SafeRef(g_d.lineText, line)) + "\"";
    }
    return SStr(r);
}

// ---------------------------------------------------------------- Schreiben
bool SafeAdd(void* mgr, const void* name, void** out) { __try { *out = g_d.addInfo(mgr, name); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
bool SafeSetStr(DialogApi::SetStrFn f, void* info, const void* s) { __try { f(info, s); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
bool SafeSetEnum(DialogApi::AccessFn f, void* info, int v) {   // bTPropertyContainer<enum>: vtable, dann der Wert
    __try { unsigned char* c = static_cast<unsigned char*>(f(info)); *reinterpret_cast<int*>(c + 4) = v; return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeVoid(DialogApi::VoidFn f, void* self) { __try { f(self); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
bool SafeRemove(void* mgr, const void* name, bool* out) { __try { *out = g_d.removeInfo(mgr, name); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
bool SafeAddCmd(void* script, const void* c, const void* e1, const void* e2, const void* i1, const void* i2, const void* idStr) {
    __try {
        void* loc = nullptr;
        g_d.locCtor(&loc, idStr);
        g_d.addCmd(script, c, e1, e2, i1, i2, &loc);
        g_d.locDtor(&loc);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool WriteReady() { return Resolve() && g_d.addInfo && g_d.setOwner && g_d.accessType && g_d.accessCond && g_d.addCmd && g_d.locCtor && g_d.locDtor; }
void* Manager() { return Resolve() ? SafeInstance() : nullptr; }

bool SafeSetByte(DialogApi::AccessFn f, void* info, bool v) { __try { *reinterpret_cast<unsigned char*>(f(info)) = v ? 1 : 0; return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
bool SafeExec(void* info, void* mgr, bool* out) { __try { *out = g_d.execInfo(info, mgr); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
// Info-Skript ohne Gespraechsfenster ausfuehren (fuer Infos, die nur unsichtbare Befehle wie AddQuestLog enthalten)
bool ExecuteInfo(const SStr& name) {
    void* mgr = Manager();
    void* info = Resolve() ? FindInfoByName(name.c_str()) : nullptr;
    bool r = false;
    return mgr && info && g_d.execInfo && SafeExec(info, mgr, &r) && r;
}
bool SetInfoGiven(const SStr& name, bool given) {   // "Info wurde schon gegeben": blendet sie aus und laesst ihre Zeilen im Questlog stehen
    void* info = Resolve() ? FindInfoByName(name.c_str()) : nullptr;
    return info && g_d.accessGiven && SafeSetByte(g_d.accessGiven, info, given);
}
bool IsInfoGiven(const SStr& name) {
    void* info = Resolve() ? FindInfoByName(name.c_str()) : nullptr;
    int v = 0;
    return info && g_d.getGiven && SafeInt(g_d.getGiven, info, &v, 0) && (v & 0xFF) != 0;
}

bool InfoExists(const SStr& name) { return Resolve() && FindInfoByName(name.c_str()) != nullptr; }

bool CreateInfo(const SStr& name, const SStr& owner) {
    if (!WriteReady()) return false;
    void* mgr = Manager();
    if (!mgr) return false;
    if (FindInfoByName(name.c_str())) return true;
    void* info = nullptr;
    if (!SafeAdd(mgr, &name, &info) || !info) return false;
    SafeSetStr(g_d.setOwner, info, &owner);
    SafeSetEnum(g_d.accessType, info, 3);   // Info
    SafeSetEnum(g_d.accessCond, info, 3);   // General
    return true;
}
bool RemoveInfoByName(const SStr& name) {
    void* mgr = Manager();
    bool r = false;
    return Resolve() && g_d.removeInfo && mgr && SafeRemove(mgr, &name, &r) && r;
}
bool SetInfoString(DialogApi::SetStrFn f, const SStr& name, const SStr& value) {
    void* info = Resolve() ? FindInfoByName(name.c_str()) : nullptr;
    return info && f && SafeSetStr(f, info, &value);
}
bool SetInfoQuest(const SStr& name, const SStr& quest) { return Resolve() && SetInfoString(g_d.setQuest, name, quest); }
bool SetInfoParent(const SStr& name, const SStr& parent) { return Resolve() && SetInfoString(g_d.setParent, name, parent); }
bool SetInfoType(const SStr& name, int type) {
    void* info = Resolve() ? FindInfoByName(name.c_str()) : nullptr;
    return info && g_d.accessType && SafeSetEnum(g_d.accessType, info, type);
}
bool SetInfoCondition(const SStr& name, int cond) {
    void* info = Resolve() ? FindInfoByName(name.c_str()) : nullptr;
    return info && g_d.accessCond && SafeSetEnum(g_d.accessCond, info, cond);
}
// Zeile an das Info-Skript haengen. Text (falls nicht leer) wird als eigene Text-ID OG3_INFO_<Info>_<n> registriert.
bool AddInfoLine(const SStr& name, const SStr& command, const SStr& e1, const SStr& e2, const SStr& id1, const SStr& id2, const SStr& text) {
    if (!WriteReady()) return false;
    void* info = FindInfoByName(name.c_str());
    if (!info) return false;
    unsigned raw[4];
    void* script = FindScript(info, raw);
    if (!script) return false;
    const int idx = SafeCmdCount(script);
    SStr idStr("");
    if (text.length() > 0) {
        std::string id = std::string("OG3_INFO_") + name.c_str() + "_" + std::to_string(idx < 0 ? 0 : idx);
        RegisterTextForId(id, text.c_str());
        idStr = SStr(id);
    }
    return SafeAddCmd(script, &command, &e1, &e2, &id1, &id2, &idStr);
}
// nach dem Anlegen/Aendern von Infos: Besitzer-Index des Managers neu aufbauen, damit sie im Gespraech erscheinen
bool RebuildInfos() {
    void* mgr = Manager();
    return Resolve() && g_d.buildMap && mgr && SafeVoid(g_d.buildMap, mgr);
}

// Befehlsvokabular aller Infos des Spiels: je Befehl Anzahl und ein Beispiel (Entity1 | Entity2 | ID1 | ID2 | Text)
SStr DescribeCommands() {
    unsigned char* d; int n;
    if (!InfoArray(&d, &n)) return SStr("(Infos nicht lesbar)");
    struct Entry { int count = 0; std::string example; };
    std::map<std::string, Entry> cmds;
    int infosOhneSkript = 0;
    for (int i = 0; i < n; ++i) {
        void* info = nullptr;
        unsigned raw[4];
        if (!SafeRaw(d + 4 * i, &info, sizeof info) || !info) continue;
        void* script = FindScript(info, raw);
        if (!script) { ++infosOhneSkript; continue; }
        const int lines = SafeCmdCount(script);
        for (int j = 0; j < lines && j < 256; ++j) {
            void* line = SafeCmd(script, j);
            if (!line) continue;
            Entry& e = cmds[S(SafeStr(g_d.lineCommand, line))];
            if (e.count++ == 0)
                e.example = S(SafeStr(g_d.lineE1, line)) + " | " + S(SafeStr(g_d.lineE2, line)) + " | " + S(SafeStr(g_d.lineId1, line)) + " | " +
                            S(SafeStr(g_d.lineId2, line)) + " | \"" + LocText(SafeRef(g_d.lineText, line)) + "\"";
        }
    }
    std::string r = "Infos: " + std::to_string(n) + ", ohne lesbares Skript: " + std::to_string(infosOhneSkript) + ", verschiedene Befehle: " + std::to_string(cmds.size());
    for (auto& kv : cmds) r += "\n  " + kv.first + " (" + std::to_string(kv.second.count) + "x): " + kv.second.example;
    return SStr(r);
}

// ---- Hook: Info-Skript beendet -> OnInfoFinished(info, owner, quest)
typedef void(__thiscall* InfoFinishedFn)(void*, const void*);
InfoFinishedFn o_infoFinished = nullptr;
ScriptHost* g_hookHost = nullptr;
void __fastcall HInfoFinished(void* self, void*, const void* info) {
    o_infoFinished(self, info);
    if (!g_hookHost || !info) return;
    const char* nm = SafeStr(g_d.getName, info);
    if (!nm) return;
    SStr name(nm), owner(S(SafeStr(g_d.getOwner, info))), quest(S(SafeStr(g_d.getQuest, info)));
    EvArg a[] = {EvArg::Str(&name), EvArg::Str(&owner), EvArg::Str(&quest)};
    g_hookHost->CallEvent("void OnInfoFinished(const bCString &in, const bCString &in, const bCString &in)", a, 3);
}

}  // namespace

bool RegisterDialogBindings(asIScriptEngine* e) {
    bool ok = true;
#define R(x) do { const int r_ = (x); if (r_ < 0) { ok = false; if (ScriptHost::s_current) { char b_[200]; std::snprintf(b_, sizeof b_, "Registrierung fehlgeschlagen (%d): %s", r_, #x); ScriptHost::s_current->Log(b_); } } } while (0)
    R(e->RegisterGlobalFunction("int GetInfoCount()", asFUNCTION(GetInfoCount), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bCString GetInfoName(int)", asFUNCTION(GetInfoName), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bCString GetInfoOwner(int)", asFUNCTION(GetInfoOwner), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bCString DescribeInfo(const bCString &in)", asFUNCTION(DescribeInfo), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bool ExecuteInfo(const bCString &in)", asFUNCTION(ExecuteInfo), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bool SetInfoGiven(const bCString &in, bool)", asFUNCTION(SetInfoGiven), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bool IsInfoGiven(const bCString &in)", asFUNCTION(IsInfoGiven), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bool InfoExists(const bCString &in)", asFUNCTION(InfoExists), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bool CreateInfo(const bCString &in name, const bCString &in owner)", asFUNCTION(CreateInfo), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bool RemoveInfo(const bCString &in)", asFUNCTION(RemoveInfoByName), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bool SetInfoQuest(const bCString &in, const bCString &in)", asFUNCTION(SetInfoQuest), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bool SetInfoParent(const bCString &in, const bCString &in)", asFUNCTION(SetInfoParent), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bool SetInfoType(const bCString &in, int)", asFUNCTION(SetInfoType), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bool SetInfoCondition(const bCString &in, int)", asFUNCTION(SetInfoCondition), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bool AddInfoLine(const bCString &in info, const bCString &in command, const bCString &in entity1, const bCString &in entity2, const bCString &in id1, const bCString &in id2, const bCString &in text)", asFUNCTION(AddInfoLine), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bool RebuildInfos()", asFUNCTION(RebuildInfos), asCALL_CDECL));
    R(e->RegisterGlobalFunction("bCString DescribeCommands()", asFUNCTION(DescribeCommands), asCALL_CDECL));
    static const int kCond[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    static const char* kCondNames[] = {"INFO_COND_CRIME", "INFO_COND_FIGHT", "INFO_COND_HELLO", "INFO_COND_GENERAL", "INFO_COND_OVERTIME", "INFO_COND_OPEN",
                                       "INFO_COND_ACTIVATOR", "INFO_COND_RUNNING", "INFO_COND_DELIVERY", "INFO_COND_PARTDELIVERY", "INFO_COND_SUCCESS",
                                       "INFO_COND_DOCANCEL", "INFO_COND_FAILED"};
    static const int kType[] = {0, 1, 2, 3, 4, 5, 7};
    static const char* kTypeNames[] = {"INFO_TYPE_REFUSE", "INFO_TYPE_IMPORTANT", "INFO_TYPE_NEWS", "INFO_TYPE_INFO", "INFO_TYPE_PARENT", "INFO_TYPE_COMMENT",
                                       "INFO_TYPE_QUESTRUNNING"};
    for (int i = 0; i < 13; ++i) { char d[64]; std::snprintf(d, sizeof d, "const int %s", kCondNames[i]); R(e->RegisterGlobalProperty(d, const_cast<int*>(&kCond[i]))); }
    for (int i = 0; i < 7; ++i) { char d[64]; std::snprintf(d, sizeof d, "const int %s", kTypeNames[i]); R(e->RegisterGlobalProperty(d, const_cast<int*>(&kType[i]))); }
#undef R
    return ok;
}

void SetInfoManagerOverride(void* mgr) { g_imOverride = mgr; }

bool InfoNames(const void* info, std::string* name, std::string* owner, std::string* quest) {
    if (!info || !Resolve()) return false;
    const char* n = SafeStr(g_d.getName, info);
    if (!n) return false;
    *name = n;
    *owner = S(SafeStr(g_d.getOwner, info));
    *quest = S(SafeStr(g_d.getQuest, info));
    return true;
}

void InstallInfoHook(ScriptHost& host) {
    static bool done = false;
    if (done) return;
    done = true;
    g_hookHost = &host;
    HMODULE game = GetModuleHandleA("Game.dll");
    void* t = game ? reinterpret_cast<void*>(GetProcAddress(game, "?OnInfoScriptFinished@gCInfoManager_PS@@QAEXPBVgCInfo_PS@@@Z")) : nullptr;
    if (!t || !Resolve()) { host.Log("Dialog-Ereignis OnInfoFinished: Export fehlt"); return; }
    const MH_STATUS st = MH_Initialize();
    if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) return;
    const bool ok = MH_CreateHook(t, (void*)&HInfoFinished, (void**)&o_infoFinished) == MH_OK && MH_EnableHook(t) == MH_OK;
    host.Log(ok ? "Dialog-Ereignis OnInfoFinished aktiv" : "Dialog-Ereignis OnInfoFinished: Einhaengen fehlgeschlagen");
}

}  // namespace og3p
