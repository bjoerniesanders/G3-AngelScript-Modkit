# API-Referenz (Skript-Schnittstelle Version 46)

Diese Seite ist zum Nachschlagen: alle Funktionen und Typen, die ein Skript aufrufen kann. Einstieg und Beispiele stehen im
[Scripting-Guide](scripting-guide.md) und unter [Beispiele](examples.md).

- Alle Funktionen sind global, soweit nicht `Entity::` davor steht oder der Abschnitt Methoden von `Entity` beschreibt (`held.GetAttribute("STR")`).
- Ohne laufendes Spiel (Tests) liefern die Spielfunktionen Standardwerte (`false`, `-1`, leerer Text).
- „ab Version N“ nennt die Skript-Schnittstelle, mit der eine Funktion kam (`GetApiVersion()`). Was sich zwischen Versionen ändern darf, steht in den
  [Versionsregeln](api-policy.md).

**Vermerke** stehen am Anfang der Beschreibung:

| Vermerk | Bedeutung |
|---|---|
| ohne | Benutzbar wie beschrieben |
| **Experimentell** | Läuft, aber die Wirkung ist nicht vollständig geprüft. Name, Parameter und Verhalten können sich noch ändern |
| **Im Spiel nicht geprüft** | Ohne Spiel getestet, im Spiel noch nie ausprobiert |
| **Funktioniert nicht** | Aufrufbar, liefert im Spiel aber kein brauchbares Ergebnis; der Ersatz steht dabei |
| **Veraltet** | Aufrufbar, meldet beim ersten Aufruf je Mod eine Warnung; der Ersatz steht dabei |

## Allgemein

| Funktion | Beschreibung |
|---|---|
| `void print(const bCString &in)` | Schreibt `[Modname] text` ins Protokoll `g3asm.log` (und ins Protokoll des Spiels) |
| `bCString ToString(int / uint / float / double / bool)` | Zahl oder bool als Text |
| `float GetFrameTime()` | Dauer des letzten Bildes in Sekunden (Spielzeit) |
| `uint GetTimeStamp()` | Zeitstempel des Spiels |
| `int GetApiVersion()` | Version der Skript-Schnittstelle (46) |
| `bCString GetGameDir()` | Spielordner mit abschließendem `\` |
| `void check(bool, const bCString &in)` | Für Tests: meldet im Protokoll, wenn die Bedingung falsch ist |

Aus den AngelScript-Add-ons: `array<T>` (`insertLast`, `length`, `[]`, ...) und `scriptmath` (`sin`, `cos`, `sqrt`, `abs`, `pow`, `floor`, ... mit `float`).

## Andere Mods und Einstellungen

| Funktion | Beschreibung |
|---|---|
| `bool IsModActive(const bCString &in name)` (ab Version 40) | `true`, wenn die Mod geladen ist (installiert, eingeschaltet, Bedingungen der `mod.ini` erfüllt), auch ohne Skript |
| `bCString GetModVersion(const bCString &in name)` (ab Version 40) | `version` aus der `mod.ini` einer aktiven Mod; leer ohne Angabe |
| `bCString GetModSetting(const bCString &in key, const bCString &in def = "")` (ab Version 44) | Einstellung der **eigenen** Mod als Text: Benutzerwert, sonst Vorgabe aus der `mod.ini`, sonst `def` |
| `int GetModSettingInt(key, int def = 0)`, `float GetModSettingFloat(key, float def = 0)`, `bool GetModSettingBool(key, bool def = false)` (ab Version 44) | Dasselbe als Zahl oder Schalter; ist der Wert nicht lesbar, kommt `def` |
| `bool HasModSetting(const bCString &in key)` (ab Version 44) | Gibt es einen Benutzerwert oder eine Vorgabe? |
| `bool SetModSetting(const bCString &in key, const bCString &in value)` (ab Version 44) | Speichert einen Benutzerwert sofort in `g3asm\settings\<Mod>.ini` |
| `bool ResetModSetting(const bCString &in key)` (ab Version 44) | Entfernt den Benutzerwert; danach gilt wieder die Vorgabe |
| `bCString CallMod(const bCString &in mod, const bCString &in func, const bCString &in arg)` (ab Version 44) | Ruft `bCString <func>(const bCString &in)` in einer anderen Mod auf und liefert deren Rückgabe; leer, wenn Mod oder Funktion fehlen |
| `bool ModFunctionExists(const bCString &in mod, const bCString &in func)` (ab Version 44) | Hat die Mod eine solche Funktion? |
| `int SendModMessage(const bCString &in topic, const bCString &in data)` (ab Version 44) | Ruft sofort `OnModMessage` in allen **anderen** Mods auf; Rückgabe: Zahl der Empfänger |

- Groß- und Kleinschreibung zählt bei Mod-Namen und Einstellungsschlüsseln nicht.
- `IsModActive` ist für optionale Zusammenarbeit gedacht. Braucht deine Mod eine andere zwingend, gehört sie in `requires` der `mod.ini`
  ([Mod-Ordner](mod-package.md#version-und-abhängigkeiten-modini-ab-version-40)).
- Einstellungen: Ganzzahlen auch hexadezimal (`0x10`), Kommazahlen mit `.` oder `,`, Schalter `1`/`0`, `true`/`false`, `ja`/`nein`, `on`/`off`.
  Schlüssel ohne `= [ ] ; #` und höchstens 100 Zeichen, Werte ohne Zeilenumbruch und höchstens 2000 Zeichen. Die `mod.ini` ändert `SetModSetting` nie,
  ein Update der Mod behält die Benutzerwerte also.
