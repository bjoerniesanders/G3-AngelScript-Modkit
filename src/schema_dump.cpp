// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 OG3-Patch authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
// Schema-Dump der Eigenschaften (Skript-Schnittstelle ab Version 43, Roadmap 6.1): alle beim Aufruf registrierten Eigenschaftsklassen aus
// bCPropertyObjectSingleton (SharedBase.dll) mit Basisklasse, Beschreibung und allen Eigenschaften (Name, Typ, Kategorie, Beschreibung, Art,
// Nur-Lesen, Offset, Groesse, Enum-Werte) als JSON-Datei.
//   int DumpPropertySchema(const bCString &in file)  -> Zahl der Klassen, < 0 bei Fehler (-1 Exporte fehlen, -2 Dateiname ungueltig, -3 Datei)
// Die Datei liegt im Ordner og3 neben og3_patch.dll (im Spiel: <Spiel>\og3\<file>). Nur ein schlichter Dateiname ist erlaubt.
// Klassen registrieren sich beim Laden ihrer DLL. Ohne Spiel (Testprogramm mit Original-SharedBase und Game.dll, 2026-11-11): 273 Klassen,
// 1584 Eigenschaften, 1534 Enum-Werte, 110 ms, 258 KB. Ob die spaet geladenen Script_*.dll im Spiel weitere Klassen anmelden, ist nicht gemessen.
// Aufbau (SDK, gemessen): Iterator der Klassenliste = {bTPtrArray<bSNode*>* Faecher, int Fach, bSNode* Knoten}, bSNode = {bCString Name,
// bCPOTypeListItem* Eintrag, bSNode* Weiter}; bTPtrArray = {T* Daten, u32 Anzahl, u32 Kapazitaet}. Virtuelle Funktionen in der Reihenfolge des SDK:
// bCPropertyObjectTypeBase [0] GetClassName [1] GetBaseClassName; bCPropertyTypeBase [0] GetMemberOffset [1] GetDataSize [3] GetPropertyClassName
// [6] HasEnumValues [7] GetEnumValueCount [8] GetEnumValue; bCEnumWrapper [0] GetValueString [1] GetValue.
// Achtung: bCPOTypeListItem::GetBaseClassName und die Getter der Eigenschaftsliste (GetPropertyType(int) u. a.) blieben im Test haengen (vermutlich
// Assert-Dialog der Original-DLL); die Liste wird deshalb direkt gelesen (bCPropertyObjectTypeBase +8 Liste, +0xC Anzahl, +0x14 Bit 0 speicherbar).
// Jeder Engine-Aufruf ist mit SEH gekapselt; harte Obergrenzen gegen kaputte Listen (Klassen, Eigenschaften je Klasse, Enum-Werte je Eigenschaft).
#include <windows.h>

#include <angelscript.h>

#include <cstdio>
#include <string>

#include "sb_api.h"
#include "script_host.h"

