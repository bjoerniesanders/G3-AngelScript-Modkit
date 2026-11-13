# API-Referenz (Skript-Schnittstelle Version 46)

Alle Funktionen sind global, soweit nicht ein Typ davor steht (`Entity::...`). Ohne laufendes Spiel (Tests) liefern die Engine-Funktionen Standardwerte (`false`, `-1`, leerer Text).

"ab Version N" nennt die Skript-Schnittstelle, mit der eine Funktion kam (`GetApiVersion()`); was sich zwischen Versionen ändern darf, steht in den [Versionsregeln](api-policy.md).

**Vermerke** (immer am Anfang der Beschreibung):

| Vermerk | Bedeutung |
|---|---|
| ohne | Benutzbar wie beschrieben. Wo es ausdrücklich gemessen ist, steht „Im Spiel bestätigt“ dabei |
| **Experimentell** | Läuft, aber Wirkung oder Grenzen sind nicht vollständig gemessen. Name, Parameter und Verhalten können sich in einer kleinen Version ändern |
| **Im Spiel nicht geprüft** | Gebaut und ohne Spiel getestet, im Spiel noch nie gemessen |
| **Funktioniert nicht** | Bleibt aufrufbar, liefert im Spiel aber kein brauchbares Ergebnis. Nicht benutzen; der Ersatz steht dabei |
| **Veraltet** | Bleibt aufrufbar und meldet beim ersten Aufruf je Mod eine Warnung. Der Ersatz steht dabei; Entfernen erst mit einer großen Version |

## Allgemein

| Funktion | Beschreibung |
|---|---|
| `void print(const bCString &in)` | Schreibt `[Modname] text` ins Protokoll (und ins Gothic-3-Protokoll) |
| `bCString ToString(int / uint / float / double / bool)` | Zahl oder bool als Text |
| `float GetFrameTime()` | Dauer des letzten Frames in Sekunden (skaliert) |
| `uint GetTimeStamp()` | Zeitstempel des Spiels (`bCTimer`) |
| `int GetApiVersion()` | Version der Skript-Schnittstelle (aktuell 46) |
| `bool IsModActive(const bCString &in name)` (ab Version 40) | `true`, wenn der Mod `name` installiert, eingeschaltet und mit erfüllten Bedingungen (`mod.ini`) geladen ist; auch für Mods ohne Skript (nur Datei-Patches oder Stringtabelle). Groß-/Kleinschreibung egal. Für optionale Zusammenarbeit; ist ein anderer Mod Pflicht, gehört er in `requires` ([Mod-Paket](mod-package.md)) |
| `bCString GetModVersion(const bCString &in name)` (ab Version 40) | `version` aus der `mod.ini` eines aktiven Mods, leer ohne Angabe oder wenn der Mod nicht aktiv ist |
| `bCString GetModSetting(const bCString &in key, const bCString &in def = "")` (ab Version 44) | Einstellung des **eigenen** Mods als Text: zuerst der Benutzerwert aus `<Spiel>\g3asm\settings\<Mod>.ini`, sonst die Vorgabe aus der `mod.ini` des Mods (Abschnitt `[settings]`), sonst `def`. Schlüssel ohne Beachtung von Groß-/Kleinschreibung. Siehe [Mod-Paket](mod-package.md) |
| `int GetModSettingInt(key, int def = 0)`, `float GetModSettingFloat(key, float def = 0)`, `bool GetModSettingBool(key, bool def = false)` (ab Version 44) | Dasselbe als Zahl oder Schalter. Ganzzahlen auch hexadezimal (`0x10`), Kommazahlen mit `.` oder `,`, Schalter `1/0`, `true/false`, `ja/nein`, `on/off`; lässt sich der Wert nicht lesen, kommt `def` |
| `bool HasModSetting(const bCString &in key)` (ab Version 44) | Gibt es einen Benutzerwert oder eine Vorgabe? |
| `bool SetModSetting(const bCString &in key, const bCString &in value)` (ab Version 44) | Schreibt einen Benutzerwert sofort in `g3asm\settings\<Mod>.ini` (die `mod.ini` bleibt unverändert, ein Update des Mods überschreibt die Werte also nicht). Schlüssel ohne `= [ ] ; #`, höchstens 100 Zeichen; Werte ohne Zeilenumbruch, höchstens 2000 Zeichen. `false` bei ungültigem Schlüssel oder wenn die Datei nicht geschrieben werden kann |
| `bool ResetModSetting(const bCString &in key)` (ab Version 44) | Entfernt den Benutzerwert, danach gilt wieder die Vorgabe |
| `bCString CallMod(const bCString &in mod, const bCString &in func, const bCString &in arg)` (ab Version 44) | Ruft im Mod `mod` die Funktion `bCString <func>(const bCString &in)` auf und liefert ihren Rückgabetext; leer, wenn Mod oder Funktion fehlen. So bietet ein Mod anderen eine Schnittstelle an; Zahlen und mehrere Werte gehen als Text (`ToString`, eigenes Trennzeichen). Während des Aufrufs läuft der Ziel-Mod (eigene Einstellungen, eigenes `print`). Höchstens 8 Aufrufe ineinander, danach leer mit Protokollzeile |
| `bool ModFunctionExists(const bCString &in mod, const bCString &in func)` (ab Version 44) | Hat `mod` eine Funktion `bCString <func>(const bCString &in)`? |
| `int SendModMessage(const bCString &in topic, const bCString &in data)` (ab Version 44) | Ruft sofort `OnModMessage(sender, topic, data)` in allen **anderen** Mods auf (Reihenfolge wie beim Laden) und liefert die Zahl der Empfänger. Siehe [Ereignisse](events.md) |
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