- `CallMod`: Zahlen und mehrere Werte gehen als Text (`ToString`, eigenes Trennzeichen). Während des Aufrufs läuft die Ziel-Mod mit ihren eigenen
  Einstellungen. Höchstens 8 Aufrufe ineinander, danach kommt ein leerer Text und eine Protokollzeile.

## Fenster, Tasten, Bildschirmtext

| Funktion | Beschreibung |
|---|---|
| `bool IsKeyDown(int vk)` | Taste gerade gedrückt (nur wenn das Spiel im Vordergrund ist) |
| Konstanten `KEY_F1` ... `KEY_F12` | Tastencodes der Funktionstasten |
| `void SetWindowTitle(const bCString &in)` | Fenstertitel setzen |
| `void PlaySystemSound()` | Windows-Systemton |
| `bool PrintScreen(const bCString &in text, float seconds = 3, int r = 255, int g = 255, int b = 255)` | Text auf dem Bildschirm |
| `bool PrintGameMessage(const bCString &in text, int type = 0)` | Meldung im Stil des Spiels |
| `int GetActivePage()` | Nummer der offenen Spielseite (Menü, Inventar, ...); `-1` = keine, also das Spiel selbst. Weitere bekannte Werte: 0 bis 6 und 17, ihre Bedeutung ist nur teilweise bekannt |

## Zeichnen (nur in `OnDraw`)

| Funktion | Beschreibung |
|---|---|
| `int CreateFont(const bCString &in face, int height, bool bold = false, bool italic = false)` (ab Version 34) | Schrift anlegen; liefert eine Nummer. Dieselben Angaben liefern immer dieselbe Nummer |
| `bool DrawText(int font, int x, int y, const bCString &in text, uint argb = 0xFFFFFFFF, bool shadow = true)` (ab Version 34) | Text zeichnen; (`x`, `y`) ist die linke obere Ecke, Farbe als `0xAARRGGBB` |
| `int TextWidth(int font, const bCString &in text)` (ab Version 34) | Breite eines Textes in dieser Schrift |
| `int LoadImage(const bCString &in name)` (ab Version 38) | Bild aus den GUI-Daten des Spiels laden; Nummer > 0, `0` = (noch) nicht ladbar |
| `bool DrawImage(int image, int x, int y, int w, int h, float angle = 0)` (ab Version 38) | Bild in das Rechteck zeichnen, `angle` dreht es (Bogenmaß) |
| `int GetScreenWidth()`, `int GetScreenHeight()` (ab Version 34) | Größe der Zeichenfläche |
| `bool WorldToScreen(const bCVector &in pos, float &out x, float &out y)` (ab Version 34) | Weltpunkt in Bildschirmkoordinaten umrechnen; `false`, wenn der Punkt hinter der Kamera liegt |

- Alles Gezeichnete liegt über HUD und Inventar.
- Koordinaten gelten für die Zeichenfläche der Spiel-GUI, nicht für Fensterpixel. Die GUI skaliert: Bei einem Fenster mit 2560 × 1440 meldet
  `GetScreenWidth()` × `GetScreenHeight()` 1365 × 768. Größe und `WorldToScreen` stimmen erst ab dem ersten `OnDraw`.
