# API-Referenz (Skript-Schnittstelle Version 17)

Alle Funktionen sind global, soweit nicht ein Typ davor steht. Ohne laufendes Spiel (Tests) liefern die Engine-Funktionen Standardwerte (`false`, `-1`, leerer Text).

## Allgemein

| Funktion | Beschreibung |
|---|---|
| `void print(const bCString &in)` | Schreibt `[Modname] text` ins Protokoll (und ins Gothic-3-Protokoll) |
| `bCString ToString(int / uint / float / double / bool)` | Zahl oder bool als Text |
| `float GetFrameTime()` | Dauer des letzten Frames in Sekunden (skaliert) |
| `uint GetTimeStamp()` | Zeitstempel des Spiels (`bCTimer`) |
| `int GetApiVersion()` | Version der Skript-Schnittstelle (aktuell 9) |
| `void check(bool, const bCString &in)` | Selbsttest: meldet im Protokoll, wenn die Bedingung falsch ist |
| `bCString GetGameDir()` | Spielordner mit abschließendem `\` |
| `bCString PackMod(ordner, volume, bool alternativeBalancing = false)` / `bool PackFolder(ordner, datei)` / `bCString NextModFile(volume, bool = false)` / `bCString GetPackError()` | Ordner als Mod-Archiv packen (nächste freie Generation `.mod`, `.m00`, ... in `Data`); siehe [Mod-Archive](archives.md) |

Aus AngelScript-Add-ons: `array<T>` (`insertLast`, `length`, `[]`, ...) und `scriptmath` (`sin`, `cos`, `sqrt`, `abs`, `pow`, `floor`, ... mit `float`).

## Fenster und Eingabe

| Funktion | Beschreibung |
|---|---|
| `void SetWindowTitle(const bCString &in)` | Fenstertitel setzen |
| `bool IsKeyDown(int vk)` | Taste gerade gedrückt (nur wenn das Spiel im Vordergrund ist) |
| `void PlaySystemSound()` | Windows-Systemton |
| Konstanten `KEY_F1` ... `KEY_F12` | Tastencodes der Funktionstasten |

## Anzeige

| Funktion | Beschreibung |
|---|---|
| `bool PrintScreen(const bCString &in text, float seconds = 3, int r = 255, int g = 255, int b = 255)` | Text auf dem Bildschirm (wie die Debug-Ausgabe des Spiels, ohne Präfix) |
| `bool PrintGameMessage(const bCString &in text, int type = 0)` | Spielmeldung |
| `int GetActivePage()` | Aktive Spielseite (Menü, Inventar, ...), Werte nur teilweise bekannt: -1 = Spiel, 5, 6, 17 treten im Spiel auf |

## Spielwelt

| Funktion | Beschreibung |
|---|---|
| `Entity GetHero()` | Der Held |
| `Entity GetPlayer()` | Der Spieler (gCSession) |
| `Entity FindEntity(const bCString &in name)` | Entity nach Name (`"Jens"`, `"PC_Hero"`), ungültig wenn nicht gefunden |
| `int GetGameDay()`, `GetGameHour()`, `GetGameMinute()` | Spielzeit, `-1` ohne Welt |
| `bool SetGameTime(int day, int hour, int minute)` | Spielzeit setzen. `day = -1` behält den Tag |

### Typ `Entity`

| Methode | Beschreibung |
|---|---|
| `bool IsValid() const` | Entity existiert und ist lesbar |
| `bCString GetName() const` | Name |
| `bCVector GetPosition() const` | Weltposition |
| `bCQuaternion GetRotation() const` | Weltrotation |
| `float GetDistanceTo(const Entity &in) const` | Abstand (≈ cm), `-1` bei ungültigen Entities |
| `bool SetPosition(const bCVector &in)` | Teleport |
| `bool opEquals` (`==`) | Gleiche Spielobjekte |
| `int GetAttribute(const bCString &in tag) const` | Aktueller Wert |
| `int GetAttributeBase(tag) const`, `int GetAttributeMax(tag) const` | Grund- und Höchstwert |
| `bool SetAttributeBase(tag, int)`, `AddAttributeBase(tag, int)` | Grundwert setzen/erhöhen (wie das Spiel bei Lernpunkten) |
| `bool SetAttribute(tag, int)`, `AddAttribute(tag, int)` | Aktuellen Wert setzen/erhöhen |
| `int GetItemCount(const bCString &in itemName) const` | Anzahl aller Stapel dieses Items, `-1` = Inventar nicht lesbar |
| `int RemoveItems(const bCString &in itemName, int count)` | Entfernt bis zu `count` Stück, gibt die entfernte Zahl zurück (`-1` = Fehler) |
| `int AddItems(const bCString &in itemName, int count)` | Erzeugt Items im Inventar (`>= 0` ok, `-1` Inventar nicht lesbar, `-2` Item unbekannt, `-4`/`-5` Funktion fehlt). Fehler stehen im Protokoll |
| `int CloneTemplate(const bCString &in base, const bCString &in newName)` (global) | Klont die Item-Vorlage `base` im Speicher unter dem Namen `newName` (`eCNode::Clone` + `SetName`). `0` angelegt, `1` war schon da, `-2` Basis unbekannt, `-6`/`-7` Klon fehlgeschlagen. Danach `AddItems(newName, n)`. Name und Beschreibung des Items stehen in der Stringtabelle unter `FO_<newName>` und `ITEMDESC_<newName>`: mit `RegisterText` setzen, sonst bleiben sie leer. Die Vorlage bekommt eine feste ID (MD5 von `OG3Clone:<newName>`) und ist in jedem Spielstart dieselbe: Klon-Items im Spielstand bleiben nach dem Neustart erhalten. Dazu muss der Mod `CloneTemplate` in `OnQuestsLoaded` aufrufen (vor dem Einlesen des Spielstands; in `main` ist das Spiel noch nicht bereit, Code `-4`) |
| `Entity::GetCurrentAttacker()` (ab Version 26) | Aktueller Gegner eines NPCs oder des Helden (`gCNPC_PS::CurrentAttackerEntity`), bei keinem Gegner oder Nicht-NPC ein ungültiges Entity. Beruht auf der KI-Zielwahl: im Kampf mit mehreren Gegnern nur eine Näherung für den Schläger |
| `Entity::GetCategory()` / `Entity::IsBuilding()` | Grobe Einordnung eines Entities: `"NPC"`, `"Building"`, `"Object"`, `"Landscape"`, `"Border"`, `"Unnamed"` (kein Name), `"Other"`, bei ungültigem Entity leer. Beruht auf Namensmustern, die im Spiel beobachtet wurden (`G3_Myrtana_House_*` = Gebäude, `G3_Object_*` = Interaktionsobjekte, `G3_Myrtana_Landscape_*`, `G3_World_Border_*`); unbenannte Treffer (Bäume, Steine, Kleinteile) lassen sich so nicht bestimmen |
| `Entity::GetTemplateName()` | Name der Vorlage, aus der das Entity entstand (leer, wenn unbekannt). Hilft bei unbenannten Treffern des Fadenkreuzes |
| `Entity::GetBoundsMin()` / `GetBoundsMax()` | Hüllquader des Entities samt Kindern in Weltkoordinaten (Größe: Max − Min). Nullvektor bei ungültigem Entity |
| `Entity::GetParent()` | Elternobjekt (das Fadenkreuz trifft oft Teilobjekte eines Gebäudes, das Elternobjekt ist dann das Gebäude). Ungültig, wenn keines da ist |
| `Entity GetFocusedEntity()` (global) | Das Entity, das der Held gerade anvisiert (Fokusname/Interaktion, `gCFocus_PS::GetFocusEntity`). Ungültiges Entity, wenn nichts anvisiert wird. Im Spiel bestätigt; Beispiel `examples/mods/focus_demo` (F7) |
| `Entity PickCenter(float maxDistance = 100000)` (global) | Das Entity genau in der Bildmitte (unter dem Fadenkreuz) samt **exaktem Trefferpunkt** (`GetPickPoint()`): die Engine schießt von der aktuellen Kamera einen Strahl (`eCCameraBase::PointToRay` + `eCPhysicsScene::TraceRayNearestHit`), der **Held wird dabei ausgeschlossen** (sonst träfe die Verfolgerkamera zuerst den Helden). Ungültiges Entity, wenn nichts getroffen wird. Im Spiel bestätigt |
| `Entity PickScreen(int x, int y, float maxDistance = 100000)` (global) | Dasselbe für einen beliebigen Bildschirmpixel (Pixel der Client-Fläche des Spielfensters), z. B. für eine Mausposition |
| `bCVector GetPickPoint()` / `bool HasPickPoint()` (global) | Weltpunkt des letzten `PickCenter`/`PickScreen`-Treffers und ob es einen gab. Damit: `SpawnNPC("Diego", GetPickPoint())` |
| `Entity GetCrosshairEntity()` (global) | Das Entity unter dem Fadenkreuz (`gCFocus_PS::GetCrossHairEntity` des Spielers). Ungültig, wenn keines getroffen wird. Im Spiel bestätigt |
| `bCVector GetPickPosition()` (global) | `gCFocus_PS::GetCurrentPickPosition`. **Liefert im Spiel immer den Nullvektor** (auch bei sichtbarem Fadenkreuz); stattdessen `PickCenter()` und `GetPickPoint()` benutzen |
| `bool SetCrosshairVisible(bool on)` (global) | Schaltet das Fadenkreuz des Spielers ein oder aus (`gCFocus_PS::SetDrawCrossHair`). Versuch, den Zielpunkt zu aktualisieren, wenn `GetPickPosition()` nur den Nullvektor liefert |
| `Entity SpawnNPC(const bCString &in templateName, const bCVector &in position)` (global) | Erzeugt aus einer Vorlage ein Entity (NPC, Objekt) in der Spielwelt: `gCWorld::CreateDynamicEntity` + `eCEntity::CreateFromTemplate`, dann Position. Gibt das Entity zurück (ungültig bei Fehler, Code im Protokoll: `-2` Vorlage unbekannt, `-5` keine Welt, `-6`/`-7` Erzeugen fehlgeschlagen). Der Vorlagenname muss exakt stimmen, siehe `FindTemplateNames`. Im Spiel bestätigt |
| `bCString FindTemplateNames(const bCString &in part, int max = 20)` (global) | Sucht Vorlagen nach Teilname: `"Name1;Name2;..."`, leer ohne Treffer |
| `bool DestroyNPC(const Entity &in)` (global) | Entfernt ein mit `SpawnNPC` erzeugtes Entity wieder (`gCWorld::DestroyDynamicEntity`). Nur NPCs (Entities mit `gCNPC_PS`), und nur solche dieser Sitzung oder aus dem Systemsektor der Welt (dorthin gehören erzeugte NPCs, auch nach Speichern und Laden). Welt-Objekte (auch im Systemsektor, z. B. `G3_World_Border_001`), Welt-NPCs, Held und Spieler werden abgelehnt (`false`, Grund im Protokoll). Den Griff danach nicht mehr verwenden |
| `bool IsNPC(const Entity &in)` (global) | `true`, wenn das Entity ein NPC ist (hat ein `gCNPC_PS`) |
| `bool IsDynamicSpawn(const Entity &in)` (global) | `true`, wenn das Entity ein NPC im Systemsektor ist (per `SpawnNPC` erzeugt oder ähnlich dynamisch) und nicht Held/Spieler |
| `int CountEntities(const bCString &in part)` / `Entity GetEntityByPart(const bCString &in part, int index)` (global) | Entities nach Teilname suchen: Anzahl und Eintrag. Beim Entfernen rückwärts durchlaufen |
| `bool Entity::SetRotation(const bCQuaternion &in)` | Setzt die Drehung (`SetLocalRotation`; Y ist oben). Für erzeugte NPCs = Weltdrehung |
| `bool Entity::LookAt(const Entity &in target, float yawOffsetDegrees = 0)` | Dreht das Entity um die Hochachse zum Ziel (Gier = atan2(dx, dz)). Schaut es vom Ziel weg, Versatz 180 angeben |

Attribut-Kennungen: `"STR"`, `"DEX"`, `"INT"`, `"HP"`, `"MP"`, `"SP"`, `"XP"`, `"LP"`, `"ALC"`, `"SMI"`, `"THF"` und weitere (wie `gCPlayerMemory_PS`).
Item-Namen sind die Namen der Item-Vorlagen: `It_Wolf_Fur`, `It_Water`, `It_Plant_Health_02`, `It_1H_Longsword_01`, ... Sie erscheinen im Protokoll von `OnReceiveItem`.

## Quests

Ausführlich auf der Seite [Quests](quests.md).

| Funktion | Beschreibung |
|---|---|
| `bool QuestExists(name)`, `bool CreateQuest(name)` | Prüfen / anlegen |
| `bool RunQuest(name)`, `SucceedQuest(name)`, `FailQuest(name)`, `CloseQuest(name)`, `RemoveQuest(name)` | Steuern |
| `int GetQuestStatus(name)`, `bool SetQuestStatus(name, int)` | Status lesen/setzen (`QUEST_*`) |
| `bool IsQuestFinished(name)`, `IsQuestSucceeded(name)` | Abfragen |
| `int GetQuestXP(name)`, `bool SetQuestXP(name, int)` | Erfahrungspunkte der Quest |
| `bool SetQuestTopic(name, text)`, `SetQuestDescription(name, text)` | Titel und Beschreibung (eigene Texte) |
| `void RegisterText(id, text)` | Eigenen Text für eine Text-ID des Spiels registrieren |
| `int GetQuestCount()`, `bCString GetQuestNameAt(int)` | Alle Quests des Spiels aufzählen |
| `bool ReloadQuestFiles()` | `LoadQuestsFromInis` |
| `bCString GetQuestDir()`, `GetQuestFilePath(name)` | Pfade der Quest-Dateien laut Spiel |
| `bool DumpQuest(name, path)`, `LoadQuestFile(name, path)` | Quest in/aus Datei (bisher ohne Erfolg im Spiel) |
| `bool SetQuestReward(name, attribut, menge)`, `bCString GetQuestReward(name)` | Attribut-Belohnung der Quest (z. B. `"DEX"`, 1): erscheint in der Erfolgsmeldung und wird vom Spiel vergeben |
| `bCString DescribeQuestRewards()` | Alle Attribut-Kennungen, die Quests des Spiels vergeben |
| Konstanten | `QUEST_OPEN=0`, `QUEST_RUNNING=1`, `QUEST_SUCCESS=2`, `QUEST_FAILED=3`, `QUEST_OBSOLETE=4`, `QUEST_CANCELLED=5`, `QUEST_LOST=6`, `QUEST_WON=7` |

## Hooks

`bool Super()` ruft in einer `Override_...`-Funktion das Original (bzw. den nächsten Mod der Kette) auf. Alle Overrides stehen auf der Seite [Hooks](hooks.md). `bool SetOverrideArg(int index, uint wert)` ändert dabei ein Zahlenargument der Kette (`uint GetOverrideArg(int index)` liest es); `Super()` reicht den neuen Wert weiter.

## Schalter im Spielstand

| Funktion | Beschreibung |
|---|---|
| `bool SetGameEvent(name)`, `ClearGameEvent(name)`, `IsGameEventSet(name)` | Ja/Nein-Schalter im Spielstand (siehe [Speichern und Laden](saving.md)) |

## Dialoge

Siehe [Dialoge](dialogs.md): `GetInfoCount`, `GetInfoName`, `GetInfoOwner`, `DescribeInfo`, `DescribeCommands`, `InfoExists`, `CreateInfo`, `RemoveInfo`, `SetInfoQuest`, `SetInfoParent`,
`SetInfoType`, `SetInfoCondition`, `AddInfoLine`, `RebuildInfos`, `SetInfoGiven`, `IsInfoGiven`; Konstanten `INFO_COND_*` und `INFO_TYPE_*`.

## Typen

### `bCString`
`+`, `+=`, `==`, `<`, `+ int/uint/float/double/bool`, `GetLength()`, `IsEmpty()`, `Compare(s)`, `CompareNoCase(s)`, `Find(s, start = 0)`, `Contains(s)`,
`Left(n)`, `Right(n)`, `Mid(start, n)`, `ToUpper()`, `ToLower()`, `GetUpper()`, `GetLower()`.

### `bCVector` (`x`, `y`, `z`)
`GetMagnitude()`, `GetSquareMagnitude()`, `Normalize()`, `GetNormalized()`, `GetDotProduct(v)`, `GetCrossProduct(v)`, `+`, `-`, `* float`, `/ float`, `-v`, `==`, `ToString()`.

### `bCVector2` (`x`, `y`), `bCVector4` (`x`, `y`, `z`, `w`)
Felder, `GetMagnitude()`, `Normalize()`. `bCVector2` hat zusätzlich `+`, `-`, `*`.

### `bCQuaternion` (`x`, `y`, `z`, `w`, Standard = Einheit)
`Clear()`, `Normalize()`, `Invert()`, `GetMagnitude()`, `*`, `SetQuaternion(bCMatrix3)`.

### `bCMatrix3` (Standard = Einheit)
`GetElement(row, col)`, `SetElement(row, col, f)`, `SetIdentity()`, `Transpose()`, `Invert()`, `* Matrix`, `* Vektor`,
`SetToRotationAxisX/Y/Z(Bogenmaß)`, `SetToRotation(bCQuaternion)`.

### `bCMatrix4` (Standard = Einheit, Version 19)
4x4-Matrix der Engine (`bCMatrix`), Speicherlage 16 Floats zeilenweise. **Zeilenvektor-Konvention** (`v' = v * M`): Zeilen 0 bis 2 sind die Achsen X, Y, Z, Zeile 3 die Verschiebung.
`GetElement(row, col)`, `SetElement(row, col, f)`, `SetIdentity()`, `Transpose()`, `Invert()`, `GetInverted()`, `GetDeterminant()`, `* bCMatrix4`, `* bCVector` (Punkt mit Verschiebung transformieren),
`GetTranslation()`, `SetTranslation(v)`, `GetXAxis()`, `GetYAxis()`, `GetZAxis()`, `SetToTranslation(v)`, `SetToScaling(v)`, `SetToRotation(bCQuaternion)`,
`SetToRotationAxisX/Y/Z(Bogenmaß)`, `GetRotationMatrix()` (3x3-Teil als `bCMatrix3`), `GetRotation()` (als `bCQuaternion`).
`Entity::GetWorldMatrix()` liefert die Weltmatrix eines Entities (Einheitsmatrix bei ungültigem Entity). Die Rechenfunktionen laufen ebenfalls über die SharedBase.

Die Rechenfunktionen laufen über die SharedBase des Spiels (Originalverhalten, einschließlich der Rundung der Original-DLL).
