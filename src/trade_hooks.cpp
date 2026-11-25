// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Additional permissions: LICENSE-EXCEPTIONS.md
// Trade: the script event OnTrade (API 47, always on) and the trade measurement (only log; ini [g3asm] tradelog=1, default off).
// OnReceiveItem (gCQuestManager_PS::OnReceiveItem) reports items from dialogs, but not trade (measured 2026-11-19: purchase from Gelford,
// 2000 gold, no event).
//
// OnTrade(Entity trader, const bCString &in item, int amount, int gold, bool bought), measured with tradelog=1 (2026-11-19):
//   gCGUIManager::BeginTrade(trader) opens the trade page. The player selects with SetTransactionAmount(stack, amount) on both inventories
//   (gold is a separate stack It_Gold). On confirm, the game calls TransferTransactionItemsTo(target, type) one time for each direction
//   (hero -> trader, then trader -> hero). Then it sets all transaction amounts back to 0.
//   Looting a body also uses TransferTransactionItemsTo, but with the transaction amount -1 (take all). Thus OnTrade only comes for amounts > 0,
//   when the hero is on exactly one side and BeginTrade came before.
//   Before the first transfer of a trade, the hook reads the selected stacks of BOTH inventories (names through gCInventorySlot::m_entTemplate,
//   because the stack numbers change during the transfer). After each transfer, it reports each item of that direction, except gold. gold is the
//   gold of the other direction (the price, the total for all items of one direction). Measured 2026-11-24 (Marlo): for a trade with items in
//   both directions, the game offsets purchase and sale. Only one direction has gold, the other direction reports gold 0.
//
// Trade measurement, installed (Game.dll exports, thiscall):
//   gCInventory_PS::PerformTransaction(gCInventory_PS*, gEStackType, gCInventory_PS*), TransferTransactionItemsTo(gCInventory_PS*, gEStackType),
//   TransferItemsTo(gCInventory_PS*, int, int, gEStackType), TransferStackTo(gCInventory_PS*, int, gEStackType), TransferAllTo(gCInventory_PS*,
//   gEStackType), SetTransactionAmount(int, int); gCGUIManager::BeginTrade(eCEntity*)
// One line for each call (maximum 400): function, owner entities of the inventories, arguments, result, stack count before and after, call location.
#include <windows.h>
#include <intrin.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "sb_api.h"
#include "script_host.h"

