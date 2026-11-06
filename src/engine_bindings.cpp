// Engine-/Game-Bindings des OG3-Patches (Skript-Schnittstelle ab Version 2): Typ Entity (Held, Spieler, Entity nach Namen),
// Name, Position, Rotation, Abstand, Position setzen.
// Alle Funktionen werden bei der ersten Benutzung aus Engine.dll/Game.dll aufgeloest (die DLLs laden nach dem Proxy).
// Sicherheit: Entity ist ein Wert mit rohem Zeiger. Vor jedem Zugriff wird geprueft, dass der Zeiger lesbar ist und seine vtable in
// Engine.dll/Game.dll/Gothic3.exe liegt; jeder Aufruf laeuft unter __try (ungueltige/geloeschte Entities liefern Standardwerte statt
// eines Absturzes). Skripte sollen Entities nicht ueber Weltwechsel hinweg aufbewahren (bei Bedarf neu holen).
#include <windows.h>

#include <angelscript.h>

#include <cmath>
#include <cstring>
#include <map>
#include <new>
#include <string>

#include "sb_api.h"
#include "script_host.h"

namespace og3p {
bool RegisterQuestBindings(asIScriptEngine* e);    // quest_bindings.cpp
bool RegisterDialogBindings(asIScriptEngine* e);   // dialog_bindings.cpp
namespace {

struct Entity {
    void* p;
};

typedef void*(__stdcall* InstFn)();
typedef void*(__thiscall* ObjFn)(void*);
typedef void*(__thiscall* FindModuleFn)(const void*, const void*);
typedef void*(__thiscall* ByNameFn)(void*, const void*);
typedef void*(__thiscall* ByIdFn)(void*, const void*, int);
typedef const void*(__thiscall* RefGetFn)(const void*);
typedef void*(__thiscall* RotFn)(const void*, void*);
typedef float(__thiscall* DistFn)(const void*, const void*);
typedef void(__thiscall* SetPosFn)(void*, const void*);

struct EngineApi {
    bool tried = false, ok = false;
    HMODULE engine = nullptr, game = nullptr;
    InstFn sessionInstance = nullptr, moduleAdmin = nullptr;
    ObjFn sessionGetHero = nullptr, sessionGetPlayer = nullptr;
    FindModuleFn findModule = nullptr;
    ByNameFn byName = nullptr;
    ByIdFn byId = nullptr;   // eCSceneAdmin::GetEntity(bCPropertyID, Hint)
    RefGetFn getName = nullptr, getWorldPos = nullptr, getWorldMat = nullptr, getTplId = nullptr, getTreeBox = nullptr, getParent = nullptr;
    RotFn getWorldRot = nullptr;
    DistFn distance = nullptr;
    int setPosSlot = -1;   // Index von SetWorldPosition in der eCEntity-vtable
    void* sceneAdmin = nullptr;
} g_e;

bool InModule(HMODULE m, const void* p) {
    if (!m) return false;
    auto* b = reinterpret_cast<const unsigned char*>(m);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(b + reinterpret_cast<const IMAGE_DOS_HEADER*>(b)->e_lfanew);
    return p >= b && p < b + nt->OptionalHeader.SizeOfImage;
}

bool Resolve() {
    if (g_e.ok) return true;
    g_e.engine = GetModuleHandleA("Engine.dll");
    g_e.game = GetModuleHandleA("Game.dll");
    if (!g_e.engine || !g_e.game) return false;   // spaeter erneut versuchen (z. B. im Testprogramm nie vorhanden)
    auto E = [](const char* n) { return reinterpret_cast<void*>(GetProcAddress(g_e.engine, n)); };
    auto G = [](const char* n) { return reinterpret_cast<void*>(GetProcAddress(g_e.game, n)); };
    g_e.sessionInstance = reinterpret_cast<InstFn>(G("?GetInstance@gCSession@@SGAAV1@XZ"));
    g_e.sessionGetHero = reinterpret_cast<ObjFn>(G("?GetHero@gCSession@@QAEPAVeCDynamicEntity@@XZ"));
    g_e.sessionGetPlayer = reinterpret_cast<ObjFn>(G("?GetPlayer@gCSession@@QAEPAVeCDynamicEntity@@XZ"));
    g_e.moduleAdmin = reinterpret_cast<InstFn>(E("?GetInstance@eCModuleAdmin@@SGAAV1@XZ"));
    g_e.findModule = reinterpret_cast<FindModuleFn>(E("?FindModule@eCModuleAdmin@@QBEPAVeCEngineComponentBase@@ABVbCString@@@Z"));
    g_e.byName = reinterpret_cast<ByNameFn>(E("?GetEntityByName@eCSceneAdmin@@QAEPAVeCEntity@@ABVbCString@@@Z"));
    g_e.byId = reinterpret_cast<ByIdFn>(E("?GetEntity@eCSceneAdmin@@QAEPAVeCNode@@ABVbCPropertyID@@W4eEGetEntityTypeHint@@@Z"));
    g_e.getName = reinterpret_cast<RefGetFn>(E("?GetName@eCEntity@@QBEABVbCString@@XZ"));
    g_e.getWorldPos = reinterpret_cast<RefGetFn>(E("?GetWorldPosition@eCEntity@@QBEABVbCVector@@XZ"));
    g_e.getWorldMat = reinterpret_cast<RefGetFn>(E("?GetWorldMatrix@eCEntity@@QBEABVbCMatrix@@XZ"));
    g_e.getTplId = reinterpret_cast<RefGetFn>(E("?GetTemplateID@eCEntity@@QBEABVbCPropertyID@@XZ"));
    g_e.getTreeBox = reinterpret_cast<RefGetFn>(E("?GetWorldTreeBoundary@eCEntity@@QBEABVbCBox@@XZ"));
    g_e.getParent = reinterpret_cast<RefGetFn>(E("?GetParent@eCEntity@@UBEPBV1@XZ"));
    g_e.getWorldRot = reinterpret_cast<RotFn>(E("?GetWorldRotation@eCEntity@@QBE?AVbCQuaternion@@XZ"));
    g_e.distance = reinterpret_cast<DistFn>(E("?GetDistanceToEntity@eCEntity@@QBEMABV1@@Z"));
    void* setPos = E("?SetWorldPosition@eCEntity@@UAEXABVbCVector@@@Z");
    void** vt = static_cast<void**>(E("??_7eCEntity@@6B@"));
    if (vt && setPos)
        for (int i = 0; i < 256 && InModule(g_e.engine, vt[i]); ++i)
            if (vt[i] == setPos) {
                g_e.setPosSlot = i;
                break;
            }
    g_e.ok = g_e.sessionInstance && g_e.sessionGetHero && g_e.getName && g_e.getWorldPos;
    if (!g_e.tried && ScriptHost::s_current) {
        ScriptHost::s_current->Log(
            g_e.ok ? (g_e.setPosSlot >= 0 ? "Engine-Bindings bereit" : "Engine-Bindings bereit (SetPosition nicht verfuegbar)")
                   : "Engine-Bindings: Exporte fehlen");
    }
    g_e.tried = true;
    return g_e.ok;
}

// Gueltigkeit: lesbarer Zeiger, vtable in einem Spielmodul
bool Valid(const void* p) {
    if (!ReadableMem(p, sizeof(void*))) return false;
    __try {
        const void* vt = *static_cast<void* const*>(p);
        return InModule(g_e.engine, vt) || InModule(g_e.game, vt) || InModule(GetModuleHandleA(nullptr), vt);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// ---- rohe, SEH-geschuetzte Aufrufe (ohne C++-Objekte mit Destruktor im __try-Block)
void* SafeHero(bool player) {
    __try {
        void* s = g_e.sessionInstance();
        ObjFn f = player && g_e.sessionGetPlayer ? g_e.sessionGetPlayer : g_e.sessionGetHero;
        return s ? f(s) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
void* SafeSceneAdmin(const void* nameStr) {
    __try {
        void* ma = g_e.moduleAdmin ? g_e.moduleAdmin() : nullptr;
        return ma && g_e.findModule ? g_e.findModule(ma, nameStr) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
void* SafeByName(void* admin, const void* nameStr) {
    __try {
        return g_e.byName(admin, nameStr);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool SafeName(const void* p, const char** out) {
    __try {
        *out = g_sb.strGetText(g_e.getName(p));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeById(void* admin, const void* pid, void** out) {
    __try {
        *out = g_e.byId(admin, pid, 2 /* eEGetEntityTypeHint_TemplateEntity */);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafePos(const void* p, SVec3* out) {
    __try {
        std::memcpy(out, g_e.getWorldPos(p), sizeof(SVec3));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeRot(const void* p, SQuat* out) {
    __try {
        g_e.getWorldRot(p, out);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeDist(const void* a, const void* b, float* out) {
    __try {
        *out = g_e.distance(a, b);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeSetPos(void* p, const SVec3* v) {
    __try {
        void** vt = *static_cast<void***>(p);
        reinterpret_cast<SetPosFn>(vt[g_e.setPosSlot])(p, v);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// ---- Skript-Funktionen
Entity GetHero() { return {Resolve() ? SafeHero(false) : nullptr}; }
Entity GetPlayer() { return {Resolve() ? SafeHero(true) : nullptr}; }
Entity FindEntity(const SStr& name) {
    if (!Resolve() || !g_e.byName) return {nullptr};
    if (!g_e.sceneAdmin || !Valid(g_e.sceneAdmin)) {
        SStr mod("eCSceneAdmin");
        g_e.sceneAdmin = SafeSceneAdmin(&mod);
    }
    return {g_e.sceneAdmin ? SafeByName(g_e.sceneAdmin, &name) : nullptr};
}
void EntConstruct(void* m) { new (m) Entity{nullptr}; }
bool EntIsValid(const Entity& e) { return g_e.ok && Valid(e.p); }
SStr EntName(const Entity& e) {
    const char* t = nullptr;
    if (EntIsValid(e) && SafeName(e.p, &t) && t) return SStr(t);
    return SStr();
}
bool SafeMat(const void* p, SMat4* out) {
    __try {
        std::memcpy(out, g_e.getWorldMat(p), sizeof(SMat4));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
SMat4 EntWorldMatrix(const Entity& e) {   // Einheitsmatrix bei Fehler
    SMat4 m;
    std::memset(&m, 0, sizeof m);
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    if (EntIsValid(e) && g_e.getWorldMat) SafeMat(e.p, &m);
    return m;
}
// ---- Bauwerke/Objekte erkennen: Vorlagenname, Huellquader (Welt, samt Kindern), Elternobjekt (das Fadenkreuz trifft oft Teilobjekte)
struct SBox {
    SVec3 min, max;
};
bool SafeBox(const void* p, SBox* out) {
    __try {
        std::memcpy(out, g_e.getTreeBox(p), sizeof(SBox));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
const void* SafeTplId(const void* p) {
    __try {
        return g_e.getTplId(p);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
const void* SafeParent(const void* p) {
    __try {
        return g_e.getParent(p);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
SStr EntTemplateName(const Entity& e) {
    if (!EntIsValid(e) || !g_e.getTplId) return SStr();
    const void* id = SafeTplId(e.p);
    return SStr(id ? TemplateNameForId(id) : std::string());
}
SVec3 EntBoundsMin(const Entity& e) {
    SBox b{{0, 0, 0}, {0, 0, 0}};
    if (EntIsValid(e) && g_e.getTreeBox) SafeBox(e.p, &b);
    return b.min;
}
SVec3 EntBoundsMax(const Entity& e) {
    SBox b{{0, 0, 0}, {0, 0, 0}};
    if (EntIsValid(e) && g_e.getTreeBox) SafeBox(e.p, &b);
    return b.max;
}
Entity EntParent(const Entity& e) {
    Entity r{nullptr};
    if (EntIsValid(e) && g_e.getParent) {
        const void* p = SafeParent(e.p);
        if (p && Valid(p)) r.p = const_cast<void*>(p);
    }
    return r;
}
SVec3 EntPos(const Entity& e) {
    SVec3 v{0, 0, 0};
    if (EntIsValid(e)) SafePos(e.p, &v);
    return v;
}
SQuat EntRot(const Entity& e) {
    SQuat q{0, 0, 0, 1};
    if (EntIsValid(e) && g_e.getWorldRot) SafeRot(e.p, &q);
    return q;
}
float EntDist(const Entity& a, const Entity& b) {
    float d = -1;
    if (EntIsValid(a) && EntIsValid(b) && g_e.distance) SafeDist(a.p, b.p, &d);
    return d;
}
bool EntSetPos(Entity& e, const SVec3& v) { return EntIsValid(e) && g_e.setPosSlot >= 0 && SafeSetPos(e.p, &v); }
bool EntEquals(const Entity& a, const Entity& b) { return a.p == b.p; }

// ---- Attribute (API 3): gCPlayerMemory_PS der Entity (Held/NPC), Kennungen wie im Spiel: "STR", "DEX", "INT", "HP", "MP", "SP",
// "XP", "LP", "ALC", "SMI", "THF" usw. Weg wie Script.dll PSPlayerMemory::AddStrength: PSPlayerMemory(eCEntity*) holt das
// Eigenschafts-Set, dann gCPlayerMemory_PS::Get/Set/AddBaseValue(bCString). Uhr: gCWorld::GetCurrentWorld()->GetWorldEntity() -> PSClock.
typedef void*(__thiscall* PSCtorFn)(void*, void*);
typedef int(__thiscall* PMGetFn)(const void*, const void*);
typedef bool(__thiscall* PMSetFn)(void*, const void*, int);
typedef int(__thiscall* ClockGetFn)(const void*);
typedef void(__thiscall* ClockSetFn)(const void*, int);
struct StatsApi {
    bool tried = false, ok = false, clockOk = false;
    PSCtorFn pmCtor = nullptr, clockCtor = nullptr;
    PMGetFn getValue = nullptr, getBase = nullptr, getMax = nullptr;
    PMSetFn setBase = nullptr, addBase = nullptr, addValue = nullptr, setValue = nullptr;
    InstFn currentWorld = nullptr;
    ObjFn worldEntity = nullptr;
    ClockGetFn day = nullptr, hour = nullptr, minute = nullptr;
    ClockSetFn setDay = nullptr, setHour = nullptr, setMinute = nullptr;
} g_s;

bool ResolveStats() {
    if (g_s.ok) return true;
    if (!Resolve()) return false;
    HMODULE script = GetModuleHandleA("Script.dll");
    if (!script) return false;
    auto S = [&](const char* n) { return reinterpret_cast<void*>(GetProcAddress(script, n)); };
    auto G = [](const char* n) { return reinterpret_cast<void*>(GetProcAddress(g_e.game, n)); };
    g_s.pmCtor = reinterpret_cast<PSCtorFn>(S("??0PSPlayerMemory@@QAE@PAVeCEntity@@@Z"));
    g_s.clockCtor = reinterpret_cast<PSCtorFn>(S("??0PSClock@@QAE@PAVeCEntity@@@Z"));
    g_s.getValue = reinterpret_cast<PMGetFn>(G("?GetValue@gCPlayerMemory_PS@@QBEHABVbCString@@@Z"));
    g_s.getBase = reinterpret_cast<PMGetFn>(G("?GetBaseValue@gCPlayerMemory_PS@@QBEHABVbCString@@@Z"));
    g_s.getMax = reinterpret_cast<PMGetFn>(G("?GetMaximum@gCPlayerMemory_PS@@QBEHABVbCString@@@Z"));
    g_s.setBase = reinterpret_cast<PMSetFn>(G("?SetBaseValue@gCPlayerMemory_PS@@QAE_NABVbCString@@H@Z"));
    g_s.addBase = reinterpret_cast<PMSetFn>(G("?AddBaseValue@gCPlayerMemory_PS@@QAE_NABVbCString@@H@Z"));
    g_s.addValue = reinterpret_cast<PMSetFn>(G("?AddValue@gCPlayerMemory_PS@@QAE_NABVbCString@@H@Z"));
    g_s.setValue = reinterpret_cast<PMSetFn>(G("?SetValue@gCPlayerMemory_PS@@QAE_NABVbCString@@H@Z"));
    g_s.currentWorld = reinterpret_cast<InstFn>(G("?GetCurrentWorld@gCWorld@@SGPAV1@XZ"));
    g_s.worldEntity = reinterpret_cast<ObjFn>(G("?GetWorldEntity@gCWorld@@QAEPAVeCEntity@@XZ"));
    g_s.day = reinterpret_cast<ClockGetFn>(S("?GetDay@PSClock@@QBEHXZ"));
    g_s.hour = reinterpret_cast<ClockGetFn>(S("?GetHour@PSClock@@QBEHXZ"));
    g_s.minute = reinterpret_cast<ClockGetFn>(S("?GetMinute@PSClock@@QBEHXZ"));
    g_s.setDay = reinterpret_cast<ClockSetFn>(S("?SetDay@PSClock@@QBEXH@Z"));
    g_s.setHour = reinterpret_cast<ClockSetFn>(S("?SetHour@PSClock@@QBEXH@Z"));
    g_s.setMinute = reinterpret_cast<ClockSetFn>(S("?SetMinute@PSClock@@QBEXH@Z"));
    g_s.ok = g_s.pmCtor && g_s.getValue && g_s.getBase && g_s.getMax && g_s.setBase && g_s.addBase && g_s.addValue && g_s.setValue;
    g_s.clockOk = g_s.clockCtor && g_s.currentWorld && g_s.worldEntity && g_s.day && g_s.hour && g_s.minute && g_s.setDay && g_s.setHour &&
                  g_s.setMinute;
    if (!g_s.tried && ScriptHost::s_current)
        ScriptHost::s_current->Log(std::string("Attribut-Bindings ") + (g_s.ok ? "bereit" : "fehlen") + ", Uhr " +
                                   (g_s.clockOk ? "bereit" : "fehlt"));
    g_s.tried = true;
    return g_s.ok;
}

void* SafePlayerMemory(void* ent) {
    void* holder = nullptr;
    __try {
        g_s.pmCtor(&holder, ent);
        return holder;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool SafePMGet(PMGetFn f, const void* ps, const void* tag, int* out) {
    __try {
        *out = f(ps, tag);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafePMSet(PMSetFn f, void* ps, const void* tag, int v) {
    __try {
        return f(ps, tag, v);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void* SafeWorldClock() {
    void* holder = nullptr;
    __try {
        void* w = g_s.currentWorld();
        if (!w) return nullptr;
        void* we = g_s.worldEntity(w);
        if (!we) return nullptr;
        g_s.clockCtor(&holder, we);
        return holder;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool SafeClockGet(ClockGetFn f, void* holder, int* out) {
    __try {
        *out = f(&holder);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeClockSet(ClockSetFn f, void* holder, int v) {
    __try {
        f(&holder, v);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// ---- Spielereignis-Flags: kleine Schalter (Text), die das Spiel im Spielstand speichert (PlayerMemory des Helden). Fuer Mods, die ueber Speichern/Laden
// hinweg wissen muessen, wie weit der Spieler ist. Script.dll PSPlayerMemory: this = das Huellobjekt (erstes Feld = gCPlayerMemory_PS*)
typedef bool(__thiscall* EventFn)(void*, const void*);
EventFn g_evSet = nullptr, g_evClear = nullptr, g_evIs = nullptr;
bool ResolveEvents() {
    if (g_evSet) return true;
    if (!ResolveStats()) return false;
    HMODULE script = GetModuleHandleA("Script.dll");
    g_evSet = reinterpret_cast<EventFn>(GetProcAddress(script, "?SetGameEvent@PSPlayerMemory@@QAE_NABVbCString@@@Z"));
    g_evClear = reinterpret_cast<EventFn>(GetProcAddress(script, "?ClearGameEvent@PSPlayerMemory@@QAE_NABVbCString@@@Z"));
    g_evIs = reinterpret_cast<EventFn>(GetProcAddress(script, "?IsSetGameEvent@PSPlayerMemory@@QAE_NABVbCString@@@Z"));
    return g_evSet && g_evClear && g_evIs;
}
bool SafeEvent(EventFn fn, void* ent, const void* name, bool* out) {
    __try {
        void* wrapper[4] = {nullptr, nullptr, nullptr, nullptr};   // Huellobjekt, genug Platz
        g_s.pmCtor(wrapper, ent);
        *out = fn(wrapper, name);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool HeroEvent(EventFn EventFn_, const SStr& name) {
    if (!ResolveEvents()) return false;
    void* hero = SafeHero(false);
    bool r = false;
    return hero && Valid(hero) && SafeEvent(EventFn_, hero, &name, &r) && r;
}
bool SetGameEvent(const SStr& name) { return ResolveEvents() && HeroEvent(g_evSet, name); }
bool ClearGameEvent(const SStr& name) { return ResolveEvents() && HeroEvent(g_evClear, name); }
bool IsGameEventSet(const SStr& name) { return ResolveEvents() && HeroEvent(g_evIs, name); }

void* PlayerMemoryOf(const Entity& e) { return EntIsValid(e) && ResolveStats() ? SafePlayerMemory(e.p) : nullptr; }
int AttrGet(const Entity& e, const SStr& tag, PMGetFn StatsApi::*fn) {
    int v = -1;
    void* ps = PlayerMemoryOf(e);
    if (ps && !SafePMGet(g_s.*fn, ps, &tag, &v)) v = -1;
    return ps ? v : -1;
}
int EntAttr(const Entity& e, const SStr& tag) { return AttrGet(e, tag, &StatsApi::getValue); }
int EntAttrBase(const Entity& e, const SStr& tag) { return AttrGet(e, tag, &StatsApi::getBase); }
int EntAttrMax(const Entity& e, const SStr& tag) { return AttrGet(e, tag, &StatsApi::getMax); }
bool AttrSet(Entity& e, const SStr& tag, int v, PMSetFn StatsApi::*fn) {
    void* ps = PlayerMemoryOf(e);
    return ps && SafePMSet(g_s.*fn, ps, &tag, v);
}
bool EntSetAttrBase(Entity& e, const SStr& tag, int v) { return AttrSet(e, tag, v, &StatsApi::setBase); }
bool EntAddAttrBase(Entity& e, const SStr& tag, int v) { return AttrSet(e, tag, v, &StatsApi::addBase); }
bool EntSetAttr(Entity& e, const SStr& tag, int v) { return AttrSet(e, tag, v, &StatsApi::setValue); }
bool EntAddAttr(Entity& e, const SStr& tag, int v) { return AttrSet(e, tag, v, &StatsApi::addValue); }

// ---- Inventar (API 7): gCInventory_PS der Entity (Script.dll PSInventory(eCEntity*) liefert das Eigenschafts-Set), Items nach Name
// der Template-Entity ("It_Wolf_Fur"): Anzahl lesen, entfernen. Stapel werden ueber FindStackIndex(bCString) gefunden.
typedef int(__thiscall* InvFindFn)(const void*, const void*, int, unsigned, int);
typedef int(__thiscall* InvCountFn)(const void*);
typedef void*(__thiscall* InvGetStackFn)(void*, int);
typedef const int&(__thiscall* StackAmountFn)(const void*);
typedef bool(__thiscall* InvDeleteFn)(void*, int, unsigned);
typedef int(__thiscall* InvCreateFn)(void*, const void*, unsigned, unsigned);
typedef int(__thiscall* CountPartFn)(void*, const void*, int);
typedef void*(__thiscall* ByPartFn)(void*, const void*, int, int);
typedef void*(__thiscall* TplCloneFn)(const void*);           // eCNode::Clone() const
typedef void(__thiscall* TplSetNameFn)(void*, const void*);   // eCTemplateEntity::SetName(const bCString&)
typedef bool(__thiscall* RegTplFn)(void*, void*);   // eCSceneAdmin::RegisterTemplateEntity(eCTemplateEntity*) (geschuetzt, aber exportiert)
typedef const void*(__thiscall* NodeIdFn)(const void*);   // eCNode::GetID() const -> const bCPropertyID&
struct PropIdVal {
    unsigned char core[16];
    unsigned count;
};                                                           // bCPropertyID (20 Byte) als Wertparameter
typedef void(__thiscall* SetIdFn)(void*, PropIdVal);         // eCNode::SetID(bCPropertyID): meldet alte ID ab, setzt neue, RegisterNodeID
typedef void(__thiscall* IdFromStrFn)(void*, const void*);   // bCPropertyID::CreateFromString(const bCString&): MD5 des Textes
typedef void*(__thiscall* IdTextFn)(const void*, void*);     // bCPropertyID::GetText() -> bCString (Rueckgabe per Zeiger)
struct InvApi {
    bool tried = false, ok = false;
    PSCtorFn ctor = nullptr;
    InvFindFn find = nullptr;
    InvCountFn stackCount = nullptr;
    InvGetStackFn getStack = nullptr;
    StackAmountFn amount = nullptr;
    InvDeleteFn del = nullptr;
    InvCreateFn create = nullptr;
    ByNameFn tplByName = nullptr;   // eCSceneAdmin::GetTemplateEntityByName
    CountPartFn countPart = nullptr;
    ByPartFn byPart = nullptr;
    TplCloneFn clone = nullptr;
    TplSetNameFn setName = nullptr;
    RegTplFn regTpl = nullptr;
    NodeIdFn nodeId = nullptr;
    IdTextFn idText = nullptr;
    SetIdFn setId = nullptr;
    IdFromStrFn idFromStr = nullptr;
} g_i;

bool ResolveInv() {
    if (g_i.ok) return true;
    if (!Resolve()) return false;
    HMODULE script = GetModuleHandleA("Script.dll");
    if (!script) return false;
    auto G = [](const char* n) { return reinterpret_cast<void*>(GetProcAddress(g_e.game, n)); };
    g_i.ctor = reinterpret_cast<PSCtorFn>(GetProcAddress(script, "??0PSInventory@@QAE@PAVeCEntity@@@Z"));
    g_i.find = reinterpret_cast<InvFindFn>(G("?FindStackIndex@gCInventory_PS@@QBEHABVbCString@@W4gEInventoryFindQuality@@KH@Z"));
    g_i.stackCount = reinterpret_cast<InvCountFn>(G("?GetStackCount@gCInventory_PS@@QBEHXZ"));
    g_i.getStack = reinterpret_cast<InvGetStackFn>(G("?GetStack@gCInventory_PS@@QAEPAVgCInventoryStack@@H@Z"));
    g_i.amount = reinterpret_cast<StackAmountFn>(G("?GetAmount@gCInventoryStack@@QBEABHXZ"));
    g_i.del = reinterpret_cast<InvDeleteFn>(G("?DeleteItems@gCInventory_PS@@QAE_NHK@Z"));
    g_i.create = reinterpret_cast<InvCreateFn>(G("?CreateItems@gCInventory_PS@@QAEHPBVeCTemplateEntity@@KK@Z"));
    g_i.tplByName = reinterpret_cast<ByNameFn>(
        GetProcAddress(g_e.engine, "?GetTemplateEntityByName@eCSceneAdmin@@QAEPAVeCTemplateEntity@@ABVbCString@@@Z"));
    g_i.countPart = reinterpret_cast<CountPartFn>(
        GetProcAddress(g_e.engine, "?GetEntityCountByPartName@eCSceneAdmin@@QAEHABVbCString@@W4eEGetEntityTypeHint@@@Z"));
    g_i.byPart = reinterpret_cast<ByPartFn>(
        GetProcAddress(g_e.engine, "?GetEntityByPartName@eCSceneAdmin@@QAEPAVeCEntity@@ABVbCString@@W4eEGetEntityTypeHint@@H@Z"));
    g_i.clone = reinterpret_cast<TplCloneFn>(GetProcAddress(g_e.engine, "?Clone@eCNode@@QBEPAV1@XZ"));
    g_i.setName = reinterpret_cast<TplSetNameFn>(GetProcAddress(g_e.engine, "?SetName@eCTemplateEntity@@UAEXABVbCString@@@Z"));
    g_i.regTpl =
        reinterpret_cast<RegTplFn>(GetProcAddress(g_e.engine, "?RegisterTemplateEntity@eCSceneAdmin@@IAE_NPAVeCTemplateEntity@@@Z"));
    g_i.nodeId = reinterpret_cast<NodeIdFn>(GetProcAddress(g_e.engine, "?GetID@eCNode@@QBEABVbCPropertyID@@XZ"));
    g_i.setId = reinterpret_cast<SetIdFn>(GetProcAddress(g_e.engine, "?SetID@eCNode@@IAEXVbCPropertyID@@@Z"));
    if (HMODULE sb = GetModuleHandleA("SharedBase.dll")) {
        g_i.idText = reinterpret_cast<IdTextFn>(GetProcAddress(sb, "?GetText@bCPropertyID@@QBE?AVbCString@@XZ"));
        g_i.idFromStr = reinterpret_cast<IdFromStrFn>(GetProcAddress(sb, "?CreateFromString@bCPropertyID@@QAEXABVbCString@@@Z"));
    }
    g_i.ok = g_i.ctor && g_i.find && g_i.getStack && g_i.amount && g_i.del;
    if (!g_i.tried && ScriptHost::s_current)
        ScriptHost::s_current->Log(g_i.ok ? "Inventar-Bindings bereit" : "Inventar-Bindings: Exporte fehlen");
    g_i.tried = true;
    return g_i.ok;
}

void* SafeInventory(void* ent) {
    void* holder = nullptr;
    __try {
        g_i.ctor(&holder, ent);
        return holder;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
// Gesamtmenge des Items ueber alle Stapel mit diesem Namen
bool SafeItemCount(void* inv, const void* name, int* out) {
    __try {
        int total = 0, idx = -1;
        for (int guard = 0; guard < 256; ++guard) {
            idx = g_i.find(inv, name, 0 /* Ignore */, 0, idx);
            if (idx < 0) break;
            void* st = g_i.getStack(inv, idx);
            if (st) total += g_i.amount(st);
        }
        *out = total;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// Entfernt bis zu n Stueck (stapelweise); Rueckgabe in *removed
bool SafeRemoveItems(void* inv, const void* name, int n, int* removed) {
    __try {
        int left = n;
        for (int guard = 0; guard < 256 && left > 0; ++guard) {
            const int idx = g_i.find(inv, name, 0, 0, -1);   // nach dem Loeschen koennen Indizes rutschen: immer neu suchen
            if (idx < 0) break;
            void* st = g_i.getStack(inv, idx);
            const int have = st ? g_i.amount(st) : 0;
            if (have <= 0) break;
            const int take = have < left ? have : left;
            if (!g_i.del(inv, idx, unsigned(take))) break;
            left -= take;
        }
        *removed = n - left;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// Items erzeugen: Template-Entity ueber den Namen holen, gCInventory_PS::CreateItems(Template, Qualitaet 0, Menge)
// Rueckgabe: >= 0 Ergebnis von CreateItems, -2 Template nicht gefunden, -3 Ausnahme
// Template-Entity nach Name; erst GetTemplateEntityByName, dann Teilnamensuche mit exaktem Namensvergleich. nullptr = unbekannt
void* SafeFindTemplate(void* admin, const void* name, const char* wanted) {
    __try {
        void* tpl = g_i.tplByName(admin, name);
        if (!tpl && g_i.countPart && g_i.byPart) {   // Ersatz: Teilnamensuche (Template-Hinweis 2)
            const int n = g_i.countPart(admin, name, 2);
            for (int i = 0; i < n && i < 64 && !tpl; ++i) {
                void* c = g_i.byPart(admin, name, 2, i);
                const char* nm = c ? g_sb.strGetText(g_e.getName(c)) : nullptr;
                if (nm && _stricmp(nm, wanted) == 0) tpl = c;
            }
        }
        return tpl;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
// Items erzeugen: Template-Entity ueber den Namen holen, gCInventory_PS::CreateItems(Template, Qualitaet 0, Menge)
// Rueckgabe: >= 0 Ergebnis von CreateItems, -2 Template nicht gefunden, -3 Ausnahme
int SafeCreateItems(void* inv, void* admin, const void* name, const char* wanted, int count) {
    void* tpl = SafeFindTemplate(admin, name, wanted);
    if (!tpl) return -2;
    __try {
        // Aufrufreihenfolge laut Disassembly von CreateItems(Vorlage, Qualitaet, Menge): der DRITTE Wert ist die Menge (muss > 0 sein)
        return g_i.create(inv, tpl, 0u, unsigned(count));
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -3; }
}
// Vorlage im Speicher klonen: eCNode::Clone (wie das Spiel: DoClone, Create, Knoten-ID registrieren, CopyFrom, an die Wurzel haengen),
// SetName (die Engine traegt den Namen selbst in die Namenstabelle ein) und - wichtig - RegisterTemplateEntity: Clone() meldet den Klon nur als
// Knoten an, nicht in der Vorlagentabelle. Ohne diesen Eintrag findet das Inventar die Vorlage ueber ihre ID nicht (Proxy ohne ID -> Absturz im Vergleich).
// RegisterTemplateEntity vergibt bei doppelter ID selbst eine neue.
// Rueckgabe: 0 angelegt, 1 war schon da, -2 Basis unbekannt, -3 Ausnahme, -6 Klon fehlgeschlagen, -7 Name nach SetName nicht auffindbar,
//            -8 Klon ueber seine ID nicht auffindbar. *idBase/*idClone: ID-Zeiger zur Protokollierung (nur gueltig, solange die Vorlagen leben).
int SafeCloneTemplate(void* admin, const void* baseName, const char* baseWanted, const void* newName, const char* newWanted,
                      const void** idBase, const void** idClone, int* regOk, const PropIdVal* fixedId) {
    if (SafeFindTemplate(admin, newName, newWanted)) return 1;
    void* base = SafeFindTemplate(admin, baseName, baseWanted);
    if (!base) return -2;
    void* clone = nullptr;
    __try {
        clone = g_i.clone(base);
        if (!clone) return -6;
        if (fixedId && g_i.setId) g_i.setId(clone, *fixedId);   // feste ID (aus dem Namen): gleiche Vorlage in jedem Spielstart
        g_i.setName(clone, newName);
        if (g_i.nodeId) {
            *idBase = g_i.nodeId(base);
            *idClone = g_i.nodeId(clone);
        }
        *regOk = g_i.regTpl ? (g_i.regTpl(admin, clone) ? 1 : 0) : -1;
        if (g_i.nodeId) *idClone = g_i.nodeId(clone);   // nach dem Eintrag neu lesen (bei doppelter ID neu vergeben)
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -3; }
    if (SafeFindTemplate(admin, newName, newWanted) != clone) return -7;
    void* byId = nullptr;
    if (*idClone && g_e.byId && (!SafeById(admin, *idClone, &byId) || byId != clone)) return -8;
    return 0;
}
bool SafeIdFromString(PropIdVal* out, const void* key) {
    __try {
        g_i.idFromStr(out, key);
        out->count = 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// bCPropertyID als Text (fuer das Protokoll)
bool SafeIdText(const void* id, void** text) {
    __try {
        g_i.idText(id, text);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
std::string IdToString(const void* id) {
    if (!id || !g_i.idText) return "?";
    void* text = nullptr;
    if (!SafeIdText(id, &text)) return "Ausnahme";
    std::string r = text ? g_sb.strGetText(&text) : "?";
    if (text) g_sb.strDtor(&text);
    return r;
}
void* InventoryOf(const Entity& e) { return EntIsValid(e) && ResolveInv() ? SafeInventory(e.p) : nullptr; }
int EntItemCount(const Entity& e, const SStr& name) {
    void* inv = InventoryOf(e);
    int n = -1;
    return inv && SafeItemCount(inv, &name, &n) ? n : -1;
}
int EntAddItems(Entity& e, const SStr& name, int count) {
    // Rueckgabe: >= 0 ok; -1 Inventar nicht lesbar; -4 Funktionen fehlen; -5 Szenenverwaltung fehlt; -2 Item unbekannt; -3 Ausnahme
    void* inv = InventoryOf(e);
    int rc;
    if (!inv || count <= 0)
        rc = -1;
    else if (!g_i.create || !g_i.tplByName)
        rc = -4;
    else {
        if (!g_e.sceneAdmin || !Valid(g_e.sceneAdmin)) {
            SStr mod("eCSceneAdmin");
            g_e.sceneAdmin = SafeSceneAdmin(&mod);
        }
        rc = g_e.sceneAdmin ? SafeCreateItems(inv, g_e.sceneAdmin, &name, name.c_str(), count) : -5;
    }
    if (rc < 0 && ScriptHost::s_current) {
        char b[256];
        std::snprintf(b, sizeof b,
                      "AddItems(%s, %d) fehlgeschlagen: Code %d (Entity %p gueltig %d, Inventar %p, create %p, Vorlage-Suche %p, Szene %p)",
                      name.c_str(), count, rc, e.p, int(EntIsValid(e)), inv, reinterpret_cast<void*>(g_i.create),
                      reinterpret_cast<void*>(g_i.tplByName), g_e.sceneAdmin);
        ScriptHost::s_current->Log(b);
    }
    return rc;
}
// Globale Skriptfunktion: CloneTemplate("It_Wolf_Fur", "OG3_Fell") -> 0 angelegt, 1 vorhanden, <0 Fehler (Protokoll). Danach hero.AddItems("OG3_Fell", n).
// Die Kopie lebt nur im Speicher (nicht im Spielstand, GUID wechselt bei jedem Start).
int CloneTemplate(const SStr& base, const SStr& newName) {
    int rc;
    if (!ResolveInv())
        rc = -4;
    else if (!g_i.clone || !g_i.setName || !g_i.tplByName || newName.length() == 0)
        rc = -4;
    else {
        if (!g_e.sceneAdmin || !Valid(g_e.sceneAdmin)) {
            SStr mod("eCSceneAdmin");
            g_e.sceneAdmin = SafeSceneAdmin(&mod);
        }
        const void *idB = nullptr, *idC = nullptr;
        int regOk = -2;
        PropIdVal fixedId = {};
        bool haveFixed = false;
        if (g_i.idFromStr &&
            g_i.setId) {   // ID = MD5("OG3Clone:" + Name) -> bleibt ueber Neustarts gleich (Spielstand findet die Vorlage wieder)
            const SStr key(std::string("OG3Clone:") + newName.c_str());
            haveFixed = SafeIdFromString(&fixedId, &key);
        }
        rc = g_e.sceneAdmin ? SafeCloneTemplate(g_e.sceneAdmin, &base, base.c_str(), &newName, newName.c_str(), &idB, &idC, &regOk,
                                                haveFixed ? &fixedId : nullptr)
                            : -5;
        if (rc == 0 && ScriptHost::s_current)
            ScriptHost::s_current->Log("CloneTemplate: " + std::string(base.c_str()) + " -> " + newName.c_str() + ", ID Basis " +
                                       IdToString(idB) + ", ID Klon " + IdToString(idC) + ", RegisterTemplateEntity " +
                                       std::to_string(regOk) + (haveFixed ? ", feste ID" : ", Zufalls-ID"));
    }
    if (ScriptHost::s_current) {
        char b[256];
        std::snprintf(b, sizeof b, "CloneTemplate(%s -> %s): Code %d (clone %p, setName %p)", base.c_str(), newName.c_str(), rc,
                      reinterpret_cast<void*>(g_i.clone), reinterpret_cast<void*>(g_i.setName));
        if (rc != 0 && rc != 1) ScriptHost::s_current->Log(b);
    }
    return rc;
}
// ---- NPCs/Entities aus einer Vorlage erzeugen (API 18). Ablauf wie in gCSession (Spiel-Hilfsfunktion bei Game.dll+0x173070, Konsolenbefehl "spawn"):
// gCWorld::GetCurrentWorld() -> CreateDynamicEntity() (leeres dynamisches Entity im Systemlayer) -> eCEntity::CreateFromTemplate(Vorlage) -> Position.
// Rueckgabe: gueltiges Entity oder ungueltig (Grund im Protokoll). Der NPC bekommt ueber die Vorlage alle Eigenschaftssets des Spiels.
typedef void*(__stdcall* WorldCurFn)();
typedef void*(__thiscall* WorldCreateDynFn)(void*);
typedef bool(__thiscall* EntFromTplFn)(void*, const void*);
typedef bool(__thiscall* WorldDestroyDynFn)(void*, void*);
typedef void*(__stdcall* SectorForEntFn)(const void*);         // gCWorld::GetSectorForEntity(const eCEntity*) (statisch)
typedef void*(__thiscall* WorldSysSectorFn)(void*);            // gCWorld::GetSystemSector() -> gCSector&
typedef void(__thiscall* SetLocalRotFn)(void*, const void*);   // eCEntity::SetLocalRotation(const bCQuaternion&)
std::map<void*, void*>
    g_spawned;   // von SpawnNPC erzeugte Entities -> Welt, in der sie erzeugt wurden (nur diese darf DestroyNPC entfernen)
struct SpawnApi {
    bool tried = false;
    WorldCurFn current = nullptr;
    WorldCreateDynFn createDyn = nullptr;
    EntFromTplFn fromTpl = nullptr;
    WorldDestroyDynFn destroyDyn = nullptr;
    SetLocalRotFn setLocalRot = nullptr;
    SectorForEntFn sectorFor = nullptr;
    WorldSysSectorFn sysSector = nullptr;
} g_sp;
bool ResolveSpawn() {
    if (g_sp.tried) return g_sp.current && g_sp.createDyn && g_sp.fromTpl;
    if (!ResolveInv()) return false;
    g_sp.tried = true;
    g_sp.current = reinterpret_cast<WorldCurFn>(GetProcAddress(g_e.game, "?GetCurrentWorld@gCWorld@@SGPAV1@XZ"));
    g_sp.createDyn =
        reinterpret_cast<WorldCreateDynFn>(GetProcAddress(g_e.game, "?CreateDynamicEntity@gCWorld@@QAEPAVeCDynamicEntity@@XZ"));
    g_sp.fromTpl = reinterpret_cast<EntFromTplFn>(GetProcAddress(g_e.engine, "?CreateFromTemplate@eCEntity@@QAE_NABVeCTemplateEntity@@@Z"));
    g_sp.destroyDyn =
        reinterpret_cast<WorldDestroyDynFn>(GetProcAddress(g_e.game, "?DestroyDynamicEntity@gCWorld@@QAE_NPAVeCDynamicEntity@@@Z"));
    g_sp.sectorFor =
        reinterpret_cast<SectorForEntFn>(GetProcAddress(g_e.game, "?GetSectorForEntity@gCWorld@@SGPAVgCSector@@PBVeCEntity@@@Z"));
    g_sp.sysSector = reinterpret_cast<WorldSysSectorFn>(GetProcAddress(g_e.game, "?GetSystemSector@gCWorld@@QAEAAVgCSector@@XZ"));
    g_sp.setLocalRot = reinterpret_cast<SetLocalRotFn>(GetProcAddress(g_e.engine, "?SetLocalRotation@eCEntity@@QAEXABVbCQuaternion@@@Z"));
    return g_sp.current && g_sp.createDyn && g_sp.fromTpl;
}
// Rueckgabe: 0 ok (*out = Entity), -2 Vorlage unbekannt, -3 Ausnahme, -5 keine Welt, -6 Entity nicht erzeugt, -7 CreateFromTemplate fehlgeschlagen
int SafeSpawn(void* admin, const void* name, const char* wanted, const SVec3* pos, void** out) {
    void* tpl = SafeFindTemplate(admin, name, wanted);
    if (!tpl) return -2;
    void* world = nullptr;
    void* ent = nullptr;
    __try {
        world = g_sp.current();
        if (!world) return -5;
        ent = g_sp.createDyn(world);
        if (!ent) return -6;
        if (!g_sp.fromTpl(ent, tpl)) {
            if (g_sp.destroyDyn) g_sp.destroyDyn(world, ent);
            return -7;
        }
        if (g_e.setPosSlot >= 0 && pos) {
            void** vt = *static_cast<void***>(ent);
            reinterpret_cast<SetPosFn>(vt[g_e.setPosSlot])(ent, pos);
        }
        *out = ent;
        g_spawned[ent] = world;
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -3; }
}
Entity SpawnNPC(const SStr& tplName, const SVec3& pos) {
    Entity result{nullptr};
    int rc;
    if (!ResolveSpawn())
        rc = -4;
    else {
        if (!g_e.sceneAdmin || !Valid(g_e.sceneAdmin)) {
            SStr mod("eCSceneAdmin");
            g_e.sceneAdmin = SafeSceneAdmin(&mod);
        }
        void* ent = nullptr;
        rc = g_e.sceneAdmin ? SafeSpawn(g_e.sceneAdmin, &tplName, tplName.c_str(), &pos, &ent) : -5;
        if (rc == 0) result.p = ent;
    }
    if (ScriptHost::s_current) {
        char b[256];
        std::snprintf(b, sizeof b, "SpawnNPC(%s, %.0f %.0f %.0f): %s (Code %d, Entity %p)", tplName.c_str(), pos.x, pos.y, pos.z,
                      rc == 0 ? "ok" : "fehlgeschlagen", rc, result.p);
        ScriptHost::s_current->Log(b);
    }
    return result;
}
// ---- Fokus-Entity (API 20): das Entity, das der Spieler gerade anvisiert (Fokusname/Interaktion). gCGUIManager::GetFocus() holt den Spieler, dessen
// gCFocus_PS (Eigenschaftsset 0x3B) und ruft GetFocusEntity(0, 0) - this wird dabei nicht benutzt (Disassembly Game.dll+0x7C290).
typedef void*(__thiscall* FocusGetFn)(const void*);
struct FocusApi {
    bool tried = false;
    ObjFn guiManager = nullptr;
    FocusGetFn focus = nullptr;
} g_f;
bool ResolveFocus() {
    if (g_f.tried) return g_f.guiManager && g_f.focus;
    if (!Resolve()) return false;
    g_f.tried = true;
    g_f.guiManager = reinterpret_cast<ObjFn>(GetProcAddress(g_e.game, "?GetGUIManager@gCSession@@QBEPAVgCGUIManager@@XZ"));
    g_f.focus = reinterpret_cast<FocusGetFn>(GetProcAddress(g_e.game, "?GetFocus@gCGUIManager@@IBEPAVeCEntity@@XZ"));
    return g_f.guiManager && g_f.focus;
}
void* SafeFocused() {
    __try {
        void* session = g_e.sessionInstance();
        void* gm = session ? g_f.guiManager(session) : nullptr;
        return gm ? g_f.focus(gm) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
// gCFocus_PS des Spielers: eCEntity::GetPropertySet(0x3B) ueber die vtable (Slot 0xEC / 4 = 59, wie in gCGUIManager::GetFocus), dann
// GetCrossHairEntity() (Entity unter dem Fadenkreuz) und GetCurrentPickPosition() (Weltpunkt, auf den der Spieler zielt)
typedef void*(__thiscall* PSGetFn)(void*, int);
typedef void*(__thiscall* FocusEntFn)(void*);
typedef const void*(__thiscall* FocusPosFn)(const void*);
FocusEntFn g_crossFn = nullptr;
FocusPosFn g_pickFn = nullptr;
void* SafeFocusSet() {
    __try {
        void* player = SafeHero(true);
        if (!player) return nullptr;
        void** vt = *static_cast<void***>(player);
        return reinterpret_cast<PSGetFn>(vt[59])(player, 0x3B);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
void* SafeCrosshair(void* fs) {
    __try {
        return g_crossFn(fs);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool SafePick(void* fs, SVec3* out) {
    __try {
        std::memcpy(out, g_pickFn(fs), sizeof(SVec3));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
typedef void(__thiscall* DrawCrossFn)(void*, const bool&);   // gCFocus_PS::SetDrawCrossHair(const bool&)
DrawCrossFn g_drawCrossFn = nullptr;
bool SafeDrawCross(void* fs, bool on) {
    __try {
        g_drawCrossFn(fs, on);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool ResolveCross() {
    if (!g_drawCrossFn) g_drawCrossFn = reinterpret_cast<DrawCrossFn>(GetProcAddress(g_e.game, "?SetDrawCrossHair@gCFocus_PS@@QAEXAB_N@Z"));
    if (!g_crossFn)
        g_crossFn = reinterpret_cast<FocusEntFn>(GetProcAddress(g_e.game, "?GetCrossHairEntity@gCFocus_PS@@QAEPAVeCEntity@@XZ"));
    if (!g_pickFn)
        g_pickFn = reinterpret_cast<FocusPosFn>(GetProcAddress(g_e.game, "?GetCurrentPickPosition@gCFocus_PS@@QBEABVbCVector@@XZ"));
    return Resolve() && g_crossFn && g_pickFn;
}
Entity GetCrosshairEntity() {
    Entity r{nullptr};
    if (!ResolveCross()) return r;
    void* fs = SafeFocusSet();
    void* p = fs && Valid(fs) ? SafeCrosshair(fs) : nullptr;
    if (p && Valid(p)) r.p = p;
    return r;
}
// Fadenkreuz des Spielers ein-/ausschalten (gCFocus_PS::SetDrawCrossHair). Vermutlich aktualisiert das Spiel den Zielpunkt (GetPickPosition) nur bei sichtbarem Fadenkreuz.
bool SetCrosshairVisible(bool on) {
    if (!ResolveCross() || !g_drawCrossFn) return false;
    void* fs = SafeFocusSet();
    return fs && Valid(fs) && SafeDrawCross(fs, on);
}
// Weltpunkt, auf den der Spieler zielt (Nullvektor, wenn nicht verfuegbar)
SVec3 GetPickPosition() {
    SVec3 v{0, 0, 0};
    if (!ResolveCross()) return v;
    void* fs = SafeFocusSet();
    if (fs && Valid(fs)) SafePick(fs, &v);
    return v;
}
// Ungueltiges Entity, wenn nichts anvisiert wird oder kein Spiel laeuft
Entity GetFocusedEntity() {
    Entity r{nullptr};
    if (ResolveFocus()) {
        void* p = SafeFocused();
        if (p && Valid(p)) r.p = p;
    }
    return r;
}
// ---- Bildschirm-Picking (API 22): eCPhysicsScene::PickEntity(x, y, maxDistanz, bCVector& Trefferpunkt) baut aus dem Bildschirmpixel einen Strahl der aktuellen
// Kamera und ruft TraceRayNearestHit (Hints 0x1083 = statische + dynamische Shapes, Trefferpunkt, Distanz ohne Richtungslaenge; Disassembly Engine.dll+0x174540).
// Rueckgabe: das getroffene Entity (oder 0) und der exakte Weltpunkt des Treffers. Der Mittelpunkt des Spielfensters entspricht dem Fadenkreuz.
typedef void*(__stdcall* PhysInstFn)();
typedef void*(__thiscall* PickEntFn)(void*, int, int, float, void*);
PhysInstFn g_physInst = nullptr;
PickEntFn g_pickEnt = nullptr;
SVec3 g_lastPick{0, 0, 0};
bool g_lastPickHit = false;
void* SafePickEntity(int x, int y, float maxDist, SVec3* hit) {
    __try {
        void* scene = g_physInst();
        return scene ? g_pickEnt(scene, x, y, maxDist, hit) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
// Eigener Strahl mit ausgeschlossenem Helden: PickEntity uebergibt kein auszuschliessendes Entity, aus der Verfolgerkamera trifft der Strahl deshalb zuerst den
// Helden (Spieltest: 'PC_Hero', Abstand 0). Ablauf wie PickEntity: eCCameraBase::PointToRay(Pixel) -> eCPhysicsScene::TraceRayNearestHit(Ursprung, Richtung,
// Ausschluss = Held, Hints 0x1083) -> Trefferentity bei Bericht+4, Trefferpunkt bei Bericht+0x68 (Bericht = Zeiger bei Szene+0xAC).
typedef void*(__stdcall* AppInstFn)();
typedef void*(__thiscall* AppCamFn)(void*);
typedef void(__thiscall* PtToRayFn)(const void*, const void*, void*);
typedef bool(__thiscall* TraceNearFn)(void*, const void*, const void*, const void*, unsigned, const void*, int, int);
AppInstFn g_appInst = nullptr;
AppCamFn g_appCam = nullptr;
PtToRayFn g_ptToRay = nullptr;
TraceNearFn g_traceNear = nullptr;
struct PickRay {
    float o[3], d[3];
};
void* SafePickExcluding(int x, int y, const void* exclude, float maxDist, SVec3* hit) {
    __try {
        void* app = g_appInst();
        void* cam = app ? g_appCam(app) : nullptr;
        void* scene = g_physInst();
        if (!cam || !scene) return nullptr;
        int pt[2] = {x, y};
        PickRay ray;
        g_ptToRay(cam, pt, &ray);
        if (!g_traceNear(scene, &ray.o[0], &ray.d[0], exclude, 0x1083u, nullptr, 0, 0)) return nullptr;
        char* report = *reinterpret_cast<char**>(static_cast<char*>(scene) + 0xAC);
        if (!report) return nullptr;
        void* ent = *reinterpret_cast<void**>(report + 4);
        const float* p = reinterpret_cast<const float*>(report + 0x68);
        const float dx = p[0] - ray.o[0], dy = p[1] - ray.o[1], dz = p[2] - ray.o[2];
        if (maxDist > 0 && dx * dx + dy * dy + dz * dz > maxDist * maxDist) return nullptr;
        hit->x = p[0];
        hit->y = p[1];
        hit->z = p[2];
        return ent;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool ResolvePick() {
    if (!Resolve()) return false;
    if (!g_appInst) g_appInst = reinterpret_cast<AppInstFn>(GetProcAddress(g_e.engine, "?GetInstance@eCApplication@@SGAAV1@XZ"));
    if (!g_appCam)
        g_appCam = reinterpret_cast<AppCamFn>(GetProcAddress(g_e.engine, "?GetCurrentCamera@eCApplication@@QAEAAVeCCameraBase@@XZ"));
    if (!g_ptToRay)
        g_ptToRay = reinterpret_cast<PtToRayFn>(GetProcAddress(g_e.engine, "?PointToRay@eCCameraBase@@QBEXABVbCPoint@@AAVbCRay@@@Z"));
    if (!g_traceNear)
        g_traceNear = reinterpret_cast<TraceNearFn>(GetProcAddress(
            g_e.engine, "?TraceRayNearestHit@eCPhysicsScene@@QAE_NABVbCVector@@0PBVeCEntity@@K1W4eEPropertySetType@@W4eEShapeGroup@@@Z"));
    if (!g_physInst) g_physInst = reinterpret_cast<PhysInstFn>(GetProcAddress(g_e.engine, "?GetInstance@eCPhysicsScene@@SGAAV1@XZ"));
    if (!g_pickEnt)
        g_pickEnt =
            reinterpret_cast<PickEntFn>(GetProcAddress(g_e.engine, "?PickEntity@eCPhysicsScene@@QAEPAVeCEntity@@HHMAAVbCVector@@@Z"));
    return g_physInst && g_pickEnt;
}
// Entity unter dem Bildschirmpixel (x, y), in Pixeln der Client-Flaeche des Spielfensters; der Trefferpunkt steht danach in GetPickPoint()
Entity PickScreen(int x, int y, float maxDist) {
    Entity r{nullptr};
    g_lastPickHit = false;
    if (!ResolvePick()) return r;
    SVec3 hit{0, 0, 0};
    const float md = maxDist > 0 ? maxDist : 100000.0f;
    void* p = nullptr;
    if (g_appInst && g_appCam && g_ptToRay && g_traceNear && g_physInst)
        p = SafePickExcluding(x, y, SafeHero(false), md, &hit);   // Held ausgeschlossen
    else
        p = SafePickEntity(x, y, md, &hit);
    if (p && Valid(p)) {
        r.p = p;
        g_lastPick = hit;
        g_lastPickHit = true;
    }
    return r;
}
// Entity im Mittelpunkt des Spielfensters (= unter dem Fadenkreuz)
Entity PickCenter(float maxDist) {
    RECT rc{};
    HWND w = GameWindow();
    if (!w || !GetClientRect(w, &rc)) {
        g_lastPickHit = false;
        return {nullptr};
    }
    return PickScreen((rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2, maxDist);
}
SVec3 GetPickPoint() { return g_lastPickHit ? g_lastPick : SVec3{0, 0, 0}; }
bool HasPickPoint() { return g_lastPickHit; }
// Von SpawnNPC erzeugtes Entity wieder entfernen (gCWorld::DestroyDynamicEntity). Rueckgabe: true = entfernt; der Griff im Skript bleibt, ein zweiter Aufruf wird abgelehnt.
// Fremde Entities (Welt-NPCs, Held) werden abgelehnt, ebenso Entities einer inzwischen anderen Welt.
bool SafeDestroy(void* world, void* ent) {
    __try {
        return g_sp.destroyDyn(world, ent);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void* SafeCurrentWorld() {
    __try {
        return g_sp.current();
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
// Gehoert das Entity zum Systemsektor der Welt (dort landen per SpawnNPC erzeugte Entities, auch nach Speichern und Laden)? Held/Spieler zaehlen nie.
bool SafeInSystemSector(void* world, void* ent, void** sector, void** sysSector) {
    __try {
        *sector = g_sp.sectorFor(ent);
        *sysSector = g_sp.sysSector(world);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// Hat das Entity ein gCNPC_PS (ist es ein NPC)? Script.dll PSNpc(eCEntity*) liefert das Eigenschaftsset oder 0
typedef void(__thiscall* PSNpcCtorFn)(void*, void*);
PSNpcCtorFn g_psNpc = nullptr;
bool SafeHasNpcSet(void* ent) {
    __try {
        void* holder = nullptr;
        g_psNpc(&holder, ent);
        return holder && Valid(holder);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool IsNpc(void* ent) {
    if (!g_psNpc) {
        if (HMODULE script = GetModuleHandleA("Script.dll"))
            g_psNpc = reinterpret_cast<PSNpcCtorFn>(GetProcAddress(script, "??0PSNpc@@QAE@PAVeCEntity@@@Z"));
    }
    return g_psNpc && ent && SafeHasNpcSet(ent);
}
bool IsNPCScript(const Entity& e) { return EntIsValid(e) && IsNpc(e.p); }
// Grobe Einordnung nach Namensmustern, die im Spieltest beobachtet wurden (G3_Myrtana_House_*, G3_Object_Interact_*, G3_Myrtana_Landscape_*, G3_World_Border_*).
// Ergebnis: "NPC", "Building", "Object", "Landscape", "Border", "Unnamed" (kein Name: Baeume, Steine, Kleinteile - nicht naeher bestimmbar), "Other", "" (ungueltig)
SStr EntCategory(const Entity& e) {
    if (!EntIsValid(e)) return SStr();
    if (IsNpc(e.p)) return SStr("NPC");
    const char* t = nullptr;
    std::string n = SafeName(e.p, &t) && t ? t : "";
    for (char& c : n) c = char(tolower(static_cast<unsigned char>(c)));
    if (n.empty()) return SStr("Unnamed");
    if (n.find("_house_") != std::string::npos) return SStr("Building");
    if (n.find("g3_object_") == 0) return SStr("Object");
    if (n.find("_landscape_") != std::string::npos) return SStr("Landscape");
    if (n.find("world_border") != std::string::npos) return SStr("Border");
    return SStr("Other");
}
bool EntIsBuilding(const Entity& e) { return std::strcmp(EntCategory(e).c_str(), "Building") == 0; }
// Im Systemsektor liegen auch Welt-Objekte (z. B. G3_World_Border_001): deshalb zusaetzlich NPC-Pflicht.
bool IsDynamicSpawn(const Entity& e) {
    if (!EntIsValid(e) || !ResolveSpawn() || !g_sp.sectorFor || !g_sp.sysSector) return false;
    if (!IsNpc(e.p)) return false;
    if (e.p == SafeHero(false) || e.p == SafeHero(true)) return false;
    void* world = SafeCurrentWorld();
    void *sec = nullptr, *sys = nullptr;
    return world && SafeInSystemSector(world, e.p, &sec, &sys) && sec && sec == sys;
}
// Entfernt ein per SpawnNPC erzeugtes Entity: eines dieser Sitzung (Liste) oder eines im Systemsektor (nach Laden eines Spielstands).
// Entities aus normalen Sektoren (Welt-NPCs), Held und Spieler werden abgelehnt. Rueckgabe: true = entfernt.
bool DestroyNPC(const Entity& e) {
    char whyBuf[120];
    const char* why = nullptr;
    bool ok = false;
    auto it = g_spawned.find(e.p);
    if (!e.p)
        why = "Entity ungueltig";
    else if (!ResolveSpawn() || !g_sp.destroyDyn)
        why = "Funktion fehlt";
    else if (e.p == SafeHero(false) || e.p == SafeHero(true))
        why = "Held/Spieler";
    else {
        void* world = SafeCurrentWorld();
        const bool mine = it != g_spawned.end() && it->second == world;
        if (!world)
            why = "keine Welt";
        else if (!IsNpc(e.p))
            why = "kein NPC (DestroyNPC entfernt nur NPCs)";
        else if (mine || IsDynamicSpawn(e)) {
            ok = SafeDestroy(world, e.p);
            if (!ok) why = "DestroyDynamicEntity lieferte false";
        } else {
            void *sec = nullptr, *sys = nullptr;
            SafeInSystemSector(world, e.p, &sec, &sys);
            std::snprintf(whyBuf, sizeof whyBuf, "nicht im Systemsektor (Sektor %p, Systemsektor %p)", sec, sys);
            why = whyBuf;
        }
    }
    if (it != g_spawned.end()) g_spawned.erase(it);
    if (ScriptHost::s_current) {
        char b[200];
        std::snprintf(b, sizeof b, "DestroyNPC(%p): %s%s%s", e.p, ok ? "ok" : "fehlgeschlagen", why ? " - " : "", why ? why : "");
        ScriptHost::s_current->Log(b);
    }
    return ok;
}
// Entities nach Teilname suchen (Zaehlen + Index): z. B. alle "Diego"
void* SceneAdminNow() {
    if (!g_e.sceneAdmin || !Valid(g_e.sceneAdmin)) {
        SStr mod("eCSceneAdmin");
        g_e.sceneAdmin = SafeSceneAdmin(&mod);
    }
    return g_e.sceneAdmin;
}
int SafeCountPart(void* admin, const void* part) {
    __try {
        return g_i.countPart(admin, part, 0 /* Entity */);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
void* SafeByPart(void* admin, const void* part, int index) {
    __try {
        return g_i.byPart(admin, part, 0, index);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
int CountEntities(const SStr& part) {
    if (!ResolveInv() || !g_i.countPart) return 0;
    void* admin = SceneAdminNow();
    return admin ? SafeCountPart(admin, &part) : 0;
}
Entity GetEntityByPart(const SStr& part, int index) {
    Entity r{nullptr};
    if (!ResolveInv() || !g_i.byPart || index < 0) return r;
    void* admin = SceneAdminNow();
    if (admin) r.p = SafeByPart(admin, &part, index);
    return r;
}
// Drehung: SetLocalRotation (fuer Entities direkt unter der Welt = Weltdrehung). Quaternion (x, y, z, w), Y ist oben.
bool SafeSetRot(void* ent, const SQuat* q) {
    __try {
        g_sp.setLocalRot(ent, q);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool EntSetRotation(Entity& e, const SQuat& q) { return EntIsValid(e) && ResolveSpawn() && g_sp.setLocalRot && SafeSetRot(e.p, &q); }
// Blickrichtung zu einem Ziel (nur um die Hochachse): Gier = atan2(dx, dz) in Grad plus Versatz (Standard 0; falls der NPC vom Ziel weg schaut: 180)
bool EntLookAt(Entity& e, const Entity& target, float yawOffsetDeg) {
    if (!EntIsValid(e) || !EntIsValid(target)) return false;
    SVec3 a{0, 0, 0}, b{0, 0, 0};
    if (!SafePos(e.p, &a) || !SafePos(target.p, &b)) return false;
    const float dx = b.x - a.x, dz = b.z - a.z;
    if (dx * dx + dz * dz < 1e-6f) return false;
    const float yaw = std::atan2(dx, dz) + yawOffsetDeg * 0.01745329252f;
    const SQuat q{0.0f, std::sin(yaw * 0.5f), 0.0f, std::cos(yaw * 0.5f)};
    return EntSetRotation(e, q);
}
// Vorlagennamen suchen (Teilname, Gross-/Kleinschreibung egal): "Name1;Name2;..." (hoechstens max Treffer, leer = keine)
bool SafeTemplateNames(void* admin, const void* part, int maxn, std::string* out) {
    __try {
        const int n = g_i.countPart(admin, part, 2);
        for (int i = 0; i < n && i < maxn; ++i) {
            void* c = g_i.byPart(admin, part, 2, i);
            const char* nm = c ? g_sb.strGetText(g_e.getName(c)) : nullptr;
            if (nm) {
                if (!out->empty()) *out += ";";
                *out += nm;
            }
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
SStr FindTemplateNames(const SStr& part, int maxn) {
    std::string r;
    if (ResolveInv() && g_i.countPart && g_i.byPart) {
        if (!g_e.sceneAdmin || !Valid(g_e.sceneAdmin)) {
            SStr mod("eCSceneAdmin");
            g_e.sceneAdmin = SafeSceneAdmin(&mod);
        }
        if (g_e.sceneAdmin) SafeTemplateNames(g_e.sceneAdmin, &part, maxn < 1 ? 1 : maxn > 200 ? 200 : maxn, &r);
    }
    return SStr(r);
}
int EntRemoveItems(Entity& e, const SStr& name, int count) {   // Rueckgabe: Zahl entfernter Stuecke (-1 bei Fehler)
    void* inv = InventoryOf(e);
    int r = 0;
    return inv && count > 0 && SafeRemoveItems(inv, &name, count, &r) ? r : -1;
}

void* WorldClock() {
    ResolveStats();
    return g_s.clockOk ? SafeWorldClock() : nullptr;
}
int ClockGet(ClockGetFn StatsApi::*fn) {
    int v = -1;
    void* c = WorldClock();
    if (!c || !SafeClockGet(g_s.*fn, c, &v)) return -1;
    return v;
}
int GameDay() { return ClockGet(&StatsApi::day); }
int GameHour() { return ClockGet(&StatsApi::hour); }
int GameMinute() { return ClockGet(&StatsApi::minute); }
bool SetGameTime(int day, int hour, int minute) {
    void* c = WorldClock();
    if (!c || hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;
    return (day < 0 || SafeClockSet(g_s.setDay, c, day)) && SafeClockSet(g_s.setHour, c, hour) && SafeClockSet(g_s.setMinute, c, minute);
}

// ---- Bildschirmtext + Menueseite (API 3): gCSession::GetScreenOutputCtrl()->PrintScreen(bCUnicodeString, bCByteAlphaColor, Sekunden)
// wie Script.dll GUI::PrintScreen_Debug, aber ohne "Debug: "-Praefix und mit eigener Farbe; gCGUIManager::PrintGameMessage/GetActivePage
// wie GUI2. Unicode-String und Farbe ueber SharedBase-Exporte in rohen Puffern (Farbe 12 Byte: vtable, r, g, b, a).
typedef void(__thiscall* ScreenPrintFn)(void*, const void*, const void*, float);
typedef void(__thiscall* GameMsgFn)(void*, const void*, int);
typedef int(__thiscall* PageFn)(const void*);
typedef void*(__thiscall* UniCtorFn)(void*, const char*);
typedef void(__thiscall* DtorFn)(void*);
typedef void*(__thiscall* ColorCtorFn)(void*, unsigned char, unsigned char, unsigned char, unsigned char);
struct ScreenApi {
    bool tried = false, ok = false;
    ObjFn screenCtrl = nullptr, guiManager = nullptr;
    ScreenPrintFn print = nullptr;
    GameMsgFn gameMsg = nullptr;
    PageFn activePage = nullptr;
    UniCtorFn uniCtor = nullptr;
    DtorFn uniDtor = nullptr, colorDtor = nullptr;
    ColorCtorFn colorCtor = nullptr;
} g_g;

bool ResolveScreen() {
    if (g_g.ok) return true;
    if (!Resolve() || !g_sb.module) return false;
    auto G = [](const char* n) { return reinterpret_cast<void*>(GetProcAddress(g_e.game, n)); };
    auto B = [](const char* n) { return reinterpret_cast<void*>(GetProcAddress(g_sb.module, n)); };
    g_g.screenCtrl = reinterpret_cast<ObjFn>(G("?GetScreenOutputCtrl@gCSession@@QAEPAVgCScreenOutputCtrl@@XZ"));
    g_g.guiManager = reinterpret_cast<ObjFn>(G("?GetGUIManager@gCSession@@QBEPAVgCGUIManager@@XZ"));
    g_g.print = reinterpret_cast<ScreenPrintFn>(G("?PrintScreen@gCScreenOutputCtrl@@QAEXABVbCUnicodeString@@ABVbCByteAlphaColor@@M@Z"));
    g_g.gameMsg = reinterpret_cast<GameMsgFn>(G("?PrintGameMessage@gCGUIManager@@QAEXABVbCUnicodeString@@W4gEGameMessageType@@@Z"));
    g_g.activePage = reinterpret_cast<PageFn>(G("?GetActivePage@gCGUIManager@@QBE?AW4gEGamePage@@XZ"));
    g_g.uniCtor = reinterpret_cast<UniCtorFn>(B("??0bCUnicodeString@@QAE@PBD@Z"));
    g_g.uniDtor = reinterpret_cast<DtorFn>(B("??1bCUnicodeString@@QAE@XZ"));
    g_g.colorCtor = reinterpret_cast<ColorCtorFn>(B("??0bCByteAlphaColor@@QAE@EEEE@Z"));
    g_g.colorDtor = reinterpret_cast<DtorFn>(B("??1bCByteAlphaColor@@QAE@XZ"));
    g_g.ok = g_g.screenCtrl && g_g.guiManager && g_g.print && g_g.gameMsg && g_g.activePage && g_g.uniCtor && g_g.uniDtor &&
             g_g.colorCtor && g_g.colorDtor;
    if (!g_g.tried && ScriptHost::s_current)
        ScriptHost::s_current->Log(g_g.ok ? "Bildschirm-Bindings bereit" : "Bildschirm-Bindings: Exporte fehlen");
    g_g.tried = true;
    return g_g.ok;
}

bool SafePrint(const char* text, float seconds, int r, int g, int b) {
    alignas(8) unsigned char uni[16] = {}, col[16] = {};
    __try {
        void* ctrl = g_g.screenCtrl(g_e.sessionInstance());
        if (!ctrl) return false;
        g_g.uniCtor(uni, text);
        g_g.colorCtor(col, static_cast<unsigned char>(r), static_cast<unsigned char>(g), static_cast<unsigned char>(b), 0xff);
        g_g.print(ctrl, uni, col, seconds);
        g_g.colorDtor(col);
        g_g.uniDtor(uni);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeGameMsg(const char* text, int type) {
    alignas(8) unsigned char uni[16] = {};
    __try {
        void* gm = g_g.guiManager(g_e.sessionInstance());
        if (!gm) return false;
        g_g.uniCtor(uni, text);
        g_g.gameMsg(gm, uni, type);
        g_g.uniDtor(uni);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
int SafeActivePage() {
    __try {
        void* gm = g_g.guiManager(g_e.sessionInstance());
        return gm ? g_g.activePage(gm) : -1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
int Clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
bool PrintScreenText(const SStr& s, float seconds, int r, int g, int b) {
    return ResolveScreen() && SafePrint(s.c_str(), seconds > 0 ? seconds : 3.0f, Clamp255(r), Clamp255(g), Clamp255(b));
}
bool PrintGameMessageText(const SStr& s, int type) { return ResolveScreen() && SafeGameMsg(s.c_str(), type); }
int GetActivePage() { return ResolveScreen() ? SafeActivePage() : -1; }

}   // namespace

bool RegisterEngineBindings(asIScriptEngine* e) {
    bool ok = true;
    auto R = [&](int r) {
        if (r < 0) ok = false;
    };
    OG3_REG(e->RegisterObjectType("Entity", sizeof(Entity), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<Entity>()));
    OG3_REG(e->RegisterObjectBehaviour("Entity", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(EntConstruct), asCALL_CDECL_OBJLAST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bool IsValid() const", asFUNCTION(EntIsValid), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bCString GetName() const", asFUNCTION(EntName), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bCVector GetPosition() const", asFUNCTION(EntPos), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bCString GetTemplateName() const", asFUNCTION(EntTemplateName), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bCVector GetBoundsMin() const", asFUNCTION(EntBoundsMin), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bCVector GetBoundsMax() const", asFUNCTION(EntBoundsMax), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "Entity GetParent() const", asFUNCTION(EntParent), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bCMatrix4 GetWorldMatrix() const", asFUNCTION(EntWorldMatrix), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bCQuaternion GetRotation() const", asFUNCTION(EntRot), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "float GetDistanceTo(const Entity &in) const", asFUNCTION(EntDist), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bool SetPosition(const bCVector &in)", asFUNCTION(EntSetPos), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bool opEquals(const Entity &in) const", asFUNCTION(EntEquals), asCALL_CDECL_OBJFIRST));
    // API 3: Attribute + Spieluhr (Fehler/kein Spiel: -1 bzw. false)
    OG3_REG(e->RegisterObjectMethod("Entity", "int GetAttribute(const bCString &in) const", asFUNCTION(EntAttr), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "int GetAttributeBase(const bCString &in) const", asFUNCTION(EntAttrBase),
                                    asCALL_CDECL_OBJFIRST));
    OG3_REG(
        e->RegisterObjectMethod("Entity", "int GetAttributeMax(const bCString &in) const", asFUNCTION(EntAttrMax), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bool SetAttributeBase(const bCString &in, int)", asFUNCTION(EntSetAttrBase),
                                    asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bool AddAttributeBase(const bCString &in, int)", asFUNCTION(EntAddAttrBase),
                                    asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bool SetAttribute(const bCString &in, int)", asFUNCTION(EntSetAttr), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bool AddAttribute(const bCString &in, int)", asFUNCTION(EntAddAttr), asCALL_CDECL_OBJFIRST));
    OG3_REG(
        e->RegisterObjectMethod("Entity", "int GetItemCount(const bCString &in) const", asFUNCTION(EntItemCount), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "int AddItems(const bCString &in, int)", asFUNCTION(EntAddItems), asCALL_CDECL_OBJFIRST));
    OG3_REG(
        e->RegisterObjectMethod("Entity", "int RemoveItems(const bCString &in, int)", asFUNCTION(EntRemoveItems), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterGlobalFunction("int CloneTemplate(const bCString &in base, const bCString &in newName)", asFUNCTION(CloneTemplate),
                                      asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("Entity SpawnNPC(const bCString &in templateName, const bCVector &in position)", asFUNCTION(SpawnNPC),
                                      asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("Entity PickScreen(int x, int y, float maxDistance = 100000)", asFUNCTION(PickScreen), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("Entity PickCenter(float maxDistance = 100000)", asFUNCTION(PickCenter), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bCVector GetPickPoint()", asFUNCTION(GetPickPoint), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool HasPickPoint()", asFUNCTION(HasPickPoint), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("Entity GetFocusedEntity()", asFUNCTION(GetFocusedEntity), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("Entity GetCrosshairEntity()", asFUNCTION(GetCrosshairEntity), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool SetCrosshairVisible(bool)", asFUNCTION(SetCrosshairVisible), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bCVector GetPickPosition()", asFUNCTION(GetPickPosition), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool DestroyNPC(const Entity &in)", asFUNCTION(DestroyNPC), asCALL_CDECL));
    OG3_REG(e->RegisterObjectMethod("Entity", "bCString GetCategory() const", asFUNCTION(EntCategory), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bool IsBuilding() const", asFUNCTION(EntIsBuilding), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterGlobalFunction("bool IsNPC(const Entity &in)", asFUNCTION(IsNPCScript), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool IsDynamicSpawn(const Entity &in)", asFUNCTION(IsDynamicSpawn), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("int CountEntities(const bCString &in part)", asFUNCTION(CountEntities), asCALL_CDECL));
    OG3_REG(
        e->RegisterGlobalFunction("Entity GetEntityByPart(const bCString &in part, int index)", asFUNCTION(GetEntityByPart), asCALL_CDECL));
    OG3_REG(
        e->RegisterObjectMethod("Entity", "bool SetRotation(const bCQuaternion &in)", asFUNCTION(EntSetRotation), asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterObjectMethod("Entity", "bool LookAt(const Entity &in, float yawOffsetDegrees = 0)", asFUNCTION(EntLookAt),
                                    asCALL_CDECL_OBJFIRST));
    OG3_REG(e->RegisterGlobalFunction("bCString FindTemplateNames(const bCString &in part, int max = 20)", asFUNCTION(FindTemplateNames),
                                      asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool SetGameEvent(const bCString &in)", asFUNCTION(SetGameEvent), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool ClearGameEvent(const bCString &in)", asFUNCTION(ClearGameEvent), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool IsGameEventSet(const bCString &in)", asFUNCTION(IsGameEventSet), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("int GetGameDay()", asFUNCTION(GameDay), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("int GetGameHour()", asFUNCTION(GameHour), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("int GetGameMinute()", asFUNCTION(GameMinute), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool SetGameTime(int day, int hour, int minute)", asFUNCTION(SetGameTime), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool PrintScreen(const bCString &in, float seconds = 3, int r = 255, int g = 255, int b = 255)",
                                      asFUNCTION(PrintScreenText), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool PrintGameMessage(const bCString &in, int type = 0)", asFUNCTION(PrintGameMessageText),
                                      asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("int GetActivePage()", asFUNCTION(GetActivePage), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("Entity GetHero()", asFUNCTION(GetHero), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("Entity GetPlayer()", asFUNCTION(GetPlayer), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("Entity FindEntity(const bCString &in)", asFUNCTION(FindEntity), asCALL_CDECL));
    if (!RegisterQuestBindings(e)) ok = false;
    if (!RegisterDialogBindings(e)) ok = false;
    return ok;
}

// Name der Template-Entity zu einer Property-ID (GUID), z. B. eines Items; leer, wenn unbekannt
std::string TemplateNameForId(const void* pid) {
    if (!pid || !Resolve() || !g_e.byId) return std::string();
    if (!g_e.sceneAdmin || !Valid(g_e.sceneAdmin)) {
        SStr mod("eCSceneAdmin");
        g_e.sceneAdmin = SafeSceneAdmin(&mod);
    }
    void* tpl = nullptr;
    if (!g_e.sceneAdmin || !SafeById(g_e.sceneAdmin, pid, &tpl) || !tpl || !Valid(tpl)) return std::string();
    const char* t = nullptr;
    return SafeName(tpl, &t) && t ? std::string(t) : std::string();
}

// Held-Treffer (game_hooks.cpp, Override_Damage): ist die Entity die Spielfigur? Lebenspunkte-Attribut "HP" des Helden um delta aendern (nicht, wenn er schon tot ist)
bool IsPlayerEntity(const void* p) { return p && Resolve() && (p == SafeHero(false) || p == SafeHero(true)); }
bool AdjustHeroHitPoints(const void* ent, int delta) {
    Entity e{const_cast<void*>(ent)};
    SStr hp("HP");
    if (delta > 0 && EntAttr(e, hp) <= 0) return false;
    return AttrSet(e, hp, delta, &StatsApi::addValue);
}

}   // namespace og3p