## Bildschirmtext

| Funktion | Beschreibung |
|---|---|
| `bool PrintScreen(const bCString &in text, float seconds = 3, int r = 255, int g = 255, int b = 255)` | Text auf dem Bildschirm (wie die Debug-Ausgabe des Spiels, ohne Präfix) |
| `bool PrintGameMessage(const bCString &in text, int type = 0)` | Spielmeldung |
| `int GetActivePage()` | Aktive Spielseite (Menü, Inventar, ...), Werte nur teilweise bekannt: -1 = Spiel, 5, 6, 17 treten im Spiel auf |

## Zeichnen (nur in `OnDraw`)

| Funktion | Beschreibung |
|---|---|
| `int CreateFont(const bCString &in face, int height, bool bold = false, bool italic = false)` (global, ab Version 34) | Legt eine Schrift der Spiel-GUI an (`eCGUIAdmin::CreateFontA`) und gibt ihr Handle zurück; dieselbe Kombination liefert immer dasselbe Handle (einmal anlegen reicht, auch in `OnDraw`). `height` in Punkten der GUI-Fläche |
| `bool DrawText(int font, int x, int y, const bCString &in text, uint argb = 0xFFFFFFFF, bool shadow = true)`, `int TextWidth(int font, const bCString &in text)` (global, ab Version 34) | Text an (`x`, `y`) = linke obere Ecke, Farbe als `0xAARRGGBB`, mit Schatten 1 Punkt versetzt. **Nur in `OnDraw()`**. Koordinaten in der GUI-Fläche (`GetScreenWidth()` x `GetScreenHeight()`, gemessen 1365x768 bei einem Fenster mit 2560x1440: die GUI skaliert). Ab Version 39 liegt alles über HUD und Inventar. Einfarbige Flächen gibt es nicht (die Rechteck-Funktionen der GUI und des Renderers sind im Spiel leer) |
| `int LoadImage(const bCString &in name)`, `bool DrawImage(int image, int x, int y, int w, int h, float angle = 0)` (global, ab Version 38) | Bild aus den GUI-Daten des Spiels laden (je Name einmal, Rückgabe Nummer > 0, 0 = (noch) nicht ladbar; ein fehlgeschlagener Name wird nach 2 s erneut versucht, in `main()` fehlen die GUI-Daten oft noch) und in `OnDraw()` in das Rechteck (`x`, `y`, `w`, `h`) der GUI-Fläche zeichnen; `angle` dreht es (Bogenmaß, wie die Kompassnadel). Namen wie in `gui.pak` `resources/g3.gui.res`, voller Pfad (`\data\gui\icons\hud\v3\icons\misc\g3_hud_icon_misc_compass.tga`) oder nur der Dateiname (`g3_hud_icon_misc_compass.tga`); in AngelScript `\\` für jeden Rückstrich. Im Spiel bestätigt |
| `int GetScreenWidth()`, `int GetScreenHeight()`, `bool WorldToScreen(const bCVector &in pos, float &out x, float &out y)` (global, ab Version 34) | Größe der GUI-Fläche und Umrechnung eines Weltpunkts in diese Fläche (Kameramatrix; `false` hinter der Kamera). Gemessen: `GetPosition()` eines NPCs ist der Fußpunkt, `+ bCVector(0, 100, 0)` die Körpermitte. Fläche und Umrechnung stimmen erst ab dem ersten `OnDraw` |

## Zeit

| Funktion | Beschreibung |
|---|---|
| `int GetGameDay()`, `GetGameHour()`, `GetGameMinute()` | Spielzeit, `-1` ohne Welt |
| `bool SetGameTime(int day, int hour, int minute)` | Spielzeit setzen. `day = -1` behält den Tag. Im Spiel bestätigt (2026-11-11): Uhr und Tageslicht springen sofort (22:00 Nacht, 10:00 Tag) |
| `float GetTimeScale()`, `bool SetTimeScale(float)` (ab Version 36) | Geschwindigkeit der ganzen Simulation (1 = normal, erlaubt 0,05 bis 10). Wird nicht gespeichert |
| `float GetClockFactor()`, `bool SetClockFactor(float)` (ab Version 36) | Geschwindigkeit der Spieluhr (Spielminuten je echter Minute, normal 12; erlaubt 0 bis 10000). **Steht im Spielstand:** vor dem Speichern in `OnBeforeSave` zurückstellen, in `OnAfterSave` wieder setzen |

## Entities finden, erzeugen, entfernen

