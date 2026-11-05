#pragma once
// AngelScript-Host des OG3-Patches: Engine, Bindings der Genome-Grundtypen (ueber die geladene SharedBase), Laden von main.as,
// Ereignisse fuer Skripte und Neuladen bei Dateiaenderung.
//
// Skript-Schnittstelle (Version OG3_SCRIPT_API_VERSION, siehe docs/og3_patch.md):
//   Typen    bCString, bCVector, bCVector2, bCVector4, bCQuaternion, bCMatrix3 (Methodennamen wie im Gothic-3-SDK)
//   Global   print(const bCString&in), ToString(int|uint|float|double|bool), GetFrameTime(), GetTimeStamp(), GetApiVersion(),
//            check(bool, const bCString&in) (Selbsttests), Mathematik aus dem scriptmath-Add-on (sin, cos, sqrt, ...), array<T>
//   Engine   (ab Version 2) Entity GetHero(), GetPlayer(), FindEntity(name); Entity: IsValid, GetName, GetPosition, GetRotation,
//            GetDistanceTo, SetPosition. Fenster/Eingabe: SetWindowTitle, IsKeyDown(KEY_F1..KEY_F12 oder VK-Code), PlaySystemSound
//   Dialoge schreiben (ab Version 11) CreateInfo, InfoExists, RemoveInfo, SetInfoQuest/Parent/Type/Condition, AddInfoLine, RebuildInfos,
//            DescribeCommands (Befehlsvokabular aller Infos); Ereignis void OnInfoFinished(const bCString &in info, const bCString &in owner, const bCString &in quest)
//   Dialoge  (ab Version 10) GetInfoCount, GetInfoName(i), GetInfoOwner(i), DescribeInfo(name) (Infos des Spiels lesen)
//   Texte    (ab Version 9) RegisterText(id, text) (eigene Text-IDs der Stringtabelle), GetQuestDir, GetQuestFilePath(name), GetGameDir
//   Override (ab Version 17) bool Override_<Ereignis>(<Argumente wie On<Ereignis>>) ersetzt die Spielfunktion; Super() ruft das Original (bzw. den naechsten Mod); Rueckgabe = Ergebnis fuer das Spiel.
//            Verfuegbar: Override_NPCKilled(Entity), Override_NPCDefeated(Entity), Override_ReceiveItem(Entity, Entity, const bCString &in name, const bCString &in id, uint count),
//            Override_Enter(Entity, const bCString &in), Override_Leave(...), Override_SpellCast(Entity, Entity, const bCString &in),
//            Override_InfoAvailable(const bCString &in info, const bCString &in owner, const bCString &in quest) (Dialogoption sichtbar?)
//   Lade-Ereignisse (ab Version 16) void OnQuestsLoaded() / void OnInfosLoaded(): nach dem Neuaufbau von Quests/Infos aus den ini-Dateien (beim Spielstart und Laden)
//   Dialoge ausfuehren (ab Version 15) ExecuteInfo(name): Info-Skript ohne Gespraechsfenster (z. B. nur AddQuestLog)
//   Speichern (ab Version 14) SetGameEvent/ClearGameEvent/IsGameEventSet(name): Schalter im Spielstand (PlayerMemory des Helden); SetInfoGiven/IsInfoGiven
//   Quest-Belohnung (ab Version 13) SetQuestReward(name, attribut, menge), GetQuestReward(name), DescribeQuestRewards()
//   Quest-Texte (ab Version 8) SetQuestTopic, SetQuestDescription, DumpQuest/LoadQuestFile (ini), GetQuestCount, GetQuestNameAt, QuestDebug
//   Inventar (ab Version 7) Entity.GetItemCount(itemName) (-1 = nicht lesbar), Entity.RemoveItems(itemName, n) (Rueckgabe: entfernte Stuecke),
//            Entity.AddItems(itemName, n) (ab Version 12; erzeugt Items im Inventar, -1 bei Fehler/unbekanntem Item)
//   Quests   (ab Version 6) QuestExists, CreateQuest, RunQuest, SucceedQuest, FailQuest, CloseQuest, RemoveQuest (je Name), GetQuestStatus,
//            SetQuestStatus, IsQuestFinished, IsQuestSucceeded, GetQuestXP, SetQuestXP, ReloadQuestFiles; Konstanten QUEST_OPEN..QUEST_WON
//   Ereignisse (optional im Skript definieren):
//            void main()                      einmal, sobald die Engine fertig initialisiert ist (bzw. nach Neuladen)
//            void OnFrame(float dt)           jeden Frame (Hauptthread)
//            void OnWorldActivate()           Welt geladen/aktiviert
//            void OnWorldDeactivate()         Welt entladen
//            void OnKey(int vk, bool down)    Taste gedrueckt/losgelassen (VK-Code, KEY_F1.., keine Wiederholung), ab Version 4
//            void OnNPCKilled(Entity victim) / OnNPCDefeated(Entity npc) / OnReceiveItem(Entity giver, Entity receiver, const bCString &in itemName, const bCString &in itemId, uint count)
//            void OnEnter(Entity who, const bCString &in area) / OnLeave(...) / OnSpellCast(Entity caster, Entity target, const bCString &in spell)
//                                             Spielereignisse (Game.dll-Hooks, ab Version 5), laufen nach dem Originalverhalten
//            void OnShutdown()                vor dem Beenden bzw. vor dem Neuladen
#include <functional>
#include <unordered_map>
#include <string>
#include <vector>

