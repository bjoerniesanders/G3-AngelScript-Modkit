# OG3-Patch: offener Patch mit AngelScript-Modding (Meilenstein 1, 2026-11-01)

## Aufbau
- `og3_patch.dll` (Ziel `og3_patch`, build32\patch): AngelScript 2.38.0 (thirdparty/angelscript, zlib-Lizenz) statisch eingebaut.
- Geladen vom dbghelp-Proxy (`og3_hooks.ini` `[patch] enable=1`), arbeitet mit der geladenen SharedBase.dll. Die Bindings rufen deren
  Exporte auf (Original oder Nachbau, gleiche Exportnamen) -> Originalverhalten, auch fuer Objekte, die das Spiel selbst anlegt.
- Einhaengen nur ueber die Importtabelle von Gothic3.exe (kein Code-Eingriff, vertraeglich mit dem PU-Patch):
  `eCApplication::OnPostInitializeModules` -> main(), `OnProcess` -> OnFrame(dt) + Neuladen, `OnWorldActivate/Deactivate`.
  Sind die Importe beim Laden noch nicht gebunden, wartet ein Hilfsthread (max. 60 s). Zeigt ein Import schon auf einen fremden Hook,
  wird dieser weitergereicht (Protokollhinweis).
- Quellen: src/{patch_main,script_host,sb_api}.cpp; Beispielskript examples/scripts/main.as.

## ini
```
[patch]
enable=0                    ; 1 = og3_patch.dll laden (liegt neben dbghelp.dll)
script=og3\scripts\main.as  ; relativ zum Spielordner oder absolut
hotreload=1                 ; Skript bei Dateiaenderung im laufenden Spiel neu laden (Pruefung 1x je Sekunde)
```
Protokoll: `<Gothic3>\og3_patch.log`; print() schreibt zusaetzlich ins Gothic-3-Protokoll (GE_MESSAGEF_INFO).

## Skript-Schnittstelle (Version 2, `GetApiVersion()`)
- `bCString`: Literale, `+ += == < `, `+ int/uint/float/double/bool`, GetLength, IsEmpty, Compare, CompareNoCase, Find(s, start=0),
  Contains, Left, Right, Mid, ToUpper, ToLower, GetUpper, GetLower.
- `bCVector` (x,y,z): GetMagnitude, GetSquareMagnitude, Normalize, GetNormalized, GetDotProduct, GetCrossProduct, + - * / Negation, ==, ToString.
  + - * / rechnet der Patch selbst, alles andere die SharedBase.
- `bCVector2`, `bCVector4`: Felder, GetMagnitude, Normalize (bCVector2 auch + - *).
- `bCQuaternion` (x,y,z,w; Standard = Einheit): Clear, Normalize, Invert, GetMagnitude, *, SetQuaternion(bCMatrix3).
- `bCMatrix3` (Standard = Einheit): GetElement/SetElement(Zeile, Spalte), SetIdentity, Transpose, Invert, * Matrix, * Vektor,
  SetToRotationAxisX/Y/Z(Bogenmass), SetToRotation(bCQuaternion).
- Global: print, ToString(...), GetFrameTime() (eCApplication::GetScaledFrameTimeInSeconds), GetTimeStamp() (bCTimer), GetApiVersion(),
  check(bool, text) fuer Tests, scriptmath (sin, cos, sqrt, abs, ...), `array<T>`.
- Engine (ab Version 2): `Entity GetHero()`, `GetPlayer()` (gCSession), `FindEntity(name)` (eCSceneAdmin::GetEntityByName);
  `Entity`: IsValid, GetName, GetPosition, GetRotation, GetDistanceTo, SetPosition (virtuelles SetWorldPosition ueber die vtable).
  Jeder Zugriff prueft den Zeiger (lesbar, vtable in Engine/Game/EXE) und laeuft unter SEH-Schutz -> ungueltige Entities liefern Standardwerte.
  Entities nicht ueber Weltwechsel aufbewahren, sondern neu holen.
- Fenster/Eingabe: SetWindowTitle, IsKeyDown(KEY_F1..KEY_F12 oder VK-Code; nur wenn das Spiel im Vordergrund ist), PlaySystemSound.
  Freie Tasten pruefen (ini\controls.ini): F5/F9 Schnellspeichern/-laden, F1-F3, F6-F8, F12 belegt; F10/F11 frei.
