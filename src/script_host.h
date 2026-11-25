// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
#pragma once
// AngelScript host of G3ASM: engine, bindings of the Genome base types (through the loaded SharedBase), load of the mod scripts,
// events and overrides for scripts, reload when a file changes.
//
// docs/modkit/ (api-reference.md, events.md, hooks.md) describes the script interface (types, functions, events, override hooks, version
// G3ASM_SCRIPT_API_VERSION). This file gives only the C++ interfaces.
#include <functional>
#include <map>
#include <unordered_map>
#include <string>
#include <thread>
#include <vector>

#define G3ASM_SCRIPT_API_VERSION 47

class asIScriptEngine;
class asIScriptContext;
class asIScriptModule;
class asIScriptFunction;
struct HWND__;
struct HINSTANCE__;

namespace g3asm {

// argument of a game event for scripts (see ScriptHost::CallEvent)
struct EvArg {
    enum Kind { Entity, Text, UInt, Bool } kind;
    const void* ptr;   // Entity: eCEntity*; text: pointer to a bCString object
    unsigned value;    // UInt; Bool: 0 or 1
    static EvArg Ent(const void* e) { return {Entity, e, 0}; }
    static EvArg Str(const void* s) { return {Text, s, 0}; }
    static EvArg U(unsigned v) { return {UInt, nullptr, v}; }
    static EvArg B(bool v) { return {Bool, nullptr, v ? 1u : 0u}; }
};

void SetPrintToGameLog(bool on);   // ini [g3asm] printgame
void SetLoadDefaultOffMods(bool on);   // also load mods with mod.ini enabled=0 (test program)

class ScriptHost {
public:
    using LogFn = std::function<void(const std::string&)>;
    bool Init(LogFn log);
    void Shutdown();
    // load a single script as the mod "main" and discard all other mods (tests); false for compile errors
    bool Load(const std::string& path);
    // add a mod (or reload it if the name already exists); false for compile errors (messages in the log)
    bool AddScript(const std::string& name, const std::string& path);
    // Mod folder: <dir>\<name>\main.as for each mod, a separate module for each mod. <dir>\load_order.txt (optional): one mod name on each line in the
    // load order, '#' = comment, '-name' = mod off. Mods that are not in the list come after them in alphabetical sequence.
    void SetModsDir(const std::string& dir) { modsDir_ = dir; }
    int ScanMods();   // add new mods and remove removed mods, apply the sequence; return value: the number of mods loaded again
    bool Loaded() const;
    int ModCount() const { return int(mods_.size()); }
    void CallMain();   // main() of all mods that did not have it yet
    void CallFrame(float dt);
    void CallKey(int vk, bool down);   // void OnKey(int vk, bool down), if it exists; repeats are not reported
    // game event with arguments to all mods, for example "void OnNPCKilled(Entity)" (Entity as a value, text as const bCString &in)
    void CallEvent(const char* decl, const EvArg* args, int n);
    void CallSimple(const char* decl);   // "void OnWorldActivate()" and similar functions in all mods, if they exist
    // Override hooks (from version 17): if a mod replaces a game function (script function "bool Override_<Name>(...)"), it runs in place of the original. Through Super(),
    // the mod calls the original (or the next mod of the chain). HasHandler: does any mod define this function?
    bool HasHandler(const char* decl);
    // true = a mod took over: *result is the result for the game, *originalCalled tells if the original ran; false = no mod has the function
    // original(args): the original of the game function; it gets the arguments of the chain (a mod can have changed them with SetOverrideArg)
    bool CallOverride(const char* decl, const EvArg* args, int n, std::function<bool(const EvArg*)> original, bool* result,
                      bool* originalCalled, EvArg* finalArgs = nullptr);   // finalArgs: arguments after the chain (n items)
    // set an argument of the override chain that runs at this time (only numbers: kind UInt); Super() gives the new value to the next mod and to the original
    bool SetOverrideArg(int index, unsigned value);
    bool GetOverrideArg(int index, unsigned* value) const;
    bool ChainSuper();   // for the script function Super()
    // check for changed mods and mod folders to reload (maximum 1 time each second); true = something reloaded
    bool CheckReload();
    int Failures() const { return failures_; }
    int Checks() const { return checks_; }
    void Log(const std::string& s) {
        if (capture_ && std::this_thread::get_id() == captureThread_) {   // dev pipe: also collect the lines of the running snippet
            *capture_ += s;
            *capture_ += '\n';
        }
        if (log_) log_(s);
    }
    // Dev pipe (dev_pipe.cpp, only in g3asm.dll): compile code as the body of "void dev()" and run it one time. mod = name of a loaded mod (the
    // code can then use the globals of this mod), empty = a separate module "dev". out gets all log lines of the call (print, compile errors,
    // script exceptions). Only in the game thread. false for a compile error or an error during execution.
    bool RunSnippet(const std::string& code, const std::string& mod, std::string* out);
    const char* CurrentMod() const { return curMod_.c_str(); }
    // mod folder (empty in single-script mode) and the folder of a loaded mod (directory of its main.as; empty if not known)
    const std::string& ModsDir() const { return modsDir_; }
    std::string ModDir(const std::string& mod) const;
    // Between mods (from version 44): call "bCString <func>(const bCString &in)" in the mod <mod>. false if the mod or the function is missing, for a script error
    // or for too deep nesting (maximum 8). out (SStr*) gets the return text. HasModTextFunction: does the function exist there?
    bool CallModText(const std::string& mod, const std::string& func, const void* arg, void* out);
    bool HasModTextFunction(const std::string& mod, const std::string& func);
    // "void OnModMessage(const bCString &in sender, const bCString &in topic, const bCString &in data)" in all other mods; return value: the number of receivers
    int SendModMessage(const void* topic, const void* data);

