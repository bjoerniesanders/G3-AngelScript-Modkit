// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Additional permissions: LICENSE-EXCEPTIONS.md
// Trade measurement (only log, no script interface; ini [g3asm] tradelog=1, default off). Goal: find out which functions move the items
// when the player buys or sells on the trade page. OnReceiveItem (gCQuestManager_PS::OnReceiveItem) reports items from dialogs, but not
// trade (measured 2026-11-19: purchase from Gelford, 2000 gold, no event). The CP manual for modders says that NPCs have a separate trade
// inventory. Installed (Game.dll exports, thiscall):
//   gCInventory_PS::PerformTransaction(gCInventory_PS*, gEStackType, gCInventory_PS*), TransferTransactionItemsTo(gCInventory_PS*, gEStackType),
//   TransferItemsTo(gCInventory_PS*, int, int, gEStackType), TransferStackTo(gCInventory_PS*, int, gEStackType), TransferAllTo(gCInventory_PS*,
//   gEStackType), SetTransactionAmount(int, int); gCGUIManager::BeginTrade(eCEntity*)
// One line for each call (maximum 400): function, owner entities of the inventories, arguments, result, stack count before and after, call location.
#include <windows.h>
#include <intrin.h>

#include <cstdio>
#include <cstring>
#include <string>

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
    void* ret = _ReturnAddress();
    const std::string before = Inv(self) + " -> " + Inv(other) + ": " + Pending(self);
    const bool r = o_transTrans(self, other, type);
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
    Line(std::string("BeginTrade mit ") + SafeName(ent) + " = " + (r ? "true" : "false"));
    return r;
}

}   // namespace

void SetTradeLog(bool enabled) { g_enabled = enabled; }

int InstallTradeTrace(ScriptHost& host) {
    if (!g_enabled || g_host) return 0;
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
    int n = 0;
    n += HookExport(game, "?PerformTransaction@gCInventory_PS@@QAE_NPAV1@W4gEStackType@@1@Z", reinterpret_cast<void*>(&HPerform),
                    reinterpret_cast<void**>(&o_perform), host);
    n += HookExport(game, "?TransferTransactionItemsTo@gCInventory_PS@@IAE_NPAV1@W4gEStackType@@@Z", reinterpret_cast<void*>(&HTransTrans),
                    reinterpret_cast<void**>(&o_transTrans), host);
    n += HookExport(game, "?TransferItemsTo@gCInventory_PS@@QAE_NPAV1@HHW4gEStackType@@@Z", reinterpret_cast<void*>(&HItems),
                    reinterpret_cast<void**>(&o_items), host);
    n += HookExport(game, "?TransferStackTo@gCInventory_PS@@QAE_NPAV1@HW4gEStackType@@@Z", reinterpret_cast<void*>(&HStack),
                    reinterpret_cast<void**>(&o_stack), host);
    n += HookExport(game, "?TransferAllTo@gCInventory_PS@@QAE_NPAV1@W4gEStackType@@@Z", reinterpret_cast<void*>(&HAll), reinterpret_cast<void**>(&o_all),
                    host);
    n += HookExport(game, "?SetTransactionAmount@gCInventory_PS@@QAE_NHH@Z", reinterpret_cast<void*>(&HSetTrans), reinterpret_cast<void**>(&o_setTrans),
                    host);
    n += HookExport(game, "?BeginTrade@gCGUIManager@@QAE_NPAVeCEntity@@@Z", reinterpret_cast<void*>(&HBegin), reinterpret_cast<void**>(&o_begin), host);
    host.Log("Handel-Messung: " + std::to_string(n) + " von 7 Hooks aktiv");
    return n;
}

}   // namespace g3asm