| Funktion | Beschreibung |
|---|---|
| `Entity GetHero()` | Der Held |
| `Entity GetPlayer()` | Der Spieler (gCSession) |
| `Entity FindEntity(const bCString &in name)` | Entity nach Name (`"Jens"`, `"PC_Hero"`), ungültig wenn nicht gefunden |
| `int CountEntities(const bCString &in part)` / `Entity GetEntityByPart(const bCString &in part, int index)` (global) | Entities nach Teilname suchen: Anzahl und Eintrag. Beim Entfernen rückwärts durchlaufen |
| `Entity SpawnNPC(const bCString &in templateName, const bCVector &in position)` (global) | Erzeugt aus einer Vorlage ein Entity (NPC, Objekt) in der Spielwelt: `gCWorld::CreateDynamicEntity` + `eCEntity::CreateFromTemplate`, dann Position. Gibt das Entity zurück (ungültig bei Fehler, Code im Protokoll: `-2` Vorlage unbekannt, `-5` keine Welt, `-6`/`-7` Erzeugen fehlgeschlagen). Der Vorlagenname muss exakt stimmen, siehe `FindTemplateNames`. Im Spiel bestätigt |
| `bool DestroyNPC(const Entity &in)` (global) | Entfernt ein mit `SpawnNPC` erzeugtes Entity wieder (`gCWorld::DestroyDynamicEntity`). Nur NPCs (Entities mit `gCNPC_PS`), und nur solche dieser Sitzung oder aus dem Systemsektor der Welt (dorthin gehören erzeugte NPCs, auch nach Speichern und Laden). Welt-Objekte (auch im Systemsektor, z. B. `G3_World_Border_001`), Welt-NPCs, Held und Spieler werden abgelehnt (`false`, Grund im Protokoll). Den Griff danach nicht mehr verwenden |
| `bool IsNPC(const Entity &in)` (global) | `true`, wenn das Entity ein NPC ist (hat ein `gCNPC_PS`) |
| `bool IsDynamicSpawn(const Entity &in)` (global) | `true`, wenn das Entity ein NPC im Systemsektor ist (per `SpawnNPC` erzeugt oder ähnlich dynamisch) und nicht Held/Spieler |
| `int CountTemplateNames(const bCString &in prefix)` (global, ab Version 28) | Anzahl der Vorlagen, deren Name mit `prefix` beginnt (mit Mehrfachnennungen, wie `FindTemplateNames`); billig, ohne die Namen zu lesen. `-1` ohne Spiel |
| `bCString FindTemplateNames(const bCString &in prefix, int max = 20)` (global) | Sucht Vorlagen, deren Name mit `prefix` **beginnt** (gemessen: `Teleport` findet `Teleport_Ardea`, aber nicht `Spl_Teleport_Ardea`; `_` findet nur Namen mit `_` am Anfang; **Groß-/Kleinschreibung wird ignoriert**, gemessen: `BEARD_HE` und `BEaRD_hE` liefern dasselbe): `"Name1;Name2;..."`, leer ohne Treffer. Jede Abfrage kostet etwa 3 ms je Treffer; Skripte werden nach 5 Sekunden abgebrochen, lange Listen deshalb auf mehrere Bilder verteilen (siehe `reference_dump`). Namen kommen meist doppelt vor. `max` ist bis 20000 möglich (bis Version 28 war es auf 200 begrenzt, längere Listen wurden abgeschnitten) |
| `Entity GetTemplateEntity(const bCString &in name)` (global, ab Version 29) | Die Vorlage (Template-Entity) mit genau diesem Namen, z. B. `"Perk_OrcSlayer"` oder `"It_1H_OrcSlayer_01"`; ungültig, wenn es sie nicht gibt. Ihre Eigenschaftssets lassen sich mit `GetProperty`/`ListProperties` lesen, ohne etwas zu erzeugen. Wird für `set` ein Name verwendet, den `eEPropertySetType` nicht kennt, gilt er als Klassenname (`"gCSkill_PS"`) |
| `int CloneTemplate(const bCString &in base, const bCString &in newName)` (global) | Klont die Item-Vorlage `base` im Speicher unter dem Namen `newName` (`eCNode::Clone` + `SetName`). `0` angelegt, `1` war schon da, `-2` Basis unbekannt, `-6`/`-7` Klon fehlgeschlagen. Danach `AddItems(newName, n)`. Name und Beschreibung des Items stehen in der Stringtabelle unter `FO_<newName>` und `ITEMDESC_<newName>`: mit `RegisterText` setzen, sonst bleiben sie leer. Die Vorlage bekommt eine feste ID (MD5 von `OG3Clone:<newName>`) und ist in jedem Spielstart dieselbe: Klon-Items im Spielstand bleiben nach dem Neustart erhalten. Dazu muss der Mod `CloneTemplate` in `OnQuestsLoaded` aufrufen (vor dem Einlesen des Spielstands; in `main` ist das Spiel noch nicht bereit, Code `-4`) |

## Typ `Entity`: Grundlagen und Lage

Ein `Entity` ist ein Griff auf ein Spielobjekt (NPC, Held, Item, Gebäude, Vorlage). Ein leeres oder ungültiges `Entity` ist erlaubt; Methoden liefern dann Standardwerte.