- Referenzparameter auf einfache Typen (`int &inout`) sind gesperrt (asEP_ALLOW_UNSAFE_REFERENCES aus).
- Ereignisse (optional): `void main()`, `void OnFrame(float dt)`, `void OnWorldActivate()`, `void OnWorldDeactivate()`, `void OnShutdown()`
  (vor dem Neuladen).
- Sicherheit: Uebersetzungsfehler mit Datei/Zeile/Spalte im Protokoll (Skript wird nicht geladen), Laufzeitfehler (z. B. Division durch 0)
  mit Funktion/Zeile, ein Aufruf laenger als 5 s wird abgebrochen (Endlosschleife). Das Spiel laeuft in allen Faellen weiter.

## Tests (ohne Spiel)
`build32\patch\og3_patch_test.exe <SharedBase.dll> tests\scripts\selftest.as [Dialogs.dll]` -> 42 Pruefungen, mit Original-SharedBase
(tmp\engtest) und Nachbau (build32\dll_mimalloc) jeweils 0 Fehlschlaege. Fehlerfaelle (Syntax, Division durch 0, Endlosschleife) geprueft.

## Mini-Mod (examples/scripts/main.as)
Fenstertitel mit FPS/Spielzeit/Heldenposition, Ton bei Engine-Start und Weltwechsel, F11 Info, F10 Position merken, Shift+F10 zurueck.
Im Spiel bestaetigt (Version 1: Titel, Ereignisse, Taste, Neuladen); Version 2 (Entity/Teleport) noch nicht im Spiel getestet.
Einbau/Update: `powershell -NoProfile -File og3-patch/tools/install_patch.ps1 [-Script]`.

- Menuetaste stumm (2026-11-01): Fensterprozedur des Spiels wird beim ersten OnProcess ersetzt; SC_KEYMENU, WM_SYSCHAR, WM_MENUCHAR werden geschluckt -> kein Windows-Piepton bei F10/Alt. Protokollzeile "Menuetaste stumm".

## Offen / naechste Schritte
- Spieltest (Proxy + og3_patch.dll nach G:\Gothic3, nur mit OK des Nutzers).
- Engine-Bindings: Entity nach Namen (eCSceneAdmin), Position/Rotation, Held, Property-Sets; Tastatur-Ereignis (eCApplication::OnKeyboard).
- x64-Exportnamen fuer Classic.

## API 3 (2026-11-01)
- Entity: `GetAttribute/GetAttributeBase/GetAttributeMax(tag)`, `SetAttributeBase/AddAttributeBase/SetAttribute/AddAttribute(tag, n)`; Kennungen "STR", "DEX", "INT", "HP", "MP", "SP", "XP", "LP", ... Weg: Script.dll `PSPlayerMemory(eCEntity*)` -> Game.dll `gCPlayerMemory_PS::*Value(bCString)` (wie `PSPlayerMemory::AddStrength` = AddBaseValue("STR")).
- Uhr: `GetGameDay/Hour/Minute()`, `SetGameTime(day|-1, h, m)` ueber `gCWorld::GetCurrentWorld()->GetWorldEntity()` + Script.dll `PSClock`.
- Bildschirm: `PrintScreen(text, sek=3, r,g,b)` = `gCScreenOutputCtrl::PrintScreen` (wie GUI::PrintScreen_Debug ohne Praefix), `PrintGameMessage(text, typ)`, `GetActivePage()` (gEGamePage, Werte noch unbekannt -> Mini-Mod protokolliert "Spielseite: N").
- Mini-Mod "Charakter-Info" (Gegenstueck zum Remake-Beispiel): F11 Info, Shift+F11 Staerke +1, F10/Shift+F10 Teleport, keine Windows-Toene mehr.
- Offen: echte Einbettung in die Charakterseite (gCHUDPage/GUI.dll-Fenster, Knopf zum Anklicken) statt Bildschirmtext.

## API 4 (2026-11-02)
- Im Spiel bestaetigt (Nutzer): Attribute (GetAttribute/Set/Add...) und PrintScreen funktionieren.
- Neues Ereignis `void OnKey(int vk, bool down)`: kommt aus der ersetzten Fensterprozedur (WM_KEYDOWN/SYSKEYDOWN/KEYUP), Wiederholungen werden nicht gemeldet,
  die Nachricht geht immer weiter ans Spiel. (eCApplication::OnKeyboard hat keine Parameter, taugt deshalb nicht.) Selbsttest: 49 Pruefungen, 0 Fehler (Original und Nachbau).
