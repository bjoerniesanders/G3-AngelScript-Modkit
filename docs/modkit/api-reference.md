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
| `bCString GetProperty(const Entity &in e, const bCString &in set, const bCString &in prop)` (global, ab Version 29) | Liest eine Eigenschaft des Eigenschaftssets eines Entities **als Text**, über den allgemeinen Eigenschaftszugriff der Engine. `set` ist der Typ des Sets, z. B. `"Item"`, `"gCItem_PS"`, `"Damage"`, `"NPC"`, `"Inventory"` (alle Werte von `eEPropertySetType`, Groß-/Kleinschreibung egal); `prop` der Eigenschaftsname wie im SDK (`"GoldValue"`, `"RequiredSkill1"`, `"DamageAmount"`). Leer, wenn Entity, Set oder Eigenschaft fehlen. Nur Lesen. Der Textwert kommt von `ConvertValueToString` und ist bei Verweisen (Vorlagen, Entities) und Listen nicht immer aussagekräftig |
| `Entity GetTemplateEntity(const bCString &in name)` (global, ab Version 29) | Die Vorlage (Template-Entity) mit genau diesem Namen, z. B. `"Perk_OrcSlayer"` oder `"It_1H_OrcSlayer_01"`; ungültig, wenn es sie nicht gibt. Ihre Eigenschaftssets lassen sich mit `GetProperty`/`ListProperties` lesen, ohne etwas zu erzeugen. Wird für `set` ein Name verwendet, den `eEPropertySetType` nicht kennt, gilt er als Klassenname (`"gCSkill_PS"`) |
| `bool Entity::LearnSkill(name)`, `ActivateSkill(name)`, `DeactivateSkill(name)`, `bool HasLearnedSkill(name)`, `bool IsSkillActive(name)`, `int GetSkillActivationCount(name)` (ab Version 30) | Talente im Inventar des Helden (`gCInventory_PS`). `name` ist die Talent-Vorlage, z. B. `"Perk_1H_1"`. Gelernte Talente stehen als Stapel im Inventar, `ActivateSkill` zählt den Aktivierungszähler hoch (so macht es eine Waffe mit `ActivateSkill1/2`, solange sie geführt wird), `DeactivateSkill` zählt herunter. Ersatz für das nicht schreibbare `ActivateSkill2`. `false`/`-1` bei unbekanntem Talent oder ohne Inventar |
| `bool SetTemplateRef(const Entity &in e, const bCString &in klasse, const bCString &in prop, const bCString &in ziel)` (global, ab Version 30) | Setzt eine **Verweis-Eigenschaft** auf eine Vorlage, z. B. `SetTemplateRef(waffe, "gCItem_PS", "ActivateSkill2", "Perk_Lifesteal")` oder `(lernItem, "gCItem_PS", "Skill", ...)`. `klasse` ist der Klassenname des Eigenschaftssets, `prop` der Name ohne Präfix (es wird `gCxxx_PS::Access<prop>()` aufgerufen, dann `eCTemplateEntityProxy::SetEntity`). Ersatz für `SetProperty` bei Verweisen, die per Text nicht gehen. Nur für Vorlagen, die die Mod selbst angelegt hat. Experimentell |
| `int Entity::DeleteItemStack(const bCString &in name)` (ab Version 30) | Entfernt alle Stapel mit diesem Item-Namen samt Stapel (`gCInventory_PS::DeleteStack`); Rückgabe: Zahl entfernter Stapel, `-1` bei Fehler. Nötig bei dauerhaften Items (`Permanent`, z. B. Lern-Items von Talenten): `RemoveItems` verringert nur die Menge und lässt einen leeren Stapel stehen |
| `bool SetProperty(const Entity &in e, const bCString &in set, const bCString &in prop, const bCString &in value)` (global, ab Version 29) | **Schreibt** eine Eigenschaft aus Text (`ConvertStringToValue`, Zuweisen). `true`, wenn die Eigenschaft existiert und der Aufruf ohne Fehler lief; ob der Wert so gelesen wurde, mit `GetProperty` prüfen. **Textwerte (z. B. `Tag`) lassen sich schreiben; Referenz-Eigenschaften auf andere Vorlagen (z. B. `ActivateSkill2`) nicht: `true` ohne Wirkung, getestet mit 11 Schreibformen (Name, Name+GUID, GUID mit/ohne Klammern).** Nur an Vorlagen und Entities verwenden, die die Mod selbst angelegt hat (`CloneTemplate`); ob Änderungen an Vorlagen in Spielständen bleiben, ist nicht untersucht. Experimentell |
| `bCString ListProperties(const Entity &in e, const bCString &in set, int max = 200)` (global, ab Version 29) | Alle Eigenschaften des Sets samt Basisklassen als `"Name=Wert;Name=Wert;..."` (höchstens `max`, bis 600; Semikolons in Werten werden zu Leerzeichen). Zum Herausfinden, was ein Item, ein Zauber oder ein NPC im Spiel wirklich enthält; die Mod `property_inspect` zeigt es |
| `Entity::GetSlotItem(int slot)` (ab Version 28) | Das Item im Ausrüstungs-Slot als Entity (ungültig bei leerem Slot); `GetName()` liefert den Vorlagennamen, z. B. `It_1H_OrcSlayer_01`. Slots wie bei `GetSlotQuality` |
| `Entity::GetSlotQuality(int slot)` / `Entity::SetSlotQuality(int slot, uint bits)` (ab Version 28) | Qualitätsbits des Inventarstapels im Ausrüstungs-Slot (`SLOT_RIGHT_HAND` 1, `SLOT_LEFT_HAND` 2, `SLOT_BACK_LEFT` 5, `SLOT_BACK_RIGHT` 6, `SLOT_BOW` 7, `SLOT_AMMO` 14). Bits: `QUALITY_DISEASED` 2, `QUALITY_POISONED` 4, `QUALITY_BURNING` 8, `QUALITY_FROZEN` 16, `QUALITY_SHARP` 32, `QUALITY_BLESSED` 64, `QUALITY_FORGED` 128, `QUALITY_WORN` 256 und **`QUALITY_LIFESTEAL` 512** (eigenes Bit dieses Patches). Die Beschreibung der Waffe zeigt gesetzte Bits als Zeile mit Symbol und Text; für `QUALITY_LIFESTEAL` kommt der Text aus dem Schlüssel `HUD_Quality_Lifesteal` der Mod-Stringtabelle (Präfix `(20 %)`, Symbol der Segnung). Verschiedene Qualitäten sind verschiedene Stapel und werden mit dem Spielstand gespeichert. `Get` liefert 0 bei leerem Slot oder ungültigem Entity, `Set` gibt `false` zurück, wenn der Slot leer ist. Die Inventaranzeige aktualisiert sich eventuell erst beim nächsten Öffnen |
| `Entity::GetStatusEffects()` / `Entity::HasStatusEffect(int)` / `Entity::SetStatusEffect(int, bool)` (ab Version 27) | Statuseffekte eines NPCs oder des Helden als Bitmaske (`gCNPC_PS`): `STATUS_BURNING` 1, `STATUS_FROZEN` 2, `STATUS_POISONED` 4, `STATUS_DISEASED` 8, `STATUS_TRANSFORMED` 16, `STATUS_IN_FOG` 64, `STATUS_GHOST` 128, `STATUS_IN_MAGIC_BARRIER` 256, `STATUS_SOULTRAVEL` 512. Bei ungültigem Entity oder Nicht-NPC `0`/`false`. `SetStatusEffect` ruft die Spielfunktion `EnableStatusEffect` auf (löst dabei auch `OnStatusEffect` aus). Ob und wie das Spiel Folgen auslöst (z. B. keine Ausdauer-Regeneration bei Krankheit), hängt vom Spiel ab; Krankheit/Gift setzt das Spiel zusätzlich mit Zeitstempeln (`TimeStampDiseased/Poisoned`), die hier nicht gesetzt werden |
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
| `int CountTemplateNames(const bCString &in prefix)` (global, ab Version 28) | Anzahl der Vorlagen, deren Name mit `prefix` beginnt (mit Mehrfachnennungen, wie `FindTemplateNames`); billig, ohne die Namen zu lesen. `-1` ohne Spiel |
| `bCString FindTemplateNames(const bCString &in prefix, int max = 20)` (global) | Sucht Vorlagen, deren Name mit `prefix` **beginnt** (gemessen: `Teleport` findet `Teleport_Ardea`, aber nicht `Spl_Teleport_Ardea`; `_` findet nur Namen mit `_` am Anfang; **Groß-/Kleinschreibung wird ignoriert**, gemessen: `BEARD_HE` und `BEaRD_hE` liefern dasselbe): `"Name1;Name2;..."`, leer ohne Treffer. Jede Abfrage kostet etwa 3 ms je Treffer; Skripte werden nach 5 Sekunden abgebrochen, lange Listen deshalb auf mehrere Bilder verteilen (siehe `reference_dump`). Namen kommen meist doppelt vor. `max` ist bis 20000 möglich (bis Version 28 war es auf 200 begrenzt, längere Listen wurden abgeschnitten) |
| `bool DestroyNPC(const Entity &in)` (global) | Entfernt ein mit `SpawnNPC` erzeugtes Entity wieder (`gCWorld::DestroyDynamicEntity`). Nur NPCs (Entities mit `gCNPC_PS`), und nur solche dieser Sitzung oder aus dem Systemsektor der Welt (dorthin gehören erzeugte NPCs, auch nach Speichern und Laden). Welt-Objekte (auch im Systemsektor, z. B. `G3_World_Border_001`), Welt-NPCs, Held und Spieler werden abgelehnt (`false`, Grund im Protokoll). Den Griff danach nicht mehr verwenden |
| `bool IsNPC(const Entity &in)` (global) | `true`, wenn das Entity ein NPC ist (hat ein `gCNPC_PS`) |
| `bool IsDynamicSpawn(const Entity &in)` (global) | `true`, wenn das Entity ein NPC im Systemsektor ist (per `SpawnNPC` erzeugt oder ähnlich dynamisch) und nicht Held/Spieler |
| `int CountEntities(const bCString &in part)` / `Entity GetEntityByPart(const bCString &in part, int index)` (global) | Entities nach Teilname suchen: Anzahl und Eintrag. Beim Entfernen rückwärts durchlaufen |
| `bool Entity::SetRotation(const bCQuaternion &in)` | Setzt die Drehung (`SetLocalRotation`; Y ist oben). Für erzeugte NPCs = Weltdrehung |
| `bool Entity::LookAt(const Entity &in target, float yawOffsetDegrees = 0)` | Dreht das Entity um die Hochachse zum Ziel (Gier = atan2(dx, dz)). Schaut es vom Ziel weg, Versatz 180 angeben |

Attribut-Kennungen: `"STR"`, `"DEX"`, `"INT"`, `"HP"`, `"MP"`, `"SP"` (Ausdauer), `"XP"`, `"LP"`, `"ALC"`, `"SMT"` (Schmiedekunst, laut Quest-Belohnungen), `"THF"` und weitere (wie `gCPlayerMemory_PS`); gesicherte Liste und Werte in [Referenz](reference.md).
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