| Funktion | Beschreibung |
|---|---|
| `bool IsValid() const` | Entity existiert und ist lesbar |
| `bCString GetName() const` | Name |
| `bCVector GetPosition() const` | Weltposition |
| `bCQuaternion GetRotation() const` | Weltrotation |
| `float GetDistanceTo(const Entity &in) const` | Abstand (≈ cm), `-1` bei ungültigen Entities |
| `bool SetPosition(const bCVector &in)` | Teleport |
| `bool opEquals` (`==`) | Gleiche Spielobjekte |
| `bool Entity::SetRotation(const bCQuaternion &in)` | Setzt die Drehung (`SetLocalRotation`; Y ist oben). Für erzeugte NPCs = Weltdrehung |
| `bool Entity::LookAt(const Entity &in target, float yawOffsetDegrees = 0)` | Dreht das Entity um die Hochachse zum Ziel (Gier = atan2(dx, dz)). Schaut es vom Ziel weg, Versatz 180 angeben |
| `bool Entity::TeleportTo(const Entity &in target)` (ab Version 33) | Teleportiert das Entity zum Ziel (`Entity::Teleport` der Script.dll, wie das Spiel es für Begleiter macht). Im Spiel bestätigt: Das Entity steht danach genau auf dem Ziel; ein Begleiter, der genau auf dem Helden landet, spricht ihn sofort an. Darum danach mit `SetPosition` etwas daneben stellen (siehe `teleport_party`) |
| `Entity::GetCategory()` / `Entity::IsBuilding()` | Grobe Einordnung eines Entities: `"NPC"`, `"Building"`, `"Object"`, `"Landscape"`, `"Border"`, `"Unnamed"` (kein Name), `"Other"`, bei ungültigem Entity leer. Beruht auf Namensmustern, die im Spiel beobachtet wurden (`G3_Myrtana_House_*` = Gebäude, `G3_Object_*` = Interaktionsobjekte, `G3_Myrtana_Landscape_*`, `G3_World_Border_*`); unbenannte Treffer (Bäume, Steine, Kleinteile) lassen sich so nicht bestimmen |
| `Entity::GetTemplateName()` | Name der Vorlage, aus der das Entity entstand (leer, wenn unbekannt). Hilft bei unbenannten Treffern des Fadenkreuzes |
| `Entity::GetBoundsMin()` / `GetBoundsMax()` | Hüllquader des Entities samt Kindern in Weltkoordinaten (Größe: Max − Min). Nullvektor bei ungültigem Entity |
| `Entity::GetParent()` | Elternobjekt (das Fadenkreuz trifft oft Teilobjekte eines Gebäudes, das Elternobjekt ist dann das Gebäude). Ungültig, wenn keines da ist |

## Attribute und Inventar

Alles hier sind Methoden von `Entity` (`hero.GetAttribute("STR")`).

| Funktion | Beschreibung |
|---|---|
| `int GetAttribute(const bCString &in tag) const` | Aktueller Wert |
| `int GetAttributeBase(tag) const`, `int GetAttributeMax(tag) const` | Grund- und Höchstwert |
| `bool SetAttributeBase(tag, int)`, `AddAttributeBase(tag, int)` | Grundwert setzen/erhöhen (wie das Spiel bei Lernpunkten) |
| `bool SetAttribute(tag, int)`, `AddAttribute(tag, int)` | Aktuellen Wert setzen/erhöhen |
| `int GetItemCount(const bCString &in itemName) const` | Anzahl aller Stapel dieses Items, `-1` = Inventar nicht lesbar |
| `int RemoveItems(const bCString &in itemName, int count)` | Entfernt bis zu `count` Stück, gibt die entfernte Zahl zurück (`-1` = Fehler) |
| `int AddItems(const bCString &in itemName, int count)` | Erzeugt Items im Inventar (`>= 0` ok, `-1` Inventar nicht lesbar, `-2` Item unbekannt, `-4`/`-5` Funktion fehlt). Fehler stehen im Protokoll |
| `int Entity::DeleteItemStack(const bCString &in name)` (ab Version 30) | Entfernt alle Stapel mit diesem Item-Namen samt Stapel (`gCInventory_PS::DeleteStack`); Rückgabe: Zahl entfernter Stapel, `-1` bei Fehler. Nötig bei dauerhaften Items (`Permanent`, z. B. Lern-Items von Talenten): `RemoveItems` verringert nur die Menge und lässt einen leeren Stapel stehen |
| `int Entity::GetInventoryStackCount() const` (ab Version 37) | Zahl der Stapel im Inventar, `-1` ohne Inventar |
| `Entity::GetSlotItem(int slot)` (ab Version 28) | Das Item im Ausrüstungs-Slot als Entity (ungültig bei leerem Slot); `GetName()` liefert den Vorlagennamen, z. B. `It_1H_OrcSlayer_01`. Slots wie bei `GetSlotQuality` |
| `Entity::GetSlotQuality(int slot)` / `Entity::SetSlotQuality(int slot, uint bits)` (ab Version 28) | Qualitätsbits des Inventarstapels im Ausrüstungs-Slot (`SLOT_RIGHT_HAND` 1, `SLOT_LEFT_HAND` 2, `SLOT_BACK_LEFT` 5, `SLOT_BACK_RIGHT` 6, `SLOT_BOW` 7, `SLOT_AMMO` 14). Bits: `QUALITY_DISEASED` 2, `QUALITY_POISONED` 4, `QUALITY_BURNING` 8, `QUALITY_FROZEN` 16, `QUALITY_SHARP` 32, `QUALITY_BLESSED` 64, `QUALITY_FORGED` 128, `QUALITY_WORN` 256 und **`QUALITY_LIFESTEAL` 512** (eigenes Bit dieses Modkits). Die Beschreibung der Waffe zeigt gesetzte Bits als Zeile mit Symbol und Text; für `QUALITY_LIFESTEAL` kommt der Text aus dem Schlüssel `HUD_Quality_Lifesteal` der Mod-Stringtabelle (Präfix `(20 %)`, Symbol der Segnung). Verschiedene Qualitäten sind verschiedene Stapel und werden mit dem Spielstand gespeichert. `Get` liefert 0 bei leerem Slot oder ungültigem Entity, `Set` gibt `false` zurück, wenn der Slot leer ist. Die Inventaranzeige aktualisiert sich eventuell erst beim nächsten Öffnen |