- Idee Quests: gCQuestManager_PS::GetInstance/AddQuest/GetQuest/RunQuest/SucceedQuest/FailQuest/CloseQuest/RemoveQuest (Game.dll) + gCQuest_PS-Eigenschaften
  (Name, Type, LogTopic, LogText, XP, Status); Quests liegen als ini-Dateien (GetQuestDir). Noch nicht angebunden.

## Mods (Schritt 1 zum G1R-Modkit-Umfang, 2026-11-02)
- Ziel: Funktionsumfang wie https://alkimiainteractive.github.io/G1R-Modkit/scripting/ (Script Hooking, Script Patches, Quests, Konversationen).
- `<Spiel>\og3\mods\<Name>\main.as` je Mod, eigenes AngelScript-Modul je Mod (Globals getrennt, `#include` relativ zur Datei). ini `[patch] mods=og3\mods`.
  Das Einzelskript (`script=`) laeuft weiter als Mod "main" und zwar zuerst.
- `mods\load_order.txt` (optional): ein Modname je Zeile in Ladereihenfolge, `#` Kommentar, `-Name` schaltet einen Mod ab; Rest alphabetisch.
- Alle Ereignisse (main, OnFrame, OnKey, OnWorldActivate/Deactivate, OnShutdown) gehen der Reihe nach an alle Mods; `print` traegt den Modnamen `[Name]`.
- Fehler sind pro Mod isoliert (Uebersetzungsfehler, Ausnahme, Zeitueberschreitung): die anderen Mods laufen weiter.
- Hot-Reload je Mod (Aenderung an main.as), neue Ordner werden im laufenden Spiel erkannt (main() laeuft dann), entfernte/abgeschaltete werden entladen (OnShutdown).
- Test: `og3_patch_test.exe <SharedBase.dll> tests\mods` (Ordner statt Skript): Reihenfolge, Fehlerisolation, Abschaltung geprueft. Selbsttest 49/49 unveraendert.
- Geplant: (2) Hooks auf Game.dll-Methoden (OnNPCKilled, OnReceiveItem, ...), (3) Quest-API, (4) Unified-Diff-Patches `ScriptPatches\*.patch` auf ini-Daten, (5) Info/Dialog-API.

## Spielereignisse (Schritt 2, API 5, 2026-11-02)
- Inline-Hooks (MinHook, thirdparty/minhook) auf sechs Eintrittspunkte in Game.dll, alle `gCQuestManager_PS`: OnNPCKilled, OnNPCDefeated,
  OnReceiveItem, OnEnter, OnLeave, OnSpellCast. Der Hook ruft zuerst das Originalverhalten, dann die Skript-Ereignisse aller Mods
  (Quest-Fortschritt im Spiel bleibt unveraendert). Eingehaengt wird einmal bei OnPostInitializeModules; ini `[patch] hooks=1` (0 = aus).
- Skript-Signaturen (exakt so definieren, alle optional):
  `void OnNPCKilled(Entity victim)`, `void OnNPCDefeated(Entity npc)`,
  `void OnReceiveItem(Entity giver, Entity receiver, const bCString &in itemName, const bCString &in itemId, uint count)` (itemName = Name der
  Template-Entity ueber eCSceneAdmin::GetEntity(GUID, Template), itemId = Property-GUID als Text, giver/receiver koennen ungueltig sein), `void OnEnter(Entity who, const bCString &in area)`, `void OnLeave(...)`,
  `void OnSpellCast(Entity caster, Entity target, const bCString &in spell)`.
- Das Protokoll meldet jedes Ereignis beim ersten Auftreten ("Spielereignis erstmals ausgeloest: ...") -> zeigt, welche Hooks das Spiel wirklich ruft.
  UNGEPRUEFT im Spiel: ob das Spiel diese Funktionen bei jedem Kill/Itemfund aufruft (sie koennten nur fuer aktive Quests laufen).
- Feuert ein Ereignis, waehrend ein Skript laeuft, wird der Kontext verschachtelt (PushState).
- Test: `OG3_TEST_GAMEDLL=G:\Gothic3\Game.dll og3_patch_test.exe ...` laedt die echte Game.dll und prueft das Einhaengen: 6 von 6 Hooks aktiv.
  Selbsttest 53/53 (Original und Nachbau), Zustellung mit Argumenten (Entity, Text, Zahl) ohne Spiel geprueft.