- **Einfarbige Flächen gibt es nicht.** Die Rechteck-Funktionen des Spiels zeichnen nichts; nimm stattdessen ein Bild.
- `LoadImage` einmal je Name aufrufen. In `main()` fehlen die GUI-Daten oft noch; ein fehlgeschlagener Name wird nach 2 Sekunden erneut versucht.
  Namen wie in `gui.pak`: voller Pfad (`\data\gui\icons\hud\v3\icons\misc\g3_hud_icon_misc_compass.tga`, im Skript jeder `\` doppelt) oder nur der
  Dateiname (`g3_hud_icon_misc_compass.tga`).
- `GetPosition()` eines NPCs ist sein Fußpunkt; `+ bCVector(0, 100, 0)` ist etwa die Körpermitte.
- Beispiel: `probe_draw`.

## Zeit

| Funktion | Beschreibung |
|---|---|
| `int GetGameDay()`, `GetGameHour()`, `GetGameMinute()` | Spielzeit, `-1` ohne Welt |
| `bool SetGameTime(int day, int hour, int minute)` | Spielzeit setzen; Uhr und Tageslicht springen sofort. `day = -1` behält den Tag |
| `float GetTimeScale()`, `bool SetTimeScale(float)` (ab Version 36) | Geschwindigkeit der ganzen Simulation (1 = normal, 0,05 bis 10). Wird nicht gespeichert |
| `float GetClockFactor()`, `bool SetClockFactor(float)` (ab Version 36) | Geschwindigkeit der Spieluhr (Spielminuten je echter Minute, normal 12, 0 bis 10000). **Steht im Spielstand**, siehe unten |

Den Uhrfaktor vor dem Speichern in `OnBeforeSave` zurückstellen und in `OnAfterSave` wieder setzen, sonst bleibt er im Spielstand, auch ohne die Mod
(Beispiel `probe_time`).

## Entities finden, erzeugen, entfernen

Ein `Entity` ist ein Griff auf ein Spielobjekt (NPC, Held, Item, Gebäude, Vorlage). Ein ungültiges `Entity` ist erlaubt; seine Methoden liefern dann
Standardwerte.

| Funktion | Beschreibung |
|---|---|
| `Entity GetHero()` | Der Held |
| `Entity GetPlayer()` | Der Spieler |
| `Entity FindEntity(const bCString &in name)` | Entity nach Name (`"Jens"`, `"PC_Hero"`); ungültig, wenn nicht gefunden |
| `int CountEntities(const bCString &in part)`, `Entity GetEntityByPart(const bCString &in part, int index)` | Entities nach Teil des Namens suchen: Anzahl und Eintrag Nummer `index` |
| `Entity SpawnNPC(const bCString &in templateName, const bCVector &in position)` | Erzeugt aus einer Vorlage einen NPC oder ein Objekt an der Position; ungültig bei Fehler |
| `bool DestroyNPC(const Entity &in)` | Entfernt einen mit `SpawnNPC` erzeugten NPC wieder |
| `bool IsNPC(const Entity &in)` | Ist das Entity ein NPC? |
| `bool IsDynamicSpawn(const Entity &in)` | Ist es ein erzeugter NPC (nicht Held, nicht Spieler)? |
| `int CountTemplateNames(const bCString &in prefix)` (ab Version 28) | Anzahl der Vorlagen, deren Name so beginnt; schnell, `-1` ohne Spiel |
| `bCString FindTemplateNames(const bCString &in prefix, int max = 20)` | Namen der Vorlagen, die so beginnen, als `"Name1;Name2;..."`; leer ohne Treffer |
| `Entity GetTemplateEntity(const bCString &in name)` (ab Version 29) | Die Vorlage mit genau diesem Namen (`"It_1H_OrcSlayer_01"`); ungültig, wenn es sie nicht gibt |
| `int CloneTemplate(const bCString &in base, const bCString &in newName)` | Kopiert eine Item-Vorlage unter neuem Namen. `0` angelegt, `1` gab es schon, negativ = Fehler |

- **`SpawnNPC`:** Der Vorlagenname muss genau stimmen (`FindTemplateNames` hilft). Fehlercodes im Protokoll: `-2` Vorlage unbekannt, `-5` keine Welt,
  `-6`/`-7` Erzeugen fehlgeschlagen. Mit `PickCenter()` und `GetPickPoint()` lässt sich ein NPC dort erzeugen, wohin das Fadenkreuz zeigt.
- **`DestroyNPC`** entfernt nur erzeugte NPCs, auch aus früheren Sitzungen. NPCs der Spielwelt, Held, Spieler und Gegenstände der Welt lehnt die
  Funktion ab (`false`, Grund im Protokoll). Das `Entity` danach nicht mehr verwenden.
- **`GetEntityByPart`:** Wer beim Durchlaufen entfernt, läuft rückwärts.
- **`FindTemplateNames`:** Der Anfang muss passen (`Teleport` findet `Teleport_Ardea`, nicht `Spl_Teleport_Ardea`), Groß- und Kleinschreibung zählt
  nicht. Namen kommen oft doppelt vor. Jeder Treffer kostet etwa 3 ms; lange Listen (`max` bis 20000) auf mehrere Bilder verteilen, sonst greift die
  Grenze von 5 Sekunden je Aufruf (Beispiel `probe_reference`).
- **`CloneTemplate`** in `OnQuestsLoaded` aufrufen, dann bleiben Items aus dem Klon in Spielständen erhalten (der Klon bekommt in jedem Spielstart
  dieselbe ID). In `main()` ist das Spiel noch nicht bereit (Code `-4`). Weitere Codes: `-2` Basis unbekannt, `-6`/`-7` Kopieren fehlgeschlagen.
  Name und Beschreibung des Items setzt du mit `RegisterText("FO_<newName>", ...)` und `RegisterText("ITEMDESC_<newName>", ...)`; danach
  `AddItems(newName, n)`. Beispiel `demo_clone`.

## Entity: Lage und Art

| Funktion | Beschreibung |
|---|---|
| `bool IsValid() const` | Entity existiert und ist lesbar |
| `bCString GetName() const` | Name |
| `bCString GetTemplateName() const` | Name der Vorlage, aus der das Entity entstand; leer, wenn unbekannt |
| `bCString GetCategory() const` | Grobe Art: `"NPC"`, `"Building"`, `"Object"`, `"Landscape"`, `"Border"`, `"Unnamed"`, `"Other"` |
| `bool IsBuilding() const` | Gebäude? |
| `bCVector GetPosition() const` | Position in der Welt |
| `bCQuaternion GetRotation() const` | Drehung in der Welt |
| `bCMatrix4 GetWorldMatrix() const` | Weltmatrix (Einheitsmatrix bei ungültigem Entity) |
| `bCVector GetBoundsMin() const`, `bCVector GetBoundsMax() const` | Umgebender Quader samt Kindern, in Weltkoordinaten |
| `Entity GetParent() const` | Elternobjekt; ungültig, wenn keines da ist |
| `float GetDistanceTo(const Entity &in) const` | Abstand in Zentimetern, `-1` bei ungültigen Entities |
| `bool SetPosition(const bCVector &in)` | Versetzt das Entity |
| `bool SetRotation(const bCQuaternion &in)` | Setzt die Drehung (Y ist oben) |
| `bool LookAt(const Entity &in target, float yawOffsetDegrees = 0)` | Dreht das Entity um die Hochachse zum Ziel; schaut es weg, Versatz 180 |
| `bool TeleportTo(const Entity &in target)` (ab Version 33) | Teleportiert zum Ziel, wie das Spiel es mit Begleitern macht |
| `==` | Dasselbe Spielobjekt? |

- `GetCategory` beruht auf Namensmustern des Spiels (`G3_Myrtana_House_*` = Gebäude, `G3_Object_*` = benutzbare Objekte). Unbenannte Treffer wie
  Bäume und Steine heißen `"Unnamed"`; `GetTemplateName` sagt dann oft mehr.
- Das Fadenkreuz trifft oft ein Teilstück eines Gebäudes; `GetParent()` ist dann das Gebäude.
- Nach `TeleportTo` steht das Entity genau auf dem Ziel. Ein Begleiter, der genau auf dem Helden landet, spricht ihn sofort an; deshalb danach mit
  `SetPosition` etwas daneben stellen (Beispiel `teleport_party`).

## Entity: Attribute und Inventar

| Funktion | Beschreibung |
|---|---|
| `int GetAttribute(const bCString &in tag) const` | Aktueller Wert |
| `int GetAttributeBase(tag) const`, `int GetAttributeMax(tag) const` | Grund- und Höchstwert |
| `bool SetAttributeBase(tag, int)`, `bool AddAttributeBase(tag, int)` | Grundwert setzen oder erhöhen (wie beim Lernen) |
| `bool SetAttribute(tag, int)`, `bool AddAttribute(tag, int)` | Aktuellen Wert setzen oder erhöhen |
| `int GetItemCount(const bCString &in itemName) const` | Anzahl dieses Items über alle Stapel, `-1` = kein lesbares Inventar |
| `int AddItems(const bCString &in itemName, int count)` | Items ins Inventar legen. `>= 0` gelungen, `-1` kein Inventar, `-2` Item unbekannt |
| `int RemoveItems(const bCString &in itemName, int count)` | Entfernt bis zu `count` Stück; Rückgabe: entfernte Zahl, `-1` = Fehler |
| `int DeleteItemStack(const bCString &in name)` (ab Version 30) | Entfernt alle Stapel dieses Items ganz; Rückgabe: Zahl der Stapel |
| `int GetInventoryStackCount() const` (ab Version 37) | Zahl der Stapel im Inventar, `-1` ohne Inventar |
| `Entity GetSlotItem(int slot) const` (ab Version 28) | Item im Ausrüstungsplatz; ungültig, wenn der Platz leer ist |
| `uint GetSlotQuality(int slot) const`, `bool SetSlotQuality(int slot, uint bits)` (ab Version 28) | Qualität (Bits `QUALITY_*`) des Items im Ausrüstungsplatz |

- Attribut-Kennungen: `"STR"`, `"DEX"`, `"INT"`, `"HP"`, `"MP"`, `"SP"` (Ausdauer), `"LP"`, `"ALC"`, `"SMT"`, `"THF"` und weitere; die geprüfte
  Liste steht in der [Referenz](reference.md#7-attribute). `"XP"` liest 0.
- Item-Namen sind Namen von Item-Vorlagen (`It_Wolf_Fur`, `It_Water`, `It_1H_Longsword_01`). `OnReceiveItem` zeigt sie im Protokoll.
- `RemoveItems` verringert nur die Menge. Dauerhafte Items (Lern-Items von Talenten) hinterlassen dabei einen leeren Stapel; die entfernt
  `DeleteItemStack`.
- Ausrüstungsplätze: `SLOT_RIGHT_HAND`, `SLOT_LEFT_HAND`, `SLOT_BACK_LEFT`, `SLOT_BACK_RIGHT`, `SLOT_BOW`, `SLOT_AMMO`. Qualitäten:
  `QUALITY_DISEASED`, `QUALITY_POISONED`, `QUALITY_BURNING`, `QUALITY_FROZEN`, `QUALITY_SHARP`, `QUALITY_BLESSED`, `QUALITY_FORGED`, `QUALITY_WORN`
  und `QUALITY_LIFESTEAL` (eigenes Bit des Modkits). Zahlenwerte in der [Referenz](reference.md).
- Die Beschreibung der Waffe zeigt gesetzte Qualitäten als Zeile mit Symbol. Für `QUALITY_LIFESTEAL` kommt der Text aus dem Schlüssel
  `HUD_Quality_Lifesteal` der Mod-Stringtabelle (Beispiel `demo_lifesteal`). Items mit verschiedener Qualität sind verschiedene Stapel und kommen mit
  in den Spielstand. Das Inventar zeigt eine Änderung unter Umständen erst beim nächsten Öffnen.

## Entity: Kampf und Statuseffekte

| Funktion | Beschreibung |
|---|---|
| `Entity GetCurrentAttacker() const` (ab Version 26) | Aktueller Gegner eines NPCs oder des Helden; ungültig, wenn es keinen gibt |
| `uint GetStatusEffects() const` (ab Version 27) | Alle Statuseffekte als Bitmaske (`STATUS_*`) |
| `bool HasStatusEffect(int) const`, `bool SetStatusEffect(int, bool)` (ab Version 27) | Einen Statuseffekt abfragen oder setzen |
| `bool Kill(const Entity &in killer)`, `bool Kill()` (ab Version 46) | Tötet einen NPC oder ein Tier. `OnNPCKilled` kommt sofort, der Täter bekommt Erfahrung, das Opfer lässt sich plündern |
| `int HurtHitPoints(int amount, int minHp = 0)` (ab Version 30) | **Experimentell.** Senkt die Lebenspunkte um `amount`, nicht unter `minHp`; Rückgabe: neue Lebenspunkte. **Tötet nicht** |
| `bool EffectExists(const bCString &in name)`, `bool StartEffect(const bCString &in name, const Entity &in at)` (ab Version 30, global) | Einen Effekt des Spiels (nur Optik) prüfen oder am Entity starten |

- `GetCurrentAttacker` folgt der Zielwahl der KI. Mit mehreren Gegnern ist es nicht sicher der, der zuletzt zugeschlagen hat.
- Statuseffekte: `STATUS_BURNING`, `STATUS_FROZEN`, `STATUS_POISONED`, `STATUS_DISEASED`, `STATUS_TRANSFORMED`, `STATUS_IN_FOG`, `STATUS_GHOST`,
  `STATUS_IN_MAGIC_BARRIER`, `STATUS_SOULTRAVEL` ([Referenz](reference.md)). `SetStatusEffect` löst `OnStatusEffect` aus. Welche Folgen das Spiel
  daraus zieht, entscheidet das Spiel; Krankheit und Gift setzt es zusätzlich mit eigener Zeitmessung, die hier nicht gesetzt wird.
- `HurtHitPoints` erscheint in `OnDamage` ohne Angreifer und Waffe. Bei 0 Lebenspunkten bleibt der NPC stehen. Zum Töten `Kill`; Schaden mit Täter
  entsteht über eine Beschwörung mit Besitzer (Beispiel `beliars_klaue`).
- Effektnamen stehen in den Zaubervorlagen, z. B. `"eff_magic_summonlightning_target_01"` (der Blitz von „Blitz beschwören“).

## Entity: Talente

| Funktion | Beschreibung |
|---|---|
| `bool LearnSkill(name)` (ab Version 30) | Talent lernen |
| `bool ActivateSkill(name)`, `bool DeactivateSkill(name)` (ab Version 30) | Talent ein- oder ausschalten (zählt wie eine Waffe mit Talent) |
| `bool HasLearnedSkill(name)`, `bool IsSkillActive(name)`, `int GetSkillActivationCount(name)` (ab Version 30) | Abfragen |

`name` ist die Talent-Vorlage, z. B. `"Perk_1H_1"`. Unbekanntes Talent oder kein Inventar: `false` bzw. `-1`. Für ein geklontes Talent liefert
`LearnSkill` `false`; die Wirkung eines eigenen Talents stellt die Mod selbst her (Beispiel `perk_lifesteal`).

## Eigenschaften lesen und schreiben

Jedes Entity und jede Vorlage hat Eigenschaftssets (`Item`, `Damage`, `NPC`, `Inventory`, ...) mit benannten Werten (`GoldValue`, `DamageAmount`, ...).

| Funktion | Beschreibung |
|---|---|
| `bCString GetProperty(const Entity &in e, const bCString &in set, const bCString &in prop)` (ab Version 29) | Eine Eigenschaft als Text lesen; leer, wenn es sie nicht gibt |
| `bCString ListProperties(const Entity &in e, const bCString &in set, int max = 200)` (ab Version 29) | Alle Eigenschaften des Sets als `"Name=Wert;Name=Wert;..."` (bis 600) |
| `bool SetProperty(const Entity &in e, const bCString &in set, const bCString &in prop, const bCString &in value)` (ab Version 29) | **Experimentell.** Eine Eigenschaft aus Text schreiben; nur Textwerte |
| `bool SetTemplateRef(const Entity &in e, const bCString &in klasse, const bCString &in prop, const bCString &in ziel)` (ab Version 30) | Verweis auf eine Vorlage setzen |
| `bool SetEntityRef(const Entity &in e, const bCString &in klasse, const bCString &in prop, const Entity &in ziel)` (ab Version 30) | Verweis auf ein Entity setzen |
| `int DumpPropertySchema(const bCString &in file)` (ab Version 43) | Alle Eigenschaftsklassen mit ihren Eigenschaften als JSON nach `g3asm\<file>` schreiben |

- `set` ist der Name des Sets (`"Item"`, `"Damage"`, `"NPC"`, Groß-/Kleinschreibung egal) oder der Klassenname (`"gCItem_PS"`). `prop` ist der Name
  der Eigenschaft wie im [Gothic-3-SDK](https://github.com/georgeto/gothic3sdk) (`"GoldValue"`, `"RequiredSkill1"`).
- Was ein Item wirklich enthält, zeigt `ListProperties`; die Mod `demo_properties` schreibt es per Taste ins Protokoll.
- Bei Verweisen und Listen ist der gelesene Text nicht immer aussagekräftig. **Verweise lassen sich mit `SetProperty` nicht schreiben** (`true`
  ohne Wirkung); dafür gibt es `SetTemplateRef` und `SetEntityRef`. Ob `SetProperty` geklappt hat, zeigt ein `GetProperty` danach.
- `SetProperty` und `SetTemplateRef` nur an Vorlagen und Entities verwenden, die die Mod selbst angelegt hat (`CloneTemplate`).
- Beispiele: `SetTemplateRef(waffe, "gCItem_PS", "ActivateSkill2", "Perk_Lifesteal")` (Waffe schaltet ein Talent ein, `perk_lifesteal`) und
  `SetEntityRef(beschwoerung, "gCInteraction_PS", "Owner", GetHero())` (wem eine Beschwörung gehört und wem ihr Schaden zählt, `beliars_klaue`).
  `klasse` ist der Klassenname des Sets, `prop` der Name der Eigenschaft.
- `DumpPropertySchema`: `file` ist ein schlichter Dateiname ohne Pfad. Rückgabe: Zahl der Klassen (im Spiel 273), `-1` ohne Spiel, `-2` Dateiname
  ungültig, `-3` nicht schreibbar. Die Datei enthält je Klasse Basisklasse und Eigenschaften mit Typ, Kategorie, Beschreibung und allen Enum-Werten;
  daraus kommen die Namen für `GetProperty`. Dauer etwa 0,1 s. Beispiel `probe_schema`.

## Gruppe (Party)

| Funktion | Beschreibung |
|---|---|
| `int GetPartyMemberCount()`, `Entity GetPartyMember(int)` (ab Version 33) | Mitglieder der Gruppe eines Anführers (beim Helden: Begleiter, Beschwörungen, Sklaven) |
| `Entity GetPartyLeader()` (ab Version 33) | Anführer, zu dessen Gruppe das Entity gehört |
| `bool IsPartyWaiting()`, `bool SetPartyWaiting(bool)` (ab Version 33) | Hat der Held „Warte hier“ gesagt? |

Ohne Gruppe: `-1` bzw. ungültig. Die Art eines Mitglieds liefert `GetProperty(m, "Party", "PartyMemberType")`: 1 Begleiter, 2 Mob, 3 Sklave,
4 kontrolliert, 5 beschworen. Beispiel `teleport_party`.

## Händler

| Funktion | Beschreibung |
|---|---|
| `bool GenerateTradeGoods()` (ab Version 37) | **Experimentell.** Ware eines Händlers neu erzeugen, wie das Spiel es tut. Liefert `true`, die Ware bleibt aber bisher gleich |

Löst `Override_TradeGenerate` und `OnTradeGenerated` aus.

## Fokus und Fadenkreuz

| Funktion | Beschreibung |
|---|---|
| `Entity GetFocusedEntity()` | Was der Held gerade anvisiert (Name über dem Ziel); ungültig, wenn nichts |
| `Entity GetCrosshairEntity()` | Was unter dem Fadenkreuz liegt; ungültig, wenn nichts |
| `Entity PickCenter(float maxDistance = 100000)` | Was genau in der Bildmitte liegt, samt Trefferpunkt; der Held zählt nicht |
| `Entity PickScreen(int x, int y, float maxDistance = 100000)` | Dasselbe für einen Pixel des Spielfensters, z. B. die Mausposition |
| `bCVector GetPickPoint()`, `bool HasPickPoint()` | Trefferpunkt des letzten `PickCenter`/`PickScreen` und ob es einen gab |
| `bool SetCrosshairVisible(bool on)` | Fadenkreuz ein- oder ausblenden |

Beispiel `demo_focus` (Taste Pos1), `SpawnNPC("Diego", GetPickPoint())` erzeugt einen NPC am Ziel.

## Spielskripte

Gothic 3 hat Spielskripte in seinen `Script_*.dll`: benannte Funktionen wie `MagicTeleport`, `GetAttitude` oder `IsHumanoid`. Ein Skript kann sie
aufrufen und ausgewählte abfangen.

| Funktion | Beschreibung |
|---|---|
| `int CallScript(const bCString &in name, const Entity &in self, const Entity &in other, int arg = 0)` (ab Version 32) | Ruft ein Spielskript auf; Rückgabe ist sein Ergebnis, 0 wenn es fehlt |
| `bool ScriptExists(const bCString &in name)`, `bCString GetScriptDll(const bCString &in name)` (ab Version 32) | Gibt es das Skript, und in welcher DLL steht es? |
| `void WatchScript(const bCString &in name)` (ab Version 32) | Meldet einen Namen für `Override_Script`/`OnScript` an (ab Version 41 auch KI-Skripte für `Override_AI`/`OnAI`) |
| `void SetScriptResult(int value)` (ab Version 32) | Ergebnis fürs Spiel, wenn `Override_Script` das Original nicht aufruft |
| Konstanten `AI_STATE` (1), `AI_FUNCTION` (2), `AI_CALLBACK` (3), `AI_ROUTINE` (4) (ab Version 41) | Art des KI-Skripts in `Override_AI`/`OnAI` |
| `bCString GetLocText(const bCString &in id)` (ab Version 41) | Text zu einer ID der Stringtabelle in der Spielsprache, auch eigene Texte |
| `bCString GetSpeechText(const bCString &in id)` (ab Version 41) | Text eines gesprochenen Satzes aus der ID von `OnAIOutput`; leer, wenn unbekannt |

- `CallScript`: Ein ungültiges `Entity` steht für „niemand“. Beispiele: `CallScript("IsHumanoid", npc, Entity())` liefert 1 für Menschen,
  `CallScript("GetAttitude", npc, GetHero())` die Haltung (2 neutral, 4 feindlich). Skripte mit Wirkung greifen sofort ins Spiel ein.
- **Nicht erreichbar** sind Skripte, die das Spiel intern direkt aufruft statt über den Namen, darunter `AssessHit`, `IsDeadlyDamage` und `CanBurn`.
  Für Treffer gibt es das Spielskript `OnHit` (über `WatchScript("OnHit")` und `Override_Script`) sowie `Override_Damage`, `OnDamage` und
  `Override_MonsterDamage` ([Hooks](hooks.md)).
- Die Script-DLLs lädt das Spiel erst nach `main()`; `ScriptExists` deshalb ab `OnWorldActivate` fragen.
- `WatchScript` gilt für alle Mods, einmal reicht, Groß-/Kleinschreibung egal.
- `GetSpeechText` liefert Dialogzeilen und Befehle des Helden (`SVM_Player_CmdPartyJoin` = „Komm mit!“). Für Umgebungsgerede gibt es keinen Text.

## Konsole

| Funktion | Beschreibung |
|---|---|
| `void SetConsoleReply(const bCString &in text)` (ab Version 35) | Antwort auf den gerade laufenden Befehl (nur in `Override_ConsoleCommand`) |
| `bool OpenConsole()` (ab Version 35) | Öffnet die Konsole; sichtbar ist sie nur im Marvin-Modus |

## Quests

Anleitung auf der Seite [Quests](quests.md).

| Funktion | Beschreibung |
|---|---|
| `bool QuestExists(name)`, `bool CreateQuest(name)` | Prüfen, anlegen |
| `bool RunQuest(name)`, `SucceedQuest(name)`, `FailQuest(name)`, `CloseQuest(name)`, `RemoveQuest(name)` | Starten, abschließen, scheitern lassen, schließen, entfernen |
| `int GetQuestStatus(name)`, `bool SetQuestStatus(name, int)` | Status lesen oder setzen (`QUEST_*`); `-1` = Quest gibt es nicht |
| `bool IsQuestFinished(name)`, `bool IsQuestSucceeded(name)` | Abfragen |
| `int GetQuestXP(name)`, `bool SetQuestXP(name, int)` | Erfahrungspunkte der Quest |
| `bool SetQuestReward(name, attribut, menge)`, `bCString GetQuestReward(name)` | Attribut-Belohnung (z. B. `"DEX"`, 1); das Spiel vergibt sie und nennt sie in der Erfolgsmeldung |
| `bool SetQuestTopic(name, text)`, `bool SetQuestDescription(name, text)` | Titel und Beschreibung im Questlog |
| `void RegisterText(id, text)` | Eigenen Text für eine Text-ID des Spiels festlegen |
| `int GetQuestCount()`, `bCString GetQuestNameAt(int)` | Alle Quests aufzählen |
| `bool ReloadQuestFiles()` | Quest-Dateien des Spiels neu einlesen |
| `bCString GetQuestDir()`, `bCString GetQuestFilePath(name)` | Ordner und Datei einer Quest |
| `bool LoadQuestFile(name, path)` | Eigenschaften einer Quest aus einer `.quest`-Datei lesen |
| `bCString DescribeQuestRewards()` | Alle Attribute, die Quests des Spiels als Belohnung vergeben |
| `void QuestDebug()` | Fehlersuche: schreibt die Rohdaten der Questliste ins Protokoll |
| Konstanten | `QUEST_OPEN` 0, `QUEST_RUNNING` 1, `QUEST_SUCCESS` 2, `QUEST_FAILED` 3, `QUEST_OBSOLETE` 4, `QUEST_CANCELLED` 5, `QUEST_LOST` 6, `QUEST_WON` 7 |

`LoadQuestFile` erwartet den Pfad so, wie das Spiel ihn benutzt: `Data/Quests/<Welt>/<Quest>_quest_<Welt>.quest`, mit `/` und relativ zum
Spielordner; die Datei darf auch in einem Archiv liegen. `GetQuestFilePath(name)` liefert genau diesen Pfad. Schreiben lassen sich Quest-Dateien nicht.

## Hooks

`bool Super()` ruft in einer `Override_...`-Funktion das Original auf (oder die nächste Mod mit demselben Override). `bool SetOverrideArg(int index, uint wert)`
ändert dabei ein Zahlenargument, `uint GetOverrideArg(int index)` liest es. Alles Weitere auf der Seite [Hooks](hooks.md).

## Schalter im Spielstand

| Funktion | Beschreibung |
|---|---|
| `bool SetGameEvent(name)`, `ClearGameEvent(name)`, `IsGameEventSet(name)` | Ja/Nein-Schalter, die mit dem Spielstand gespeichert werden ([Speichern und Laden](saving.md)) |

## Dialoge

Anleitung und Funktionen auf der Seite [Dialoge](dialogs.md): `GetInfoCount`, `GetInfoName`, `GetInfoOwner`, `DescribeInfo`, `DescribeCommands`,
`InfoExists`, `CreateInfo`, `RemoveInfo`, `SetInfoQuest`, `SetInfoParent`, `SetInfoType`, `SetInfoCondition`, `AddInfoLine`, `RebuildInfos`,
`SetInfoGiven`, `IsInfoGiven`; Konstanten `INFO_COND_*` und `INFO_TYPE_*`.

## Mod-Archive

`PackMod`, `PackFolder`, `NextModFile`, `GetPackError`: Ordner als Mod-Archiv packen. Siehe [Mod-Archive](archives.md#skript-schnittstelle-ab-version-24).

## Typen

Die Rechenfunktionen der Typen laufen über die Bibliothek des Spiels und rechnen deshalb genau wie das Spiel.

### `bCString`
Text (Windows-1252). `+`, `+=`, `==`, `<`, `+ int/uint/float/double/bool`, `GetLength()`, `IsEmpty()`, `Compare(s)`, `CompareNoCase(s)`,
`Find(s, start = 0)`, `Contains(s)`, `Left(n)`, `Right(n)`, `Mid(start, n)`, `ToUpper()`, `ToLower()`, `GetUpper()`, `GetLower()`.

### `bCVector` (`x`, `y`, `z`)
Punkt oder Richtung, Y ist oben, Einheit Zentimeter. `GetMagnitude()`, `GetSquareMagnitude()`, `Normalize()`, `GetNormalized()`, `GetDotProduct(v)`,
`GetCrossProduct(v)`, `+`, `-`, `* float`, `/ float`, `-v`, `==`, `ToString()`.

### `bCVector2` (`x`, `y`), `bCVector4` (`x`, `y`, `z`, `w`)
Felder, `GetMagnitude()`, `Normalize()`. `bCVector2` hat zusätzlich `+`, `-`, `*`.

### `bCQuaternion` (`x`, `y`, `z`, `w`)
Drehung, Standardwert ist „keine Drehung“. `Clear()`, `Normalize()`, `Invert()`, `GetMagnitude()`, `*`, `SetQuaternion(bCMatrix3)`.

### `bCMatrix3`
3 × 3-Drehmatrix, Standardwert ist die Einheitsmatrix. `GetElement(row, col)`, `SetElement(row, col, f)`, `SetIdentity()`, `Transpose()`, `Invert()`,
`* Matrix`, `* Vektor`, `SetToRotationAxisX/Y/Z(Bogenmaß)`, `SetToRotation(bCQuaternion)`.

### `bCMatrix4` (ab Version 19)
4 × 4-Matrix, Standardwert ist die Einheitsmatrix. Zeilenvektoren (`v' = v * M`): Zeilen 0 bis 2 sind die Achsen X, Y, Z, Zeile 3 die Verschiebung.
`GetElement(row, col)`, `SetElement(row, col, f)`, `SetIdentity()`, `Transpose()`, `Invert()`, `GetInverted()`, `GetDeterminant()`, `* bCMatrix4`,
`* bCVector` (Punkt samt Verschiebung), `GetTranslation()`, `SetTranslation(v)`, `GetXAxis()`, `GetYAxis()`, `GetZAxis()`, `SetToTranslation(v)`,
`SetToScaling(v)`, `SetToRotation(bCQuaternion)`, `SetToRotationAxisX/Y/Z(Bogenmaß)`, `GetRotationMatrix()` (als `bCMatrix3`), `GetRotation()`
(als `bCQuaternion`). Die Weltmatrix eines Entities liefert `GetWorldMatrix()`.