Attribut-Kennungen: `"STR"`, `"DEX"`, `"INT"`, `"HP"`, `"MP"`, `"SP"` (Ausdauer), `"XP"`, `"LP"`, `"ALC"`, `"SMT"` (Schmiedekunst, laut Quest-Belohnungen), `"THF"` und weitere (wie `gCPlayerMemory_PS`); gesicherte Liste und Werte in [Referenz](reference.md).
Item-Namen sind die Namen der Item-Vorlagen: `It_Wolf_Fur`, `It_Water`, `It_Plant_Health_02`, `It_1H_Longsword_01`, ... Sie erscheinen im Protokoll von `OnReceiveItem`.

## Kampf, Statuseffekte und Effekte

| Funktion | Beschreibung |
|---|---|
| `Entity::GetCurrentAttacker()` (ab Version 26) | Aktueller Gegner eines NPCs oder des Helden (`gCNPC_PS::CurrentAttackerEntity`), bei keinem Gegner oder Nicht-NPC ein ungültiges Entity. Beruht auf der KI-Zielwahl: im Kampf mit mehreren Gegnern nur eine Näherung für den Schläger |
| `Entity::GetStatusEffects()` / `Entity::HasStatusEffect(int)` / `Entity::SetStatusEffect(int, bool)` (ab Version 27) | Statuseffekte eines NPCs oder des Helden als Bitmaske (`gCNPC_PS`): `STATUS_BURNING` 1, `STATUS_FROZEN` 2, `STATUS_POISONED` 4, `STATUS_DISEASED` 8, `STATUS_TRANSFORMED` 16, `STATUS_IN_FOG` 64, `STATUS_GHOST` 128, `STATUS_IN_MAGIC_BARRIER` 256, `STATUS_SOULTRAVEL` 512. Bei ungültigem Entity oder Nicht-NPC `0`/`false`. `SetStatusEffect` ruft die Spielfunktion `EnableStatusEffect` auf (löst dabei auch `OnStatusEffect` aus). Ob und wie das Spiel Folgen auslöst (z. B. keine Ausdauer-Regeneration bei Krankheit), hängt vom Spiel ab; Krankheit/Gift setzt das Spiel zusätzlich mit Zeitstempeln (`TimeStampDiseased/Poisoned`), die hier nicht gesetzt werden |
| `bool Entity::Kill(const Entity &in killer)`, `bool Entity::Kill()` (ab Version 46) | Tötet einen NPC oder ein Tier wie die Script-DLL Chszonszcze (statisch analysiert): Schaden 0 setzen, `killer` als letzten und aktuellen Angreifer eintragen, Spielskript `SetHitPoints(opfer, None, 0)`, KI anhalten (`PSRoutine::FullStop`) und Zustand `ZS_RagDollDead`. Ersatz für das entfernte `DoDamage`. Im Spiel bestätigt (2026-11-11, Schwein und Sklave, Täter Held): `OnNPCKilled` kommt sofort, der Held bekommt Erfahrung, das Opfer lässt sich plündern |
| `int Entity::HurtHitPoints(int amount, int minHp = 0)` (ab Version 30) | Zieht einem NPC `amount` Lebenspunkte ab (`gCDamageReceiver_PS::SetHitPoints`), nicht unter `minHp`; liegen die Lebenspunkte schon bei oder unter `minHp`, passiert nichts. Rückgabe: neue Lebenspunkte, `-1` bei Fehler. Der Abzug erscheint in `OnDamage` **ohne Angreifer und Waffe** (Besitzer ungültig); aus `OnDamage` heraus aufgerufen wird er nicht noch einmal gemeldet. **Tötet nicht:** Bei 0 Lebenspunkten bleibt der NPC stehen (der Tod entsteht nur in der Treffer-Kette); für echten Zauberschaden siehe `beliars_klaue` (Beschwörung mit Besitzer). |
| `bool EffectExists(const bCString &in name)` und `bool StartEffect(const bCString &in name, const Entity &in at)` (global, ab Version 30) | Prüft bzw. startet einen benannten Effekt der Effektverwaltung am Entity (`EffectSystem::StartEffect`), z. B. `"eff_magic_summonlightning_target_01"` (der Blitz von „Blitz beschwören“). Die Namen stehen in den Zaubervorlagen (`EffectItemCast`, `EffectTargetCast` ...). `false`, wenn der Effekt unbekannt ist. Der Effekt hat keinen Schaden, nur Optik |

## Talente

| Funktion | Beschreibung |
|---|---|
| `bool Entity::LearnSkill(name)`, `ActivateSkill(name)`, `DeactivateSkill(name)`, `bool HasLearnedSkill(name)`, `bool IsSkillActive(name)`, `int GetSkillActivationCount(name)` (ab Version 30) | Talente im Inventar des Helden (`gCInventory_PS`). `name` ist die Talent-Vorlage, z. B. `"Perk_1H_1"`. Gelernte Talente stehen als Stapel im Inventar, `ActivateSkill` zählt den Aktivierungszähler hoch (so macht es eine Waffe mit `ActivateSkill1/2`, solange sie geführt wird), `DeactivateSkill` zählt herunter. Ersatz für das nicht schreibbare `ActivateSkill2`. `false`/`-1` bei unbekanntem Talent oder ohne Inventar |

