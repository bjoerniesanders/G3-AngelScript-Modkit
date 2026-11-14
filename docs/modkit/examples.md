# Beispiele

Die fertigen Mods liegen im Repository unter `examples/mods/` und werden durch Kopieren nach `<Gothic 3>\g3asm\mods\` benutzt.

Die Namen sagen, was eine Mod ist:

| Name | Art |
|---|---|
| `demo_*` | Lern-Beispiel: zeigt eine Funktion des Modkits, klein und zum Kopieren gedacht |
| `probe_*` | Messmod: misst oder protokolliert etwas im Spiel, für die Entwicklung des Modkits |
| andere (`perk_lifesteal`, `beliars_klaue`, `jens_quest`, `jens_dialog`, `teleport_party`) | kleine fertige Mods mit Spielinhalt |

**Alle Beispiele sind standardmäßig aus** (`mod.ini` mit `[mod] enabled=0` im Mod-Ordner). Zum Testen eine Mod in `g3asm\mods\load_order.txt` mit `+Name` zuschalten, z. B. `+demo_lifesteal`. Ein Spielstand, der mit einer Mod gespeichert wurde, braucht sie beim Laden unter Umständen weiter (z. B. `perk_lifesteal`, siehe [Einschränkungen](limitations.md)).

## 1. Protokoll aller Ereignisse (`demo_events`)

Zeigt, was das Spiel meldet. Gut, um Namen von NPCs, Items und Zaubern herauszufinden.

```angelscript
bool IstHeld(Entity e) { return e.IsValid() && e == GetHero(); }

void OnNPCKilled(Entity opfer) {
    print("getötet: " + opfer.GetName());
    PrintScreen("Getötet: " + opfer.GetName(), 3, 255, 120, 120);
}

void OnReceiveItem(Entity von, Entity an, const bCString &in item, const bCString &in guid, uint anzahl) {
    print("Item " + item + " x" + anzahl + " von " + von.GetName() + " an " + an.GetName());
    if (IstHeld(an)) PrintScreen("+ " + item + " x" + anzahl, 3, 140, 255, 140);
}

void OnSpellCast(Entity wer, Entity ziel, const bCString &in zauber) {
    print(wer.GetName() + " zaubert " + zauber + " auf " + ziel.GetName());
}
```

## 2. Teleport-Lesezeichen

```angelscript
bCVector merk;
bool gemerkt = false;

void OnKey(int vk, bool down) {
    if (!down) return;
    Entity held = GetHero();
    if (vk == 0x61 && held.IsValid()) {            // Numblock 1: Position merken
        merk = held.GetPosition();
        gemerkt = true;
        PrintScreen("Position gemerkt: " + merk.ToString());
    } else if (vk == 0x62 && gemerkt) {            // Numblock 2: zurück
        held.SetPosition(merk);
    }
}
```

## 3. Attribute und Zeit

```angelscript
void OnKey(int vk, bool down) {
    if (!down) return;
    Entity held = GetHero();
    if (vk == 0x63) {                              // Numblock 3: +1 Stärke
        held.AddAttributeBase("STR", 1);
        PrintScreen("Stärke: " + held.GetAttribute("STR"));
    } else if (vk == 0x64) {                       // Numblock 4: Mittag
        SetGameTime(-1, 12, 0);
    }
}
```

## 4. Quest mit Abgabe (`jens_quest`)

Der Held nähert sich Jens (Kap Dun): er bittet um ein Wolfsfell, die Quest startet. Abgabe: Rollen-Taste bei Jens (nimmt ein Fell aus dem Inventar) oder
Übergabe im Spiel (`OnReceiveItem`). Tötest du Jens, scheitert die Quest.

Ausschnitt:

```angelscript
const bCString NPC = "Jens", QUEST = "G3ASM_Jens_Wolfsfell", ITEM = "It_Wolf_Fur";