- Beispiel-Mod: examples/mods/ereignisse/main.as (Bildschirmtext bei Kill, Protokoll fuer alle Ereignisse).
- Noch offen in Schritt 2: `override`-artige Hooks mit Aufruf/Ersatz des Originals (Super), Abbrechen von Aktionen, weitere Hooks (Inventar, Dialog, Schaden).

### Spieltest 2026-11-02 (Nutzer) und Folgen
- Protokoll: 6 von 6 Hooks aktiv; Kill per Zauber ergab OnSpellCast (Spl_Fireball, Spl_HealOther) -> OnNPCKilled "Jens", danach OnReceiveItem fuer die Beute
  (Jens -> PC_Hero); OnNPCDefeated kam bei einem besiegten (nicht getoeteten) Slave. Das Spiel ruft die Quest-Eintrittspunkte also immer, nicht nur bei aktiven Quests.
- OnEnter/OnLeave melden fast nur interne NPC-Navigationszonen (Nav_Zone, Nav_Path, Nav_NavZone 1 Edge; ~1180 Zeilen in 2 Minuten):
  jetzt standardmaessig herausgefiltert (`Nav_*`); ini `[patch] navevents=1` meldet sie wieder.
- OnReceiveItem hat jetzt zusaetzlich den Itemnamen (Aenderung der Signatur, API 5): Name der Template-Entity zur GUID.

## Quest-API (Schritt 3, API 6, 2026-11-02)
- src/quest_bindings.cpp: Game.dll `gCQuestManager_PS` (Singleton `GetInstance`) und `gCQuest_PS`; alles unter SEH-Schutz, ohne Spiel false/-1.
- Skript: `QuestExists(name)`, `CreateQuest(name)` (AddQuest, true wenn sie danach existiert), `RunQuest`, `SucceedQuest`, `FailQuest`, `CloseQuest`, `RemoveQuest`,
  `GetQuestStatus(name)` (-1 unbekannt; Wert aus bTPropertyContainer bei Quest+0xa8 -> +4), `SetQuestStatus(name, status)` (gCQuest_PS::SetStatus),
  `IsQuestFinished`, `IsQuestSucceeded`, `GetQuestXP/SetQuestXP`, `ReloadQuestFiles()` (LoadQuestsFromInis). Konstanten QUEST_OPEN=0, RUNNING=1, SUCCESS=2,
  FAILED=3, OBSOLETE=4, CANCELLED=5, LOST=6, WON=7. Selftest 56/56.
- Beispiel-Mod examples/mods/quest_demo (Numblock 1-6: Status, anlegen+starten, erfolgreich, fehlgeschlagen, entfernen, XP; Bild hoch: auflisten, Einfg: neu laden).
- NOCH NICHT im Spiel getestet: ob eine per Skript angelegte Quest im Tagebuch erscheint / gespeichert wird; ob die Status-Offsets stimmen.
- Offen: Quest-Texte (LogTopic/LogText sind eCLocString = Text-ID, die Texte kommen aus der Stringtabelle -> eigene IDs brauchen einen Override auf
  eCLocString::GetString), LogText-Array (bTObjArray, Template nicht exportiert), Questliste (bTObjArray<bTAutoPOSmartPtr<gCQuest_PS>> bei Manager+0x14),
  Typ setzen, Quest-ini-Dateien aus Mods.

## Inventar + Jens-Quest (API 7, 2026-11-02)
- `Entity.GetItemCount(itemName)` (Summe ueber alle Staepel, -1 = nicht lesbar), `Entity.RemoveItems(itemName, n)` (Rueckgabe = entfernte Stuecke): Script.dll
  `PSInventory(eCEntity*)` -> gCInventory_PS::FindStackIndex(bCString)/GetStack/DeleteItems. itemName = Template-Name ("It_Wolf_Fur", wie in OnReceiveItem).
- Beispiel-Mod examples/mods/jens_quest: Naehert sich der Held Jens (<= 4 m), startet die Quest "OG3_Jens_Wolfsfell" (Bitte als Bildschirmtext);
  Abgabe mit der Rollen-Taste bei Jens (entfernt ein It_Wolf_Fur) oder per OnReceiveItem (Held -> Jens); Belohnung 100 Quest-XP; Tod von Jens = Quest gescheitert.
- Ohne Dialogsystem (Info-API noch offen) ersetzt Naehe + Taste das Gespraech. UNGEPRUEFT im Spiel: Itemname des Stapels, ob SucceedQuest die XP vergibt.