## Eigenschaften lesen und schreiben

| Funktion | Beschreibung |
|---|---|
| `bCString GetProperty(const Entity &in e, const bCString &in set, const bCString &in prop)` (global, ab Version 29) | Liest eine Eigenschaft des Eigenschaftssets eines Entities **als Text**, über den allgemeinen Eigenschaftszugriff der Engine. `set` ist der Typ des Sets, z. B. `"Item"`, `"gCItem_PS"`, `"Damage"`, `"NPC"`, `"Inventory"` (alle Werte von `eEPropertySetType`, Groß-/Kleinschreibung egal); `prop` der Eigenschaftsname wie im SDK (`"GoldValue"`, `"RequiredSkill1"`, `"DamageAmount"`). Leer, wenn Entity, Set oder Eigenschaft fehlen. Nur Lesen. Der Textwert kommt von `ConvertValueToString` und ist bei Verweisen (Vorlagen, Entities) und Listen nicht immer aussagekräftig |
| `bCString ListProperties(const Entity &in e, const bCString &in set, int max = 200)` (global, ab Version 29) | Alle Eigenschaften des Sets samt Basisklassen als `"Name=Wert;Name=Wert;..."` (höchstens `max`, bis 600; Semikolons in Werten werden zu Leerzeichen). Zum Herausfinden, was ein Item, ein Zauber oder ein NPC im Spiel wirklich enthält; die Mod `property_inspect` zeigt es |
| `bool SetProperty(const Entity &in e, const bCString &in set, const bCString &in prop, const bCString &in value)` (global, ab Version 29) | **Experimentell.** **Schreibt** eine Eigenschaft aus Text (`ConvertStringToValue`, Zuweisen). `true`, wenn die Eigenschaft existiert und der Aufruf ohne Fehler lief; ob der Wert so gelesen wurde, mit `GetProperty` prüfen. **Textwerte (z. B. `Tag`) lassen sich schreiben; Referenz-Eigenschaften auf andere Vorlagen (z. B. `ActivateSkill2`) nicht: `true` ohne Wirkung, getestet mit 11 Schreibformen (Name, Name+GUID, GUID mit/ohne Klammern).** Nur an Vorlagen und Entities verwenden, die die Mod selbst angelegt hat (`CloneTemplate`); ob Änderungen an Vorlagen in Spielständen bleiben, ist nicht untersucht. |
| `bool SetTemplateRef(const Entity &in e, const bCString &in klasse, const bCString &in prop, const bCString &in ziel)` (global, ab Version 30) | Im Spiel bestätigt (`perk_lifesteal`). Setzt eine **Verweis-Eigenschaft** auf eine Vorlage, z. B. `SetTemplateRef(waffe, "gCItem_PS", "ActivateSkill2", "Perk_Lifesteal")` oder `(lernItem, "gCItem_PS", "Skill", ...)`. `klasse` ist der Klassenname des Eigenschaftssets, `prop` der Name ohne Präfix (es wird `gCxxx_PS::Access<prop>()` aufgerufen, dann `eCTemplateEntityProxy::SetEntity`). Ersatz für `SetProperty` bei Verweisen, die per Text nicht gehen. Nur für Vorlagen, die die Mod selbst angelegt hat. |
| `int DumpPropertySchema(const bCString &in file)` (global, ab Version 43) | Schreibt das **Schema aller Eigenschaftsklassen** der Engine (`bCPropertyObjectSingleton`) als JSON nach `<Spiel>\g3asm\<file>`: je Klasse Name, Basisklasse, Beschreibung, speicherbar, und je Eigenschaft Name, Typ (z. B. `long`, `bTPropertyContainer<enum gEItemCategory>`), Kategorie, Beschreibung, Art (1 normal, 2 Enum-Behälter, 3 Eigenschaftsobjekt), Nur-Lesen, Offset und Größe im Set sowie alle Enum-Werte mit Namen und Zahl. Damit findet man die Namen für `GetProperty`/`SetProperty`. `file` muss ein schlichter Dateiname sein (kein Pfad, kein `..`). Rückgabe: Zahl der Klassen, `-1` Engine fehlt (ohne Spiel), `-2` Dateiname ungültig, `-3` Datei nicht schreibbar. Ohne Spiel mit der Original-SharedBase und Game.dll gemessen (2026-11-11): 273 Klassen, 1584 Eigenschaften, 1534 Enum-Werte, 78 bis 110 ms, 258 KB. Im Spiel bestätigt (2026-11-11): 273 Klassen, 1587 Eigenschaften, 62 ms; im Hauptmenü und nach dem Laden eines Spielstands byte-gleich, die Script-DLLs melden also keine weiteren Klassen an. Drei Eigenschaften haben im Spiel keinen Namen (je eine in `eCEngineCaps_PS`, `gCDamage_PS`, `gCInfoManager_PS`; ohne Spiel fehlen sie). Beispiel `schema_dump` |
| `bool SetEntityRef(const Entity &in e, const bCString &in klasse, const bCString &in prop, const Entity &in ziel)` (global, ab Version 30) | Im Spiel bestätigt (`beliars_klaue`). Setzt eine **Entity-Verweis-Eigenschaft**, z. B. `SetEntityRef(beschwoerung, "gCInteraction_PS", "Owner", GetHero())` (wem gehört die Beschwörung/das Geschoss; davon hängt ab, wen der Schaden als Besitzer meldet). `klasse` ist der Klassenname des Sets, `prop` der Name ohne Präfix. Gegenstück zu `SetTemplateRef` (das für Vorlagen gilt). |