#define OG3_SCRIPT_API_VERSION 24

class asIScriptEngine;
class asIScriptContext;
class asIScriptModule;
class asIScriptFunction;
struct HWND__;

namespace og3p {

// Argument eines Spiel-Ereignisses fuer Skripte (siehe ScriptHost::CallEvent)
struct EvArg {
    enum Kind { Entity, Text, UInt } kind;
    const void* ptr;     // Entity: eCEntity*; Text: Zeiger auf ein bCString-Objekt
    unsigned value;      // UInt
    static EvArg Ent(const void* e) { return {Entity, e, 0}; }
    static EvArg Str(const void* s) { return {Text, s, 0}; }
    static EvArg U(unsigned v) { return {UInt, nullptr, v}; }
};

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
    int ScanMods();                       // neue/entfernte Mods einarbeiten, Reihenfolge anwenden; Rueckgabe: Zahl neu geladener Mods
    bool Loaded() const;
    int ModCount() const { return int(mods_.size()); }
    void CallMain();                      // main() aller Mods, die es noch nicht hatten
    void CallFrame(float dt);
    void CallKey(int vk, bool down);      // void OnKey(int vk, bool down), falls vorhanden; Wiederholungen werden nicht gemeldet
    // Spiel-Ereignis mit Argumenten an alle Mods, z. B. "void OnNPCKilled(Entity)" (Entity als Wert, Text als const bCString &in)
    void CallEvent(const char* decl, const EvArg* args, int n);
    void CallSimple(const char* decl);    // "void OnWorldActivate()" usw. in allen Mods, falls vorhanden
    // Override-Hooks (ab Version 17): ersetzt ein Mod eine Spielfunktion (Skriptfunktion "bool Override_<Name>(...)"), laeuft sie statt des Originals; ueber Super()
    // ruft der Mod das Original (bzw. den naechsten Mod der Kette) auf. HasHandler: definiert irgendein Mod diese Funktion?
    bool HasHandler(const char* decl);
    // true = ein Mod hat uebernommen: *result ist das Ergebnis fuer das Spiel, *originalCalled sagt, ob das Original gelaufen ist; false = niemand hat die Funktion
    // original(args): das Original der Spielfunktion; bekommt die Argumente der Kette (ein Mod kann sie mit SetOverrideArg veraendert haben)
    bool CallOverride(const char* decl, const EvArg* args, int n, std::function<bool(const EvArg*)> original, bool* result, bool* originalCalled);
    // Argument der gerade laufenden Override-Kette setzen (nur Zahlen: Kind UInt); Super() reicht den neuen Wert an den naechsten Mod und das Original weiter
    bool SetOverrideArg(int index, unsigned value);
    bool GetOverrideArg(int index, unsigned* value) const;
    bool ChainSuper();                    // fuer die Skriptfunktion Super()
    // Neuladen geaenderter Mods und Mod-Ordner pruefen (hoechstens einmal je Sekunde); true = etwas neu geladen
    bool CheckReload();
    int Failures() const { return failures_; }
    int Checks() const { return checks_; }
    void Log(const std::string& s) { if (log_) log_(s); }
    const char* CurrentMod() const { return curMod_.c_str(); }

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
        size_t next = 0;                  // naechster Mod, der in der Kette dran ist
        std::function<bool(const EvArg*)> original;
        bool originalCalled = false;
    };
    std::vector<Chain> chain_;
    asIScriptFunction* FindFn(Mod& m, const char* decl);
    bool Execute(void* func, float* argFloat, const int* argInt = nullptr, const bool* argBool = nullptr, const EvArg* ev = nullptr, int nev = 0, bool* retBool = nullptr);
    bool Build(Mod& m);                   // (neu) uebersetzen; vorher OnShutdown + Verwerfen des alten Moduls
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

// Eigenen Text fuer eine Text-ID registrieren (quest_bindings.cpp; installiert bei Bedarf die eCLocString-Hooks)
void RegisterTextForId(const std::string& id, const std::string& text);
void RegisterTextWide(const std::string& id, const std::wstring& text);   // Unicode; speichert immer, Hooks werden bei Bedarf nachgeholt
bool EnsureTextHooks();                                                   // true, sobald die Text-Hooks aktiv sind
std::string CurrentGameLanguage();                                        // "English", "German", ...; leer, solange das Spiel nicht bereit ist
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
bool AdjustHeroHitPoints(const void* ent, int delta);

// Spielereignisse (game_hooks.cpp): Hooks auf Game.dll einhaengen; Rueckgabe = Zahl aktiver Hooks
int InstallGameHooks(ScriptHost& host);

// Spielfenster (groesstes sichtbares Hauptfenster des Prozesses), nullptr falls noch keins
HWND__* GameWindow();
// Ist [p, p + n) lesbar (eingecheckter Speicher, Leseschutz)? Prueft per VirtualQuery ohne Ausnahme. Dient dazu, geratene Zeiger vorab zu verwerfen, statt den
// Zugriffsfehler per SEH abzufangen (jede abgefangene Ausnahme erscheint im Absturzprotokoll des Proxys als "ABSTURZ-Kandidat" und kostet Zeit).
bool ReadableMem(const void* p, size_t n);

}  // namespace og3p
