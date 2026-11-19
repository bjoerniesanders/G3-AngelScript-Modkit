// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Additional permissions: LICENSE-EXCEPTIONS.md
// Engine and game bindings of G3ASM (script interface from version 2): type Entity (hero, player, entity by name),
// name, position, rotation, distance, set the position.
// The code resolves all functions from Engine.dll and Game.dll at the first use (the DLLs load after the proxy).
// Safety: Entity is a value with a raw pointer. Before each access, the code makes sure that the pointer is readable and that its vtable is in
// Engine.dll, Game.dll or Gothic3.exe. Each call runs under __try (entities that are not valid or deleted give default values in place
// of a crash). Scripts must not keep entities across a world change (get them again when necessary).
#include <windows.h>

#include <angelscript.h>

#include <cmath>
#include <cstring>
#include <map>
#include <new>
#include <set>
#include <string>

#include "sb_api.h"
#include "script_host.h"

namespace g3asm {
bool RegisterQuestBindings(asIScriptEngine* e);    // quest_bindings.cpp
bool RegisterDialogBindings(asIScriptEngine* e);   // dialog_bindings.cpp
bool RegisterScriptCallBindings(asIScriptEngine* e);   // script_calls.cpp
bool RegisterDrawBindings(asIScriptEngine* e);         // draw_api.cpp
bool RegisterMoreEventBindings(asIScriptEngine* e);    // more_events.cpp
bool RegisterSchemaBindings(asIScriptEngine* e);       // schema_dump.cpp
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
    int setPosSlot = -1;   // index of SetWorldPosition in the eCEntity vtable
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
    if (!g_e.engine || !g_e.game) return false;   // try again later (for example never available in the test program)
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

// validity: readable pointer, vtable in a game module
bool Valid(const void* p) {
    if (!ReadableMem(p, sizeof(void*))) return false;
    __try {
        const void* vt = *static_cast<void* const*>(p);   // NOLINT(clang-analyzer-core.NullDereference): ReadableMem above, and SEH
        return InModule(g_e.engine, vt) || InModule(g_e.game, vt) || InModule(GetModuleHandleA(nullptr), vt);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// ---- raw calls with SEH protection (without C++ objects with a destructor in the __try block)
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
SMat4 EntWorldMatrix(const Entity& e) {   // identity matrix if there is an error
    SMat4 m;
    std::memset(&m, 0, sizeof m);
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    if (EntIsValid(e) && g_e.getWorldMat) SafeMat(e.p, &m);
    return m;
}
// ---- detect buildings and objects: template name, bounding box (world, with children), parent object (the crosshair frequently hits parts of objects)
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

// ---- Attributes (API 3): gCPlayerMemory_PS of the entity (hero or NPC), tags as in the game: "STR", "DEX", "INT", "HP", "MP", "SP",
// "XP", "LP", "ALC", "SMI", "THF" and others. Path as Script.dll PSPlayerMemory::AddStrength: PSPlayerMemory(eCEntity*) gets the
// property set, then gCPlayerMemory_PS::Get/Set/AddBaseValue(bCString). Clock: gCWorld::GetCurrentWorld()->GetWorldEntity() -> PSClock.
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

// ---- Game event flags: small switches (text) that the game saves in the saved game (PlayerMemory of the hero). For mods that must know across save and load
// how far the player is. Script.dll PSPlayerMemory: this = the wrapper object (first field = gCPlayerMemory_PS*)
typedef bool(__thiscall* EventFn)(void*, const void*);
EventFn g_evSet = nullptr, g_evClear = nullptr, g_evIs = nullptr;
bool ResolveEvents() {
    if (g_evSet && g_evClear && g_evIs) return true;
    if (!ResolveStats()) return false;
    HMODULE script = GetModuleHandleA("Script.dll");
    g_evSet = reinterpret_cast<EventFn>(GetProcAddress(script, "?SetGameEvent@PSPlayerMemory@@QAE_NABVbCString@@@Z"));
    g_evClear = reinterpret_cast<EventFn>(GetProcAddress(script, "?ClearGameEvent@PSPlayerMemory@@QAE_NABVbCString@@@Z"));
    g_evIs = reinterpret_cast<EventFn>(GetProcAddress(script, "?IsSetGameEvent@PSPlayerMemory@@QAE_NABVbCString@@@Z"));
    return g_evSet && g_evClear && g_evIs;
}
bool SafeEvent(EventFn fn, void* ent, const void* name, bool* out) {
    __try {
        void* wrapper[4] = {nullptr, nullptr, nullptr, nullptr};   // wrapper object, sufficient space
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

// ---- Inventory (API 7): gCInventory_PS of the entity (Script.dll PSInventory(eCEntity*) gives the property set), items by the name
// of the template entity ("It_Wolf_Fur"): read the quantity, remove. The code finds stacks through FindStackIndex(bCString).
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
typedef bool(__thiscall* RegTplFn)(void*, void*);   // eCSceneAdmin::RegisterTemplateEntity(eCTemplateEntity*) (protected, but exported)
typedef const void*(__thiscall* NodeIdFn)(const void*);   // eCNode::GetID() const -> const bCPropertyID&
struct PropIdVal {
    unsigned char core[16];
    unsigned count;
};                                                           // bCPropertyID (20 bytes) as a value parameter
typedef void(__thiscall* SetIdFn)(void*, PropIdVal);         // eCNode::SetID(bCPropertyID): unregisters the old ID, sets the new one, RegisterNodeID
typedef void(__thiscall* IdFromStrFn)(void*, const void*);   // bCPropertyID::CreateFromString(const bCString&): MD5 of the text
typedef void*(__thiscall* IdTextFn)(const void*, void*);     // bCPropertyID::GetText() -> bCString (return value through a pointer)
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
// total quantity of the item across all stacks with this name
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
// removes up to n items (stack by stack); return value in *removed
bool SafeRemoveItems(void* inv, const void* name, int n, int* removed) {
    __try {
        int left = n;
        for (int guard = 0; guard < 256 && left > 0; ++guard) {
            const int idx = g_i.find(inv, name, 0, 0, -1);   // after a delete, indexes can move: always search again
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
// Make items: get the template entity through the name, gCInventory_PS::CreateItems(template, quality 0, quantity)
// Return value: >= 0 result of CreateItems, -2 template not found, -3 exception
// template entity by name; first GetTemplateEntityByName, then a search by part of the name with an exact name comparison. nullptr = not known
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
// Make items: get the template entity through the name, gCInventory_PS::CreateItems(template, quality 0, quantity)
// Return value: >= 0 result of CreateItems, -2 template not found, -3 exception
int SafeCreateItems(void* inv, void* admin, const void* name, const char* wanted, int count) {
    void* tpl = SafeFindTemplate(admin, name, wanted);
    if (!tpl) return -2;
    __try {
        // call sequence in accordance with the disassembly of CreateItems(template, quality, quantity): the THIRD value is the quantity (must be > 0)
        return g_i.create(inv, tpl, 0u, unsigned(count));
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -3; }
}
// Clone a template in memory: eCNode::Clone (as the game does: DoClone, Create, register the node ID, CopyFrom, attach to the root),
// SetName (the engine itself puts the name into the name table) and - important - RegisterTemplateEntity: Clone() registers the clone only as a
// node, not in the template table. Without this entry, the inventory does not find the template through its ID (proxy without an ID -> crash in the comparison).
// For a duplicate ID, RegisterTemplateEntity itself gives a new one.
// Return value: 0 made, 1 already existed, -2 base not known, -3 exception, -6 clone failed, -7 name not found after SetName,
//               -8 clone not found through its ID. *idBase/*idClone: ID pointers for the log (valid only while the templates exist).
int SafeCloneTemplate(void* admin, const void* baseName, const char* baseWanted, const void* newName, const char* newWanted,
                      const void** idBase, const void** idClone, int* regOk, const PropIdVal* fixedId) {
    if (SafeFindTemplate(admin, newName, newWanted)) return 1;
    void* base = SafeFindTemplate(admin, baseName, baseWanted);
    if (!base) return -2;
    void* clone = nullptr;
    __try {
        clone = g_i.clone(base);
        if (!clone) return -6;
        if (fixedId && g_i.setId) g_i.setId(clone, *fixedId);   // fixed ID (from the name): the same template at each game start
        g_i.setName(clone, newName);
        if (g_i.nodeId) {
            *idBase = g_i.nodeId(base);
            *idClone = g_i.nodeId(clone);
        }
        *regOk = g_i.regTpl ? (g_i.regTpl(admin, clone) ? 1 : 0) : -1;
        if (g_i.nodeId) *idClone = g_i.nodeId(clone);   // read again after the entry (a new ID is given for a duplicate ID)
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
// bCPropertyID as text (for the log)
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
    // return value: >= 0 ok; -1 inventory not readable; -4 functions missing; -5 scene management missing; -2 item not known; -3 exception
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
// ---- Skills (API 30): the inventory of the hero knows learned skills and has an activation counter for each skill (weapons with ActivateSkill1/2 increase it
// while the hero uses them). gCInventory_PS::LearnSkill/ActivateSkill/DeactivateSkill/HasLearnedSkill/IsSkillActive/GetSkillActivationCount
// take the skill template (eCTemplateEntity). Replacement for ActivateSkill2, which cannot be written (references cannot be set as text).
typedef bool(__thiscall* SkillBoolFn)(void*, const void*);
typedef int(__thiscall* SkillCountFn)(const void*, const void*);
struct SkillApi {
    bool tried = false;
    SkillBoolFn learn = nullptr, activate = nullptr, deactivate = nullptr, hasLearned = nullptr, isActive = nullptr;
    SkillCountFn count = nullptr;
} g_sk;

bool ResolveSkills() {
    if (g_sk.tried) return g_sk.learn && g_sk.activate && g_sk.deactivate && g_sk.hasLearned && g_sk.isActive && g_sk.count;
    if (!ResolveInv()) return false;
    g_sk.tried = true;
    auto G = [](const char* n) { return reinterpret_cast<void*>(GetProcAddress(g_e.game, n)); };
    g_sk.learn = reinterpret_cast<SkillBoolFn>(G("?LearnSkill@gCInventory_PS@@QAE_NPBVeCTemplateEntity@@@Z"));
    g_sk.activate = reinterpret_cast<SkillBoolFn>(G("?ActivateSkill@gCInventory_PS@@QAE_NPBVeCTemplateEntity@@@Z"));
    g_sk.deactivate = reinterpret_cast<SkillBoolFn>(G("?DeactivateSkill@gCInventory_PS@@QAE_NPBVeCTemplateEntity@@@Z"));
    g_sk.hasLearned = reinterpret_cast<SkillBoolFn>(G("?HasLearnedSkill@gCInventory_PS@@QBE_NPBVeCTemplateEntity@@@Z"));
    g_sk.isActive = reinterpret_cast<SkillBoolFn>(G("?IsSkillActive@gCInventory_PS@@QBE_NPBVeCTemplateEntity@@@Z"));
    g_sk.count = reinterpret_cast<SkillCountFn>(G("?GetSkillActivationCount@gCInventory_PS@@QBEHPBVeCTemplateEntity@@@Z"));
    return g_sk.learn && g_sk.activate && g_sk.deactivate && g_sk.hasLearned && g_sk.isActive && g_sk.count;
}
// op: 0 learn, 1 activate, 2 deactivate, 3 learned?, 4 active?, 5 activation counter. Return value -1 error or not known (also exception)
int SafeSkillOp(void* inv, const void* tpl, int op) {
    __try {
        switch (op) {
            case 0: return g_sk.learn(inv, tpl) ? 1 : 0;
            case 1: return g_sk.activate(inv, tpl) ? 1 : 0;
            case 2: return g_sk.deactivate(inv, tpl) ? 1 : 0;
            case 3: return g_sk.hasLearned(inv, tpl) ? 1 : 0;
            case 4: return g_sk.isActive(inv, tpl) ? 1 : 0;
            default: return g_sk.count(inv, tpl);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
int SkillOp(const Entity& e, const SStr& skill, int op) {
    void* inv = InventoryOf(e);
    if (!inv || !ResolveSkills()) return -1;
    if (!g_e.sceneAdmin || !Valid(g_e.sceneAdmin)) {
        SStr mod("eCSceneAdmin");
        g_e.sceneAdmin = SafeSceneAdmin(&mod);
    }
    void* tpl = g_e.sceneAdmin ? SafeFindTemplate(g_e.sceneAdmin, &skill, skill.c_str()) : nullptr;
    return tpl ? SafeSkillOp(inv, tpl, op) : -1;
}
bool EntLearnSkill(const Entity& e, const SStr& s) { return SkillOp(e, s, 0) == 1; }
bool EntActivateSkill(const Entity& e, const SStr& s) { return SkillOp(e, s, 1) == 1; }
bool EntDeactivateSkill(const Entity& e, const SStr& s) { return SkillOp(e, s, 2) == 1; }
bool EntHasLearnedSkill(const Entity& e, const SStr& s) { return SkillOp(e, s, 3) == 1; }
bool EntIsSkillActive(const Entity& e, const SStr& s) { return SkillOp(e, s, 4) == 1; }
int EntSkillActivationCount(const Entity& e, const SStr& s) { return SkillOp(e, s, 5); }
// Global script function: CloneTemplate("It_Wolf_Fur", "G3ASM_Fell") -> 0 made, 1 exists, <0 error (log). Then hero.AddItems("G3ASM_Fell", n).
// The copy exists only in memory (not in the saved game), and the code makes it again at each start. Its ID is fixed (from the name, see below).
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
            g_i.setId) {   // ID = MD5("OG3Clone:" + name) -> stays the same across new starts (the saved game finds the template again)
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
// ---- Make NPCs or entities from a template (API 18). Sequence as in gCSession (game helper function at Game.dll+0x173070, console command "spawn"):
// gCWorld::GetCurrentWorld() -> CreateDynamicEntity() (empty dynamic entity in the system layer) -> eCEntity::CreateFromTemplate(template) -> position.
// Return value: a valid entity, or not valid (reason in the log). Through the template, the NPC gets all property sets of the game.
typedef void*(__stdcall* WorldCurFn)();
typedef void*(__thiscall* WorldCreateDynFn)(void*);
typedef bool(__thiscall* EntFromTplFn)(void*, const void*);
typedef bool(__thiscall* WorldDestroyDynFn)(void*, void*);
typedef void*(__stdcall* SectorForEntFn)(const void*);         // gCWorld::GetSectorForEntity(const eCEntity*) (statisch)
typedef void*(__thiscall* WorldSysSectorFn)(void*);            // gCWorld::GetSystemSector() -> gCSector&
typedef void(__thiscall* SetLocalRotFn)(void*, const void*);   // eCEntity::SetLocalRotation(const bCQuaternion&)
std::map<void*, void*>
    g_spawned;   // entities made by SpawnNPC -> the world in which they were made (DestroyNPC can remove only these)
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
// return value: 0 ok (*out = entity), -2 template not known, -3 exception, -5 no world, -6 entity not made, -7 CreateFromTemplate failed
int SafeSpawn(void* admin, const void* name, const char* wanted, const SVec3* pos, void** out, void** outWorld) {
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
        *outWorld = world;   // the caller keeps the list g_spawned (no std::map access under __try)
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
        void *ent = nullptr, *world = nullptr;
        rc = g_e.sceneAdmin ? SafeSpawn(g_e.sceneAdmin, &tplName, tplName.c_str(), &pos, &ent, &world) : -5;
        if (rc == 0) {
            result.p = ent;
            g_spawned[ent] = world;
        }
    }
    if (ScriptHost::s_current) {
        char b[256];
        std::snprintf(b, sizeof b, "SpawnNPC(%s, %.0f %.0f %.0f): %s (Code %d, Entity %p)", tplName.c_str(), pos.x, pos.y, pos.z,
                      rc == 0 ? "ok" : "fehlgeschlagen", rc, result.p);
        ScriptHost::s_current->Log(b);
    }
    return result;
}
// ---- Focus entity (API 20): the entity that the player targets at this time (focus name, interaction). gCGUIManager::GetFocus() gets the player, its
// gCFocus_PS (property set 0x3B) and calls GetFocusEntity(0, 0) - this is not used (disassembly Game.dll+0x7C290).
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
// gCFocus_PS of the player: eCEntity::GetPropertySet(0x3B) through the vtable (slot 0xEC / 4 = 59, as in gCGUIManager::GetFocus), then
// GetCrossHairEntity() (entity below the crosshair) and GetCurrentPickPosition() (world point at which the player aims)
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
// Turn the crosshair of the player on or off (API 46): HUD gCGUIManager::ShowCrossHair(bool) (SDK ge_guimanager.h), and as before
// gCFocus_PS::SetDrawCrossHair. Return value: true if ShowCrossHair was called.
typedef void(__thiscall* ShowCrossFn)(void*, bool);
ShowCrossFn g_showCross = nullptr;
bool SafeShowCross(bool on) {
    __try {
        void* session = g_e.sessionInstance();
        void* gm = session ? g_f.guiManager(session) : nullptr;
        if (!gm) return false;
        g_showCross(gm, on);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SetCrosshairVisible(bool on) {
    if (!ResolveFocus()) return false;
    if (!g_showCross) g_showCross = reinterpret_cast<ShowCrossFn>(GetProcAddress(g_e.game, "?ShowCrossHair@gCGUIManager@@QAEX_N@Z"));
    if (ResolveCross() && g_drawCrossFn) {
        void* fs = SafeFocusSet();
        if (fs && Valid(fs)) SafeDrawCross(fs, on);
    }
    return g_showCross && SafeShowCross(on);
}
// entity that is not valid if nothing is targeted or if no game runs
Entity GetFocusedEntity() {
    Entity r{nullptr};
    if (ResolveFocus()) {
        void* p = SafeFocused();
        if (p && Valid(p)) r.p = p;
    }
    return r;
}
// ---- Screen picking (API 22): eCPhysicsScene::PickEntity(x, y, maxDistance, bCVector& hitPoint) builds a ray of the current camera from the screen pixel
// and calls TraceRayNearestHit (hints 0x1083 = static + dynamic shapes, hit point, distance without the length of the direction; disassembly Engine.dll+0x174540).
// Return value: the entity that was hit (or 0) and the exact world point of the hit. The center of the game window agrees with the crosshair.
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
// Own ray with the hero excluded: PickEntity gives no entity to exclude, thus from the follow camera the ray first hits the
// hero (game test: 'PC_Hero', distance 0). Sequence as PickEntity: eCCameraBase::PointToRay(pixel) -> eCPhysicsScene::TraceRayNearestHit(origin, direction,
// exclusion = hero, hints 0x1083) -> hit entity at report+4, hit point at report+0x68 (report = pointer at scene+0xAC).
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
// entity below the screen pixel (x, y), in pixels of the client area of the game window; GetPickPoint() then gives the hit point
Entity PickScreen(int x, int y, float maxDist) {
    Entity r{nullptr};
    g_lastPickHit = false;
    if (!ResolvePick()) return r;
    SVec3 hit{0, 0, 0};
    const float md = maxDist > 0 ? maxDist : 100000.0f;
    void* p = nullptr;
    if (g_appInst && g_appCam && g_ptToRay && g_traceNear && g_physInst)
        p = SafePickExcluding(x, y, SafeHero(false), md, &hit);   // hero excluded
    else
        p = SafePickEntity(x, y, md, &hit);
    if (p && Valid(p)) {
        r.p = p;
        g_lastPick = hit;
        g_lastPickHit = true;
    }
    return r;
}
// entity at the center of the game window (= below the crosshair)
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
// Remove an entity that SpawnNPC made (gCWorld::DestroyDynamicEntity). Return value: true = removed. The handle in the script stays; a second call is refused.
// Entities of other sources (world NPCs, hero) are refused, and also entities of a world that is now different.
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
// Does the entity belong to the system sector of the world (entities made by SpawnNPC go there, also after a save and a load)? Hero and player never count.
bool SafeInSystemSector(void* world, void* ent, void** sector, void** sysSector) {
    __try {
        *sector = g_sp.sectorFor(ent);
        *sysSector = g_sp.sysSector(world);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// Does the entity have a gCNPC_PS (is it an NPC)? Script.dll PSNpc(eCEntity*) gives the property set or 0
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
// General classification by name patterns that were seen in the game test (G3_Myrtana_House_*, G3_Object_Interact_*, G3_Myrtana_Landscape_*, G3_World_Border_*).
// Result: "NPC", "Building", "Object", "Landscape", "Border", "Unnamed" (no name: trees, stones, small parts - not more specific), "Other", "" (not valid)
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
Entity EntCurrentAttacker(const Entity& e) { return {EntIsValid(e) ? CurrentAttackerOf(e.p) : nullptr}; }
// ---- Status effects (API 27): gCNPC_PS of the NPC (the holder from PSNpc(eCEntity*) is the property set, as for the attributes). gEStatusEffect is a bit mask:
// 1 burning, 2 frozen, 4 poisoned, 8 diseased, 16 transformed, 64 fog, 128 ghost, 256 magic barrier, 512 soul travel. Disease and poison are also set through PSNpc::EnableDiseased/
// EnablePoisoned (time stamp TimeStampDiseased/Poisoned in the set). Here, the code only reads and writes the bit mask.
typedef bool(__thiscall* HasSEFn)(void*, int);
typedef void(__thiscall* EnableSEFn)(void*, int, bool);
typedef const unsigned long&(__thiscall* MaskSEFn)(const void*);
struct StatusApi {
    bool tried = false, ok = false;
    HasSEFn has = nullptr;
    EnableSEFn enable = nullptr;
    MaskSEFn mask = nullptr;
} g_se;
bool ResolveStatus() {
    if (g_se.tried) return g_se.ok;
    HMODULE game = GetModuleHandleA("Game.dll");
    if (!game) return false;
    g_se.tried = true;
    g_se.has = reinterpret_cast<HasSEFn>(GetProcAddress(game, "?HasStatusEffect@gCNPC_PS@@QBE_NW4gEStatusEffect@@@Z"));
    g_se.enable = reinterpret_cast<EnableSEFn>(GetProcAddress(game, "?EnableStatusEffect@gCNPC_PS@@QAEXW4gEStatusEffect@@_N@Z"));
    g_se.mask = reinterpret_cast<MaskSEFn>(GetProcAddress(game, "?GetStatusEffects@gCNPC_PS@@QBEABKXZ"));
    g_se.ok = g_se.has && g_se.enable && g_se.mask;
    if (ScriptHost::s_current) ScriptHost::s_current->Log(std::string("Statuseffekt-Bindings ") + (g_se.ok ? "bereit" : "fehlen"));
    return g_se.ok;
}
void* SafeNpcSet(void* ent) {
    __try {
        void* holder = nullptr;
        g_psNpc(&holder, ent);
        return holder && Valid(holder) ? holder : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
void* NpcSetOf(const Entity& e) {
    if (!EntIsValid(e) || !IsNpc(e.p) || !ResolveStatus()) return nullptr;
    return SafeNpcSet(e.p);
}
bool SafeMaskSE(void* ps, unsigned* out) {
    __try {
        *out = static_cast<unsigned>(g_se.mask(ps));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeEnableSE(void* ps, int effect, bool on) {
    __try {
        g_se.enable(ps, effect, on);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// bit mask of the status effects (0 for an entity that is not valid or not an NPC)
unsigned EntStatusMask(const Entity& e) {
    void* ps = NpcSetOf(e);
    unsigned m = 0;
    return ps && SafeMaskSE(ps, &m) ? m : 0;
}
bool EntHasStatus(const Entity& e, int effect) { return effect > 0 && (EntStatusMask(e) & static_cast<unsigned>(effect)) != 0; }
bool EntSetStatus(Entity& e, int effect, bool on) {
    void* ps = NpcSetOf(e);
    return ps && effect > 0 && SafeEnableSE(ps, effect, on);
}

// ---- Item quality (API 28): bit mask of the inventory stack in an equipment slot (gCInventory_PS::FindStack(gESlot), gCInventoryStack::Get/SetQuality).
// Bits: 2 diseased, 4 poisoned, 8 burning, 16 frozen, 32 sharp, 64 blessed, 128 self-made, 256 worn; 512 = life steal (a bit of this modkit; the description
// of the weapon shows it, see game_hooks.cpp). The quality belongs to the stack: different qualities are different stacks, and the game saves them with the saved game.
// Slots (gESlot): 1 right hand, 2 left hand, 5 back left, 6 back right, 7 bow, 14 ammunition.
typedef void*(__thiscall* PSGetFn2)(void*, int);
typedef const void*(__thiscall* FindStackSlotFn)(const void*, int);
typedef const int&(__thiscall* StackGetQFn)(const void*);
typedef void(__thiscall* StackSetQFn)(void*, const int*);
struct QualityApi {
    bool tried = false, ok = false;
    PSGetFn2 getPS = nullptr;
    FindStackSlotFn find = nullptr;
    StackGetQFn get = nullptr;
    StackSetQFn set = nullptr;
} g_qa;
bool ResolveQuality() {
    if (g_qa.tried) return g_qa.ok;
    HMODULE game = GetModuleHandleA("Game.dll"), eng = GetModuleHandleA("Engine.dll");
    if (!game || !eng) return false;
    g_qa.tried = true;
    g_qa.getPS = reinterpret_cast<PSGetFn2>(GetProcAddress(eng, "?GetPropertySet@eCEntity@@UAEPAVeCEntityPropertySet@@W4eEPropertySetType@@@Z"));
    g_qa.find = reinterpret_cast<FindStackSlotFn>(GetProcAddress(game, "?FindStack@gCInventory_PS@@QBEPBVgCInventoryStack@@W4gESlot@@@Z"));
    g_qa.get = reinterpret_cast<StackGetQFn>(GetProcAddress(game, "?GetQuality@gCInventoryStack@@QBEABHXZ"));
    g_qa.set = reinterpret_cast<StackSetQFn>(GetProcAddress(game, "?SetQuality@gCInventoryStack@@QAEXABH@Z"));
    g_qa.ok = g_qa.getPS && g_qa.find && g_qa.get && g_qa.set;
    if (ScriptHost::s_current) ScriptHost::s_current->Log(std::string("Qualitaets-Bindings ") + (g_qa.ok ? "bereit" : "fehlen"));
    return g_qa.ok;
}
// stack in the slot (nullptr if empty); SEH, without C++ objects
void* SafeSlotStack(void* ent, int slot) {
    __try {
        void* inv = g_qa.getPS(ent, 31);   // eEPropertySetType_Inventory
        return inv ? const_cast<void*>(g_qa.find(inv, slot)) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool SafeStackQuality(void* stack, int* out) {
    __try {
        *out = g_qa.get(stack);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeSetStackQuality(void* stack, int q) {
    __try {
        g_qa.set(stack, &q);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
unsigned EntSlotQuality(const Entity& e, int slot) {
    if (!EntIsValid(e) || !ResolveQuality()) return 0;
    void* st = SafeSlotStack(e.p, slot);
    int q = 0;
    return st && SafeStackQuality(st, &q) ? static_cast<unsigned>(q) : 0;
}
bool EntSetSlotQuality(Entity& e, int slot, unsigned bits) {
    if (!EntIsValid(e) || !ResolveQuality()) return false;
    void* st = SafeSlotStack(e.p, slot);
    return st && SafeSetStackQuality(st, static_cast<int>(bits));
}
Entity EntSlotItem(const Entity& e, int slot) { return {EntIsValid(e) ? SlotItemOf(e.p, slot) : nullptr}; }
// ---- Read properties (API 29): general property access of the engine (SharedBase bCAccessorProperty, 12 bytes) on the property set of an entity
// (eCEntity::GetPropertySet(eEPropertySetType)). The accessor goes through all properties of the set and of its base classes (++). ConvertValueToString gives the value as text.
// Read only. All calls into the engine have SEH wrappers. Accessor objects are in buffers and are destroyed by hand.
struct PsName {
    const char* name;
    int id;
};
static const PsName kPsNames[] = {
    {"Unknown", 0},
    {"Base", 1},
    {"MeshVisual", 2},
    {"DummyVisual", 3},
    {"Advanced", 4},
    {"Navigation", 5},
    {"DynamicCollisionCircle", 6},
    {"CollisionCircle", 7},
    {"NavZone", 8},
    {"NegZone", 9},
    {"NavPath", 10},
    {"PrefPath", 11},
    {"NavHelper", 12},
    {"RigidBody", 13},
    {"CollisionShape", 14},
    {"DynamicLight", 15},
    {"DirectionalLight", 16},
    {"PointLight", 17},
    {"SpotLight", 18},
    {"Hemisphere", 19},
    {"MovementBase", 20},
    {"CharacterMovement", 21},
    {"CharacterControl", 22},
    {"CharacterSensor", 23},
    {"CameraAI", 24},
    {"Editor", 25},
    {"AIHelper", 26},
    {"AIHelperFreePoint", 27},
    {"AIHelperSpawnPoint", 28},
    {"AIHelperPlayerStart", 29},
    {"NPC", 30},
    {"Inventory", 31},
    {"Clock", 32},
    {"Effect", 33},
    {"ParticleEffect", 34},
    {"EventFilter", 35},
    {"EventDebugger", 36},
    {"EventLogic", 37},
    {"EventScript", 38},
    {"Trigger", 39},
    {"TriggerList", 40},
    {"TriggerCombiner", 41},
    {"Skydome", 42},
    {"EditorVisual", 43},
    {"EngineCaps", 44},
    {"ScriptRoutine", 45},
    {"SpeedTree", 46},
    {"SpeedTreeWind", 47},
    {"Item", 48},
    {"Interaction", 49},
    {"TouchDamage", 50},
    {"Damage", 51},
    {"DamageReceiver", 52},
    {"QuestManager", 53},
    {"Quest", 54},
    {"InfoManager", 55},
    {"Info", 56},
    {"InfoScript", 57},
    {"InfoScriptLine", 58},
    {"Focus", 59},
    {"PlayerMemory", 60},
    {"NavOffset", 61},
    {"Enclave", 62},
    {"Arena", 63},
    {"Area", 64},
    {"Vegetation", 65},
    {"VegetationBrush", 66},
    {"VegetationRubber", 67},
    {"Dialog", 68},
    {"Lock", 69},
    {"Door", 70},
    {"StaticLight", 71},
    {"StaticPointLight", 72},
    {"StaticSpotLight", 73},
    {"Illuminated", 74},
    {"Statistics", 75},
    {"PhysicScene", 76},
    {"Party", 77},
    {"Anchor", 78},
    {"Letter", 79},
    {"Book", 80},
    {"Map", 81},
    {"LinkContainer", 82},
    {"Projectile", 83},
    {"Sound", 84},
    {"Strip", 85},
    {"ForceField", 86},
    {"Particle", 87},
    {"ParticleSystem", 88},
    {"ParticleEmitter", 89},
    {"SpriteEmitter", 90},
    {"BeamEmitter", 91},
    {"SparkEmitter", 92},
    {"MeshEmitter", 93},
    {"AIZone", 94},
    {"AudioEmitter", 95},
    {"Effect2", 96},
    {"LightStreaks", 97},
    {"WeatherZone", 98},
    {"Precipitation", 99},
    {"Animation", 100},
    {"Magic", 101},
    {"Skill", 102},
    {"Mover", 103},
    {"Teleporter", 104},
    {"ArmorSet", 105},
    {"SectorPersistence", 106},
    {"Flock", 107},
    {"TreasureSet", 108},
    {"Recipe", 109},
    {"WaterZone", 110},
    {"DistanceTrigger", 111},
    {"TimeZone", 112},
    {"Decal", 113},
    {"MagicBarrier", 114}
};
typedef void*(__thiscall* AccCtorFn)(void*, const void*);
typedef void*(__thiscall* AccIndexFn)(void*, void*, const void*);
typedef void*(__thiscall* AccConvFn)(void*, void*);
typedef bool(__thiscall* AccValidFn)(const void*);
typedef void(__thiscall* AccDtorFn)(void*);
typedef const void*(__thiscall* AccNameFn)(const void*);
typedef void*(__thiscall* AccIncFn)(void*);
typedef void*(__thiscall* PsGetFn3)(void*, int);
typedef void*(__thiscall* PsGetByNameFn)(void*, const void*);   // eCEntity::GetPropertySet(const bCString& Klassenname)
typedef void(__thiscall* AccConvStrFn)(void*, const void*, int);   // ConvertStringToValue(const bCString&, eEOperandValue)
struct PropApi {
    bool tried = false, ok = false;
    AccCtorFn ctor = nullptr;
    AccIndexFn index = nullptr;
    AccConvFn conv = nullptr;
    AccValidFn valid = nullptr;
    AccDtorFn dtor = nullptr;
    AccNameFn name = nullptr;
    AccIncFn inc = nullptr;
    PsGetFn3 getPS = nullptr;
    PsGetByNameFn getPSByName = nullptr;   // optional
    AccConvStrFn convStr = nullptr;        // optional (write)
} g_pa;
bool ResolveProps() {
    if (g_pa.tried) return g_pa.ok;
    HMODULE sb = GetModuleHandleA("SharedBase.dll"), eng = GetModuleHandleA("Engine.dll");
    if (!sb || !eng) return false;
    g_pa.tried = true;
    g_pa.ctor = reinterpret_cast<AccCtorFn>(GetProcAddress(sb, "??0bCAccessorProperty@@QAE@PBVbCObjectRefBase@@@Z"));
    g_pa.index = reinterpret_cast<AccIndexFn>(GetProcAddress(sb, "??AbCAccessorProperty@@QAE?AV0@ABVbCString@@@Z"));
    g_pa.conv = reinterpret_cast<AccConvFn>(GetProcAddress(sb, "?ConvertValueToString@bCAccessorProperty@@QAE?AVbCString@@XZ"));
    g_pa.valid = reinterpret_cast<AccValidFn>(GetProcAddress(sb, "?IsValid@bCAccessorProperty@@QBE_NXZ"));
    g_pa.dtor = reinterpret_cast<AccDtorFn>(GetProcAddress(sb, "??1bCAccessorProperty@@QAE@XZ"));
    g_pa.name = reinterpret_cast<AccNameFn>(GetProcAddress(sb, "?GetPropertyName@bCAccessorProperty@@QBEABVbCString@@XZ"));
    g_pa.inc = reinterpret_cast<AccIncFn>(GetProcAddress(sb, "??EbCAccessorProperty@@QAEAAV0@XZ"));
    g_pa.getPS = reinterpret_cast<PsGetFn3>(GetProcAddress(eng, "?GetPropertySet@eCEntity@@UAEPAVeCEntityPropertySet@@W4eEPropertySetType@@@Z"));
    g_pa.getPSByName = reinterpret_cast<PsGetByNameFn>(GetProcAddress(eng, "?GetPropertySet@eCEntity@@QAEPAVeCEntityPropertySet@@ABVbCString@@@Z"));
    g_pa.convStr = reinterpret_cast<AccConvStrFn>(GetProcAddress(sb, "?ConvertStringToValue@bCAccessorProperty@@QAEXABVbCString@@W4eEOperandValue@1@@Z"));
    g_pa.ok = g_pa.ctor && g_pa.index && g_pa.conv && g_pa.valid && g_pa.dtor && g_pa.name && g_pa.inc && g_pa.getPS;
    if (ScriptHost::s_current) ScriptHost::s_current->Log(std::string("Eigenschaften-Bindings ") + (g_pa.ok ? "bereit" : "fehlen"));
    return g_pa.ok;
}
// name of a property set -> eEPropertySetType: "Item", "gCItem_PS", "eCRigidBody_PS" (no case sensitivity), -1 = not known
int PsTypeId(const char* text) {
    std::string n = text ? text : "";
    for (char& c : n) c = char(tolower(static_cast<unsigned char>(c)));
    if (n.size() > 2 && (n.compare(0, 2, "gc") == 0 || n.compare(0, 2, "ec") == 0)) n.erase(0, 2);
    if (n.size() > 3 && n.compare(n.size() - 3, 3, "_ps") == 0) n.erase(n.size() - 3);
    for (const PsName& p : kPsNames) {
        std::string t = p.name;
        for (char& c : t) c = char(tolower(static_cast<unsigned char>(c)));
        if (t == n) return p.id;
    }
    return -1;
}
void* SafePropSet(void* ent, int id) {
    __try {
        return g_pa.getPS(ent, id);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
void* SafePropSetByName(void* ent, const void* nameStr) {
    __try {
        return g_pa.getPSByName(ent, nameStr);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
// property set of an entity: type name from eEPropertySetType ("Item", "gCItem_PS") or, if that is not known, the class name ("gCSkill_PS", ...) through GetPropertySet(bCString)
void* PropSetFor(const Entity& e, const SStr& set) {
    const int id = PsTypeId(g_sb.strGetText(&set));
    if (id >= 0) return SafePropSet(e.p, id);
    return g_pa.getPSByName ? SafePropSetByName(e.p, &set) : nullptr;
}
// value of a property as text; 1 read, 0 property not known, -1 error
int SafePropOne(void* ps, const void* nameStr, std::string* out) {
    char acc[32] = {}, sub[32] = {}, ret[32] = {};
    __try {
        g_pa.ctor(acc, ps);
        g_pa.index(acc, sub, nameStr);
        int r = 0;
        if (g_pa.valid(sub)) {
            g_pa.conv(sub, ret);
            const char* t = g_sb.strGetText(ret);
            out->assign(t ? t : "");
            g_sb.strDtor(ret);
            r = 1;
        }
        g_pa.dtor(sub);
        g_pa.dtor(acc);
        return r;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
// list: make the accessor / step (append the current property and continue) / clean up
bool SafePropStart(void* ps, void* acc) {
    __try {
        g_pa.ctor(acc, ps);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafePropValid(void* acc) {
    __try {
        return g_pa.valid(acc);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// copy the name of the current property into a buffer
bool SafePropName(void* acc, char* buf, size_t n) {
    buf[0] = 0;
    __try {
        const char* t = g_sb.strGetText(g_pa.name(acc));
        size_t i = 0;
        for (; t && i + 1 < n && t[i]; ++i) buf[i] = t[i];
        buf[i] = 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { buf[0] = 0; return false; }
}
// value of the current property as text into a buffer (truncated)
bool SafePropValue(void* acc, char* buf, size_t n) {
    char ret[32] = {};
    buf[0] = 0;
    __try {
        g_pa.conv(acc, ret);
        const char* t = g_sb.strGetText(ret);
        size_t i = 0;
        for (; t && i + 1 < n && t[i]; ++i) buf[i] = t[i];
        buf[i] = 0;
        g_sb.strDtor(ret);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { buf[0] = 0; return false; }
}
bool SafePropInc(void* acc) {
    __try {
        g_pa.inc(acc);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void SafePropEnd(void* acc) {
    __try {
        g_pa.dtor(acc);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
std::string PropClean(std::string s, size_t maxlen) {
    for (char& c : s)
        if (c == ';' || c == '\r' || c == '\n' || c == '\t') c = ' ';
    if (s.size() > maxlen) s.resize(maxlen);
    return s;
}
// write a value from text (ConvertStringToValue, assign); 1 written, 0 property not known, -1 error
int SafePropWrite(void* ps, const void* nameStr, const void* valueStr) {
    char acc[32] = {}, sub[32] = {};
    __try {
        g_pa.ctor(acc, ps);
        g_pa.index(acc, sub, nameStr);
        int r = 0;
        if (g_pa.valid(sub)) {
            g_pa.convStr(sub, valueStr, 0);
            r = 1;
        }
        g_pa.dtor(sub);
        g_pa.dtor(acc);
        return r;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
SStr GetPropertyText(const Entity& e, const SStr& set, const SStr& prop) {
    if (!EntIsValid(e) || !ResolveProps()) return SStr();
    void* ps = PropSetFor(e, set);
    std::string r;
    if (!ps || SafePropOne(ps, &prop, &r) != 1) return SStr();
    if (r.size() > 3000) r.resize(3000);
    return SStr(r);
}
// Set a property from text. Changes game data: use it only for templates and entities that the mod made itself. An incorrect text form can cause incorrect values.
bool SetPropertyText(const Entity& e, const SStr& set, const SStr& prop, const SStr& value) {
    if (!EntIsValid(e) || !ResolveProps() || !g_pa.convStr) return false;
    void* ps = PropSetFor(e, set);
    return ps && SafePropWrite(ps, &prop, &value) == 1;
}
// Set a reference property (eCTemplateEntityProxy, for example Item.ActivateSkill2, Item.Skill, gCSkill_PS.ReqSkill1) to a template. This is not possible as text (ConvertStringToValue)
// (form test 2026-11-06). In place of that: gCxxx_PS::Access<property>() gives the proxy, and eCTemplateEntityProxy::SetEntity(eCTemplateEntity const*) sets the target.
typedef void*(__thiscall* AccessProxyFn)(void*);
typedef void(__thiscall* ProxySetEntityFn)(void*, const void*);
bool SafeSetRef(void* ps, AccessProxyFn access, ProxySetEntityFn setEntity, const void* tpl) {
    __try {
        void* proxy = access(ps);
        if (!proxy) return false;
        setEntity(proxy, tpl);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SetTemplateRefScript(const Entity& e, const SStr& set, const SStr& prop, const SStr& target) {
    if (!EntIsValid(e) || !ResolveInv() || !ResolveProps()) return false;   // ResolveProps: PropSetFor needs the property functions
    char mangled[200];
    // set is the class name here (gCItem_PS, gCSkill_PS, gCArmorSet_PS ...)
    std::snprintf(mangled, sizeof mangled, "?Access%s@%s@@QAEAAVeCTemplateEntityProxy@@XZ", prop.c_str(), set.c_str());
    auto access = reinterpret_cast<AccessProxyFn>(GetProcAddress(g_e.game, mangled));
    auto setEntity = reinterpret_cast<ProxySetEntityFn>(GetProcAddress(g_e.engine, "?SetEntity@eCTemplateEntityProxy@@QAEXPBVeCTemplateEntity@@@Z"));
    if (!access || !setEntity) return false;
    void* ps = PropSetFor(e, set);
    if (!ps) return false;
    if (!g_e.sceneAdmin || !Valid(g_e.sceneAdmin)) {
        SStr mod("eCSceneAdmin");
        g_e.sceneAdmin = SafeSceneAdmin(&mod);
    }
    void* tpl = g_e.sceneAdmin ? SafeFindTemplate(g_e.sceneAdmin, &target, target.c_str()) : nullptr;
    return tpl && SafeSetRef(ps, access, setEntity, tpl);
}
// set an entity reference (eCEntityProxy, for example gCInteraction_PS.Owner) to an entity: gCxxx_PS::Access<property>() gives the proxy, eCEntityProxy::SetEntity(eCEntity const*) sets the target.
bool SetEntityRefScript(const Entity& e, const SStr& set, const SStr& prop, const Entity& target) {
    if (!EntIsValid(e) || !EntIsValid(target) || !ResolveInv() || !ResolveProps()) return false;
    char mangled[200];
    std::snprintf(mangled, sizeof mangled, "?Access%s@%s@@QAEAAVeCEntityProxy@@XZ", prop.c_str(), set.c_str());
    auto access = reinterpret_cast<AccessProxyFn>(GetProcAddress(g_e.game, mangled));
    auto setEntity = reinterpret_cast<ProxySetEntityFn>(GetProcAddress(g_e.engine, "?SetEntity@eCEntityProxy@@QAEXPBVeCEntity@@@Z"));
    if (!access || !setEntity) return false;
    void* ps = PropSetFor(e, set);
    return ps && SafeSetRef(ps, access, setEntity, target.p);
}
// Group and companions (API 33): gCParty_PS (Game.dll). The members are at the LEADER (GetMembers: bTObjArray<eCEntityProxy>, field 0 pointer,
// field 1 count; eCEntityProxy 0x1C bytes, SDK). Each member refers to the leader with PartyLeaderEntity. Teleport through Script.dll Entity::Teleport(Entity const &)
// with real Script.dll entities (MakeScriptEntity).
typedef const void*(__thiscall* PartyMembersFn)(const void*);
typedef const void*(__thiscall* PartyLeaderFn)(const void*);
typedef void(__thiscall* PartySetWaitFn)(void*, const bool*);
typedef const bool*(__thiscall* PartyGetWaitFn)(const void*);
typedef void*(__thiscall* ProxyEntFn)(const void*);
typedef bool(__thiscall* TeleportFn)(void*, const void*);
struct PartyApi {
    bool tried = false;
    PartyMembersFn members = nullptr;
    PartyLeaderFn leader = nullptr;
    PartySetWaitFn setWait = nullptr;
    PartyGetWaitFn getWait = nullptr;
    ProxyEntFn proxyEnt = nullptr;
    TeleportFn teleport = nullptr;
};
PartyApi& Party() {
    static PartyApi a;
    if (!a.tried && Resolve()) {
        a.tried = true;
        a.members = reinterpret_cast<PartyMembersFn>(GetProcAddress(g_e.game, "?GetMembers@gCParty_PS@@QBEABV?$bTObjArray@VeCEntityProxy@@@@XZ"));
        a.leader = reinterpret_cast<PartyLeaderFn>(GetProcAddress(g_e.game, "?GetPartyLeaderEntity@gCParty_PS@@QBEABVeCEntityProxy@@XZ"));
        a.setWait = reinterpret_cast<PartySetWaitFn>(GetProcAddress(g_e.game, "?SetWaiting@gCParty_PS@@QAEXAB_N@Z"));
        a.getWait = reinterpret_cast<PartyGetWaitFn>(GetProcAddress(g_e.game, "?GetWaiting@gCParty_PS@@QBEAB_NXZ"));
        a.proxyEnt = reinterpret_cast<ProxyEntFn>(GetProcAddress(g_e.engine, "?GetEntity@eCEntityProxy@@QAEPAVeCEntity@@XZ"));
    }
    if (!a.teleport)   // Script.dll loads only after main(): until then, search again
        if (HMODULE script = GetModuleHandleA("Script.dll"))
            a.teleport = reinterpret_cast<TeleportFn>(GetProcAddress(script, "?Teleport@Entity@@QAE_NABV1@@Z"));
    return a;
}
void* PartyPS(const Entity& e) {
    if (!EntIsValid(e) || !ResolveProps()) return nullptr;
    SStr set("Party");
    return PropSetFor(e, set);
}
// member i (i < 0: count only); return value count or -1
int SafePartyMember(void* ps, int i, void** out) {
    PartyApi& a = Party();
    if (!a.members || !a.proxyEnt) return -1;
    __try {
        const void* arr = a.members(ps);
        if (!arr) return -1;
        const unsigned char* data = *static_cast<const unsigned char* const*>(arr);
        const int n = int(static_cast<const unsigned*>(arr)[1]);
        if (n < 0 || n > 1000) return -1;
        if (i >= 0 && i < n && data) *out = a.proxyEnt(data + 0x1C * i);
        return n;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
void* SafePartyLeader(void* ps) {
    PartyApi& a = Party();
    if (!a.leader || !a.proxyEnt) return nullptr;
    __try {
        const void* px = a.leader(ps);
        return px ? a.proxyEnt(px) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
int EntPartyCount(const Entity& e) {
    void* ps = PartyPS(e);
    void* dummy = nullptr;
    return ps ? SafePartyMember(ps, -1, &dummy) : -1;
}
Entity EntPartyMember(const Entity& e, int i) {
    void* ps = PartyPS(e);
    void* m = nullptr;
    if (ps && i >= 0) SafePartyMember(ps, i, &m);
    return {m && Valid(m) ? m : nullptr};
}
Entity EntPartyLeader(const Entity& e) {
    void* ps = PartyPS(e);
    void* l = ps ? SafePartyLeader(ps) : nullptr;
    return {l && Valid(l) ? l : nullptr};
}
bool SafeSetWait(void* ps, bool w) {
    __try {
        Party().setWait(ps, &w);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeGetWait(void* ps) {
    __try {
        const bool* b = Party().getWait(ps);
        return b && *b;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool EntSetPartyWaiting(const Entity& e, bool w) {
    void* ps = PartyPS(e);
    return ps && Party().setWait && SafeSetWait(ps, w);
}
bool EntIsPartyWaiting(const Entity& e) {
    void* ps = PartyPS(e);
    return ps && Party().getWait && SafeGetWait(ps);
}
bool SafeTeleport(void* self, const void* target) {
    __try {
        return Party().teleport(self, target);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool EntTeleportTo(const Entity& e, const Entity& target) {
    if (!EntIsValid(e) || !EntIsValid(target) || !Party().teleport) return false;
    alignas(8) unsigned char a[kScriptEntityBytes], b[kScriptEntityBytes];
    if (!MakeScriptEntity(a, e.p) || !MakeScriptEntity(b, target.p)) return false;
    return SafeTeleport(a, b);
}
// Kill an NPC (API 46, replacement for DoDamage): recipe from the plugin Chszonszcze (static analysis 2026-11-08):
//   PSDamageReceiver.DamageAmount = 0, PSNpc::SetLastAttacker/SetCurrentAttacker(killer), game script SetHitPoints(victim, None, 0),
//   PSRoutine::FullStop, PSRoutine::SetTask("ZS_RagDollDead").
// Script.dll entity (SDK gs_entity.h): +0 eCEntity*, then for each 4 bytes the property set wrappers in the sequence of declaration: Routine +0x04, NPC +0x34,
// DamageReceiver +0x4C. Properties (GS_PROPERTY) work with this = address of the wrapper. SetTask takes the bCString by value (the function destroys it).
struct KillApi {
    bool tried = false;
    void*(__thiscall* dmgAssign)(void*, const long*) = nullptr;
    void(__thiscall* setLast)(void*, const void*) = nullptr;
    void(__thiscall* setCur)(void*, const void*) = nullptr;
    void(__thiscall* fullStop)(void*) = nullptr;
    void(__thiscall* setTask)(void*, void*) = nullptr;   // really (bCString by value): 4 bytes of raw bCString that the function frees
};
KillApi& Kills() {
    static KillApi a;
    if (!a.tried) {
        HMODULE script = GetModuleHandleA("Script.dll");
        if (!script) return a;   // Script.dll loads late: try again later
        a.tried = true;
        a.dmgAssign = reinterpret_cast<void*(__thiscall*)(void*, const long*)>(GetProcAddress(script, "??4PropertyDamageAmount@PSDamageReceiver@@QAEAAV01@ABJ@Z"));
        a.setLast = reinterpret_cast<void(__thiscall*)(void*, const void*)>(GetProcAddress(script, "?SetLastAttacker@PSNpc@@QAEXABVEntity@@@Z"));
        a.setCur = reinterpret_cast<void(__thiscall*)(void*, const void*)>(GetProcAddress(script, "?SetCurrentAttacker@PSNpc@@QAEXABVEntity@@@Z"));
        a.fullStop = reinterpret_cast<void(__thiscall*)(void*)>(GetProcAddress(script, "?FullStop@PSRoutine@@QAEXXZ"));
        a.setTask = reinterpret_cast<void(__thiscall*)(void*, void*)>(GetProcAddress(script, "?SetTask@PSRoutine@@QAEXVbCString@@@Z"));
    }
    return a;
}
// steps 1 and 2 (before the script call); k = script entity of the killer or nullptr
bool SafeKillPrepare(KillApi& a, unsigned char* v, const void* k) {
    __try {
        const long zero = 0;
        a.dmgAssign(v + 0x4C, &zero);
        if (k) {
            a.setLast(v + 0x34, k);
            a.setCur(v + 0x34, k);
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeKillFinish(KillApi& a, unsigned char* v, void* rawTask) {
    __try {
        a.fullStop(v + 0x04);
        a.setTask(v + 0x04, rawTask);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool EntKill(const Entity& victim, const Entity& killer) {
    KillApi& a = Kills();
    if (!EntIsValid(victim) || !a.dmgAssign || !a.setLast || !a.setCur || !a.fullStop || !a.setTask) return false;
    alignas(8) unsigned char v[kScriptEntityBytes], k[kScriptEntityBytes];
    if (!MakeScriptEntity(v, victim.p)) return false;
    const bool hasKiller = EntIsValid(killer) && MakeScriptEntity(k, killer.p);
    if (!SafeKillPrepare(a, v, hasKiller ? k : nullptr)) return false;
    CallGameScript("SetHitPoints", victim.p, nullptr, 0);
    void* rawTask = nullptr;   // own copy for the function that destroys its value parameter
    SStr task("ZS_RagDollDead");
    g_sb.strCopy(&rawTask, &task);
    const bool ok = SafeKillFinish(a, v, rawTask);
    if (ScriptHost::s_current) {
        static int logged = 0;
        if (logged < 20) {
            ++logged;
            ScriptHost::s_current->Log(std::string("Kill: ") + (ok ? "ausgefuehrt" : "Ausnahme abgefangen") + ", Taeter " + (hasKiller ? "gesetzt" : "keiner"));
        }
    }
    return ok;
}
bool EntKillNoKiller(const Entity& victim) { return EntKill(victim, Entity{nullptr}); }
// Effects (API 30): Script.dll EffectSystem::ExistsEffect(name) and StartEffect(name, entity) start a named effect (for example "eff_magic_summonlightning_target_01", the lightning
// of the spell "Blitz beschwoeren", summon lightning) at the entity. The code does not keep the effect handle (Effect, 4 bytes, return value through a pointer). The effect management ends it itself.
typedef bool(__stdcall* EffectExistsFn)(const void*);
typedef void*(__stdcall* EffectStartFn)(void*, const void*, const void*);
bool SafeEffectExists(EffectExistsFn fn, const void* name) {
    __try {
        return fn(name);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeEffectStart(EffectStartFn fn, const void* name, const void* ent) {
    __try {
        void* handle = nullptr;
        fn(&handle, name, ent);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
struct EffectApi {
    bool tried = false;
    EffectExistsFn exists = nullptr;
    EffectStartFn start = nullptr;
};
EffectApi& Effects() {
    static EffectApi a;
    if (!a.tried) {
        a.tried = true;
        if (HMODULE script = GetModuleHandleA("Script.dll")) {
            a.exists = reinterpret_cast<EffectExistsFn>(GetProcAddress(script, "?ExistsEffect@EffectSystem@@SG_NABVbCString@@@Z"));
            a.start = reinterpret_cast<EffectStartFn>(GetProcAddress(script, "?StartEffect@EffectSystem@@SG?AVEffect@@ABVbCString@@ABVEntity@@@Z"));
        }
    }
    return a;
}
bool EffectExistsScript(const SStr& name) {
    EffectApi& a = Effects();
    return a.exists && SafeEffectExists(a.exists, &name);
}
bool StartEffectScript(const SStr& name, const Entity& at) {
    EffectApi& a = Effects();
    if (!a.start || !a.exists || !EntIsValid(at) || !SafeEffectExists(a.exists, &name)) return false;
    alignas(8) unsigned char se[kScriptEntityBytes];   // real Script.dll entity (0xA8 bytes) in place of only the pointer
    if (!MakeScriptEntity(se, at.p)) return false;
    return SafeEffectStart(a.start, &name, se);
}
// template (template entity) by exact name; not valid if it does not exist. Its property sets can be read as for an entity (and written for own templates)
Entity GetTemplateEntityScript(const SStr& name) {
    if (!ResolveInv()) return {nullptr};
    if (!g_e.sceneAdmin || !Valid(g_e.sceneAdmin)) {
        SStr mod("eCSceneAdmin");
        g_e.sceneAdmin = SafeSceneAdmin(&mod);
    }
    return {g_e.sceneAdmin ? SafeFindTemplate(g_e.sceneAdmin, &name, name.c_str()) : nullptr};
}
// "name=value; name=value; ..." for all properties of the set (and of its base classes), maximum max items; ';' in values becomes a space
SStr ListPropertyTexts(const Entity& e, const SStr& set, int maxn) {
    if (!EntIsValid(e) || !ResolveProps()) return SStr();
    void* ps = PropSetFor(e, set);
    if (!ps) return SStr();
    char acc[32] = {};
    if (!SafePropStart(ps, acc)) return SStr();
    std::string all;
    for (int i = 0; i < (maxn < 1 ? 1 : maxn > 600 ? 600 : maxn); ++i) {
        if (!SafePropValid(acc)) break;
        char nm[160], val[1200];
        SafePropName(acc, nm, sizeof nm);
        if (!SafePropValue(acc, val, sizeof val)) std::snprintf(val, sizeof val, "<Fehler>");
        if (!all.empty()) all += ";";
        all += PropClean(std::string(nm) + "=" + val, 420);
        if (!SafePropInc(acc)) break;
    }
    SafePropEnd(acc);
    return SStr(all);
}
bool EntIsBuilding(const Entity& e) { return std::strcmp(EntCategory(e).c_str(), "Building") == 0; }
// the system sector also contains world objects (for example G3_World_Border_001): thus the NPC requirement too.
bool IsDynamicSpawn(const Entity& e) {
    if (!EntIsValid(e) || !ResolveSpawn() || !g_sp.sectorFor || !g_sp.sysSector) return false;
    if (!IsNpc(e.p)) return false;
    if (e.p == SafeHero(false) || e.p == SafeHero(true)) return false;
    void* world = SafeCurrentWorld();
    void *sec = nullptr, *sys = nullptr;
    return world && SafeInSystemSector(world, e.p, &sec, &sys) && sec && sec == sys;
}
// Removes an entity that SpawnNPC made: one of this session (list) or one in the system sector (after the load of a saved game).
// Entities from normal sectors (world NPCs), hero and player are refused. Return value: true = removed.
bool DestroyNPC(const Entity& e) {
    char whyBuf[120];
    const char* why = nullptr;
    bool ok = false;
    auto it = g_spawned.find(e.p);
    if (!EntIsValid(e))   // also an entry from g_spawned that was already removed (the pointer does not point to a game object anymore)
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
// find entities by part of the name (count + index): for example all "Diego"
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
// rotation: SetLocalRotation (for entities directly below the world = world rotation). Quaternion (x, y, z, w), Y is up.
bool SafeSetRot(void* ent, const SQuat* q) {
    __try {
        g_sp.setLocalRot(ent, q);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool EntSetRotation(Entity& e, const SQuat& q) { return EntIsValid(e) && ResolveSpawn() && g_sp.setLocalRot && SafeSetRot(e.p, &q); }
// view direction to a target (only around the vertical axis): yaw = atan2(dx, dz) in degrees plus offset (default 0; if the NPC looks away from the target: 180)
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
// Template names whose name starts with part (no case sensitivity). The engine gives each template two times (measured 2026-11-19 in the game:
// CountTemplateNames("Perk_1H") 12 for 6 templates, FindTemplateNames each name two times). Thus the names are read and merged.
// SEH part: the raw count, and the name texts of the entries [from, from + cap) (the texts belong to the templates and stay valid)
int SafeTemplateRawCount(void* admin, const void* part) {
    __try {
        return g_i.countPart(admin, part, 2);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
int SafeTemplateNameChunk(void* admin, const void* part, int from, int to, const char** names) {
    __try {
        int k = 0;
        for (int i = from; i < to; ++i) {
            void* c = g_i.byPart(admin, part, 2, i);
            names[k++] = c ? g_sb.strGetText(g_e.getName(c)) : nullptr;
        }
        return k;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
void* TemplateAdmin() {
    if (!ResolveInv() || !g_i.countPart || !g_i.byPart) return nullptr;
    if (!g_e.sceneAdmin || !Valid(g_e.sceneAdmin)) {
        SStr mod("eCSceneAdmin");
        g_e.sceneAdmin = SafeSceneAdmin(&mod);
    }
    return g_e.sceneAdmin;
}
// different template names in the order of the engine; stops at maxUnique; false without the game or if there is an error
bool TemplateNamesUnique(const SStr& part, int maxUnique, std::vector<std::string>* out) {
    void* admin = TemplateAdmin();
    if (!admin) return false;
    const int raw = SafeTemplateRawCount(admin, &part);
    if (raw < 0) return false;
    std::set<std::string> seen;
    const char* chunk[256];
    for (int from = 0; from < raw && from < 40000 && int(out->size()) < maxUnique; from += 256) {
        const int to = from + 256 < raw ? from + 256 : raw;
        const int got = SafeTemplateNameChunk(admin, &part, from, to, chunk);
        if (got < 0) return false;
        for (int i = 0; i < got && int(out->size()) < maxUnique; ++i)
            if (chunk[i] && seen.insert(chunk[i]).second) out->push_back(chunk[i]);
    }
    return true;
}
int CountTemplateNames(const SStr& part) {
    std::vector<std::string> names;
    return TemplateNamesUnique(part, 40000, &names) ? int(names.size()) : -1;
}
SStr FindTemplateNames(const SStr& part, int maxn) {
    std::vector<std::string> names;
    std::string r;
    if (TemplateNamesUnique(part, maxn < 1 ? 1 : maxn > 20000 ? 20000 : maxn, &names))
        for (const std::string& n : names) {
            if (!r.empty()) r += ";";
            r += n;
        }
    return SStr(r);
}
int EntRemoveItems(Entity& e, const SStr& name, int count) {   // return value: number of removed items (-1 if there is an error)
    void* inv = InventoryOf(e);
    int r = 0;
    return inv && count > 0 && SafeRemoveItems(inv, &name, count, &r) ? r : -1;
}

// Removes all stacks with this name, with the stack itself (gCInventory_PS::DeleteStack). RemoveItems decreases only the quantity. For permanent items
// (Permanent, for example learn items of skills), an empty stack then stays. Return value: number of removed stacks, -1 if there is an error
typedef bool(__thiscall* InvDeleteStackFn)(void*, int);
bool SafeDeleteStacks(void* inv, const void* name, int* removed) {
    static InvDeleteStackFn del = reinterpret_cast<InvDeleteStackFn>(GetProcAddress(g_e.game, "?DeleteStack@gCInventory_PS@@QAE_NH@Z"));
    if (!del) return false;
    __try {
        for (int guard = 0; guard < 64; ++guard) {
            const int idx = g_i.find(inv, name, 0, 0, -1);
            if (idx < 0) break;
            if (!del(inv, idx)) break;
            ++*removed;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
int EntDeleteItemStack(Entity& e, const SStr& name) {
    void* inv = InventoryOf(e);
    int r = 0;
    return inv && SafeDeleteStacks(inv, &name, &r) ? r : -1;
}

// Entity::DoDamage removed with API 46 (deprecated since 39, reversed: this was the victim). Replacement: Entity::Kill, HurtHitPoints.
// warning for deprecated functions: 1 time for each mod and function
void WarnDeprecatedOnce(const char* what, const char* hint) {
    static std::set<std::string> warned;
    ScriptHost* h = ScriptHost::s_current;
    if (!h) return;
    const std::string key = std::string(h->CurrentMod()) + "|" + what;
    if (!warned.insert(key).second) return;
    h->Log(std::string("[") + h->CurrentMod() + "] VERALTET: " + what + " - " + hint);
}

// Change the hit points of an NPC directly (API 30): gCDamageReceiver_PS::GetHitPoints/SetHitPoints. This is the location where the game applies damage. The damage hook of the modkit
// is on SetHitPoints, thus the change shows as a normal hit in OnDamage (owner then without attacker context). Return value: new HP, -1 if there is an error
typedef const long&(__thiscall* GetHPFn)(const void*);
typedef void(__thiscall* SetHPFn)(void*, const long&);
int SafeHurt(void* ps, GetHPFn get, SetHPFn set, int amount, int minHp) {
    __try {
        long hp = get(ps);
        if (hp <= minHp) return int(hp);   // already at or below the lower limit: write nothing (if not, the increase to minHp would heal)
        long neu = hp - amount;
        if (neu < minHp) neu = minHp;
        SetScriptHurt(true);   // the damage hook reports this decrease without an attacker (LastInflictor on the victim is an old value here)
        set(ps, neu);
        SetScriptHurt(false);
        return int(neu);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetScriptHurt(false);
        return -1;
    }
}
int EntHurtHP(const Entity& e, int amount, int minHp) {
    static GetHPFn get = nullptr;
    static SetHPFn set = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        if (HMODULE game = GetModuleHandleA("Game.dll")) {
            get = reinterpret_cast<GetHPFn>(GetProcAddress(game, "?GetHitPoints@gCDamageReceiver_PS@@QBEABJXZ"));
            set = reinterpret_cast<SetHPFn>(GetProcAddress(game, "?SetHitPoints@gCDamageReceiver_PS@@QAEXABJ@Z"));
        }
    }
    if (!get || !set || !EntIsValid(e) || amount <= 0 || !ResolveProps()) return -1;
    SStr cls("gCDamageReceiver_PS");
    void* ps = PropSetFor(e, cls);
    return ps ? SafeHurt(ps, get, set, amount, minHp) : -1;
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

// ---- Time rate (API 36): eCApplication::Get/SetTimeScaling (engine; speed of the full simulation as the slow motion of the game, not in the saved game) and
// gCClock_PS::Get/SetFactor (game; speed of the game clock, a property of the world clock and thus IN the saved game). The wrapper from WorldClock() is the
// PSClock object of Script.dll. Its first field is the gCClock_PS pointer (as for PSPlayerMemory).
typedef float(__thiscall* AppScaleGetFn)(const void*);
typedef void(__thiscall* AppScaleSetFn)(void*, float);
typedef const float*(__thiscall* FactorGetFn)(const void*);
typedef void(__thiscall* FactorSetFn)(void*, const float*);
struct TimeApi {
    bool tried = false;
    InstFn app = nullptr;
    AppScaleGetFn getScale = nullptr;
    AppScaleSetFn setScale = nullptr;
    FactorGetFn getFactor = nullptr;
    FactorSetFn setFactor = nullptr;
};
TimeApi& Time() {
    static TimeApi a;
    if (!a.tried && Resolve()) {
        a.tried = true;
        a.app = reinterpret_cast<InstFn>(GetProcAddress(g_e.engine, "?GetInstance@eCApplication@@SGAAV1@XZ"));
        a.getScale = reinterpret_cast<AppScaleGetFn>(GetProcAddress(g_e.engine, "?GetTimeScaling@eCApplication@@QBEMXZ"));
        a.setScale = reinterpret_cast<AppScaleSetFn>(GetProcAddress(g_e.engine, "?SetTimeScaling@eCApplication@@QAEXM@Z"));
        a.getFactor = reinterpret_cast<FactorGetFn>(GetProcAddress(g_e.game, "?GetFactor@gCClock_PS@@QBEABMXZ"));
        a.setFactor = reinterpret_cast<FactorSetFn>(GetProcAddress(g_e.game, "?SetFactor@gCClock_PS@@QAEXABM@Z"));
    }
    return a;
}
bool SafeAppScale(bool set, float* v) {
    TimeApi& a = Time();
    __try {
        void* app = a.app ? a.app() : nullptr;
        if (!app) return false;
        if (set)
            a.setScale(app, *v);
        else
            *v = a.getScale(app);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
float GetTimeScaleScript() {
    float v = -1;
    return Time().getScale && SafeAppScale(false, &v) ? v : -1.0f;
}
bool SetTimeScaleScript(float v) {
    if (!Time().setScale || !(v >= 0.05f && v <= 10.0f)) return false;
    return SafeAppScale(true, &v);
}
bool SafeFactor(void* holder, bool set, float* v) {
    TimeApi& a = Time();
    __try {
        void* ps = *static_cast<void**>(holder);
        if (!ps) return false;
        if (set)
            a.setFactor(ps, v);
        else {
            const float* f = a.getFactor(ps);
            if (!f) return false;
            *v = *f;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
float GetClockFactorScript() {
    void* c = WorldClock();
    float v = -1;
    return c && Time().getFactor && SafeFactor(&c, false, &v) ? v : -1.0f;
}
bool SetClockFactorScript(float v) {
    void* c = WorldClock();
    if (!c || !Time().setFactor || !(v >= 0.0f && v <= 10000.0f)) return false;
    return SafeFactor(&c, true, &v);
}

// ---- Trader goods (API 37): gCInventory_PS::GenerateTradeInvFromTreasureSets goes through the hook in more_events.cpp (Override_TradeGenerate)
typedef bool(__thiscall* GenTradeFn)(void*);
bool SafeGenTrade(GenTradeFn f, void* inv, bool* ok) {
    __try {
        *ok = f(inv);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeStackCount(void* inv, int* n) {
    __try {
        *n = g_i.stackCount(inv);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
int EntStackCount(const Entity& e) {
    void* inv = InventoryOf(e);
    int n = -1;
    return inv && g_i.stackCount && SafeStackCount(inv, &n) ? n : -1;
}
bool EntGenerateTradeGoods(const Entity& e) {
    void* inv = InventoryOf(e);   // also resolves g_e.game
    static GenTradeFn f = nullptr;
    if (!f && g_e.game) f = reinterpret_cast<GenTradeFn>(GetProcAddress(g_e.game, "?GenerateTradeInvFromTreasureSets@gCInventory_PS@@QAE_NXZ"));
    bool ok = false;
    return inv && f && SafeGenTrade(f, inv, &ok) && ok;
}

// ---- Screen text + menu page (API 3): gCSession::GetScreenOutputCtrl()->PrintScreen(bCUnicodeString, bCByteAlphaColor, seconds)
// as Script.dll GUI::PrintScreen_Debug, but without the "Debug: " prefix and with an own color; gCGUIManager::PrintGameMessage/GetActivePage
// as GUI2. Unicode string and color through SharedBase exports in raw buffers (color 12 bytes: vtable, r, g, b, a).
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
    G3ASM_REG(e->RegisterObjectType("Entity", sizeof(Entity), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<Entity>()));
    G3ASM_REG(e->RegisterObjectBehaviour("Entity", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(EntConstruct), asCALL_CDECL_OBJLAST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool IsValid() const", asFUNCTION(EntIsValid), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bCString GetName() const", asFUNCTION(EntName), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bCVector GetPosition() const", asFUNCTION(EntPos), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bCString GetTemplateName() const", asFUNCTION(EntTemplateName), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bCVector GetBoundsMin() const", asFUNCTION(EntBoundsMin), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bCVector GetBoundsMax() const", asFUNCTION(EntBoundsMax), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "Entity GetParent() const", asFUNCTION(EntParent), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bCMatrix4 GetWorldMatrix() const", asFUNCTION(EntWorldMatrix), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bCQuaternion GetRotation() const", asFUNCTION(EntRot), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "float GetDistanceTo(const Entity &in) const", asFUNCTION(EntDist), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool SetPosition(const bCVector &in)", asFUNCTION(EntSetPos), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool opEquals(const Entity &in) const", asFUNCTION(EntEquals), asCALL_CDECL_OBJFIRST));
    // API 3: attributes + game clock (error or no game: -1 or false)
    G3ASM_REG(e->RegisterObjectMethod("Entity", "int GetAttribute(const bCString &in) const", asFUNCTION(EntAttr), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "int GetAttributeBase(const bCString &in) const", asFUNCTION(EntAttrBase),
                                    asCALL_CDECL_OBJFIRST));
    G3ASM_REG(
        e->RegisterObjectMethod("Entity", "int GetAttributeMax(const bCString &in) const", asFUNCTION(EntAttrMax), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool SetAttributeBase(const bCString &in, int)", asFUNCTION(EntSetAttrBase),
                                    asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool AddAttributeBase(const bCString &in, int)", asFUNCTION(EntAddAttrBase),
                                    asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool SetAttribute(const bCString &in, int)", asFUNCTION(EntSetAttr), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool AddAttribute(const bCString &in, int)", asFUNCTION(EntAddAttr), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(
        e->RegisterObjectMethod("Entity", "int GetItemCount(const bCString &in) const", asFUNCTION(EntItemCount), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "int AddItems(const bCString &in, int)", asFUNCTION(EntAddItems), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(
        e->RegisterObjectMethod("Entity", "int RemoveItems(const bCString &in, int)", asFUNCTION(EntRemoveItems), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool LearnSkill(const bCString &in)", asFUNCTION(EntLearnSkill), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool ActivateSkill(const bCString &in)", asFUNCTION(EntActivateSkill), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool DeactivateSkill(const bCString &in)", asFUNCTION(EntDeactivateSkill), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool HasLearnedSkill(const bCString &in) const", asFUNCTION(EntHasLearnedSkill), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool IsSkillActive(const bCString &in) const", asFUNCTION(EntIsSkillActive), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "int GetSkillActivationCount(const bCString &in) const", asFUNCTION(EntSkillActivationCount), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "int DeleteItemStack(const bCString &in)", asFUNCTION(EntDeleteItemStack), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "int HurtHitPoints(int amount, int minHp = 0)", asFUNCTION(EntHurtHP), asCALL_CDECL_OBJFIRST));   // experimentell
    G3ASM_REG(e->RegisterGlobalFunction("int CloneTemplate(const bCString &in base, const bCString &in newName)", asFUNCTION(CloneTemplate),
                                      asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("Entity SpawnNPC(const bCString &in templateName, const bCVector &in position)", asFUNCTION(SpawnNPC),
                                      asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("Entity PickScreen(int x, int y, float maxDistance = 100000)", asFUNCTION(PickScreen), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("Entity PickCenter(float maxDistance = 100000)", asFUNCTION(PickCenter), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bCVector GetPickPoint()", asFUNCTION(GetPickPoint), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool HasPickPoint()", asFUNCTION(HasPickPoint), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("Entity GetFocusedEntity()", asFUNCTION(GetFocusedEntity), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("Entity GetCrosshairEntity()", asFUNCTION(GetCrosshairEntity), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool SetCrosshairVisible(bool)", asFUNCTION(SetCrosshairVisible), asCALL_CDECL));   // API 46: gCGUIManager::ShowCrossHair
    G3ASM_REG(e->RegisterGlobalFunction("bool DestroyNPC(const Entity &in)", asFUNCTION(DestroyNPC), asCALL_CDECL));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bCString GetCategory() const", asFUNCTION(EntCategory), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "Entity GetCurrentAttacker() const", asFUNCTION(EntCurrentAttacker), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "uint GetStatusEffects() const", asFUNCTION(EntStatusMask), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool HasStatusEffect(int) const", asFUNCTION(EntHasStatus), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool SetStatusEffect(int, bool)", asFUNCTION(EntSetStatus), asCALL_CDECL_OBJFIRST));
    {
        static const int kSE[] = {1, 2, 4, 8, 16, 64, 128, 256, 512};
        static const char* kSEN[] = {"STATUS_BURNING", "STATUS_FROZEN",  "STATUS_POISONED",     "STATUS_DISEASED",     "STATUS_TRANSFORMED",
                                     "STATUS_IN_FOG",  "STATUS_GHOST",   "STATUS_IN_MAGIC_BARRIER", "STATUS_SOULTRAVEL"};
        for (int i = 0; i < 9; ++i) {
            char d[64];
            std::snprintf(d, sizeof d, "const int %s", kSEN[i]);
            G3ASM_REG(e->RegisterGlobalProperty(d, const_cast<int*>(&kSE[i])));
        }
    }
    G3ASM_REG(e->RegisterObjectMethod("Entity", "Entity GetSlotItem(int) const", asFUNCTION(EntSlotItem), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "uint GetSlotQuality(int) const", asFUNCTION(EntSlotQuality), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool SetSlotQuality(int, uint)", asFUNCTION(EntSetSlotQuality), asCALL_CDECL_OBJFIRST));
    {
        static const int kQ[] = {2, 4, 8, 16, 32, 64, 128, 256, 512};
        static const char* kQN[] = {"QUALITY_DISEASED", "QUALITY_POISONED", "QUALITY_BURNING", "QUALITY_FROZEN", "QUALITY_SHARP",
                                    "QUALITY_BLESSED",  "QUALITY_FORGED",   "QUALITY_WORN",    "QUALITY_LIFESTEAL"};
        for (int i = 0; i < 9; ++i) {
            char d[64];
            std::snprintf(d, sizeof d, "const int %s", kQN[i]);
            G3ASM_REG(e->RegisterGlobalProperty(d, const_cast<int*>(&kQ[i])));
        }
        static const int kSlots[] = {1, 2, 5, 6, 7, 14};
        static const char* kSN[] = {"SLOT_RIGHT_HAND", "SLOT_LEFT_HAND", "SLOT_BACK_LEFT", "SLOT_BACK_RIGHT", "SLOT_BOW", "SLOT_AMMO"};
        for (int i = 0; i < 6; ++i) {
            char d[64];
            std::snprintf(d, sizeof d, "const int %s", kSN[i]);
            G3ASM_REG(e->RegisterGlobalProperty(d, const_cast<int*>(&kSlots[i])));
        }
    }
    G3ASM_REG(e->RegisterGlobalFunction("bCString GetProperty(const Entity &in, const bCString &in set, const bCString &in prop)", asFUNCTION(GetPropertyText), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool SetProperty(const Entity &in, const bCString &in set, const bCString &in prop, const bCString &in value)", asFUNCTION(SetPropertyText), asCALL_CDECL));   // experimentell
    G3ASM_REG(e->RegisterGlobalFunction("bool SetTemplateRef(const Entity &in, const bCString &in classSet, const bCString &in prop, const bCString &in target)", asFUNCTION(SetTemplateRefScript), asCALL_CDECL));   // experimentell
    G3ASM_REG(e->RegisterGlobalFunction("bool SetEntityRef(const Entity &in, const bCString &in classSet, const bCString &in prop, const Entity &in target)", asFUNCTION(SetEntityRefScript), asCALL_CDECL));   // experimentell
    G3ASM_REG(e->RegisterObjectMethod("Entity", "int GetPartyMemberCount() const", asFUNCTION(EntPartyCount), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "Entity GetPartyMember(int index) const", asFUNCTION(EntPartyMember), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "Entity GetPartyLeader() const", asFUNCTION(EntPartyLeader), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool IsPartyWaiting() const", asFUNCTION(EntIsPartyWaiting), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool SetPartyWaiting(bool)", asFUNCTION(EntSetPartyWaiting), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool TeleportTo(const Entity &in target)", asFUNCTION(EntTeleportTo), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool Kill(const Entity &in killer)", asFUNCTION(EntKill), asCALL_CDECL_OBJFIRST));   // API 46
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool Kill()", asFUNCTION(EntKillNoKiller), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "int GetInventoryStackCount() const", asFUNCTION(EntStackCount), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool GenerateTradeGoods()", asFUNCTION(EntGenerateTradeGoods), asCALL_CDECL_OBJFIRST));   // experimentell
    G3ASM_REG(e->RegisterGlobalFunction("bool EffectExists(const bCString &in name)", asFUNCTION(EffectExistsScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool StartEffect(const bCString &in name, const Entity &in at)", asFUNCTION(StartEffectScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("Entity GetTemplateEntity(const bCString &in name)", asFUNCTION(GetTemplateEntityScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bCString ListProperties(const Entity &in, const bCString &in set, int max = 200)", asFUNCTION(ListPropertyTexts), asCALL_CDECL));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool IsBuilding() const", asFUNCTION(EntIsBuilding), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterGlobalFunction("bool IsNPC(const Entity &in)", asFUNCTION(IsNPCScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool IsDynamicSpawn(const Entity &in)", asFUNCTION(IsDynamicSpawn), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("int CountEntities(const bCString &in part)", asFUNCTION(CountEntities), asCALL_CDECL));
    G3ASM_REG(
        e->RegisterGlobalFunction("Entity GetEntityByPart(const bCString &in part, int index)", asFUNCTION(GetEntityByPart), asCALL_CDECL));
    G3ASM_REG(
        e->RegisterObjectMethod("Entity", "bool SetRotation(const bCQuaternion &in)", asFUNCTION(EntSetRotation), asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterObjectMethod("Entity", "bool LookAt(const Entity &in, float yawOffsetDegrees = 0)", asFUNCTION(EntLookAt),
                                    asCALL_CDECL_OBJFIRST));
    G3ASM_REG(e->RegisterGlobalFunction("int CountTemplateNames(const bCString &in prefix)", asFUNCTION(CountTemplateNames), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bCString FindTemplateNames(const bCString &in part, int max = 20)", asFUNCTION(FindTemplateNames),
                                      asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool SetGameEvent(const bCString &in)", asFUNCTION(SetGameEvent), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool ClearGameEvent(const bCString &in)", asFUNCTION(ClearGameEvent), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool IsGameEventSet(const bCString &in)", asFUNCTION(IsGameEventSet), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("int GetGameDay()", asFUNCTION(GameDay), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("int GetGameHour()", asFUNCTION(GameHour), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("int GetGameMinute()", asFUNCTION(GameMinute), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool SetGameTime(int day, int hour, int minute)", asFUNCTION(SetGameTime), asCALL_CDECL));   // not checked in the game
    G3ASM_REG(e->RegisterGlobalFunction("float GetTimeScale()", asFUNCTION(GetTimeScaleScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool SetTimeScale(float scale)", asFUNCTION(SetTimeScaleScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("float GetClockFactor()", asFUNCTION(GetClockFactorScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool SetClockFactor(float factor)", asFUNCTION(SetClockFactorScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool PrintScreen(const bCString &in, float seconds = 3, int r = 255, int g = 255, int b = 255)",
                                      asFUNCTION(PrintScreenText), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool PrintGameMessage(const bCString &in, int type = 0)", asFUNCTION(PrintGameMessageText),
                                      asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("int GetActivePage()", asFUNCTION(GetActivePage), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("Entity GetHero()", asFUNCTION(GetHero), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("Entity GetPlayer()", asFUNCTION(GetPlayer), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("Entity FindEntity(const bCString &in)", asFUNCTION(FindEntity), asCALL_CDECL));
    if (!RegisterQuestBindings(e)) ok = false;
    if (!RegisterDialogBindings(e)) ok = false;
    if (!RegisterScriptCallBindings(e)) ok = false;
    if (!RegisterDrawBindings(e)) ok = false;
    if (!RegisterMoreEventBindings(e)) ok = false;
    if (!RegisterSchemaBindings(e)) ok = false;
    return ok;
}

// name of the template entity for a property ID (GUID), for example of an item; empty if not known
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

// hero hit (game_hooks.cpp, Override_Damage): is the entity the player character? Change the hit point attribute "HP" of the hero by delta (not if the hero is already dead)
bool IsPlayerEntity(const void* p) { return p && Resolve() && (p == SafeHero(false) || p == SafeHero(true)); }
bool AdjustHeroHitPoints(const void* ent, int delta) {
    Entity e{const_cast<void*>(ent)};
    SStr hp("HP");
    if (delta > 0 && EntAttr(e, hp) <= 0) return false;
    return AttrSet(e, hp, delta, &StatsApi::addValue);
}

}   // namespace g3asm