## Gruppe (Party)

| Funktion | Beschreibung |
|---|---|
| `int Entity::GetPartyMemberCount()`, `Entity GetPartyMember(int)`, `Entity GetPartyLeader()`, `bool IsPartyWaiting()`, `bool SetPartyWaiting(bool)` (ab Version 33) | Gruppe (`gCParty_PS`): Die Mitglieder stehen beim **Anführer** (beim Helden seine Begleiter, Beschwörungen, Sklaven), jedes Mitglied verweist mit `GetPartyLeader` auf ihn. `-1`/ungültig ohne Gruppen-Set. Die Art steht in `GetProperty(m, "Party", "PartyMemberType")` (1 Party, 2 Mob, 3 Slave, 4 Controlled, 5 Summoned). `IsPartyWaiting` = der Held hat „Warte hier“ gesagt. Im Spiel bestätigt (`teleport_party`) |

## Händler

| Funktion | Beschreibung |
|---|---|
| `bool Entity::GenerateTradeGoods()` (ab Version 37) | **Experimentell.** Erzeugt die Händlerware aus den Schatz-Sets des Entity neu (wie das Spiel; löst `Override_TradeGenerate`/`OnTradeGenerated` aus). Gemessen: Jens 19 Stapel vorher und nachher; Esiel (2026-11-11) `true`, 42 Stapel und 13300 Gold vorher und nachher. Die Ware ändert sich also nicht sichtbar; vermutlich erzeugt das Spiel nur neu, wenn die alte Ware vorher entfernt wurde (das Plugin RefreshGeneratedTrade liest dafür `gCInventory_PS::GetGeneratedTrade`), nicht gemessen |

## Fokus und Fadenkreuz

| Funktion | Beschreibung |
|---|---|
| `Entity GetFocusedEntity()` (global) | Das Entity, das der Held gerade anvisiert (Fokusname/Interaktion, `gCFocus_PS::GetFocusEntity`). Ungültiges Entity, wenn nichts anvisiert wird. Im Spiel bestätigt; Beispiel `examples/mods/focus_demo` (F7) |
| `Entity PickCenter(float maxDistance = 100000)` (global) | Das Entity genau in der Bildmitte (unter dem Fadenkreuz) samt **exaktem Trefferpunkt** (`GetPickPoint()`): die Engine schießt von der aktuellen Kamera einen Strahl (`eCCameraBase::PointToRay` + `eCPhysicsScene::TraceRayNearestHit`), der **Held wird dabei ausgeschlossen** (sonst träfe die Verfolgerkamera zuerst den Helden). Ungültiges Entity, wenn nichts getroffen wird. Im Spiel bestätigt |
| `Entity PickScreen(int x, int y, float maxDistance = 100000)` (global) | Dasselbe für einen beliebigen Bildschirmpixel (Pixel der Client-Fläche des Spielfensters), z. B. für eine Mausposition |
| `bCVector GetPickPoint()` / `bool HasPickPoint()` (global) | Weltpunkt des letzten `PickCenter`/`PickScreen`-Treffers und ob es einen gab. Damit: `SpawnNPC("Diego", GetPickPoint())` |
| `Entity GetCrosshairEntity()` (global) | Das Entity unter dem Fadenkreuz (`gCFocus_PS::GetCrossHairEntity` des Spielers). Ungültig, wenn keines getroffen wird. Im Spiel bestätigt |
| `bool SetCrosshairVisible(bool on)` (global) | Im Spiel bestätigt (2026-11-11). Schaltet das Fadenkreuz des HUD ein oder aus (ab Version 46 `gCGUIManager::ShowCrossHair`, dazu `gCFocus_PS::SetDrawCrossHair`) |

## Spielskripte

