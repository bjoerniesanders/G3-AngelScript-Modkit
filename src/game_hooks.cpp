// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Additional permissions: LICENSE-EXCEPTIONS.md
// Game events for scripts (script interface from version 5): inline hooks (MinHook) on the event entry points of the
// quest system in Game.dll. There, the game reports kills, item transfers, entering and leaving areas, and spells to
// gCQuestManager_PS. The hook first calls the original behavior, and then the script events of all mods
// (OnNPCKilled, OnNPCDefeated, OnReceiveItem, OnEnter, OnLeave, OnSpellCast).
// Calling convention: member functions = thiscall, intercepted here as __fastcall(this, edx, ...) (same stack cleanup).
// bCString value parameters (OnSpellCast) are 4 bytes and go without changes to the original (the callee destroys them).
// For the script, the code first keeps a reference (copy).
#include <windows.h>

#include <cstdio>
#include <functional>
#include <cstring>
#include <string>

#include <MinHook.h>

#include "sb_api.h"
#include "script_host.h"

namespace g3asm {
namespace {

ScriptHost* g_host = nullptr;

typedef bool(__thiscall* Fn1)(void*, const void*);
typedef bool(__thiscall* FnItem)(void*, const void*, const void*, const void*, DWORD);
typedef bool(__thiscall* FnArea)(void*, const void*, const void*);
typedef bool(__thiscall* FnSpell)(void*, const void*, const void*, void*);
typedef void*(__thiscall* TextFn)(const void*, void*);

Fn1 o_killed, o_defeated;
FnItem o_item;
FnArea o_enter, o_leave;
FnSpell o_spell;
TextFn g_idText = nullptr;
long g_count[7] = {};
bool g_navEvents = false;

bool IsNav(const void* area) {
    const char* t = SafeStrText(area, nullptr);
    return t && std::strncmp(t, "Nav_", 4) == 0;
}
// bCPropertyID::GetText into the raw memory text (bCString); false if there is an error (then text does not change)
bool SafeIdText(const void* id, void** text) {
    if (!g_idText || !ReadableMem(id, 16)) return false;
    __try {
        g_idText(id, text);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void Seen(int i, const char* name) {
    if (InterlockedIncrement(&g_count[i]) == 1 && g_host) g_host->Log(std::string("Spielereignis erstmals ausgeloest: ") + name);
}

// copy of a raw bCString value (keeps the characters across the call of the original)
struct StrHold {
    void* raw;
    explicit StrHold(void* r) { g_sb.strCopy(&raw, &r); }
    ~StrHold() { g_sb.strDtor(&raw); }
};

// call with the override chain: gives the result for the game; *called tells if the original ran (the events On<Name> after it only in that case)
bool RunHook(const char* overrideDecl, const EvArg* a, int n, const std::function<bool(const EvArg*)>& original, bool* called) {
    bool res = false;
    *called = false;
    if (g_host->CallOverride(overrideDecl, a, n, original, &res, called)) return res;
    res = original(a);
    *called = true;
    return res;
}

bool __fastcall HKilled(void* self, void*, const void* e) {
    if (!e) {
        NoteNull("OnNPCKilled: Entity");
        return o_killed(self, e);
    }
    EvArg a[] = {EvArg::Ent(e)};
    bool called;
    const bool r = RunHook(
        "bool Override_NPCKilled(Entity)", a, 1, [&](const EvArg*) { return o_killed(self, e); }, &called);
    if (called) {
        Seen(0, "OnNPCKilled");
        g_host->CallEvent("void OnNPCKilled(Entity)", a, 1);
    }
    return r;
}
bool __fastcall HDefeated(void* self, void*, const void* e) {
    if (!e) {
        NoteNull("OnNPCDefeated: Entity");
        return o_defeated(self, e);
    }
    EvArg a[] = {EvArg::Ent(e)};
    bool called;
    const bool r = RunHook(
        "bool Override_NPCDefeated(Entity)", a, 1, [&](const EvArg*) { return o_defeated(self, e); }, &called);
    if (called) {
        Seen(1, "OnNPCDefeated");
        g_host->CallEvent("void OnNPCDefeated(Entity)", a, 1);
    }
    return r;
}
bool __fastcall HItem(void* self, void*, const void* giver, const void* receiver, const void* itemId, DWORD count) {
    void* text = nullptr;   // bCString (4 bytes), built by the export in raw memory
    if (!itemId) NoteNull("OnReceiveItem: Item-ID");
    if (!SafeIdText(itemId, &text)) text = nullptr;
    if (!text) g_sb.strCtor(&text);
    const std::string nm = TemplateNameForId(itemId);
    SStr name(nm.empty() ? g_sb.strGetText(&text) : nm.c_str());   // without a name: GUID text
    EvArg a[] = {EvArg::Ent(giver), EvArg::Ent(receiver), EvArg::Str(&name), EvArg::Str(&text), EvArg::U(count)};
    bool called;
    // ar[4] = quantity: a mod can change it with SetOverrideArg(4, ..), then the original gets the new value
    const bool r = RunHook(
        "bool Override_ReceiveItem(Entity, Entity, const bCString &in, const bCString &in, uint)", a, 5,
        [&](const EvArg* ar) {
            if (ar[4].value != count)
                g_host->Log("OnReceiveItem: das Original bekommt Menge " + std::to_string(ar[4].value) + " (Spiel meldete " +
                            std::to_string(count) + ")");
            return o_item(self, giver, receiver, itemId, ar[4].value);
        },
        &called);
    if (called) {
        Seen(2, "OnReceiveItem");
        g_host->CallEvent("void OnReceiveItem(Entity, Entity, const bCString &in, const bCString &in, uint)", a, 5);
    }
    g_sb.strDtor(&text);
    return r;
}
bool __fastcall HEnter(void* self, void*, const void* e, const void* area) {
    if (!area) NoteNull("OnEnter: Bereichsname");
    if (!area || (!g_navEvents && IsNav(area))) return o_enter(self, e, area);
    EvArg a[] = {EvArg::Ent(e), EvArg::Str(area)};
    bool called;
    const bool r = RunHook(
        "bool Override_Enter(Entity, const bCString &in)", a, 2, [&](const EvArg*) { return o_enter(self, e, area); }, &called);
    if (called) {
        Seen(3, "OnEnter");
        g_host->CallEvent("void OnEnter(Entity, const bCString &in)", a, 2);
    }
    return r;
}
bool __fastcall HLeave(void* self, void*, const void* e, const void* area) {
    if (!area) NoteNull("OnLeave: Bereichsname");
    if (!area || (!g_navEvents && IsNav(area))) return o_leave(self, e, area);
    EvArg a[] = {EvArg::Ent(e), EvArg::Str(area)};
    bool called;
    const bool r = RunHook(
        "bool Override_Leave(Entity, const bCString &in)", a, 2, [&](const EvArg*) { return o_leave(self, e, area); }, &called);
    if (called) {
        Seen(4, "OnLeave");
        g_host->CallEvent("void OnLeave(Entity, const bCString &in)", a, 2);
    }
    return r;
}
bool __fastcall HSpell(void* self, void*, const void* caster, const void* target, void* spell) {
    StrHold keep(spell);
    EvArg a[] = {EvArg::Ent(caster), EvArg::Ent(target), EvArg::Str(&keep.raw)};
    bool called;
    const bool r = RunHook(
        "bool Override_SpellCast(Entity, Entity, const bCString &in)", a, 3,
        [&](const EvArg*) { return o_spell(self, caster, target, spell); }, &called);
    if (called) {
        Seen(5, "OnSpellCast");
        g_host->CallEvent("void OnSpellCast(Entity, Entity, const bCString &in)", a, 3);
    }
    return r;
}

// Damage: gCDamageReceiver_PS::SetHitPoints(const long&). Measured in the game (fireball 200 -> 115 for OnDamage quantity 85, melee 400 -> 323 for 77): the game
// applies the damage through SetHitPoints. gCEntity::OnDamage comes only after it and is only the message. gCEntity::DoDamage is not used for this.
// If the value decreases, this is a hit: Override_Damage(victim, attacker, quantity, type) can change the quantity (SetOverrideArg(2, ..)) or prevent the hit fully
// without Super(). Quantity = loss of hit points (limited to 0 at death).
// Field layout gCDamageReceiver_PS: +0x14 HitPoints, +0x18 HitPointsMax, +0x30 damage type, +0x34 DamageAmount, +0x38 LastInflictor (eCEntityProxy).
typedef void(__thiscall* FnSetHP)(void*, const long*);
typedef void*(__thiscall* FnGetEnt)(void*);
FnSetHP o_setHP = nullptr;
FnGetEnt g_psEntity = nullptr, g_proxyEntity = nullptr;
bool g_inDamage = false;   // protection: do not report SetHitPoints from the script (override) again as a hit
bool g_scriptHurt = false;   // Entity.HurtHitPoints runs at this time: report the decrease without an attacker (LastInflictor would be an old value)
void* SafeCall0(FnGetEnt f, void* obj) {
    if (!f || !obj || !ReadableMem(obj, sizeof(void*))) return nullptr;
    __try {
        return f(obj);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

// ---- attacker of the victim: gCNPC_PS::CurrentAttackerEntity / LastAttackerEntity ----
typedef const void*(__thiscall* FnGetRef)(const void*);
typedef void*(__thiscall* FnGetPS)(void*, int);
FnGetRef g_lastAtk = nullptr, g_curAtk = nullptr;
FnGetPS g_getPS = nullptr;

// eCEntityProxy reference (return value of Get...Entity) -> eCEntity*
void* ProxyTarget(const void* proxyRef) {
    if (!proxyRef || !g_proxyEntity) return nullptr;
    return SafeCall0(g_proxyEntity, const_cast<void*>(proxyRef));
}
// gCNPC_PS of the entity (eEPropertySetType_NPC = 30) and from it the last or current attacker; false if the entity is not an NPC
bool NpcAttackers(void* ent, void** last, void** cur) {
    *last = *cur = nullptr;
    if (!ent || !g_getPS || !g_lastAtk || !g_curAtk || !ReadableMem(ent, 8)) return false;
    __try {
        void* ps = g_getPS(ent, 30);
        if (!ps) return false;
        *last = ProxyTarget(g_lastAtk(ps));
        *cur = ProxyTarget(g_curAtk(ps));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// item in an equipment slot of the entity: gCInventory_PS (eEPropertySetType_Inventory = 31)::GetStackFromSlot(gESlot).
// gESlot: 1 RightHand, 2 LeftHand, 3 EquipLeftHand, 7 Bow, 14 Ammo.
typedef void*(__thiscall* FnStackFromSlot)(const void*, int);
FnStackFromSlot g_stackFromSlot = nullptr;
void* SlotItem(void* ent, int slot) {
    if (!ent || !g_getPS || !g_stackFromSlot || !ReadableMem(ent, 8)) return nullptr;
    __try {
        void* ps = g_getPS(ent, 31);
        return ps ? g_stackFromSlot(ps, slot) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
// copy the name of an entity into a buffer (SEH: no C++ objects with a destructor in this function)
typedef const void*(__thiscall* FnEntName)(const void*);
FnEntName g_entName = nullptr;
bool EntNameBuf(const void* ent, char* buf, size_t n) {
    buf[0] = 0;
    if (!ent || !g_entName || !ReadableMem(ent, 0x20)) return false;
    __try {
        const char* t = g_sb.strGetText(g_entName(ent));
        if (!t) return false;
        size_t i = 0;
        for (; i + 1 < n && t[i]; ++i) buf[i] = t[i];
        buf[i] = 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        buf[0] = 0;
        return false;
    }
}
bool NameHasBow(const void* ent) {   // bow or crossbow: the name contains "bow" (It_Bow_Ranger, ...Crossbow...); not spells ("It_Spell_*") and not melee weapons
    char nm[64];
    if (!EntNameBuf(ent, nm, sizeof nm)) return false;
    for (char* p = nm; *p; ++p) *p = static_cast<char>(tolower(static_cast<unsigned char>(*p)));
    return std::strstr(nm, "bow") != nullptr;
}
// What the attacker uses against the victim (inflictor, when the hero is the victim): melee = the weapon in the right hand (for animals the item "Fist"), spell = the
// spell item. Ranged = a bow in the left hand: then the projectile, thus the ammunition item (slot 14, "Arrow"), so that the hero gets the arrow as the inflictor,
// as an NPC victim does (there, the engine gives the projectile). Without an ammunition item, the bow. Measured 2026-11-06: drawn weapon right hand, drawn bow left hand,
// bow that is put away slot 7. Note: the ammunition item is the arrow in the quiver, not the flying projectile (same name, different entity).
void* HeldWeapon(void* owner) {
    if (!owner) return nullptr;
    if (void* w = SlotItem(owner, 1)) return w;
    void* w = SlotItem(owner, 2);
    if (w && NameHasBow(w)) {
        if (void* ammo = SlotItem(owner, 14)) return ammo;
    }
    return w;
}

// Type of the hit (gEDamageType: 1 Impact, 2 Blade, 3 Missile, 4 Fire, 5 Ice, 6 Lightning, 7 Physics) from the item of the attacker: gCDamage_PS (eEPropertySetType_Damage = 51)
// of the inflictor, access through gCDamage_PS::AccessDamageType (container: 4-byte header, then the value; from the assembler code: container at this+0x14, value at +0x18).
// But the value in the damage receiver of the victim (previously reported as "type") is a property of the victim and was always 2 for NPCs.
typedef void*(__thiscall* FnDmgTypeAcc)(void*);
FnDmgTypeAcc g_dmgTypeAcc = nullptr;
bool SafeDamageType(void* ent, int* out) {
    if (!ent || !g_getPS || !g_dmgTypeAcc || !ReadableMem(ent, 8)) return false;
    __try {
        void* ps = g_getPS(ent, 51);
        if (!ps) return false;
        void* c = g_dmgTypeAcc(ps);
        if (!c) return false;
        *out = *reinterpret_cast<int*>(static_cast<char*>(c) + 4);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
int HitDamageType(void* inflictor, int fallback) {
    int t = 0;
    // Measured 2026-11-06: club 1 (Impact), sword, halberd and spear 2 (Blade), arrow 3 (Missile, also the ammunition item), Mis_Fireball 4 (Fire). Not readable: no inflictor
    // and the spell item in the hand of the caster (It_Spell_*). Then the fallback applies.
    return SafeDamageType(inflictor, &t) && t >= 0 && t <= 7 ? t : fallback;
}

// True cause of a hit (API 25, parameter "owner"): if the inflictor (weapon, projectile, spell) itself is an NPC, that NPC. If not, the current opponent
// of the victim (gCNPC_PS::CurrentAttackerEntity; in the game test 2026-11-06, the opponent that hit, for the hero and for NPCs). Can be empty. In a group fight, the
// fallback is only an approximation (it is the opponent that the victim targets at this time, not always the fighter that hit).
void* DamageOwner(void* victim, void* inflictor) {
    void *l, *c;
    if (inflictor && NpcAttackers(inflictor, &l, &c)) return inflictor;
    if (victim && NpcAttackers(victim, &l, &c) && c && c != victim) return c;
    return nullptr;
}
// Damage chain: first Override_Damage(victim, inflictor, owner, amount, type) (API 25), then, as the "original" of this chain, the old form
// Override_Damage(victim, attacker, amount, type). Its original is apply(quantity) = the real effect in the game. Thus mods with the old and the new form work
// together. *called: did the effect in the game run? (then OnDamage follows in both forms, with the initial quantity)
void DamageDispatch(void* victim, void* inflictor, void* owner, int amount, int type, const std::function<bool(int)>& apply, bool* called) {
    static const char* kNew = "bool Override_Damage(Entity, Entity, Entity, int, int)";
    static const char* kOld = "bool Override_Damage(Entity, Entity, int, int)";
    EvArg an[] = {EvArg::Ent(victim), EvArg::Ent(inflictor), EvArg::Ent(owner), EvArg::U(static_cast<unsigned>(amount)), EvArg::U(static_cast<unsigned>(type))};
    bool outer = false, applied = false;
    RunHook(
        kNew, an, 5,
        [&](const EvArg* n) {
            EvArg ao[] = {EvArg::Ent(victim), EvArg::Ent(inflictor), n[3], n[4]};
            bool inner = false;
            RunHook(kOld, ao, 4, [&](const EvArg* o) { return apply(static_cast<int>(o[2].value)); }, &inner);
            if (inner) applied = true;
            return true;
        },
        &outer);
    *called = applied;
    if (applied) {
        Seen(6, "OnDamage");
        EvArg e5[] = {EvArg::Ent(victim), EvArg::Ent(inflictor), EvArg::Ent(owner), EvArg::U(static_cast<unsigned>(amount)), EvArg::U(static_cast<unsigned>(type))};
        g_host->CallEvent("void OnDamage(Entity, Entity, Entity, int, int)", e5, 5);
        EvArg e4[] = {EvArg::Ent(victim), EvArg::Ent(inflictor), EvArg::U(static_cast<unsigned>(amount)), EvArg::U(static_cast<unsigned>(type))};
        g_host->CallEvent("void OnDamage(Entity, Entity, int, int)", e4, 4);
    }
}

void __fastcall HSetHP(void* self, void*, const long* v) {
    if (g_inDamage || OnForeignThread() || !v || !self || !ReadableMem(self, 0x40)) {   // different thread: g_inDamage belongs to the game thread
        o_setHP(self, v);
        return;
    }
    char* ps = static_cast<char*>(self);
    const long alt = *reinterpret_cast<long*>(ps + 0x14), neu = *v;
    if (neu >= alt || alt <= 0) {
        o_setHP(self, v);
        return;
    }
    void* victim = SafeCall0(g_psEntity, self);
    if (!victim) NoteNull("Schaden (SetHitPoints): Opfer");
    void* attacker = g_scriptHurt ? nullptr : SafeCall0(g_proxyEntity, ps + 0x38);
    void* owner = g_scriptHurt ? nullptr : DamageOwner(victim, attacker);
    const long amount = alt - neu;
    const int type = HitDamageType(attacker, *reinterpret_cast<int*>(ps + 0x30));   // type from the item of the attacker; if not available, the value of the damage receiver
    bool called = false;
    g_inDamage = true;
    DamageDispatch(
        victim, attacker, owner, static_cast<int>(amount), type,
        [&](int mi) {
            long neuHp = neu;
            const long m = static_cast<long>(mi);
            if (m != amount) {
                const long max = *reinterpret_cast<long*>(ps + 0x18);
                neuHp = alt - m;
                if (neuHp < 0) neuHp = 0;
                if (max > 0 && neuHp > max) neuHp = max;
                g_host->Log("Schaden: das Original bekommt " + std::to_string(m) + " (Spiel meldete " + std::to_string(amount) +
                            "), Lebenspunkte " + std::to_string(alt) + " -> " + std::to_string(neuHp));
            }
            o_setHP(self, &neuHp);
            return true;
        },
        &called);
    g_inDamage = false;
}

// Animal bites and other game script damage to the hero: Script_Game.dll decreases the attribute "HP" through gCPlayerMemory_PS::SetBaseValue(tag, value), without
// gCEntity::OnDamage (measured with a breakpoint 2026-11-06: wolf bites 21 points for each bite). The hook reports the decrease BEFORE the write to the same chain
// Override_Damage/OnDamage as the hits. A changed quantity is written directly. Without Super(), there is no loss. The type is not known here (0).
// If gCEntity::OnDamage (HeroHit) comes later for the same hit, it is not reported a second time (g_baseHitAmount/-Tick).
typedef bool(__thiscall* FnPMSetBase)(void*, const void*, int);
typedef int(__thiscall* FnPMGetBase)(const void*, const void*);
FnPMSetBase o_pmSetBase = nullptr;
FnPMGetBase g_pmGetBase = nullptr;
volatile LONG g_baseHitAmount = -1;
volatile DWORD g_baseHitTick = 0;
bool SafeIsHpTag(const void* tag) {
    __try {
        const char* t = tag ? g_sb.strGetText(tag) : nullptr;
        return t && std::strcmp(t, "HP") == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafePMBase(const void* ps, const void* tag, int* out) {
    __try {
        *out = g_pmGetBase(ps, tag);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// Environment damage (fall and similar) in place of a combat hit: for combat hits (bite, blow, spell), the engine (Game.dll) calls the script damage routine (Script_Game.dll)
// directly. For a fall, a different script routine of the same DLL calls it (measured 2026-11-06: return addresses Script_Game.dll+0x35BF2 < Game.dll
// in combat, Script_Game.dll+0x35BF2 < Script_Game.dll+0xADF8F < Game.dll for a fall). The detection is structural: after the first Script_Game frame of the call stack
// comes a second Script_Game frame. Then there is no attacker: owner and inflictor stay empty (if not, the last opponent would be there).
bool IsScriptNested() {
    static HMODULE sg = nullptr;
    if (!sg) sg = GetModuleHandleA("Script_Game.dll");
    if (!sg) return false;
    const char* b = reinterpret_cast<const char*>(sg);
    const DWORD size = reinterpret_cast<const IMAGE_NT_HEADERS*>(b + reinterpret_cast<const IMAGE_DOS_HEADER*>(b)->e_lfanew)->OptionalHeader.SizeOfImage;
    void* fr[10];
    const USHORT n = CaptureStackBackTrace(1, 10, fr, nullptr);
    for (USHORT i = 0; i < n; ++i) {
        const char* a = static_cast<const char*>(fr[i]);
        if (a >= b && a < b + size) return i + 1 < n && static_cast<const char*>(fr[i + 1]) >= b && static_cast<const char*>(fr[i + 1]) < b + size;
    }
    return false;
}
bool __fastcall HPMSetBase(void* self, void*, const void* tag, int v) {
    int alt = 0;
    if (g_inDamage || OnForeignThread() || !g_pmGetBase || !g_psEntity || !SafeIsHpTag(tag) || !SafePMBase(self, tag, &alt) || v >= alt || alt <= 0) return o_pmSetBase(self, tag, v);
    void* hero = SafeCall0(g_psEntity, self);
    if (!hero || !IsPlayerEntity(hero)) return o_pmSetBase(self, tag, v);
    const int amount = alt - v;
    void* owner = IsScriptNested() ? nullptr : DamageOwner(hero, nullptr);
    void* weapon = HeldWeapon(owner);
    bool res = true, called = false;
    g_inDamage = true;
    DamageDispatch(
        hero, weapon, owner, amount, HitDamageType(weapon, 0),
        [&](int m) {
            int neu = m == amount ? v : alt - m;
            if (neu < 0) neu = 0;
            res = o_pmSetBase(self, tag, neu);
            return true;
        },
        &called);
    g_inDamage = false;
    g_baseHitAmount = amount;
    g_baseHitTick = GetTickCount();
    return res;
}

// Hits on the hero: the gCDamageReceiver_PS of the hero is always 1. The hit points are in the attribute "HP" and are already decreased when gCEntity::OnDamage is called
// (this = a = b = hero, quantity, type) (measured). The hook reports the hit to the same chain Override_Damage as for NPCs. A changed quantity
// has an effect afterward: the difference is given back to the attribute or also subtracted. Without Super(), there is no hit (full refund).
// A hero that is already dead does not come back to life. If the fall or environment damage takes a different path, the hook does not see it.
typedef void(__thiscall* FnOnDmg)(void*, void*, void*, int, int, void*);
FnOnDmg o_gOnDmg = nullptr;
void HeroHit(void* hero, int amount, int type) {
    if (g_inDamage || amount <= 0 || OnForeignThread()) return;
    if (amount == g_baseHitAmount && GetTickCount() - g_baseHitTick < 150) {   // the same hit was already reported through SetBaseValue
        g_baseHitAmount = -1;
        return;
    }
    int neu = amount;
    bool called = false;
    g_inDamage = true;
    void* owner = DamageOwner(hero, nullptr);
    DamageDispatch(
        hero, HeldWeapon(owner), owner, amount, type,
        [&](int m) {
            neu = m;
            return true;
        },
        &called);
    g_inDamage = false;
    if (!called) neu = 0;   // override without Super(): hit prevented
    if (neu < 0) neu = 0;
    if (neu != amount) {
        g_inDamage = true;   // do not report the correction of the modkit as a change from another source
        const bool ok = AdjustHeroHitPoints(hero, amount - neu);
        g_inDamage = false;
        g_host->Log("Schaden am Helden: " + std::to_string(amount) + " -> " + std::to_string(neu) +
                    (ok ? " (Lebenspunkte korrigiert)" : " (Korrektur nicht moeglich)"));
    }
}
void __fastcall HGOnDmg(void* self, void*, void* a, void* b, int amount, int type, void* it) {
    if (self && a == self && b == self && IsPlayerEntity(self)) HeroHit(self, amount, type);
    o_gOnDmg(self, a, b, amount, type, it);
}

// gCInfo_PS::AreConditionsFulfilled: are the conditions of a dialog option met, thus can the player select it at this time? The info manager calls this function directly
// (IsAvailable has no callers). A mod can change the result with Override_InfoAvailable.
typedef bool(__thiscall* InfoAvailFn)(const void*);
InfoAvailFn o_infoAvail = nullptr;
bool __fastcall HInfoAvail(const void* self, void*) {
    static const char* kDecl = "bool Override_InfoAvailable(const bCString &in, const bCString &in, const bCString &in)";
    if (!g_host->HasHandler(kDecl)) return o_infoAvail(self);
    std::string n, o, q;
    if (!InfoNames(self, &n, &o, &q)) return o_infoAvail(self);
    SStr sn(n), so(o), sq(q);
    EvArg a[] = {EvArg::Str(&sn), EvArg::Str(&so), EvArg::Str(&sq)};
    bool called;
    return RunHook(
        kDecl, a, 3, [&](const EvArg*) { return o_infoAvail(self); }, &called);
}


// ---- Status effects as an event (API 27): OnStatusEffect(npc, effect, enabled) for each change of the bit mask gCNPC_PS::m_u32StatusEffects. Two paths change it:
// EnableStatusEffect(effect, on) and SetStatusEffects(mask). The code hooks the two. A counter prevents duplicate reports if one calls the other.
// The hook reports each changed bit, only if the value really changes (the game also calls EnableStatusEffect continuously with the same value).
typedef void(__thiscall* FnEnableSE)(void*, int, bool);
typedef void(__thiscall* FnSetSE)(void*, const unsigned long*);
typedef const unsigned long&(__thiscall* FnGetSE)(const void*);
FnEnableSE o_enableSE = nullptr;
FnSetSE o_setSE = nullptr;
FnGetSE g_getSE = nullptr;
volatile LONG g_seDepth = 0;
bool SafeStatusMask(void* ps, unsigned* out) {
    if (!g_getSE || !ps || !ReadableMem(ps, 8)) return false;
    __try {
        *out = static_cast<unsigned>(g_getSE(ps));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void ReportStatusChange(void* ps, unsigned before, unsigned after) {
    const unsigned diff = before ^ after;
    if (!diff || !g_host) return;
    void* npc = SafeCall0(g_psEntity, ps);
    for (int bit = 0; bit < 32; ++bit) {
        const unsigned m = 1u << bit;
        if (!(diff & m)) continue;
        EvArg a[] = {EvArg::Ent(npc), EvArg::U(m), EvArg::U((after & m) ? 1u : 0u)};
        g_host->CallEvent("void OnStatusEffect(Entity, int, int)", a, 3);
    }
}
void __fastcall HEnableSE(void* self, void*, int effect, bool on) {
    unsigned before = 0;
    const bool top = InterlockedIncrement(&g_seDepth) == 1;
    if (top) SafeStatusMask(self, &before);
    o_enableSE(self, effect, on);
    unsigned after = before;
    if (top && SafeStatusMask(self, &after)) ReportStatusChange(self, before, after);
    InterlockedDecrement(&g_seDepth);
}
void __fastcall HSetSE(void* self, void*, const unsigned long* v) {
    unsigned before = 0;
    const bool top = InterlockedIncrement(&g_seDepth) == 1;
    if (top) SafeStatusMask(self, &before);
    o_setSE(self, v);
    unsigned after = before;
    if (top && SafeStatusMask(self, &after)) ReportStatusChange(self, before, after);
    InterlockedDecrement(&g_seDepth);
}

// ---- Own item quality "Lebensraub" (life steal) in the description (API 28) ----
// The game builds the quality lines of the item description (poisoned, sharp, blessed, ...) in a function at Game.dll+0xA3DF0: eax = gCItem_PS* (quality at +0x18), ecx = inventory stack
// or 0 (quality at +0x40), one stack parameter = tooltip object (ret 4), return value eax = number of lines. For each set bit, it gets the text through eCLocString(key).GetString(), builds a
// prefix ("(+10)") and calls the line helper at Game.dll+0xA48B0 (eax = type 5, ecx = tooltip, stack: symbol, &prefix, &text; ret 0xC). After that comes a separator line (type 6) if there were lines.
// This hook first calls the modkit line (if the bit is set) and then the original. If the original had no lines, the hook adds the separator line. Text: key
// HUD_Quality_Lifesteal from the string table of a mod. Protection: byte check of the function before the hook is installed, SEH around all calls into the engine.
const unsigned kQualityLifesteal = 512;
const DWORD kTipFuncRva = 0xA3DF0, kTipLineRva = 0xA48B0;
typedef void*(__thiscall* FnUniCtor)(void*, const char*);
typedef void(__thiscall* FnUniDtor)(void*);
typedef void*(__thiscall* FnLocCtor)(void*, const void*);
typedef void*(__thiscall* FnLocGet)(void*, void*);
typedef void(__thiscall* FnLocDtor)(void*);
FnUniCtor g_uniCtor = nullptr;
FnUniDtor g_uniDtor = nullptr;
FnLocCtor g_locCtor = nullptr;
FnLocGet g_locGet = nullptr;
FnLocDtor g_locDtor = nullptr;
char* g_tipLine = nullptr;      // line helper
void* o_tipFunc = nullptr;      // original of the description function
volatile long g_tipLogged = 0;

__declspec(naked) void __cdecl CallTipLine(void* fn, void* tooltip, void* text, void* prefix, int icon, int kind) {
    __asm {
        push ebp
        mov ebp, esp
        push dword ptr [ebp+24]   // Symbol
        push dword ptr [ebp+20]   // prefix
        push dword ptr [ebp+16]   // Text
        mov ecx, dword ptr [ebp+12]
        mov eax, dword ptr [ebp+28]
        call dword ptr [ebp+8]
        pop ebp
        ret
    }
}
__declspec(naked) int __cdecl CallTipOrig(void* orig, void* item, void* stack, void* tooltip) {
    __asm {
        mov eax, dword ptr [esp+8]
        mov ecx, dword ptr [esp+12]
        push dword ptr [esp+16]
        call dword ptr [esp+8]
        ret
    }
}
// line "(20 %) <text for the key>" with symbol 2 (the symbol of the blessing); false if there is an error. No C++ objects with a destructor (SEH).
bool TipAddLifestealLine(void* tooltip, const void* keyStr) {
    char prefix[32] = {}, text[32] = {}, loc[32] = {};
    __try {
        g_uniCtor(prefix, "(20 %)");
        g_locCtor(loc, keyStr);
        g_locGet(loc, text);
        CallTipLine(g_tipLine, tooltip, text, prefix, 2, 5);
        g_uniDtor(text);
        g_locDtor(loc);
        g_uniDtor(prefix);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool TipAddSeparator(void* tooltip) {
    char a[32] = {}, b[32] = {};
    __try {
        g_uniCtor(a, "");
        g_uniCtor(b, "");
        CallTipLine(g_tipLine, tooltip, b, a, -1, 6);
        g_uniDtor(b);
        g_uniDtor(a);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool TipHasLifesteal(const char* item, const char* stack) {
    unsigned q = 0;
    if (item && ReadableMem(item + 0x18, 4)) q |= *reinterpret_cast<const unsigned*>(item + 0x18);
    if (stack && ReadableMem(stack + 0x40, 4)) q |= *reinterpret_cast<const unsigned*>(stack + 0x40);
    return (q & kQualityLifesteal) != 0;
}
int __cdecl TipDetour(void* item, void* stack, void* tooltip) {
    int added = 0;
    if (tooltip && TipHasLifesteal(static_cast<const char*>(item), static_cast<const char*>(stack))) {
        SStr key("HUD_Quality_Lifesteal");
        added = TipAddLifestealLine(tooltip, &key) ? 1 : 0;
        if (!added && g_host && InterlockedIncrement(&g_tipLogged) <= 3) g_host->Log("Qualitaets-Tooltip: Zeile 'Lebensraub' konnte nicht angelegt werden");
    }
    const int lines = CallTipOrig(o_tipFunc, item, stack, tooltip);
    if (added && lines == 0) TipAddSeparator(tooltip);
    return lines + added;
}
__declspec(naked) void TipStub() {
    __asm {
        push dword ptr [esp+4]   // Tooltip
        push ecx                 // Stapel
        push eax                 // Item
        call TipDetour
        add esp, 12
        ret 4
    }
}
void InstallQualityTooltip(HMODULE game, ScriptHost& host) {
    HMODULE sb = GetModuleHandleA("SharedBase.dll"), eng = GetModuleHandleA("Engine.dll");
    if (!game || !sb || !eng) return;
    g_uniCtor = reinterpret_cast<FnUniCtor>(GetProcAddress(sb, "??0bCUnicodeString@@QAE@PBD@Z"));
    g_uniDtor = reinterpret_cast<FnUniDtor>(GetProcAddress(sb, "??1bCUnicodeString@@QAE@XZ"));
    g_locCtor = reinterpret_cast<FnLocCtor>(GetProcAddress(eng, "??0eCLocString@@QAE@ABVbCString@@@Z"));
    g_locGet = reinterpret_cast<FnLocGet>(GetProcAddress(eng, "?GetString@eCLocString@@QBE?BVbCUnicodeString@@XZ"));
    g_locDtor = reinterpret_cast<FnLocDtor>(GetProcAddress(eng, "??1eCLocString@@QAE@XZ"));
    if (!g_uniCtor || !g_uniDtor || !g_locCtor || !g_locGet || !g_locDtor) {
        host.Log("Qualitaets-Tooltip: Engine-Funktionen fehlen - nicht eingehaengt");
        return;
    }
    char* target = reinterpret_cast<char*>(game) + kTipFuncRva;
    char* line = reinterpret_cast<char*>(game) + kTipLineRva;
    static const unsigned char kSig[] = {0x83, 0xEC, 0x10, 0x8B, 0x40, 0x18, 0x53, 0x56, 0x33, 0xDB, 0x85, 0xC9};   // sub esp,10h / mov eax,[eax+18h] / push ebx / push esi / xor ebx,ebx / test ecx,ecx
    // line helper: sub esp,2Ch / push ebx / mov ebx,ecx / push esi / lea ecx,[esp+18h] / mov esi,eax. The call [import] that follows stays outside (absolute address).
    static const unsigned char kLineSig[] = {0x83, 0xEC, 0x2C, 0x53, 0x8B, 0xD9, 0x56, 0x8D, 0x4C, 0x24, 0x18, 0x8B, 0xF0};
    if (!ReadableMem(target, sizeof kSig) || std::memcmp(target, kSig, sizeof kSig) != 0) {
        host.Log("Qualitaets-Tooltip: Funktion bei Game.dll+0xA3DF0 hat nicht die erwarteten Bytes (andere Spielversion?) - nicht eingehaengt");
        return;
    }
    if (!ReadableMem(line, sizeof kLineSig) || std::memcmp(line, kLineSig, sizeof kLineSig) != 0) {
        host.Log("Qualitaets-Tooltip: Zeilen-Helfer bei Game.dll+0xA48B0 hat nicht die erwarteten Bytes (andere Spielversion?) - nicht eingehaengt");
        return;
    }
    g_tipLine = line;
    const bool ok = MH_CreateHook(target, reinterpret_cast<void*>(&TipStub), &o_tipFunc) == MH_OK && MH_EnableHook(target) == MH_OK;
    host.Log(std::string("Qualitaets-Tooltip (Lebensraub): ") + (ok ? "eingehaengt" : "FEHLER beim Einhaengen"));
}

// ---- Load and save: at a load, the game makes the quests and infos again from the ini files. Own quests and infos (made by script) must already be THERE
// before the game reads the saved run-time data (status, quest log). The hooks report the end of the setup to the scripts:
//   OnQuestsLoaded()  after gCQuestManager_PS::LoadQuestsFromInis        OnInfosLoaded()  after gCInfoManager_PS::LoadInfosFromInis
// and log the sequence (read and write of the quest run-time data) for troubleshooting.
typedef bool(__thiscall* BoolMethod)(void*);
typedef void(__thiscall* VoidMethodT)(void*);
typedef int(__thiscall* RuntimeFn)(void*, void*);
typedef const void*(__thiscall* GetNameFn)(const void*);
BoolMethod o_loadQuests = nullptr, o_loadInfos = nullptr;
VoidMethodT o_resetQuests = nullptr;
RuntimeFn o_readRuntime = nullptr, o_writeRuntime = nullptr, o_readInfoSave = nullptr;
GetNameFn g_questName = nullptr;
long g_readCount = 0, g_writeCount = 0;

void LogLine(const char* s) {
    if (g_host) g_host->Log(s);
}
const char* QuestNameOf(const void* q) {
    if (!ReadableMem(q, sizeof(void*))) return nullptr;
    __try {
        const void* n = g_questName ? g_questName(q) : nullptr;
        return n ? g_sb.strGetText(n) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool __fastcall HLoadQuests(void* self, void*) {
    const bool r = o_loadQuests(self);
    {
        char tb[96];
        std::snprintf(tb, sizeof tb, "Laden: LoadQuestsFromInis fertig -> OnQuestsLoaded (Thread %lu)", static_cast<unsigned long>(GetCurrentThreadId()));
        LogLine(tb);
    }
    SetQuestManagerOverride(self);   // the bindings now work on the manager that the game just made
    EnsureTextHooks();   // quest log entries with the text in the ID (AddQuestLogEntry, from v0.47.1) are in the saved game: resolve them also without own texts
    g_host->CallEvent("void OnQuestsLoaded()", nullptr, 0);
    SetQuestManagerOverride(nullptr);
    return r;
}
bool __fastcall HLoadInfos(void* self, void*) {
    // LoadInfosFromInis empties only the info list, not the name table: remove own infos first, if not their names count as in use after it
    const int forgot = ForgetCreatedInfos(self);
    const bool r = o_loadInfos(self);
    {
        char tb[128];
        std::snprintf(tb, sizeof tb, "Laden: LoadInfosFromInis fertig (%d eigene Info(s) vorher ausgetragen) -> OnInfosLoaded", forgot);
        LogLine(tb);
    }
    SetInfoManagerOverride(self);
    g_host->CallEvent("void OnInfosLoaded()", nullptr, 0);
    SetInfoManagerOverride(nullptr);
    return r;
}
typedef bool(__thiscall* ReadCompiledFn)(void*, const void*);
ReadCompiledFn o_readCompiled = nullptr;
bool __fastcall HReadCompiled(void* self, void*,
                              const void* path) {   // the game loads the infos in this way at a load (compiled), not from ini
    const bool r = o_readCompiled(self, path);
    LogLine("Laden: ReadCompiledInfoManager fertig -> OnInfosLoaded");
    SetInfoManagerOverride(self);
    g_host->CallEvent("void OnInfosLoaded()", nullptr, 0);
    SetInfoManagerOverride(nullptr);
    return r;
}
void __fastcall HResetQuests(void* self, void*) {
    LogLine("Laden: ResetQuests");
    o_resetQuests(self);
}
int __fastcall HReadRuntime(void* self, void*, void* stream) {
    const long n = InterlockedIncrement(&g_readCount);
    const int r = o_readRuntime(self, stream);
    const char* nm = QuestNameOf(self);
    if (n <= 3 || (nm && std::strncmp(nm, "G3ASM", 5) == 0)) {
        char b[160];
        std::snprintf(b, sizeof b, "Laden: Quest-Laufzeitdaten gelesen #%ld fuer '%s'", n, nm ? nm : "?");
        LogLine(b);
    }
    return r;
}
int __fastcall HWriteRuntime(void* self, void*, void* stream) {
    const long n = InterlockedIncrement(&g_writeCount);
    const char* nm = QuestNameOf(self);
    if (nm && std::strncmp(nm, "G3ASM", 5) == 0) {
        char b[160];
        std::snprintf(b, sizeof b, "Speichern: Quest-Laufzeitdaten geschrieben #%ld fuer '%s'", n, nm);
        LogLine(b);
    }
    return o_writeRuntime(self, stream);
}
int __fastcall HReadInfoSave(void* self, void*, void* stream) {
    LogLine("Laden: Info-Manager ReadSaveGame");
    return o_readInfoSave(self, stream);
}

// ---- Save: gCWorld::SaveGameWorld(name, image, pointer, DWORD) runs exactly 1 time for each save (menu and quick save), in the same thread as the load
// (measured 2026-11-08). Before it OnBeforeSave(name), after it OnAfterSave(name, ok): mods can remove permanent conditions before the write and restore them after it.
// (gCWorld::CreateSaveGame is not part of the save: it never occurred during a save.)
typedef bool(__thiscall* SaveWorldFn)(void*, const void*, void*, void*, DWORD);
SaveWorldFn o_saveWorld = nullptr;
long g_saveDepth = 0;

void LogSave(const char* phase, const void* name, long depth, int result) {
    char b[256];
    std::snprintf(b, sizeof b, "Speichern: SaveGameWorld('%.80s') %s, Thread %lu, Tiefe %ld, Ergebnis %d", SafeStrText(name, "?"), phase,
                  static_cast<unsigned long>(GetCurrentThreadId()), depth, result);
    LogLine(b);
}
bool __fastcall HSaveWorld(void* self, void*, const void* name, void* img, void* p, DWORD k) {
    const long d = InterlockedIncrement(&g_saveDepth);
    const bool outer = d == 1;   // nested calls (never measured) report no events
    if (!name) NoteNull("OnBeforeSave/OnAfterSave: Name des Spielstands");
    LogSave("Beginn", name, d, 0);
    if (outer) {
        EvArg a[] = {EvArg::Str(name)};
        g_host->CallEvent("void OnBeforeSave(const bCString &in)", a, 1);
    }
    const bool r = o_saveWorld(self, name, img, p, k);
    LogSave("Ende", name, d, r ? 1 : 0);
    if (outer) {
        EvArg a[] = {EvArg::Str(name), EvArg::U(r ? 1u : 0u)};
        g_host->CallEvent("void OnAfterSave(const bCString &in, int)", a, 2);
    }
    InterlockedDecrement(&g_saveDepth);
    return r;
}
void InstallSaveHooks(HMODULE game, ScriptHost& host) {
    g_questName = reinterpret_cast<GetNameFn>(GetProcAddress(game, "?GetName@gCQuest_PS@@QBEABVbCString@@XZ"));
    int n = 0;
    n += HookExport(game, "?LoadQuestsFromInis@gCQuestManager_PS@@QAE_NXZ", reinterpret_cast<void*>(&HLoadQuests),
              reinterpret_cast<void**>(&o_loadQuests), host);
    n += HookExport(game, "?LoadInfosFromInis@gCInfoManager_PS@@QAE_NXZ", reinterpret_cast<void*>(&HLoadInfos),
              reinterpret_cast<void**>(&o_loadInfos), host);
    n += HookExport(game, "?ReadCompiledInfoManager@gCInfoManager_PS@@QAE_NABVbCString@@@Z", reinterpret_cast<void*>(&HReadCompiled),
              reinterpret_cast<void**>(&o_readCompiled), host);
    n += HookExport(game, "?ResetQuests@gCQuestManager_PS@@QAEXXZ", reinterpret_cast<void*>(&HResetQuests),
              reinterpret_cast<void**>(&o_resetQuests), host);
    n += HookExport(game, "?ReadRuntimeData@gCQuest_PS@@QAE?AW4bEResult@@AAVbCIStream@@@Z", reinterpret_cast<void*>(&HReadRuntime),
              reinterpret_cast<void**>(&o_readRuntime), host);
    n += HookExport(game, "?WriteRuntimeData@gCQuest_PS@@QAE?AW4bEResult@@AAVbCOStream@@@Z", reinterpret_cast<void*>(&HWriteRuntime),
              reinterpret_cast<void**>(&o_writeRuntime), host);
    n += HookExport(game, "?ReadSaveGame@gCInfoManager_PS@@UAE?AW4bEResult@@AAVbCIStream@@@Z", reinterpret_cast<void*>(&HReadInfoSave),
              reinterpret_cast<void**>(&o_readInfoSave), host);
    n += HookExport(game, "?SaveGameWorld@gCWorld@@QAE_NABVbCString@@PAVeCGfxImageData@@PAXK@Z", reinterpret_cast<void*>(&HSaveWorld),
              reinterpret_cast<void**>(&o_saveWorld), host);
    char b[96];
    std::snprintf(b, sizeof b, "Lade-/Speicher-Hooks: %d von 8 aktiv", n);
    host.Log(b);

}

}   // namespace

void SetGameHookOptions(bool navEvents) { g_navEvents = navEvents; }

// Installs the game events (1 time, after Game.dll is loaded). Return value: the number of active hooks.
int InstallGameHooks(ScriptHost& host) {
    static bool done = false;
    if (done) return 0;
    done = true;
    g_host = &host;
    HMODULE game = GetModuleHandleA("Game.dll");
    if (!game) {
        host.Log("Spielereignisse: Game.dll nicht geladen");
        return 0;
    }
    MH_STATUS st = MH_Initialize();
    if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) {
        host.Log("Spielereignisse: MinHook nicht startbar");
        return 0;
    }
    if (HMODULE sb = GetModuleHandleA("SharedBase.dll"))
        g_idText = reinterpret_cast<TextFn>(GetProcAddress(sb, "?GetText@bCPropertyID@@QBE?AVbCString@@XZ"));
    int n = 0;
    n += HookExport(game, "?OnNPCKilled@gCQuestManager_PS@@QAE_NABVeCEntity@@@Z", reinterpret_cast<void*>(&HKilled),
              reinterpret_cast<void**>(&o_killed), host);
    n += HookExport(game, "?OnNPCDefeated@gCQuestManager_PS@@QAE_NABVeCEntity@@@Z", reinterpret_cast<void*>(&HDefeated),
              reinterpret_cast<void**>(&o_defeated), host);
    n += HookExport(game, "?OnReceiveItem@gCQuestManager_PS@@QAE_NPBVeCEntity@@0ABVbCPropertyID@@K@Z", reinterpret_cast<void*>(&HItem),
              reinterpret_cast<void**>(&o_item), host);
    n += HookExport(game, "?OnEnter@gCQuestManager_PS@@QAE_NABVeCEntity@@ABVbCString@@@Z", reinterpret_cast<void*>(&HEnter),
              reinterpret_cast<void**>(&o_enter), host);
    n += HookExport(game, "?OnLeave@gCQuestManager_PS@@QAE_NABVeCEntity@@ABVbCString@@@Z", reinterpret_cast<void*>(&HLeave),
              reinterpret_cast<void**>(&o_leave), host);
    n += HookExport(game, "?OnSpellCast@gCQuestManager_PS@@QAE_NABVeCEntity@@0VbCString@@@Z", reinterpret_cast<void*>(&HSpell),
              reinterpret_cast<void**>(&o_spell), host);
    n += HookExport(game, "?AreConditionsFulfilled@gCInfo_PS@@IBE_NXZ", reinterpret_cast<void*>(&HInfoAvail),
              reinterpret_cast<void**>(&o_infoAvail), host);
    if (HMODULE eng = GetModuleHandleA("Engine.dll")) {
        g_psEntity = reinterpret_cast<FnGetEnt>(GetProcAddress(eng, "?GetEntity@eCEntityPropertySet@@UAEPAVeCEntity@@XZ"));
        g_proxyEntity = reinterpret_cast<FnGetEnt>(GetProcAddress(eng, "?GetEntity@eCEntityProxy@@QAEPAVeCEntity@@XZ"));
        g_getPS = reinterpret_cast<FnGetPS>(GetProcAddress(eng, "?GetPropertySet@eCEntity@@UAEPAVeCEntityPropertySet@@W4eEPropertySetType@@@Z"));
        g_lastAtk = reinterpret_cast<FnGetRef>(GetProcAddress(game, "?GetLastAttackerEntity@gCNPC_PS@@QBEABVeCEntityProxy@@XZ"));
        g_curAtk = reinterpret_cast<FnGetRef>(GetProcAddress(game, "?GetCurrentAttackerEntity@gCNPC_PS@@QBEABVeCEntityProxy@@XZ"));
        g_pmGetBase = reinterpret_cast<FnPMGetBase>(GetProcAddress(game, "?GetBaseValue@gCPlayerMemory_PS@@QBEHABVbCString@@@Z"));
        g_dmgTypeAcc = reinterpret_cast<FnDmgTypeAcc>(GetProcAddress(game, "?AccessDamageType@gCDamage_PS@@QAEAAV?$bTPropertyContainer@W4gEDamageType@@@@XZ"));
        g_entName = reinterpret_cast<FnEntName>(GetProcAddress(eng, "?GetName@eCEntity@@QBEABVbCString@@XZ"));
        g_stackFromSlot = reinterpret_cast<FnStackFromSlot>(GetProcAddress(game, "?GetStackFromSlot@gCInventory_PS@@QBEPAVeCEntity@@W4gESlot@@@Z"));
    }
    n += HookExport(game, "?SetHitPoints@gCDamageReceiver_PS@@QAEXABJ@Z", reinterpret_cast<void*>(&HSetHP), reinterpret_cast<void**>(&o_setHP),
              host);
    HookExport(game, "?SetBaseValue@gCPlayerMemory_PS@@QAE_NABVbCString@@H@Z", reinterpret_cast<void*>(&HPMSetBase),
               reinterpret_cast<void**>(&o_pmSetBase), host);
    HookExport(game, "?OnDamage@gCEntity@@UAEXPAVeCEntity@@0HHAAVeCContactIterator@@@Z", reinterpret_cast<void*>(&HGOnDmg),
         reinterpret_cast<void**>(&o_gOnDmg), host);
    char b[96];
    std::snprintf(b, sizeof b, "Spielereignisse: %d von 8 Hooks aktiv (Game.dll, gCQuestManager_PS, gCDamageReceiver_PS::SetHitPoints)", n);
    host.Log(b);
    g_getSE = reinterpret_cast<FnGetSE>(GetProcAddress(game, "?GetStatusEffects@gCNPC_PS@@QBEABKXZ"));
    {
        int ns = 0;
        ns += HookExport(game, "?EnableStatusEffect@gCNPC_PS@@QAEXW4gEStatusEffect@@_N@Z", reinterpret_cast<void*>(&HEnableSE),
                         reinterpret_cast<void**>(&o_enableSE), host);
        ns += HookExport(game, "?SetStatusEffects@gCNPC_PS@@QAEXABK@Z", reinterpret_cast<void*>(&HSetSE), reinterpret_cast<void**>(&o_setSE), host);
        host.Log("Statuseffekt-Hooks: " + std::to_string(ns) + " von 2 aktiv (OnStatusEffect)");
    }
    InstallQualityTooltip(game, host);
    InstallInfoHook(host);
    InstallSaveHooks(game, host);
    InstallScriptCallHooks(host);
    InstallDrawHooks(host);
    InstallMoreEventHooks(host);
    InstallAiTrace(host);   // only with ini [g3asm] aitrace=1
    InstallTradeHooks(host);   // OnTrade; the trade measurement only with ini [g3asm] tradelog=1
    return n;
}

void* SlotItemOf(void* ent, int slot) { return SlotItem(ent, slot); }
void SetScriptHurt(bool on) { g_scriptHurt = on; }

void* CurrentAttackerOf(void* ent) {
    void *l, *c;
    return NpcAttackers(ent, &l, &c) ? c : nullptr;
}

bool HookExport(HMODULE m, const char* sym, void* detour, void** orig, ScriptHost& host) {
    void* target = m ? reinterpret_cast<void*>(GetProcAddress(m, sym)) : nullptr;
    if (!target) {
        host.Log(std::string("Hook: Export fehlt: ") + sym);
        return false;
    }
    if (MH_CreateHook(target, detour, orig) != MH_OK || MH_EnableHook(target) != MH_OK) {
        host.Log(std::string("Hook: Einhaengen fehlgeschlagen: ") + sym);
        return false;
    }
    return true;
}

}   // namespace g3asm