## Quest-Texte, Questliste (API 8/9, 2026-11-02)
- Spieltest (Nutzer): Quest startet, Status 1/2 stimmen, Inventar-Abgabe geht. Quest-Titel im Log leer (Text-ID existiert nicht in der Stringtabelle); Questlog des Spiels
  besteht aus Dialogen (Infos), daher leer ohne Dialog-API. Das Spiel vergibt beim Abschluss selbst 500 XP (Meldung "Quest abgeschlossen") unabhaengig von SetQuestXP(100).
- Questliste funktioniert: gCQuestManager_PS+0x14 = {Daten, Anzahl, Kapazitaet}, Elemente = gCQuest_PS* (571 Quests). GetQuestCount/GetQuestNameAt.
- Texte: `RegisterText(id, text)` + eCLocString-Hooks (Engine.dll GetString(), GetString(bCString), operator bCUnicodeString): registrierte IDs liefern unseren Text
  (Windows-1252 -> Unicode), sonst Original. SetQuestTopic/SetQuestDescription legen dafuer IDs OG3_QTOPIC_<Quest>/OG3_QDESC_<Quest> an.
- DumpQuest(name, pfad) mit relativem Pfad schlug fehl (false); neu: GetQuestDir(), GetQuestFilePath(name), GetGameDir() zum Ausprobieren absoluter/Spielpfade.

## Modkit-Dokumentation
Einsteigerseiten (Deutsch, nach dem Vorbild des G1R-Modkits): docs/modkit/index.md (Mod-Ordner, Scripting-Guide, Ereignisse, API-Referenz, Quests, Beispiele, Grenzen).

## Dialog-API (API 10/11, 2026-11-02)
- Spieltest: 4591 Infos lesbar (gCInfoManager_PS::AccessInfos = {Daten, Anzahl, Kapazitaet}, Elemente gCInfo_PS*). gCInfo_PS+0x100 = bTPropertyObject-Huelle, das
  gCInfoScript_PS wird durch vtable-Vergleich gefunden. Jens hat 11 Infos (u. a. Quest KapDun_Hunter_Fur = zehn Wolfsfelle, Activator/PartDelivery/Delivery).
- Info = Name, Owner (NPC), Parent, Quest, CondType (gEInfoCondType: 3 General, 5 Open, 6 Activator, 7 Running, 8 Delivery, 9 PartDelivery, 10 Success, 12 Failed),
  Type (3 Info, 2 News), Skriptzeilen: `Say | player/npc | | ID1 (z. B. SD_Thoughtful) | | Text`, `SetGameEvent`/`ClearGameEvent` (ID1), `SetTradeEnabled`/`SetTeachEnabled`
  (npc, ID1 true), `CloseQuest` (ID1 = Quest), `SetRoutine` (Entity, ID1), `SetSlaveryEnabled`, `Give` (npc, player, ID1 Item, ID2 Menge), `End`, `GiveXP` (ID1).
  `DescribeCommands()` listet das gesamte Vokabular mit Beispiel (Bild runter im quest_demo).
- Schreiben: CreateInfo(name, owner), SetInfoQuest/Parent/Type/Condition, AddInfoLine(info, cmd, e1, e2, id1, id2, text) (Text wird als OG3_INFO_<info>_<n> registriert),
  RebuildInfos() (gCInfoManager_PS::BuildMapInfosByOwners), RemoveInfo, InfoExists. Ereignis OnInfoFinished(info, owner, quest) per Hook auf OnInfoScriptFinished.
- Mod jens_dialog: "Hast du Arbeit fuer mich?" startet die Quest, "Ich habe das Wolfsfell." (Cond Running + Quest) gibt sie ab. UNGEPRUEFT im Spiel.

## Speichern/Laden (API 14, 2026-11-02)
- Spieltest: Per Skript angelegte Quests/Infos sind Laufzeit-Objekte und fehlen nach dem Laden eines Spielstands.
- Loesung fuer Mods: Schalter im Spielstand (`SetGameEvent/ClearGameEvent/IsGameEventSet`, Script.dll PSPlayerMemory des Helden, vom Spiel mitgespeichert; IsInfoGiven/SetInfoGiven).
  Mod legt Quest und Dialoge nach jedem Laden neu an (OnFrame prueft InfoExists/QuestExists) und stellt den Stand aus den Schaltern wieder her
  (Quest laeuft / Status Erfolg bei Belohnung 0, Info "gegeben" fuer das Questlog). UNGEPRUEFT im Spiel.

