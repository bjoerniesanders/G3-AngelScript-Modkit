// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
#pragma once
// AngelScript-Host von G3ASM: Engine, Bindings der Genome-Grundtypen (ueber die geladene SharedBase), Laden der Mod-Skripte,
// Ereignisse und Overrides fuer Skripte, Neuladen bei Dateiaenderung.
//
// Die Skript-Schnittstelle (Typen, Funktionen, Ereignisse, Override-Hooks, Version G3ASM_SCRIPT_API_VERSION) ist in docs/modkit/
// (api-reference.md, events.md, hooks.md) beschrieben; hier stehen nur die C++-Schnittstellen.
#include <functional>
#include <unordered_map>
#include <string>
#include <vector>

#define G3ASM_SCRIPT_API_VERSION 46

class asIScriptEngine;
class asIScriptContext;
class asIScriptModule;
class asIScriptFunction;
struct HWND__;
struct HINSTANCE__;

namespace g3asm {

// Argument eines Spiel-Ereignisses fuer Skripte (siehe ScriptHost::CallEvent)
struct EvArg {
    enum Kind { Entity, Text, UInt } kind;
    const void* ptr;   // Entity: eCEntity*; Text: Zeiger auf ein bCString-Objekt
    unsigned value;    // UInt
    static EvArg Ent(const void* e) { return {Entity, e, 0}; }
    static EvArg Str(const void* s) { return {Text, s, 0}; }
    static EvArg U(unsigned v) { return {UInt, nullptr, v}; }
};

void SetPrintToGameLog(bool on);   // ini [g3asm] printgame
void SetLoadDefaultOffMods(bool on);   // auch Mods mit mod.ini enabled=0 laden (Testprogramm)

class ScriptHost {
public:
    using LogFn = std::function<void(const std::string&)>;
    bool Init(LogFn log);
    void Shutdown();
    // Einzelnes Skript als Mod "main" laden und alle anderen Mods verwerfen (Tests); false bei Uebersetzungsfehlern
    bool Load(const std::string& path);
    // Mod hinzufuegen (bzw. neu laden, wenn der Name schon existiert); false bei Uebersetzungsfehlern (Meldungen im Protokoll)
    bool AddScript(const std::string& name, const std::string& path);
    // Mod-Ordner: <dir>\<Name>\main.as je Mod, eigenes Modul je Mod. <dir>\load_order.txt (optional): ein Modname je Zeile in
    // Ladereihenfolge, '#' = Kommentar, '-Name' = Mod abgeschaltet; nicht genannte Mods folgen alphabetisch.
    void SetModsDir(const std::string& dir) { modsDir_ = dir; }
    int ScanMods();   // neue/entfernte Mods einarbeiten, Reihenfolge anwenden; Rueckgabe: Zahl neu geladener Mods
    bool Loaded() const;
    int ModCount() const { return int(mods_.size()); }
    void CallMain();   // main() aller Mods, die es noch nicht hatten
    void CallFrame(float dt);
    void CallKey(int vk, bool down);   // void OnKey(int vk, bool down), falls vorhanden; Wiederholungen werden nicht gemeldet
    // Spiel-Ereignis mit Argumenten an alle Mods, z. B. "void OnNPCKilled(Entity)" (Entity als Wert, Text als const bCString &in)
    void CallEvent(const char* decl, const EvArg* args, int n);
    void CallSimple(const char* decl);   // "void OnWorldActivate()" usw. in allen Mods, falls vorhanden
    // Override-Hooks (ab Version 17): ersetzt ein Mod eine Spielfunktion (Skriptfunktion "bool Override_<Name>(...)"), laeuft sie statt des Originals; ueber Super()
    // ruft der Mod das Original (bzw. den naechsten Mod der Kette) auf. HasHandler: definiert irgendein Mod diese Funktion?
    bool HasHandler(const char* decl);
    // true = ein Mod hat uebernommen: *result ist das Ergebnis fuer das Spiel, *originalCalled sagt, ob das Original gelaufen ist; false = niemand hat die Funktion
    // original(args): das Original der Spielfunktion; bekommt die Argumente der Kette (ein Mod kann sie mit SetOverrideArg veraendert haben)
    bool CallOverride(const char* decl, const EvArg* args, int n, std::function<bool(const EvArg*)> original, bool* result,
                      bool* originalCalled, EvArg* finalArgs = nullptr);   // finalArgs: Argumente nach der Kette (n Stueck)
    // Argument der gerade laufenden Override-Kette setzen (nur Zahlen: Kind UInt); Super() reicht den neuen Wert an den naechsten Mod und das Original weiter
    bool SetOverrideArg(int index, unsigned value);
    bool GetOverrideArg(int index, unsigned* value) const;
    bool ChainSuper();   // fuer die Skriptfunktion Super()
    // Neuladen geaenderter Mods und Mod-Ordner pruefen (hoechstens einmal je Sekunde); true = etwas neu geladen
    bool CheckReload();
    int Failures() const { return failures_; }
    int Checks() const { return checks_; }
    void Log(const std::string& s) {
        if (log_) log_(s);
    }
    const char* CurrentMod() const { return curMod_.c_str(); }
    // Mod-Ordner (leer im Einzelskript-Betrieb) und Ordner eines geladenen Mods (Verzeichnis seiner main.as; leer, wenn unbekannt)
    const std::string& ModsDir() const { return modsDir_; }
    std::string ModDir(const std::string& mod) const;
    // Zwischen Mods (ab Version 44): "bCString <func>(const bCString &in)" im Mod <mod> aufrufen. false, wenn Mod oder Funktion fehlt, bei Skriptfehler
    // oder zu tiefer Verschachtelung (hoechstens 8). out (SStr*) bekommt den Rueckgabetext. HasModTextFunction: gibt es die Funktion dort?
    bool CallModText(const std::string& mod, const std::string& func, const void* arg, void* out);
    bool HasModTextFunction(const std::string& mod, const std::string& func);
    // "void OnModMessage(const bCString &in sender, const bCString &in topic, const bCString &in data)" in allen anderen Mods; Rueckgabe: Zahl der Empfaenger
    int SendModMessage(const void* topic, const void* data);