    // for the bindings
    static ScriptHost* s_current;
    void Check(bool ok, const char* msg);
    // Coroutines (from version 47): each one has its own context and belongs to the mod that started it. TickCoroutines runs 1 time for each
    // frame in the game thread (after OnFrame): it adds dt to the coroutine clock and continues each coroutine whose wait time is over, at most
    // 1 time for each frame. Each part between two waits has the 5-s stop. Unload stops the coroutines of the mod.
    int StartCoroutine(asIScriptFunction* f, void* args);   // args: CScriptDictionary* or nullptr; return value: ID > 0, or -1
    void WaitCoroutine(float seconds);                      // only in a coroutine; if not, a script exception
    bool StopCoroutine(int id);                             // only coroutines of the current mod
    int StopAllCoroutines();                                // all coroutines of the current mod; return value: the number
    bool IsCoroutineRunning(int id) const;
    void TickCoroutines(float dt);
    int CoroutineCount() const { return int(coroutines_.size()); }
    float (*frameTime)() = nullptr;

private:
    struct Mod {
        std::string name, path;
        asIScriptModule* module = nullptr;
        unsigned long long mtime = 0;
        bool mainDone = false;
        asIScriptFunction *fMain = nullptr, *fFrame = nullptr, *fKey = nullptr;
        std::unordered_map<std::string, asIScriptFunction*> fnCache;   // declaration -> function (nullptr is also kept)
    };
    struct Chain {
        const char* decl;
        EvArg args[8];
        int n = 0;
        size_t next = 0;   // next mod in the chain
        std::function<bool(const EvArg*)> original;
        bool originalCalled = false;
    };
    std::vector<Chain> chain_;
    asIScriptFunction* FindFn(Mod& m, const char* decl);
    bool Execute(void* func, float* argFloat, const int* argInt = nullptr, const bool* argBool = nullptr, const EvArg* ev = nullptr,
                 int nev = 0, bool* retBool = nullptr, void* retText = nullptr);   // retText: SStr* for functions with a bCString return value
    int modCallDepth_ = 0;
    bool Build(Mod& m);   // compile (again); first OnShutdown and discard the old module
    void Unload(Mod& m, bool keepGlobals = false);   // keepGlobals: keep the globals for the next Build (hot reload)
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
    std::string* capture_ = nullptr;   // RunSnippet: target for the log lines of the call
    std::thread::id captureThread_;
    asIScriptModule* devModule_ = nullptr;
    struct Coroutine {
        int id;
        std::string mod;
        asIScriptContext* ctx;
        double wakeAt;        // coroutine clock in seconds
        unsigned lastTick;    // tick in which it ran last (or was started): at most 1 run for each frame
        bool stop;
    };
    std::vector<Coroutine> coroutines_;
    double coClock_ = 0;
    unsigned coTick_ = 0;
    int nextCoId_ = 1, runningCo_ = 0;   // runningCo_: ID of the coroutine that runs now (0 = none)
    bool coTicking_ = false;
    void StopCoroutinesOf(const std::string& mod);   // stop and release now (Unload, Shutdown)
    void SweepCoroutines();                          // release the coroutines with stop = true
};

// Hot reload (keep_globals.cpp, from version 47): keep the global variables of a module and give them to the new module
struct SavedGlobals;
SavedGlobals* SaveGlobals(asIScriptModule* mod);              // nullptr if it failed
int RestoreGlobals(SavedGlobals* saved, asIScriptModule* mod);   // number of globals with a kept value, -1 for an error
void FreeSavedGlobals(SavedGlobals* saved);
// check a registration with AngelScript: if there is an error (r < 0), write to the log and set ok to false
inline void CheckRegistration(int r, const char* what, bool& ok) {
    if (r >= 0) return;
    ok = false;
    if (ScriptHost::s_current) ScriptHost::s_current->Log("Registrierung fehlgeschlagen (" + std::to_string(r) + "): " + what);
}
// call site: G3ASM_REG(engine->RegisterGlobalFunction(...)); needs a variable "bool ok" in the scope and also writes the call expression as text
#define G3ASM_REG(call) ::g3asm::CheckRegistration((call), #call, ok)

// register your own text for a text ID (quest_bindings.cpp; installs the eCLocString hooks when necessary)
void RegisterTextForId(const std::string& id, const std::string& text);
void RegisterTextWide(const std::string& id, const std::wstring& text);   // Unicode; always saves, the hooks are installed later when necessary
bool EnsureTextHooks();                                                   // true when the text hooks are active
// properties of a property object that is not an entity (engine_bindings.cpp; text form as GetProperty/SetProperty)
class SStr;
bool GetObjectPropertyText(void* ps, const SStr& prop, SStr* out);
bool SetObjectPropertyText(void* ps, const SStr& prop, const SStr& value);
SStr ListObjectPropertyTexts(void* ps, int maxn);
std::string CurrentGameLanguage();   // "English", "German", ...; empty while the game is not ready
// During a load: set the manager that the game makes again temporarily as the "current" manager for the bindings (nullptr = back)
void SetQuestManagerOverride(void* mgr);
void SetInfoManagerOverride(void* mgr);
// before gCInfoManager_PS::LoadInfosFromInis: remove infos that scripts made (if not, their names stay in use in the name table); return value = count
int ForgetCreatedInfos(void* mgr);
// hook on gCInfoManager_PS::OnInfoScriptFinished -> script event OnInfoFinished (dialog_bindings.cpp)
void InstallInfoHook(ScriptHost& host);

// name of the template entity (for example an item) for a property ID (GUID); empty if not known (engine_bindings.cpp)
std::string TemplateNameForId(const void* propertyId);
// mod names in the load order (load_order.txt, without the mods that are off) - script_host.cpp
std::vector<std::string> DiscoverModNames(const std::string& modsDir);
// File patches (file_patches.cpp): apply <mod>\ScriptPatches\*.patch (unified diff) to data files of the game; return value = number of patched files
int ApplyScriptPatches(const std::string& gameDir, const std::string& modsDir, const std::function<void(const std::string&)>& log);
// Packed mods (packed_mods.cpp): unpack <game>\Data\G3ASM.mod/.m00/... to <modsDir>\<mod>\ (with the marker .g3asm_packed), remove old
// unpacked mods. Loose folders without a marker stay without changes and have priority. Return value = number of packed mods that are now ready
int ExtractPackedMods(const std::string& gameDir, const std::string& modsDir, const std::function<void(const std::string&)>& log);
// names of an info (info, owner, quest) for the override hook (dialog_bindings.cpp)
bool InfoNames(const void* info, std::string* name, std::string* owner, std::string* quest);
// Options of the game events: report Nav_* areas (internal navigation zones of the NPCs) to OnEnter/OnLeave? (default: no)
void SetGameHookOptions(bool navEvents);

// hero hit: detect the player character, change the hit point attribute (engine_bindings.cpp)
bool IsPlayerEntity(const void* p);
// current opponent of an NPC or the hero (gCNPC_PS::CurrentAttackerEntity); nullptr if there is none or no NPC (game_hooks.cpp)
void* CurrentAttackerOf(void* ent);
// item (eCEntity*) in the equipment slot of an entity, or nullptr (game_hooks.cpp)
void* SlotItemOf(void* ent, int slot);
void SetScriptHurt(bool on);   // HurtHitPoints: the damage hook reports the next decrease without an attacker
bool AdjustHeroHitPoints(const void* ent, int delta);

// Game events (game_hooks.cpp): install hooks on Game.dll; return value = number of active hooks
int InstallGameHooks(ScriptHost& host);
// Game scripts by name (script_calls.cpp): hooks on gCScriptAdmin::RunScript/RunScriptFromScript, bindings CallScript/WatchScript/...
int InstallScriptCallHooks(ScriptHost& host);   // after InstallGameHooks (Game.dll loaded, MinHook ready)
void SetScriptCallOptions(bool stats);   // ini [g3asm] scriptlog
void ScriptCallsFrame();                 // 1 time for each frame in the game thread
void ScriptCallsReport();
// Drawing API (draw_api.cpp): hook on GFC CFFGFCWnd::OnPaint -> event OnDraw(); DrawFrameTick 1 time for each game frame
int InstallDrawHooks(ScriptHost& host);
void DrawFrameTick();
// Does the caller run in the game thread (the thread of OnFrame)? false while no frame ran yet (script_calls.cpp)
bool OnGameThread();
bool OnForeignThread();   // game thread known, and the caller is a different thread
// Events 7.3 (more_events.cpp): console, quest status
int InstallMoreEventHooks(ScriptHost& host);
void MoreEventsFrame();
// Settings for each mod (mod_settings.cpp, from version 44): bindings; ModSettingsForget when a mod reloads (discard the cache)
bool RegisterModSettingsBindings(asIScriptEngine* e);
void ModSettingsForget(const std::string& mod);
// section of an ini file (keys in lower case, values trimmed; UTF-8 BOM permitted); empty if the file or the section is missing
std::map<std::string, std::string> ReadIniSection(const std::string& path, const std::string& section);
// AI tracer (ai_trace.cpp): only log, ini [g3asm] aitrace/aitracefilter/aitracemax; InstallAiTrace installs only if turned on
void SetAiTraceOptions(bool enabled, const char* filter, int maxLines);
void SetAniSpeedLog(bool enabled);   // ini [g3asm] anispeedlog: Messung SetPlaySpeed/GetPlaySpeed (Roadmap 6.3)
int InstallAiTrace(ScriptHost& host);
// Trade (trade_hooks.cpp): event OnTrade (always) and trade measurement (only log, ini [g3asm] tradelog=1)
void SetTradeLog(bool enabled);
int InstallTradeHooks(ScriptHost& host);
// Changes in the middle of game scripts (combat_patches.cpp, original bytes checked): Override_MonsterDamage; call at each world activation (effect only 1 time)
int InstallScriptGamePatches(ScriptHost& host);   // 1 time for each frame (game cycle): OnTouch handler, transformation hooks, measurement
// build a Script.dll entity object (0xA8 bytes, measured) with the constructor of Script.dll; buffer with kScriptEntityBytes, alignment 8
constexpr unsigned kScriptEntityBytes = 0x100;
bool MakeScriptEntity(void* buf, void* ent);                // write the measurement now (when a world unloads)
int CallGameScript(const char* name, void* self, void* other, int arg);   // game script by name (script_calls.cpp), 0 if not possible

// Dev pipe (dev_pipe.cpp): only g3asm.dll contains it, the script DLL for players does not. The pointers stay nullptr there.
// DevPipeStart: start the server thread (ini [dev] pipe=1); DevPipeFrame: run the waiting requests, 1 time for each frame in the game thread.
extern bool (*g_devPipeStart)(ScriptHost& host);
extern void (*g_devPipeFrame)();

// game window (largest visible main window of the process), nullptr if there is none yet
HWND__* GameWindow();
// Is [p, p + n) readable (committed memory, read protection)? Checks with VirtualQuery without an exception. Use it to discard guessed pointers first, in place of
// catching the access violation with SEH (each caught exception shows in the crash log of the proxy as "ABSTURZ-Kandidat" (crash candidate) and costs time).
bool ReadableMem(const void* p, size_t n);
// text of a bCString (SharedBase); fallback for a null pointer, unreadable memory or an exception. The pointer is valid only while the bCString exists.
const char* SafeStrText(const void* s, const char* fallback);
// redirect the exported function sym from module m with MinHook and turn it on; errors (export missing, install failed) are in the log.
bool HookExport(HINSTANCE__* m, const char* sym, void* detour, void** orig, ScriptHost& host);
// unexpected null pointer from the engine in a hook: log it 1 time for each location ("where" must be a fixed text, it is the key).
void NoteNull(const char* where);

}   // namespace g3asm