namespace g3asm {
namespace {

ScriptHost* g_host = nullptr;
bool g_enabled = false;
int g_lines = 0;
const int kMaxLines = 600;

typedef const void*(__thiscall* EntNameFn)(const void*);
typedef void*(__thiscall* PsEntityFn)(void*);
typedef int(__thiscall* StackCountFn)(const void*);
typedef void*(__thiscall* GetStackFn)(void*, int);
typedef const int&(__thiscall* StackIntRefFn)(const void*);
typedef int(__thiscall* StackIntFn)(const void*);
EntNameFn g_entName = nullptr;
PsEntityFn g_psEntity = nullptr;
StackCountFn g_stackCount = nullptr;
GetStackFn g_getStack = nullptr;
StackIntRefFn g_amount = nullptr, g_transCount = nullptr;
StackIntFn g_gold = nullptr;
typedef void*(__thiscall* ProxyEntityFn)(void*);
ProxyEntityFn g_proxyEntity = nullptr;   // eCTemplateEntityProxy::GetEntity
unsigned g_resets = 0;                   // SetTransactionAmount(..., 0): the game resets all stacks after a trade, only counted

const char* SafeName(const void* e) {
    if (!e || !g_entName) return "?";
    __try {
        const void* n = g_entName(e);
        const char* t = n ? g_sb.strGetText(n) : nullptr;
        return t && *t ? t : "?";
    } __except (EXCEPTION_EXECUTE_HANDLER) { return "?"; }
}
void* SafeOwner(void* ps) {
    if (!ps || !g_psEntity) return nullptr;
    __try {
        return g_psEntity(ps);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
int SafeStackCount(void* inv) {
    if (!inv || !g_stackCount) return -1;
    __try {
        return g_stackCount(inv);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
// stack of inv at index: amount, transaction count, gold value of the stack (-1 if not readable)
bool SafeStackInfo(void* inv, int idx, int* amount, int* trans, int* gold) {
    *amount = *trans = *gold = -1;
    if (!inv || !g_getStack || idx < 0) return false;
    __try {
        void* st = g_getStack(inv, idx);
        if (!st) return false;
        if (g_amount) *amount = g_amount(st);
        if (g_transCount) *trans = g_transCount(st);
        if (g_gold) *gold = g_gold(st);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// template name of the stack at idx (gCInventorySlot::m_entTemplate at +0xC, SDK ge_inventoryslot.h)
const char* SafeStackName(void* inv, int idx) {
    if (!inv || !g_getStack || !g_proxyEntity || idx < 0) return "?";
    __try {
        auto* st = static_cast<unsigned char*>(g_getStack(inv, idx));
        if (!st || !ReadableMem(st + 0xC, 12)) return "?";
        void* tpl = g_proxyEntity(st + 0xC);
        return SafeName(tpl);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return "?"; }
}
// all stacks of inv with a transaction amount > 0: "Name xMenge (Goldwert), ..."
std::string Pending(void* inv) {
    std::string r;
    const int n = SafeStackCount(inv);
    for (int i = 0; i < n && i < 512; ++i) {
        int am, tr, gd;
        if (!SafeStackInfo(inv, i, &am, &tr, &gd) || tr <= 0) continue;
        char b[160];
        std::snprintf(b, sizeof b, "%s%.60s x%d (Stapel %d, Goldwert %d)", r.empty() ? "" : ", ", SafeStackName(inv, i), tr, i, gd);
        r += b;
    }
    return r.empty() ? "nichts" : r;
}
std::string Site(void* ret) {
    HMODULE m = nullptr;
    char name[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCSTR>(ret), &m) && m) {
        GetModuleFileNameA(m, name, MAX_PATH);
        const char* base = std::strrchr(name, '\\');
        char b[MAX_PATH + 32];
        std::snprintf(b, sizeof b, "%s+0x%X", base ? base + 1 : name, unsigned(reinterpret_cast<uintptr_t>(ret) - reinterpret_cast<uintptr_t>(m)));
        return b;
    }
    return "?";
}
std::string Inv(void* inv) {
    char b[160];
    std::snprintf(b, sizeof b, "%.60s[%d]", SafeName(SafeOwner(inv)), SafeStackCount(inv));
    return b;
}
void Line(const std::string& s) {
    if (!g_host || g_lines >= kMaxLines || OnForeignThread()) return;
    g_host->Log("Handel: " + s);
    if (++g_lines == kMaxLines) g_host->Log("Handel: Obergrenze erreicht, Messung endet");
}

// ---- OnTrade
void* g_partner = nullptr;   // trader of the last BeginTrade (there is no export for the end of the trade page)
bool g_ownerNoted = false;

struct TradeItem {
    std::string name;
    int amount;
};
struct TradeSide {
    void* inv = nullptr;
    std::vector<TradeItem> items;   // without gold
    int gold = 0;                   // selected gold
    bool done = false;              // its transfer came
};
struct PendingTrade {
    TradeSide side[2];   // [0] = inventory of the first transfer (self), [1] = the other inventory
    DWORD tick = 0;
    bool valid = false;
} g_trade;

// selected stacks (transaction amount > 0) of inv. Looting gives -1 and is not counted.
TradeSide ReadSide(void* inv) {
    TradeSide t;
    t.inv = inv;
    const int n = SafeStackCount(inv);
    for (int i = 0; i < n && i < 512; ++i) {
        int am, tr, gd;
        if (!SafeStackInfo(inv, i, &am, &tr, &gd) || tr <= 0) continue;
        const char* nm = SafeStackName(inv, i);
        if (_stricmp(nm, "It_Gold") == 0) t.gold += tr;
        else t.items.push_back({nm, tr});
    }
    return t;
}
TradeSide* SideOf(void* inv) {
    for (TradeSide& t : g_trade.side)
        if (t.inv == inv) return &t;
    return nullptr;
}
// before the original: is it a trade? Reads the selected stacks of both sides at the first transfer. *trader = the trader entity
bool TradeBefore(void* self, void* other, void** trader) {
    if (!g_partner || !g_host || !self || !other || OnForeignThread()) return false;
    void* oSelf = SafeOwner(self);
    void* oOther = SafeOwner(other);
    const bool heroSelf = IsPlayerEntity(oSelf), heroOther = IsPlayerEntity(oOther);
    if (heroSelf == heroOther) return false;   // the hero must be on exactly one side
    void* owner = heroSelf ? oOther : oSelf;
    if (owner != g_partner && !g_ownerNoted) {   // trade inventory with a different owner entity: report one time
        g_ownerNoted = true;
        g_host->Log(std::string("OnTrade: Besitzer des Handelsinventars ") + SafeName(owner) + " ist nicht der Haendler aus BeginTrade " +
                    SafeName(g_partner) + " - gemeldet wird der Haendler aus BeginTrade");
    }
    *trader = g_partner;
    // same trade: the second transfer of the pair within 2 s; otherwise read both sides again
    TradeSide* s = g_trade.valid && GetTickCount() - g_trade.tick < 2000 ? SideOf(self) : nullptr;
    if (!s || s->done || !SideOf(other)) {
        g_trade.side[0] = ReadSide(self);
        g_trade.side[1] = ReadSide(other);
        g_trade.tick = GetTickCount();
        g_trade.valid = !g_trade.side[0].items.empty() || !g_trade.side[1].items.empty() || g_trade.side[0].gold > 0 || g_trade.side[1].gold > 0;
    }
    return g_trade.valid;
}
// after a successful transfer self -> other: OnTrade for each item of this direction
void TradeAfter(void* self, void* other, void* trader) {
    TradeSide* s = SideOf(self);
    TradeSide* o = SideOf(other);
    if (!s || !o || s->done) return;
    s->done = true;
    const bool bought = IsPlayerEntity(SafeOwner(other));   // the items go to the hero
    const std::vector<TradeItem> items = s->items;          // copy: a mod can start the next trade from the event
    const int gold = o->gold;
    if (g_trade.side[0].done && g_trade.side[1].done) g_trade.valid = false;
    for (const TradeItem& it : items) {
        SStr name(it.name.c_str());
        EvArg a[] = {EvArg::Ent(trader), EvArg::Str(&name), EvArg::U(unsigned(it.amount)), EvArg::U(unsigned(gold)), EvArg::B(bought)};
        g_host->CallEvent("void OnTrade(Entity, const bCString &in, int, int, bool)", a, 5);
    }
}

typedef bool(__thiscall* PerformFn)(void*, void*, int, void*);
typedef bool(__thiscall* InvTypeFn)(void*, void*, int);
typedef bool(__thiscall* ItemsFn)(void*, void*, int, int, int);
typedef bool(__thiscall* StackFn)(void*, void*, int, int);
typedef bool(__thiscall* SetTransFn)(void*, int, int);
typedef bool(__thiscall* BeginTradeFn)(void*, void*);
PerformFn o_perform = nullptr;
InvTypeFn o_transTrans = nullptr, o_all = nullptr;
ItemsFn o_items = nullptr;
StackFn o_stack = nullptr;
SetTransFn o_setTrans = nullptr;
BeginTradeFn o_begin = nullptr;

bool __fastcall HPerform(void* self, void*, void* other, int type, void* third) {
    void* ret = _ReturnAddress();
    const std::string before = Inv(self) + " <-> " + Inv(other) + " / " + Inv(third);
    const bool r = o_perform(self, other, type, third);
    Line("PerformTransaction " + before + " Typ " + std::to_string(type) + " = " + (r ? "true" : "false") + ", danach " + Inv(self) + " " + Inv(other) +
         " (" + Site(ret) + ")");
    return r;
}
bool __fastcall HTransTrans(void* self, void*, void* other, int type) {
    void* trader = nullptr;
    const bool trade = TradeBefore(self, other, &trader);
    if (!g_enabled) {
        const bool r = o_transTrans(self, other, type);
        if (trade && r) TradeAfter(self, other, trader);
        return r;
    }
    void* ret = _ReturnAddress();
    const std::string before = Inv(self) + " -> " + Inv(other) + ": " + Pending(self);
    const bool r = o_transTrans(self, other, type);
    if (trade && r) TradeAfter(self, other, trader);
    Line("TransferTransactionItemsTo " + before + " | Typ " + std::to_string(type) + " = " + (r ? "true" : "false") + ", danach " + Inv(self) + " " +
         Inv(other) + " (" + Site(ret) + ")");
    return r;
}
bool __fastcall HAll(void* self, void*, void* other, int type) {
    void* ret = _ReturnAddress();
    const std::string before = Inv(self) + " -> " + Inv(other);
    const bool r = o_all(self, other, type);
    Line("TransferAllTo " + before + " Typ " + std::to_string(type) + " = " + (r ? "true" : "false") + " (" + Site(ret) + ")");
    return r;
}
bool __fastcall HItems(void* self, void*, void* other, int a, int b, int type) {
    void* ret = _ReturnAddress();
    int am, tr, gd;
    SafeStackInfo(self, a, &am, &tr, &gd);
    char info[200];
    std::snprintf(info, sizeof info, " Arg %d, %d (Stapel %d %.40s: Menge %d, Transaktion %d, Goldwert %d)", a, b, a, SafeStackName(self, a), am, tr, gd);
    const std::string before = Inv(self) + " -> " + Inv(other);
    const bool r = o_items(self, other, a, b, type);
    Line("TransferItemsTo " + before + info + " Typ " + std::to_string(type) + " = " + (r ? "true" : "false") + ", danach " + Inv(self) + " " +
         Inv(other) + " (" + Site(ret) + ")");
    return r;
}
bool __fastcall HStack(void* self, void*, void* other, int idx, int type) {
    void* ret = _ReturnAddress();
    int am, tr, gd;
    SafeStackInfo(self, idx, &am, &tr, &gd);
    char info[200];
    std::snprintf(info, sizeof info, " Stapel %d (Menge %d, Transaktion %d, Goldwert %d)", idx, am, tr, gd);
    const std::string before = Inv(self) + " -> " + Inv(other);
    const bool r = o_stack(self, other, idx, type);
    Line("TransferStackTo " + before + info + " Typ " + std::to_string(type) + " = " + (r ? "true" : "false") + " (" + Site(ret) + ")");
    return r;
}
bool __fastcall HSetTrans(void* self, void*, int idx, int amount) {
    void* ret = _ReturnAddress();
    const bool r = o_setTrans(self, idx, amount);
    if (amount == 0) {   // reset after a trade or when the page closes: one stack after the other, only count
        ++g_resets;
        return r;
    }
    int am, tr, gd;
    SafeStackInfo(self, idx, &am, &tr, &gd);
    char b[400];
    std::snprintf(b, sizeof b, "SetTransactionAmount %s Stapel %d %.60s Menge %d = %s (Stapel: Menge %d, Transaktion %d, Goldwert %d; %u Ruecksetzungen bisher) (%s)",
                  Inv(self).c_str(), idx, SafeStackName(self, idx), amount, r ? "true" : "false", am, tr, gd, g_resets, Site(ret).c_str());
    Line(b);
    return r;
}
bool __fastcall HBegin(void* gui, void*, void* ent) {
    const bool r = o_begin(gui, ent);
    if (r && ent) {
        g_partner = ent;
        g_trade.valid = false;
    }
    Line(std::string("BeginTrade mit ") + SafeName(ent) + " = " + (r ? "true" : "false"));
    return r;
}

}   // namespace

void SetTradeLog(bool enabled) { g_enabled = enabled; }

int InstallTradeHooks(ScriptHost& host) {
    if (g_host) return 0;
    g_host = &host;
    HMODULE eng = GetModuleHandleA("Engine.dll"), game = GetModuleHandleA("Game.dll");
    if (!eng || !game) return 0;
    g_entName = reinterpret_cast<EntNameFn>(GetProcAddress(eng, "?GetName@eCEntity@@QBEABVbCString@@XZ"));
    g_psEntity = reinterpret_cast<PsEntityFn>(GetProcAddress(eng, "?GetEntity@eCEntityPropertySet@@UAEPAVeCEntity@@XZ"));
    g_stackCount = reinterpret_cast<StackCountFn>(GetProcAddress(game, "?GetStackCount@gCInventory_PS@@QBEHXZ"));
    g_getStack = reinterpret_cast<GetStackFn>(GetProcAddress(game, "?GetStack@gCInventory_PS@@QAEPAVgCInventoryStack@@H@Z"));
    g_amount = reinterpret_cast<StackIntRefFn>(GetProcAddress(game, "?GetAmount@gCInventoryStack@@QBEABHXZ"));
    g_transCount = reinterpret_cast<StackIntRefFn>(GetProcAddress(game, "?GetTransactionCount@gCInventoryStack@@QBEABHXZ"));
    g_gold = reinterpret_cast<StackIntFn>(GetProcAddress(game, "?GetStackGoldValue@gCInventoryStack@@QBEHXZ"));
    g_proxyEntity = reinterpret_cast<ProxyEntityFn>(GetProcAddress(eng, "?GetEntity@eCTemplateEntityProxy@@QAEPAVeCTemplateEntity@@XZ"));
    // OnTrade: always
    int ev = 0;
    ev += HookExport(game, "?TransferTransactionItemsTo@gCInventory_PS@@IAE_NPAV1@W4gEStackType@@@Z", reinterpret_cast<void*>(&HTransTrans),
                     reinterpret_cast<void**>(&o_transTrans), host);
    ev += HookExport(game, "?BeginTrade@gCGUIManager@@QAE_NPAVeCEntity@@@Z", reinterpret_cast<void*>(&HBegin), reinterpret_cast<void**>(&o_begin), host);
    const bool read = g_entName && g_psEntity && g_stackCount && g_getStack && g_transCount && g_proxyEntity;
    host.Log("Handel (OnTrade): " + std::to_string(ev) + " von 2 Hooks aktiv" + (read ? "" : ", Lesefunktionen fehlen"));
    if (!g_enabled) return ev;
    int n = 0;
    n += HookExport(game, "?PerformTransaction@gCInventory_PS@@QAE_NPAV1@W4gEStackType@@1@Z", reinterpret_cast<void*>(&HPerform),
                    reinterpret_cast<void**>(&o_perform), host);
    n += HookExport(game, "?TransferItemsTo@gCInventory_PS@@QAE_NPAV1@HHW4gEStackType@@@Z", reinterpret_cast<void*>(&HItems),
                    reinterpret_cast<void**>(&o_items), host);
    n += HookExport(game, "?TransferStackTo@gCInventory_PS@@QAE_NPAV1@HW4gEStackType@@@Z", reinterpret_cast<void*>(&HStack),
                    reinterpret_cast<void**>(&o_stack), host);
    n += HookExport(game, "?TransferAllTo@gCInventory_PS@@QAE_NPAV1@W4gEStackType@@@Z", reinterpret_cast<void*>(&HAll), reinterpret_cast<void**>(&o_all),
                    host);
    n += HookExport(game, "?SetTransactionAmount@gCInventory_PS@@QAE_NHH@Z", reinterpret_cast<void*>(&HSetTrans), reinterpret_cast<void**>(&o_setTrans),
                    host);
    host.Log("Handel-Messung: " + std::to_string(n + ev) + " von 7 Hooks aktiv");
    return ev + n;
}

}   // namespace g3asm