    // fuer die Bindings
    static ScriptHost* s_current;
    void Check(bool ok, const char* msg);
    float (*frameTime)() = nullptr;

private:
    struct Mod {
        std::string name, path;
        asIScriptModule* module = nullptr;
        unsigned long long mtime = 0;
        bool mainDone = false;
        asIScriptFunction *fMain = nullptr, *fFrame = nullptr, *fKey = nullptr;
        std::unordered_map<std::string, asIScriptFunction*> fnCache;   // Deklaration -> Funktion (auch nullptr gemerkt)
    };
    struct Chain {
        const char* decl;
        EvArg args[8];
        int n = 0;
        size_t next = 0;   // naechster Mod, der in der Kette dran ist
        std::function<bool(const EvArg*)> original;
        bool originalCalled = false;
    };
    std::vector<Chain> chain_;
    asIScriptFunction* FindFn(Mod& m, const char* decl);
    bool Execute(void* func, float* argFloat, const int* argInt = nullptr, const bool* argBool = nullptr, const EvArg* ev = nullptr,
                 int nev = 0, bool* retBool = nullptr, void* retText = nullptr);   // retText: SStr* fuer Funktionen mit bCString-Rueckgabe
    int modCallDepth_ = 0;
    bool Build(Mod& m);   // (neu) uebersetzen; vorher OnShutdown + Verwerfen des alten Moduls
    void Unload(Mod& m);
    Mod* Find(const std::string& name);
    void CallIn(Mod& m, const char* decl);
    asIScriptEngine* engine_ = nullptr;
    asIScriptContext* ctx_ = nullptr;
    std::vector<Mod> mods_;
    std::string modsDir_, curMod_;
    LogFn log_;
    unsigned lastCheckTick_ = 0;
    int failures_ = 0, checks_ = 0;
    bool busyKey_ = false;
};

// Registrierung bei AngelScript pruefen: bei einem Fehler (r < 0) ins Protokoll schreiben und ok auf false setzen
inline void CheckRegistration(int r, const char* what, bool& ok) {
    if (r >= 0) return;
    ok = false;
    if (ScriptHost::s_current) ScriptHost::s_current->Log("Registrierung fehlgeschlagen (" + std::to_string(r) + "): " + what);
}
// Aufrufstelle: G3ASM_REG(engine->RegisterGlobalFunction(...)); braucht eine Variable "bool ok" im Gueltigkeitsbereich und gibt den Aufrufausdruck als Text mit aus
#define G3ASM_REG(call) ::g3asm::CheckRegistration((call), #call, ok)

// Eigenen Text fuer eine Text-ID registrieren (quest_bindings.cpp; installiert bei Bedarf die eCLocString-Hooks)
void RegisterTextForId(const std::string& id, const std::string& text);
void RegisterTextWide(const std::string& id, const std::wstring& text);   // Unicode; speichert immer, Hooks werden bei Bedarf nachgeholt
bool EnsureTextHooks();                                                   // true, sobald die Text-Hooks aktiv sind
std::string CurrentGameLanguage();   // "English", "German", ...; leer, solange das Spiel nicht bereit ist
// Beim Laden: Manager, der gerade neu aufgebaut wird, fuer die Bindings vorruebergehend als "aktueller" Manager setzen (nullptr = zurueck)
void SetQuestManagerOverride(void* mgr);
void SetInfoManagerOverride(void* mgr);
// Hook auf gCInfoManager_PS::OnInfoScriptFinished -> Skript-Ereignis OnInfoFinished (dialog_bindings.cpp)
void InstallInfoHook(ScriptHost& host);

// Name der Template-Entity (z. B. Item) zu einer Property-ID (GUID); leer, wenn unbekannt (engine_bindings.cpp)
std::string TemplateNameForId(const void* propertyId);
// Mod-Namen in Ladereihenfolge (load_order.txt, abgeschaltete weggelassen) - script_host.cpp
std::vector<std::string> DiscoverModNames(const std::string& modsDir);
// Datei-Patches (file_patches.cpp): <Mod>\ScriptPatches\*.patch (Unified Diff) auf Datendateien des Spiels anwenden; Rueckgabe = Zahl gepatchter Dateien
int ApplyScriptPatches(const std::string& gameDir, const std::string& modsDir, const std::function<void(const std::string&)>& log);
// Namen einer Info (Info, Besitzer, Quest) fuer den Override-Hook (dialog_bindings.cpp)
bool InfoNames(const void* info, std::string* name, std::string* owner, std::string* quest);
// Optionen der Spielereignisse: Nav_*-Bereiche (interne Navigationszonen der NPCs) an OnEnter/OnLeave melden? (Standard: nein)
void SetGameHookOptions(bool navEvents);

// Held-Treffer: Spielfigur erkennen, Lebenspunkte-Attribut aendern (engine_bindings.cpp)
bool IsPlayerEntity(const void* p);
// Aktueller Gegner eines NPCs/Helden (gCNPC_PS::CurrentAttackerEntity); nullptr, wenn keiner oder kein NPC (game_hooks.cpp)
void* CurrentAttackerOf(void* ent);
// Item (eCEntity*) im Ausruestungs-Slot eines Entity oder nullptr (game_hooks.cpp)
void* SlotItemOf(void* ent, int slot);
void SetScriptHurt(bool on);   // HurtHitPoints: der Schaden-Hook meldet den naechsten Abzug ohne Angreifer
bool AdjustHeroHitPoints(const void* ent, int delta);

// Spielereignisse (game_hooks.cpp): Hooks auf Game.dll einhaengen; Rueckgabe = Zahl aktiver Hooks
int InstallGameHooks(ScriptHost& host);
// Spielskripte nach Namen (script_calls.cpp): Hooks auf gCScriptAdmin::RunScript/RunScriptFromScript, Bindings CallScript/WatchScript/...
int InstallScriptCallHooks(ScriptHost& host);   // nach InstallGameHooks (Game.dll geladen, MinHook bereit)
void SetScriptCallOptions(bool stats);   // ini [g3asm] scriptlog
void ScriptCallsFrame();                 // einmal je Bild im Spiel-Thread
void ScriptCallsReport();
// Zeichen-API (draw_api.cpp): Hook auf GFC CFFGFCWnd::OnPaint -> Ereignis OnDraw(); DrawFrameTick einmal je Spielbild
int InstallDrawHooks(ScriptHost& host);
void DrawFrameTick();
// Laeuft der Aufrufer im Spiel-Thread (dem Thread von OnFrame)? false, solange noch kein Bild lief (script_calls.cpp)
bool OnGameThread();
// Ereignisse 7.3 (more_events.cpp): Konsole, Quest-Status
int InstallMoreEventHooks(ScriptHost& host);
void MoreEventsFrame();
// Einstellungen je Mod (mod_settings.cpp, ab Version 44): Bindings; ModSettingsForget beim Neuladen eines Mods (Zwischenspeicher verwerfen)
bool RegisterModSettingsBindings(asIScriptEngine* e);
void ModSettingsForget(const std::string& mod);
// KI-Tracer (ai_trace.cpp): nur Protokoll, ini [g3asm] aitrace/aitracefilter/aitracemax; InstallAiTrace haengt nur ein, wenn eingeschaltet
void SetAiTraceOptions(bool enabled, const char* filter, int maxLines);
void SetAniSpeedLog(bool enabled);   // ini [g3asm] anispeedlog: Messung SetPlaySpeed/GetPlaySpeed (Roadmap 6.3)
int InstallAiTrace(ScriptHost& host);
// Eingriffe mitten in Spielskripten (combat_patches.cpp, Originalbytes geprueft): Override_MonsterDamage; bei jeder Weltaktivierung aufrufen (einmalig wirksam)
int InstallScriptGamePatches(ScriptHost& host);   // einmal je Bild (Spiel-Takt): OnTouch-Handler, Verwandlungs-Hooks, Messung
// Script.dll-Entity-Objekt (0xA8 Byte, gemessen) per Konstruktor der Script.dll bauen; Puffer mit kScriptEntityBytes, Ausrichtung 8
constexpr unsigned kScriptEntityBytes = 0x100;
bool MakeScriptEntity(void* buf, void* ent);                // Messung jetzt schreiben (beim Entladen einer Welt)
int CallGameScript(const char* name, void* self, void* other, int arg);   // Spielskript nach Namen (script_calls.cpp), 0 wenn nicht moeglich

// Spielfenster (groesstes sichtbares Hauptfenster des Prozesses), nullptr falls noch keins
HWND__* GameWindow();
// Ist [p, p + n) lesbar (eingecheckter Speicher, Leseschutz)? Prueft per VirtualQuery ohne Ausnahme. Dient dazu, geratene Zeiger vorab zu verwerfen, statt den
// Zugriffsfehler per SEH abzufangen (jede abgefangene Ausnahme erscheint im Absturzprotokoll des Proxys als "ABSTURZ-Kandidat" und kostet Zeit).
bool ReadableMem(const void* p, size_t n);
// Text eines bCString (SharedBase), fallback bei Nullzeiger, unlesbarem Speicher oder Ausnahme. Der Zeiger gilt nur, solange der bCString lebt.
const char* SafeStrText(const void* s, const char* fallback);
// Exportierte Funktion sym aus Modul m per MinHook umleiten und einschalten; Fehler (Export fehlt, Einhaengen fehlgeschlagen) stehen im Protokoll.
bool HookExport(HINSTANCE__* m, const char* sym, void* detour, void** orig, ScriptHost& host);
// Unerwarteter Nullzeiger aus der Engine in einem Hook: einmal je Stelle protokollieren ("where" muss ein fester Text sein, er dient als Schluessel).
void NoteNull(const char* where);

}   // namespace g3asm