| Funktion | Beschreibung |
|---|---|
| `int CallScript(const bCString &in name, const Entity &in self, const Entity &in other, int arg = 0)` (global, ab Version 32) | Ruft ein Spielskript der `Script_*.dll` nach Namen auf (`gCScriptAdmin::CallScriptFromScript`, so wie die Spielskripte einander aufrufen), z. B. `CallScript("GetAttitude", npc, GetHero())`. Rückgabe = Ergebnis des Skripts; 0, wenn es das Skript nicht gibt (Meldung im Protokoll) oder das Spiel gerade keine Skripte ausführt. Ein leeres `Entity` steht für „None“. Skripte mit Wirkung (z. B. `SetHitPoints`) greifen sofort ins Spiel ein. Gemessen (2026-11-09): `IsHumanoid` liefert 1 für Held und NPC; das Spiel selbst ruft z. B. `GetStaminaPoints(npc, None, 0)` (Ausdauer) und `GetAttitude(npc, anderer, 0)` (2 Neutral, 4 Feindlich). **Nicht** nach Namen erreichbar sind Skripte, die `Script_Game.dll` intern direkt aufruft (`AssessHit`, `IsDeadlyDamage`, `CanBurn`: im Spiel nie über den Namen gelaufen); für Treffer ist `OnHit` der Einstieg. |
| `bool ScriptExists(const bCString &in name)`, `bCString GetScriptDll(const bCString &in name)` (global, ab Version 32) | Gibt es ein Spielskript dieses Namens, und in welcher `Script_*.dll` steht es? Die Script-DLLs lädt das Spiel erst nach `main()`; ab `OnWorldActivate` fragen |
| `void WatchScript(const bCString &in name)`, `void SetScriptResult(int value)` (global, ab Version 32) | Meldet ein Spielskript für `Override_Script`/`OnScript` (ab Version 41 auch KI-Zustände, KI-Funktionen, Callbacks und Routinen für `Override_AI`/`OnAI`) an (Groß-/Kleinschreibung egal, einmal reicht, gilt für alle Mods). `SetScriptResult` setzt in `Override_Script` das Ergebnis fürs Spiel, wenn das Original nicht aufgerufen wird |
| Konstanten `AI_STATE` (1), `AI_FUNCTION` (2), `AI_CALLBACK` (3), `AI_ROUTINE` (4) (ab Version 41) | Art des KI-Skripts in `Override_AI`/`OnAI` |
| `bCString GetLocText(const bCString &in id)` (global, ab Version 41) | Text zu einer ID der Stringtabelle in der Spielsprache (`eCLocString`), für Satz-IDs aus `OnAIOutput` siehe `GetSpeechText`; Texte aus `RegisterText` und den Mod-Stringtabellen gehören dazu. Leer ohne Spiel. Was das Spiel bei einer unbekannten ID liefert, ist nicht gemessen |
| `bCString GetSpeechText(const bCString &in id)` (global, ab Version 41) | Text eines gesprochenen Satzes aus der ID von `OnAIOutput`. Die ID ist der Name der Sprachdatei (`INFO_<Stimme>_<Zeile>_<Sprache>`), die Stringtabelle kennt die Zeile aber als `INFO_<Zeile>`: Die Funktion schneidet die Sprache ab und probiert die möglichen Schlüssel von kurz nach lang. Leer, wenn keiner passt. Im Spiel gemessen (2026-11-10): Dialogzeilen (`INFO_...`) und Befehle des Helden (`SVM_Player_CmdPartyJoin` = „Komm mit!“) liefern ihren Text, das Umgebungsgerede (`SVM_..._Smalltalk_NN`) bleibt leer |

## Konsole

| Funktion | Beschreibung |
|---|---|
| `void SetConsoleReply(const bCString &in text)`, `bool OpenConsole()` (global, ab Version 35) | Antwort des gerade laufenden Konsolenbefehls (nur in `Override_ConsoleCommand`); `OpenConsole` öffnet die Konsole, sichtbar ist sie aber nur im Marvin-Modus |
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
| `bool LoadQuestFile(name, path)` | Liest die Eigenschaften einer Quest aus einer `.quest`-Datei (ini-Text: `[Quest]`, `Name=`, `Type=`, `LogTopic=`, `ExperiencePoints=` …; `gCQuest_PS::Read` über `eCConfigFile`). `path` ist ein **virtueller Pfad** wie das Spiel ihn benutzt: `Data/Quests/<Welt>/<Quest>_quest_<Welt>.quest` (Schrägstriche, relativ zum Spielordner, auch aus Archiven); `GetQuestFilePath(name)` liefert ihn. Im Spiel bestätigt (2026-11-11, `Innos_FreeMyrtana`: `true`). `DumpQuest` (Schreiben) wurde mit Version 46 entfernt: `gCQuest_PS::Write` schrieb mit keinem Pfad eine Datei, und das Spiel ruft es selbst nie auf |
| `void QuestDebug()` | Diagnose: schreibt die Rohwerte der Questliste des Spiels ins Protokoll (zur Fehlersuche bei `GetQuestCount`) |
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

### `bCMatrix4` (Standard = Einheit, ab Version 19)
4x4-Matrix der Engine (`bCMatrix`), Speicherlage 16 Floats zeilenweise. **Zeilenvektor-Konvention** (`v' = v * M`): Zeilen 0 bis 2 sind die Achsen X, Y, Z, Zeile 3 die Verschiebung.
`GetElement(row, col)`, `SetElement(row, col, f)`, `SetIdentity()`, `Transpose()`, `Invert()`, `GetInverted()`, `GetDeterminant()`, `* bCMatrix4`, `* bCVector` (Punkt mit Verschiebung transformieren),
`GetTranslation()`, `SetTranslation(v)`, `GetXAxis()`, `GetYAxis()`, `GetZAxis()`, `SetToTranslation(v)`, `SetToScaling(v)`, `SetToRotation(bCQuaternion)`,
`SetToRotationAxisX/Y/Z(Bogenmaß)`, `GetRotationMatrix()` (3x3-Teil als `bCMatrix3`), `GetRotation()` (als `bCQuaternion`).
`Entity::GetWorldMatrix()` liefert die Weltmatrix eines Entities (Einheitsmatrix bei ungültigem Entity). Die Rechenfunktionen laufen ebenfalls über die SharedBase.

Die Rechenfunktionen laufen über die SharedBase des Spiels (Originalverhalten, einschließlich der Rundung der Original-DLL).