void OnFrame(float dt) {
    timer += dt;
    if (timer < 0.5f) return;
    timer = 0;
    Entity held = GetHero(), jens = FindEntity(NPC);
    if (!InReichweite(held, jens)) { gebeten = false; return; }
    int s = GetQuestStatus(QUEST);
    if (s == QUEST_SUCCESS || s == QUEST_FAILED) return;
    if (s != QUEST_RUNNING && !gebeten) {
        gebeten = true;
        CreateQuest(QUEST);
        SetQuestTopic(QUEST, "Wolfsfell fuer Jens");
        SetQuestDescription(QUEST, "Jens braucht ein Wolfsfell. Bring es ihm.");
        RunQuest(QUEST);
        PrintScreen("Jens: Hey du! Bring mir ein Wolfsfell, ich bezahle gut.", 5);
    }
}

void OnKey(int vk, bool down) {
    if (!down || vk != 0x91) return;               // Rollen-Taste
    Entity held = GetHero();
    if (held.GetItemCount(ITEM) < 1) return;
    if (held.RemoveItems(ITEM, 1) < 1) return;
    SetQuestXP(QUEST, 100);
    SucceedQuest(QUEST);
}
```

## 5. Quest-Werkzeuge (`demo_quest`)

Numblock 1-6 und Bild hoch/runter, Einfg steuern die Demo-Quest `G3ASM_DemoQuest` (Numblock 1-6: Status zeigen, anlegen und starten, erfolgreich, fehlgeschlagen, entfernen, XP;
Bild hoch: alle Quests auflisten, Bild runter: Infos ansehen, Einfg: Quest-Dateien neu laden). Zum Ausprobieren der Quest-Funktionen und zum Herausfinden von Statuswerten.

## 6. Eigenschaften ansehen (`demo_properties`)

Strg + Ziffernblock `/` schreibt alle Eigenschaften des Items in der rechten Hand (sonst linke Hand, Rücken, sonst das Entity unter dem Fadenkreuz) mit ihren Werten ins Protokoll, getrennt nach den Sets
`Item`, `Damage` und `Interaction`. Damit lässt sich herausfinden, was ein Item im Spiel wirklich enthält (Wert, Voraussetzungen, `RequiredSkill`, `ActivateSkill`, `ModAttrib…`, Schadensart und -menge). Nur Lesen.
Die Funktionen dahinter sind `GetProperty` und `ListProperties` ([API](api-reference.md)).

## 7. Lebensraub (`demo_lifesteal`)

„Lebensraub“ ist eine **Eigenschaft der Waffe** wie „geschärft“ oder „gesegnet“: ein eigenes Qualitätsbit (`QUALITY_LIFESTEAL`), das in der Beschreibung der Waffe als Zeile `(20 %) Lebensraub` erscheint
(Text aus `stringtable.ini`, Schlüssel `HUD_Quality_Lifesteal`) und mit dem Spielstand gespeichert wird. **Ziffernblock \*** setzt oder entfernt die Eigenschaft an der Waffe des Helden (rechte Hand, sonst Rücken rechts
bzw. links). Schlägt der Held mit einer so markierten Waffe in der Hand zu, bekommt er 20 % des angerichteten Schadens als Lebenspunkte zurück (höchstens bis zum Maximum) und sieht `Lebensraub: +N HP`.
Zeigt `OnDamage` mit `owner` und `inflictor`, Qualität pro Slot lesen und setzen, einen eigenen Text über die Mod-Stringtabelle und einen Eingriff im Kampf. Der angerichtete Schaden ist die Menge von
`OnDamage`, bei einem tödlichen Treffer also nur der Rest der Lebenspunkte des Opfers. Abschalten mit `-demo_lifesteal` in `load_order.txt`.

## 8. Talent mit Kachel und Heilung (`perk_lifesteal`) und Klaue Beliars (`beliars_klaue`)

`perk_lifesteal` baut ein eigenes Bonustalent „Lebensraub“ für den Orkschlächter (Talent-Vorlage klonen, Lern-Item klonen und per `SetTemplateRef` verweisen, Stapel dem Helden geben, `ActivateSkill2` der Waffe setzen) samt Kachel im Charakterbogen und Heilung in `OnDamage`. Schalter `ENTFERNEN` nimmt den Stapel vor dem Abschalten die Mod wieder heraus (sonst Absturz im Charakterbogen), siehe [Einschränkungen](limitations.md).

`beliars_klaue` löst bei einem **starken** Treffer mit dem Orkschlächter (mindestens `MIN_TREFFER` = 55 Schaden) gegen einen **Gegner** (Haltung `AttitudeToPlayer2` feindlich oder Raubtier, bei Tieren auch verärgert) mit `CHANCE_PROZENT` = 30 % Chance einen Blitz aus: `Smn_Lightning` am Opfer erzeugen, den Helden mit `SetEntityRef(..., "gCInteraction_PS", "Owner", hero)` als Besitzer eintragen (nur dann läuft der Schaden durch die echte Kette: Quelle `Smn_Lightning`, Besitzer Held, Art 6) und den Blitz mit `StartEffect("eff_magic_summonlightning_target_01", opfer)` sichtbar machen. Gemessen: Der Blitz macht 17 bis über 500 Schaden (das Skript `ZS_Lightning` rechnet selbst) und kann Umstehende treffen. Zwischen zwei Blitzen liegen mindestens 4 Sekunden.

## 9. Zeichnen prüfen (`probe_draw`, Messmod)

Ab Version 34, 38 und 39. Zeigt oben links Bildgröße und Bildzähler, darunter Kompass (Pfad und Dateiname), einen sich drehenden Kompass (`OnFrame` dreht
ihn über die Zeit) und ein Orts-Symbol (`LoadImage`/`DrawImage`), Marken in den vier Ecken, einen Text in der Bildmitte und roten Text über der
Statusleiste (alles über HUD und Inventar), dazu „Held“ und „Mitte“ an der Position des Helden (`WorldToScreen`). Standardmäßig aus (`+probe_draw`).

## 10. Spielskripte nach Namen (`probe_script`, Messmod)

Ab Version 32. Beim ersten Weltstart prüft die Mod mit `ScriptExists`/`GetScriptDll`, welche bekannten Skriptnamen es gibt. Sie meldet einige Skripte
(`AssessHit`, `MagicTeleport`, `IsDeadlyDamage`, ...) mit `WatchScript` an, reicht sie in `Override_Script` unverändert durch und schreibt die ersten
Aufrufe je Name mit Self, Other, Zahl und Ergebnis ins Protokoll (`OnScript`). Strg + Ziffernblock + ruft lesende Skripte (`IsHumanoid`,
`GetStaminaPoints`, `GetAttitude`, ...) per `CallScript` für den Helden und das anvisierte Entity auf. Standardmäßig aus (`+probe_script`).
Mit `scriptlog=1` in `g3asm.ini` schreibt das Modkit zusätzlich alle 60 s die häufigsten Skriptnamen und den Zeitaufwand seiner Hooks.

## 11. Begleiter teleportieren mit (`teleport_party`)

Ab Version 33. Fängt das Spielskript `MagicTeleport` ab (`WatchScript`/`Override_Script`) und holt nach dem Teleport des Helden seine Begleiter, Beschwörungen
und Sklaven mit `TeleportTo` zu ihm und stellt sie 1,5 m neben ihn, mehrere im Kreis (genau auf dem Helden würden sie ihn sofort ansprechen); wartende Begleiter bleiben (`WARTENDE_MIT`). Weil `MagicTeleport` selbst alle Begleiter auf Warten schaltet, gilt der Wartestatus von vor dem Teleport; die Mod hebt das Warten der Mitgenommenen danach wieder auf. Nach 1,5 s holt die Mod alle nach, die noch weiter als 15 m weg sind.
Strg + Ziffernblock , schreibt die Gruppe ins Protokoll. Standardmäßig aus (`+teleport_party`).

## 12. Zeitrate und Spieluhr (`probe_time`, Messmod)

Ab Version 36. Umschalt + Ziffernblock + / - verdoppelt bzw. halbiert die Zeitrate (`SetTimeScale`), Umschalt + Ziffernblock \* schaltet die Spieluhr
auf das Zehnfache (`SetClockFactor`) und zurück. Weil der Uhrfaktor im Spielstand steht, stellt die Mod ihn in `OnBeforeSave` zurück und setzt ihn in
`OnAfterSave` wieder. Die Werte stehen oben links (`OnDraw`). Standardmäßig aus (`+probe_time`).

## 13. KI-Skripte und Untertitel (`probe_ai`, Messmod)

Ab Version 41. Meldet die Namen an, die das Plugin G3Fixes umleitet (`ZS_ObserveSuspect`, `_AI_UseInventoryItem`, `OnFollowPlayer`, `OnGuidePlayer`), reicht
sie in `Override_AI` unverändert durch und schreibt die ersten fünf Aufrufe je Name ins Protokoll (`OnAI`). Jeder gesprochene Satz (`OnAIOutput`) kommt mit
ID und Text (`GetSpeechText`) ins Protokoll und steht vier Sekunden unten in der Bildmitte: ein einfacher Nachbau des Plugins Subtitles. Standardmäßig aus (`+probe_ai`).

## 14. Monsterschaden (`probe_monster_damage`, Messmod)

Ab Version 42. Ersetzt die Teilung des Monsterschadens an NPCs (durch 10, bei Beschworenen durch 2) durch den Faktor 0,2 wie das Plugin MonsterDamage
(Standardwert `Game.MonsterDamageMultiplicator=0.2`): Monster richten an NPCs doppelt so viel Schaden an wie im Original. Die ersten zehn Treffer stehen mit
rohem, originalem und neuem Wert im Protokoll. Standardmäßig aus (`+probe_monster_damage`).

## 15. Lernkosten (`probe_learn_cost`, Messmod)

Ab Version 42. Halbiert bei Lehrern die Lernpunkte (aufgerundet) und das Gold für Attribute; hat das Spiel wegen „Alternative Balance“ ab 250 verdoppelt,
nimmt die Mod das zuerst zurück (wie der Modus `NoThreshold` des Plugins LearningThresholdTweaks). Die ersten zehn Abfragen stehen im Protokoll.
Standardmäßig aus (`+probe_learn_cost`).

## 16. Schema der Eigenschaften (`probe_schema`, Messmod)

Ab Version 43. Ruft `DumpPropertySchema` bei den ersten beiden Weltaktivierungen auf (Hauptmenü, dann nach dem Laden) und schreibt
`<Spiel>\g3asm\property_schema_1.json` und `_2.json`; die Zahl der Klassen steht im Protokoll. Keine Taste, etwa 0,1 s je Dump.
Standardmäßig aus (`+probe_schema`).

## 17. Angriffsgeschwindigkeit (`probe_attack_speed`, Messmod)

Ab Version 45. Ändert in `Override_CombatMove` die Geschwindigkeit der Kampfbewegungen des Helden um `Faktor` aus den Einstellungen
(`mod.ini [settings]`, Standard 2,0, damit es deutlich sichtbar ist; `AlleNPCs=ja` gilt für alle). Die ersten 20 Änderungen stehen im Protokoll.
Zeigt nebenbei die Einstellungen je Mod (Version 44). Standardmäßig aus (`+probe_attack_speed`).

## 18. Freeze-Liste (`probe_freeze`, Messmod)

Ab Version 46. Misst die Funktionen, die mit Version 46 neu dazukamen oder ihren Vermerk verloren: nach dem Laden automatisch `LoadQuestFile` mit zwei Pfadformen; Strg + Umschalt + Ziffernblock `*` `Kill` am anvisierten Wesen, `-` `GenerateTradeGoods` am anvisierten NPC, `+` der Reihe nach Fadenkreuz aus/an und Uhr 22:00/10:00; dazu `OnTransformStart`/`OnTransformEnd` und `OnNPCKilled`. Standardmäßig aus (`+probe_freeze`).

## Weitere Beispiele

Diese Mods haben hier keinen eigenen Abschnitt; die Beschreibung steht am Anfang der jeweiligen `main.as` und auf der genannten Seite.

| Mod | Zeigt | Seite |
|---|---|---|
| `demo_hooks` | Spielfunktionen mit `Override_...` und `Super()` abfangen | [Hooks](hooks.md) |
| `jens_dialog` | Eigene Gesprächsoptionen mit Questlog-Verbindung, Quest aus einem Mod-Archiv | [Dialoge](dialogs.md), [Mod-Archive](archives.md) |
| `demo_packmod` | Mod-Archiv schreiben und prüfen, ob das Spiel es liest | [Mod-Archive](archives.md) |
| `demo_patch` | Datei-Patch auf eine Quest-Datei des Spiels | [Datei-Patches](patches.md) |
| `demo_text` | Eigene Texte über die Stringtabelle der Mod (nur `stringtable.ini`, kein Skript) | [Mod-Ordner](mod-package.md) |
| `demo_clone` | Item-Vorlage klonen (`CloneTemplate`) mit eigenem Namen und Text | [API-Referenz](api-reference.md) |
| `demo_spawn`, `demo_focus` | NPCs erzeugen und entfernen, Fokus und Fadenkreuz lesen | [API-Referenz](api-reference.md) |
| `probe_lightning`, `probe_events`, `probe_reference` | Messmods: Blitz auf ein Ziel, Konsole und Quest-Status, geprüfte Listen fürs Protokoll | [Referenz](reference.md) |

## Tastenbelegung der Beispiel-Mods

Die Mods werden einzeln zum Testen zugeschaltet; einige teilen sich Tasten (`demo_spawn` mit `demo_lifesteal` bei `*` und mit `probe_reference` bei `/`), dann nicht beide gleichzeitig zuschalten. **Nicht benutzt werden Tasten, die das Spiel selbst belegt:** Numblock 7, 8, 9 (Debug-Zähler), F1 bis F4, F6 bis F9 (Zeitlupe, Spieler zurücksetzen, Schnellladen) und F12,
dazu Strg+Numblock 1-6. Frei sind außerdem Pos1, Bild hoch/runter, Einfg, Entf und die Rollen-Taste (ohne Strg). Pause fehlt auf vielen Tastaturen; Ende und Strg+Ende kamen im Test nicht beim Skript an (im Protokoll stand stattdessen Tastencode 3, Ursache nicht geklärt) und werden nicht benutzt.

| Mod | Tasten |
|---|---|
| `demo_quest` | Numblock 1-6 (Quest), Bild hoch (Quests auflisten), Bild runter (Infos ansehen), Einfg (Quest-Dateien neu laden) |
| `demo_clone` | Numblock 0 (Klon-Fell ins Inventar) |
| `demo_spawn` | Numblock `+` (Vorlagen suchen), `,` (NPC erzeugen: am Zielpunkt des Fadenkreuzes, sonst neben dem Helden), `-` (letzten entfernen), `*` (alle dieser Sitzung), `/` (alle erzeugten Diegos, auch aus früheren Sitzungen) |
| `demo_focus` | Pos1 (Fokus/Fadenkreuz/Zielpunkt melden), Entf (erzeugten NPC unter dem Fadenkreuz entfernen) |
| `jens_quest` | Rollen-Taste (Wolfsfell abgeben; Mod ist standardmäßig abgeschaltet) |
| `demo_properties` | Strg + Ziffernblock / (Eigenschaften des Items in der Hand oder unter dem Fadenkreuz ins Protokoll) |
| `probe_reference` | Ziffernblock / (Referenz-Dump ins Protokoll) |
| `demo_lifesteal` | Ziffernblock \* (Lebensraub an der geführten Waffe umschalten) |
| `probe_lightning` | Strg + Ziffernblock - (Blitz auf den anvisierten NPC, Messmod) |
| `teleport_party` | Strg + Ziffernblock , (Gruppe des Helden ins Protokoll) |
| `probe_time` | Umschalt + Ziffernblock + / - (Zeitrate), Umschalt + Ziffernblock \* (Spieluhr x10), Messmod |
| `probe_events` | Strg + Ziffernblock \* (Konsole öffnen), Umschalt + Ziffernblock / (Ware des anvisierten Händlers neu erzeugen), Messmod |
| `probe_script` | Strg + Ziffernblock + (lesende Spielskripte per `CallScript` aufrufen, Messmod) |
| `examples/scripts/main.as` | F10, F11 |
| `index.md` Schnellstart | Rollen-Taste |

## Tipps zum Entwickeln

- Zuerst mit `print()` herausfinden, welche Namen das Spiel verwendet, dann die Namen in der Mod fest eintragen.
- Eigene Mods klein anfangen und ein Ereignis nach dem anderen hinzufügen. Hot-Reload macht das schnell: speichern, 1 Sekunde warten, testen.
- Ziffernblock-Tasten als Test-Auslöser verwenden, sie sind im Spiel unbelegt.
- Mod abschalten: `-Name` in `g3asm\mods\load_order.txt`; Beispiel-Mods zuschalten: `+Name`.