## Laden/Speichern, Teil 2 (API 16, 2026-11-02)
- Spieltest: AddItems funktioniert (Parameterreihenfolge CreateItems(Vorlage, Qualitaet, Menge) per Disassembly). Nach dem Laden sind Quest und Dialoge weg (Manager hat 570 Quests);
  RunQuest beim Wiederherstellen loest erneut die Meldung "Quest angenommen" aus (stattdessen SetQuestStatus); unsichtbare Info mit AddQuestLog via ExecuteInfo laeuft nicht (kein OnInfoFinished).
- Disassembly: gCQuest_PS::WriteRuntimeData schreibt Status, Aktivierungszeit, Lieferzaehler und das Log (LogText); die Ladefunktion des Quest-Managers sucht die Quest nach Namen
  und liest die Laufzeitdaten hinein. Eigene Quests muessen also VOR dem Einlesen existieren.
- Neu: Hooks auf LoadQuestsFromInis/LoadInfosFromInis -> Ereignisse `OnQuestsLoaded()`/`OnInfosLoaded()`; Protokollhooks fuer ResetQuests, Quest-ReadRuntimeData/WriteRuntimeData, Info-ReadSaveGame
  (zeigt die Reihenfolge beim Laden/Speichern). jens_dialog legt Quest und Infos dort an; Notfall-Wiederherstellung ueber Schalter bleibt; Jens-Tod => FailQuest + Schalter OG3_JensWolf_Failed.

## Override-Hooks (Schritt 2b, API 17, 2026-11-02)
- Spielfunktionen abfangen, aendern oder abbrechen: ein Mod definiert `bool Override_<Name>(<Argumente>)`. Der Hook ruft diese Funktion statt des Originals; `Super()` ruft das Original
  (bzw. den naechsten Mod der Kette, Reihenfolge wie load_order), die Rueckgabe des Overrides ist das Ergebnis fuer das Spiel. Ohne Super() wird das Original uebersprungen (Abbrechen).
  Die Nachereignisse On<Name> feuern nur, wenn das Original lief. Skriptfehler im Override => das Spiel bekommt sein Original.
- Verfuegbar (7 Hooks): Override_NPCKilled(Entity), Override_NPCDefeated(Entity), Override_ReceiveItem(Entity, Entity, name, id, uint), Override_Enter/Leave(Entity, area),
  Override_SpellCast(Entity, Entity, spell) und neu Override_InfoAvailable(info, owner, quest) = Hook auf gCInfo_PS::IsAvailable (Dialogoption sichtbar?).
- Nicht moeglich: Argumente aendern (Super() ruft immer mit den Originalargumenten), Rueckgabewerte ausser bool. Kein exportierter Schadens-/Treffer-Eintrittspunkt in Game.dll gefunden.
- Tests: selftest 67/67, tests\mods 6/6 (Kette zeta -> alpha -> Original). Beispiel: examples/mods/hooks_demo (Jens' Mod-Arbeit nur 8-20 Uhr; Itemuebergaben mitschreiben).

## Datei-Patches (Schritt 4, 2026-11-02)
- src/file_patches.cpp: `<Mod>\ScriptPatches\*.patch` (Unified Diff) werden beim Start (og3_patch_start, vor dem ersten Lesen durch das Spiel) auf Datendateien angewendet.
  Basis = eigene physische Datei oder G3V0-Archiv (Data\<Archiv>.pak, unkomprimierte Eintraege; Quests/Infos sind unkomprimiert, stringtable.ini nicht), Ergebnis als physische Overlay-Datei
  Data\<Archiv>\<Pfad> (Dateisystem-Modul bevorzugt physische Dateien); Liste og3\patched_files.txt, beim naechsten Start entfernt und neu berechnet. Mods nur mit ScriptPatches (ohne main.as) werden erkannt.
- Getestet ohne Spiel (OG3_TEST_PATCHES="<Spielordner>|<Modordner>"): Stapeln ueber Mods, kaputter Patch uebersprungen, neue Datei, idempotent, CRLF erhalten, Overlay verschwindet mit dem Mod.
- Mod patch_demo (Abbas_Artefakt ExperiencePoints 300 -> 600). OFFEN: Bestaetigung im Spiel; ob Infos-Patches wirken (Infos werden beim Laden aus Infos.cpt gelesen).