namespace og3p {
namespace {

constexpr int kMaxClasses = 20000, kMaxProps = 4000, kMaxEnum = 2000, kMaxBuckets = 1 << 20;

typedef void*(__stdcall* GetInstanceFn)();
typedef void*(__thiscall* IterFn)(const void*, void*);           // Rueckgabe des Iterators ueber versteckten Zeiger
typedef const void*(__thiscall* PtrFn)(const void*);             // const bCString& / Zeiger
typedef int(__thiscall* IntFn)(const void*);
typedef bool(__thiscall* BoolFn)(const void*);
typedef const void*(__thiscall* IndexFn)(const void*, int);

struct SchemaApi {
    bool tried = false, ok = false;
    GetInstanceFn instance = nullptr;
    IterFn begin = nullptr;
    PtrFn itemType = nullptr, typeDesc = nullptr;
    PtrFn propName = nullptr, propCat = nullptr, propDesc = nullptr;
    IntFn propKind = nullptr;
    BoolFn propRO = nullptr;
} g_api;

bool Resolve() {
    if (g_api.tried) return g_api.ok;
    HMODULE sb = GetModuleHandleA("SharedBase.dll");
    if (!sb) return false;
    g_api.tried = true;
    auto get = [sb](const char* n) { return GetProcAddress(sb, n); };
    g_api.instance = reinterpret_cast<GetInstanceFn>(get("?GetInstance@bCPropertyObjectSingleton@@SGAAV1@XZ"));
    g_api.begin = reinterpret_cast<IterFn>(
        get("?BeginIterator@bCPropertyObjectSingleton@@QBE?AVbCConstIterator@?$bTPtrMap@VbCString@@PAVbCPOTypeListItem@bCPropertyObjectSingleton@@@@XZ"));
    g_api.itemType = reinterpret_cast<PtrFn>(get("?GetObjectType@bCPOTypeListItem@bCPropertyObjectSingleton@@QBEPBVbCPropertyObjectTypeBase@@XZ"));
    g_api.typeDesc = reinterpret_cast<PtrFn>(get("?GetDescription@bCPropertyObjectTypeBase@@QBEABVbCString@@XZ"));
    g_api.propName = reinterpret_cast<PtrFn>(get("?GetPropertyName@bCPropertyTypeBase@@QBEABVbCString@@XZ"));
    g_api.propCat = reinterpret_cast<PtrFn>(get("?GetPropertyCategory@bCPropertyTypeBase@@QBEABVbCString@@XZ"));
    g_api.propDesc = reinterpret_cast<PtrFn>(get("?GetPropertyDescription@bCPropertyTypeBase@@QBEABVbCString@@XZ"));
    g_api.propKind = reinterpret_cast<IntFn>(get("?GetPropertyType@bCPropertyTypeBase@@QBE?AW4bEPropertyType@@XZ"));
    g_api.propRO = reinterpret_cast<BoolFn>(get("?IsReadOnly@bCPropertyTypeBase@@QBE_NXZ"));
    g_api.ok = g_api.instance && g_api.begin && g_api.itemType && g_api.typeDesc && g_api.propName && g_api.propCat && g_api.propDesc && g_api.propKind && g_api.propRO;
    return g_api.ok;
}

// ---- kleine SEH-Huellen (keine C++-Objekte mit Destruktor)
const char* SafeTextOf(PtrFn fn, const void* self) {   // Funktion liefert const bCString&
    if (!self) return nullptr;
    __try {
        const void* s = fn(self);
        return s ? g_sb.strGetText(s) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
const void* SafePtr(PtrFn fn, const void* self) {
    if (!self) return nullptr;
    __try {
        return fn(self);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
int SafeInt(IntFn fn, const void* self, int fallback) {
    if (!self) return fallback;
    __try {
        return fn(self);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return fallback; }
}
int SafeBool(BoolFn fn, const void* self) {   // 1/0, -1 = Ausnahme
    if (!self) return -1;
    __try {
        return fn(self) ? 1 : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
void* VFunc(const void* obj, int slot) {
    if (!obj || !ReadableMem(obj, 4)) return nullptr;
    void* const* vt = *reinterpret_cast<void* const* const*>(obj);
    if (!ReadableMem(vt + slot, 4)) return nullptr;
    return vt[slot];
}
const char* SafeVText(const void* obj, int slot) {
    PtrFn fn = reinterpret_cast<PtrFn>(VFunc(obj, slot));
    return fn ? SafeTextOf(fn, obj) : nullptr;
}
long long SafeVInt(const void* obj, int slot) {   // -1 = nicht lesbar
    IntFn fn = reinterpret_cast<IntFn>(VFunc(obj, slot));
    if (!fn) return -1;
    __try {
        return static_cast<unsigned>(fn(obj));
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
int SafeVBool(const void* obj, int slot) {
    BoolFn fn = reinterpret_cast<BoolFn>(VFunc(obj, slot));
    if (!fn) return -1;
    __try {
        return fn(obj) ? 1 : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
const void* SafeVIndex(const void* obj, int slot, int i) {
    IndexFn fn = reinterpret_cast<IndexFn>(VFunc(obj, slot));
    if (!fn) return nullptr;
    __try {
        return fn(obj, i);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
struct Iter {
    const unsigned* nodes;   // bTPtrArray<bSNode*>: Daten, Anzahl, Kapazitaet
    int hash;
    const unsigned* node;    // bSNode: Name, Eintrag, Weiter
};
bool SafeBegin(Iter* it) {
    __try {
        void* inst = g_api.instance();
        if (!inst) return false;
        g_api.begin(inst, it);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void Json(std::string& o, const char* s) {
    o += '"';
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s ? s : ""); *p; ++p) {
        if (*p == '"' || *p == '\\') {
            o += '\\';
            o += char(*p);
        } else if (*p < 0x20) {
            char b[8];
            std::snprintf(b, sizeof b, "\\u%04x", *p);
            o += b;
        } else if (*p < 0x80) {
            o += char(*p);
        } else {   // Windows-1252/Latin-1 -> UTF-8 (Umlaute in Beschreibungen)
            o += char(0xC0 | (*p >> 6));
            o += char(0x80 | (*p & 0x3F));
        }
    }
    o += '"';
}

std::string OutputDir() {
    char path[MAX_PATH] = "";
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(&OutputDir),
                       &self);
    GetModuleFileNameA(self, path, MAX_PATH);
    std::string d(path);
    d = d.substr(0, d.find_last_of("\\/") + 1) + "og3\\";
    CreateDirectoryA(d.c_str(), nullptr);
    return d;
}

void DumpProperty(std::string& o, const void* prop, int& enumTotal) {
    o += "{\"name\":";
    Json(o, SafeTextOf(g_api.propName, prop));
    o += ",\"type\":";
    Json(o, SafeVText(prop, 3));
    const char* cat = SafeTextOf(g_api.propCat, prop);
    const char* desc = SafeTextOf(g_api.propDesc, prop);
    if (cat && *cat) o += ",\"category\":", Json(o, cat);
    if (desc && *desc) o += ",\"description\":", Json(o, desc);
    char b[96];
    std::snprintf(b, sizeof b, ",\"kind\":%d,\"readOnly\":%s,\"offset\":%lld,\"size\":%lld", SafeInt(g_api.propKind, prop, -1),
                  SafeBool(g_api.propRO, prop) == 1 ? "true" : "false", SafeVInt(prop, 0), SafeVInt(prop, 1));
    o += b;
    if (SafeVBool(prop, 6) == 1) {
        long long n = SafeVInt(prop, 7);
        if (n > kMaxEnum) n = kMaxEnum;
        o += ",\"enum\":[";
        for (int i = 0; i < n; ++i) {
            const void* w = SafeVIndex(prop, 8, i);
            if (i) o += ',';
            o += "{\"name\":";
            Json(o, SafeVText(w, 0));
            std::snprintf(b, sizeof b, ",\"value\":%lld}", w ? SafeVInt(w, 1) : -1);
            o += b;
            ++enumTotal;
        }
        o += ']';
    }
    o += '}';
}

int DumpPropertySchemaImpl(const std::string& file, std::string& report) {
    if (file.empty() || file.find_first_of("\\/:") != std::string::npos || file.find("..") != std::string::npos) return -2;
    if (!Resolve()) return -1;
    const DWORD t0 = GetTickCount();
    Iter it{};
    if (!SafeBegin(&it) || !ReadableMem(it.nodes, 12)) return -1;
    const unsigned* buckets = reinterpret_cast<const unsigned*>(static_cast<uintptr_t>(it.nodes[0]));
    int nBuckets = static_cast<int>(it.nodes[1]);
    if (nBuckets < 0 || nBuckets > kMaxBuckets || !ReadableMem(buckets, size_t(nBuckets) * 4)) return -1;
    std::string o;
    o.reserve(4 << 20);
    o += "{\"format\":\"og3-property-schema\",\"version\":1,\"classes\":[\n";
    int classes = 0, props = 0, enums = 0, broken = 0;
    for (int h = 0; h < nBuckets && classes < kMaxClasses; ++h) {
        const unsigned* node = reinterpret_cast<const unsigned*>(static_cast<uintptr_t>(buckets[h]));
        for (int chain = 0; node && chain < kMaxClasses && classes < kMaxClasses; ++chain) {
            if (!ReadableMem(node, 12)) {
                ++broken;
                break;
            }
            const void* item = reinterpret_cast<const void*>(static_cast<uintptr_t>(node[1]));
            const void* type = SafePtr(g_api.itemType, item);
            const char* key = SafeStrText(node, "");
            const char* name = type ? SafeVText(type, 0) : nullptr;
            if (classes) o += ",\n";
            o += "{\"name\":";
            Json(o, name ? name : key);
            o += ",\"base\":";
            Json(o, type ? SafeVText(type, 1) : nullptr);   // bCPOTypeListItem::GetBaseClassName haengt (im Test gemessen), daher virtuell am Typ
            if (type) {
                const char* desc = SafeTextOf(g_api.typeDesc, type);
                if (desc && *desc) o += ",\"description\":", Json(o, desc);
                // Felder direkt (SDK-Layout, 0x18 Byte): +8 bCPropertyTypeBase** Liste, +0xC Anzahl, +0x14 Bit 0 = speicherbar.
                // Die Getter der Original-DLL pruefen mit Assert-Dialogen (im Test haengengeblieben), daher hier nur lesen.
                const unsigned* tb = static_cast<const unsigned*>(type);
                const bool readable = ReadableMem(tb, 0x18);
                o += readable && (reinterpret_cast<const unsigned char*>(tb)[0x14] & 1) ? ",\"persistable\":true" : ",\"persistable\":false";
                int n = readable ? static_cast<int>(tb[3]) : 0;
                const unsigned* list = readable ? reinterpret_cast<const unsigned*>(static_cast<uintptr_t>(tb[2])) : nullptr;
                if (n < 0 || n > kMaxProps) n = n < 0 ? 0 : kMaxProps;
                if (n && !ReadableMem(list, size_t(n) * 4)) n = 0, ++broken;
                o += ",\"properties\":[";
                int written = 0;
                for (int i = 0; i < n; ++i) {
                    const void* p = reinterpret_cast<const void*>(static_cast<uintptr_t>(list[i]));
                    if (!p || !ReadableMem(p, 4)) {
                        ++broken;
                        continue;
                    }
                    if (written++) o += ',';
                    DumpProperty(o, p, enums);
                    ++props;
                }
                o += ']';
            } else {
                ++broken;
            }
            o += '}';
            ++classes;
            node = reinterpret_cast<const unsigned*>(static_cast<uintptr_t>(node[2]));
        }
    }
    o += "\n]}\n";
    const std::string path = OutputDir() + file;
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return -3;
    std::fwrite(o.data(), 1, o.size(), f);
    std::fclose(f);
    char b[400];
    std::snprintf(b, sizeof b, "Eigenschafts-Schema: %d Klassen, %d Eigenschaften, %d Enum-Werte, %d unlesbar, %lu ms, %u KB -> %s", classes, props,
                  enums, broken, GetTickCount() - t0, unsigned(o.size() / 1024), path.c_str());
    report = b;
    return classes;
}

int DumpPropertySchemaScript(const SStr& file) {
    std::string report;
    const int r = DumpPropertySchemaImpl(file.c_str(), report);
    if (ScriptHost::s_current) {
        if (r >= 0)
            ScriptHost::s_current->Log(report);
        else
            ScriptHost::s_current->Log("Eigenschafts-Schema: fehlgeschlagen (Code " + std::to_string(r) + ")");
    }
    return r;
}

}   // namespace

bool RegisterSchemaBindings(asIScriptEngine* e) {
    bool ok = true;
    OG3_REG(e->RegisterGlobalFunction("int DumpPropertySchema(const bCString &in file)", asFUNCTION(DumpPropertySchemaScript), asCALL_CDECL));
    return ok;
}

}   // namespace og3p
