// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
//
// Keep the global variables of a mod through a hot reload (from version 47), with the AngelScript add-on serializer.
// The add-on itself keeps numbers, POD types (Entity, bCVector, ...), script classes and handles to them. This file adds the user types
// bCString, string, array and dictionary. Not kept: const globals (the new value in the code applies) and function handles (they would
// point into the old module); in a dictionary also handles and script objects.
#include <angelscript.h>
#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "sb_api.h"
#include "script_host.h"
#include "scriptarray/scriptarray.h"
#include "scriptdictionary/scriptdictionary.h"
#include "serializer/serializer.h"

namespace g3asm {
namespace {

struct BStringType : public CUserType {   // bCString: keep the text, make a new bCString when restoring
    void* AllocateUnitializedMemory(CSerializedValue*) override { return nullptr; }   // value type: never a handle
    void Store(CSerializedValue* val, void* ptr) override { val->SetUserData(new std::string(static_cast<SStr*>(ptr)->c_str())); }
    void Restore(CSerializedValue* val, void* ptr) override {
        if (auto* s = static_cast<std::string*>(val->GetUserData())) *static_cast<SStr*>(ptr) = SStr(*s);
    }
    void CleanupUserData(CSerializedValue* val) override { delete static_cast<std::string*>(val->GetUserData()); }
};

struct StdStringType : public CUserType {
    void* AllocateUnitializedMemory(CSerializedValue*) override { return nullptr; }
    void Store(CSerializedValue* val, void* ptr) override { val->SetUserData(new std::string(*static_cast<std::string*>(ptr))); }
    void Restore(CSerializedValue* val, void* ptr) override {
        if (auto* s = static_cast<std::string*>(val->GetUserData())) *static_cast<std::string*>(ptr) = *s;
    }
    void CleanupUserData(CSerializedValue* val) override { delete static_cast<std::string*>(val->GetUserData()); }
};

struct ArrayType : public CUserType {   // as in the documentation of the add-on
    void* AllocateUnitializedMemory(CSerializedValue* val) override { return CScriptArray::Create(val->GetType()); }
    void Store(CSerializedValue* val, void* ptr) override {
        auto* arr = static_cast<CScriptArray*>(ptr);
        for (asUINT i = 0; i < arr->GetSize(); ++i) val->m_children.push_back(new CSerializedValue(val, "", "", arr->At(i), arr->GetElementTypeId()));
    }
    void Restore(CSerializedValue* val, void* ptr) override {
        auto* arr = static_cast<CScriptArray*>(ptr);
        arr->Resize(asUINT(val->m_children.size()));
        for (size_t i = 0; i < val->m_children.size(); ++i) val->m_children[i]->Restore(arr->At(asUINT(i)), arr->GetElementTypeId());
    }
};

// dictionary: one child for each entry with a primitive or an application value type (bCString, string, Entity, bCVector, ...).
// The key and the type ID are in the user data, because CSerializedValue does not give them out.
struct DictEntries {
    std::vector<std::string> keys;
    std::vector<int> typeIds;
};
struct DictionaryType : public CUserType {
    void* AllocateUnitializedMemory(CSerializedValue* val) override {
        asITypeInfo* t = val->GetType();
        return t ? CScriptDictionary::Create(t->GetEngine()) : nullptr;
    }
    void Store(CSerializedValue* val, void* ptr) override {
        auto* dict = static_cast<CScriptDictionary*>(ptr);
        auto* e = new DictEntries;
        for (CScriptDictionary::CIterator it = dict->begin(); it != dict->end(); ++it) {
            const int tid = it.GetTypeId();
            if (tid & (asTYPEID_OBJHANDLE | asTYPEID_SCRIPTOBJECT)) continue;   // not kept (see the start of the file)
            e->keys.push_back(it.GetKey());
            e->typeIds.push_back(tid);
            val->m_children.push_back(new CSerializedValue(val, it.GetKey(), "", const_cast<void*>(it.GetAddressOfValue()), tid));
        }
        val->SetUserData(e);
    }
    void Restore(CSerializedValue* val, void* ptr) override {
        auto* dict = static_cast<CScriptDictionary*>(ptr);
        auto* e = static_cast<DictEntries*>(val->GetUserData());
        asITypeInfo* dictType = val->GetType();
        if (!e || !dict || !dictType) return;
        asIScriptEngine* engine = dictType->GetEngine();
        for (size_t i = 0; i < e->keys.size() && i < val->m_children.size(); ++i) {
            const int tid = e->typeIds[i];
            if (!(tid & asTYPEID_MASK_OBJECT)) {   // bool, numbers, enums: 8 bytes are enough
                asQWORD buf = 0;
                val->m_children[i]->Restore(&buf, tid);
                dict->Set(e->keys[i], &buf, tid);
                continue;
            }
            asITypeInfo* t = engine->GetTypeInfoById(tid);
            if (!t) continue;
            void* obj = engine->CreateScriptObject(t);   // default value, then the stored content
            if (!obj) continue;
            val->m_children[i]->Restore(obj, tid);
            dict->Set(e->keys[i], obj, tid);   // the dictionary keeps its own copy
            engine->ReleaseScriptObject(obj, t);
        }
    }
    void CleanupUserData(CSerializedValue* val) override { delete static_cast<DictEntries*>(val->GetUserData()); }
};

// CSerializer with access to the stored root: remove the values of const globals before the restore
class Keeper : public CSerializer {
public:
    Keeper() {
        AddUserType(new BStringType, "bCString");   // the serializer deletes the user types
        AddUserType(new StdStringType, "string");
        AddUserType(new ArrayType, "array");
        AddUserType(new DictionaryType, "dictionary");
    }
    void DropConsts(asIScriptModule* mod) {
        for (asUINT i = 0; i < mod->GetGlobalVarCount(); ++i) {
            const char *name = nullptr, *ns = nullptr;
            bool isConst = false;
            mod->GetGlobalVar(i, &name, &ns, nullptr, &isConst);
            if (!isConst) continue;
            CSerializedValue* v = m_root.FindByName(name, ns ? ns : "");
            if (!v) continue;
            for (size_t c = 0; c < m_root.m_children.size(); ++c)
                if (m_root.m_children[c] == v) {
                    m_root.m_children.erase(m_root.m_children.begin() + static_cast<std::ptrdiff_t>(c));
                    delete v;
                    break;
                }
        }
    }
    int CountKnown(asIScriptModule* mod) {   // globals of the new module with a stored value
        int n = 0;
        for (asUINT i = 0; i < mod->GetGlobalVarCount(); ++i) {
            const char *name = nullptr, *ns = nullptr;
            mod->GetGlobalVar(i, &name, &ns);
            if (m_root.FindByName(name, ns ? ns : "")) ++n;
        }
        return n;
    }
};

// set function handles in the globals to null: after the restore they could point into the discarded module
void ClearFunctionHandles(asIScriptModule* mod) {
    asIScriptEngine* engine = mod->GetEngine();
    for (asUINT i = 0; i < mod->GetGlobalVarCount(); ++i) {
        int tid = 0;
        mod->GetGlobalVar(i, nullptr, nullptr, &tid);
        if (!(tid & asTYPEID_OBJHANDLE)) continue;
        asITypeInfo* t = engine->GetTypeInfoById(tid);
        if (!t || !(t->GetFlags() & asOBJ_FUNCDEF)) continue;
        auto** p = static_cast<asIScriptFunction**>(mod->GetAddressOfGlobalVar(i));
        if (p && *p) {
            (*p)->Release();
            *p = nullptr;
        }
    }
}

// the serializer works on script memory: a crash there must not end the game (SEH needs a function without C++ objects to unwind)
struct StoreJob {
    Keeper* k;
    asIScriptModule* mod;
};
struct RestoreJob {
    Keeper* k;
    asIScriptModule* mod;
    int count;
};
void DoStore(void* p) {
    auto* j = static_cast<StoreJob*>(p);
    j->k->Store(j->mod);
}
void DoRestore(void* p) {
    auto* j = static_cast<RestoreJob*>(p);
    j->k->DropConsts(j->mod);
    j->count = j->k->CountKnown(j->mod);
    j->k->Restore(j->mod);
    ClearFunctionHandles(j->mod);
}
bool SehRun(void (*fn)(void*), void* arg) {
    __try {
        fn(arg);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

}   // namespace

SavedGlobals* SaveGlobals(asIScriptModule* mod) {
    if (!mod) return nullptr;
    auto* k = new Keeper;
    StoreJob j{k, mod};
    if (!SehRun(&DoStore, &j)) {
        // do not delete k: its state is unknown after the exception (a small leak is better than a second crash)
        return nullptr;
    }
    return reinterpret_cast<SavedGlobals*>(k);
}

int RestoreGlobals(SavedGlobals* saved, asIScriptModule* mod) {
    if (!saved || !mod) return -1;
    RestoreJob j{reinterpret_cast<Keeper*>(saved), mod, 0};
    return SehRun(&DoRestore, &j) ? j.count : -1;
}

void FreeSavedGlobals(SavedGlobals* saved) { delete reinterpret_cast<Keeper*>(saved); }

}   // namespace g3asm
