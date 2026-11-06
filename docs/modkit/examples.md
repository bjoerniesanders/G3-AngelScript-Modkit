# Beispiele

Die fertigen Mods liegen im Repository unter `examples/mods/` und werden mit `tools/install_patch.ps1` bzw. durch Kopieren nach `<Gothic 3>\og3\mods\` benutzt.

## 1. Protokoll aller Ereignisse (`ereignisse`)

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
const bCString NPC = "Jens", QUEST = "OG3_Jens_Wolfsfell", ITEM = "It_Wolf_Fur";

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

## 5. Quest-Werkzeuge (`quest_demo`)

Numblock 1-6 und Bild hoch/runter, Einfg steuern die Demo-Quest `OG3_DemoQuest` (Numblock 1-6: Status zeigen, anlegen und starten, erfolgreich, fehlgeschlagen, entfernen, XP;
Bild hoch: alle Quests auflisten, Bild runter: Infos ansehen, Einfg: Quest-Dateien neu laden). Zum Ausprobieren der Quest-Funktionen und zum Herausfinden von Statuswerten.

## 7. Eigenschaften ansehen (`property_inspect`)

Strg + Ziffernblock `/` schreibt alle Eigenschaften des Items in der rechten Hand (sonst linke Hand, Rücken, sonst das Entity unter dem Fadenkreuz) mit ihren Werten ins Protokoll, getrennt nach den Sets
`Item`, `Damage` und `Interaction`. Damit lässt sich herausfinden, was ein Item im Spiel wirklich enthält (Wert, Voraussetzungen, `RequiredSkill`, `ActivateSkill`, `ModAttrib…`, Schadensart und -menge). Nur Lesen.
Die Funktionen dahinter sind `GetProperty` und `ListProperties` ([API](api-reference.md)).

## 6. Lebensraub (`lifesteal_demo`)

„Lebensraub“ ist eine **Eigenschaft der Waffe** wie „geschärft“ oder „gesegnet“: ein eigenes Qualitätsbit (`QUALITY_LIFESTEAL`), das in der Beschreibung der Waffe als Zeile `(20 %) Lebensraub` erscheint
(Text aus `stringtable.ini`, Schlüssel `HUD_Quality_Lifesteal`) und mit dem Spielstand gespeichert wird. **Ziffernblock \*** setzt oder entfernt die Eigenschaft an der Waffe des Helden (rechte Hand, sonst Rücken rechts
bzw. links). Schlägt der Held mit einer so markierten Waffe in der Hand zu, bekommt er 20 % des angerichteten Schadens als Lebenspunkte zurück (höchstens bis zum Maximum) und sieht `Lebensraub: +N HP`.
Zeigt `OnDamage` mit `owner` und `inflictor`, Qualität pro Slot lesen und setzen, einen eigenen Text über die Mod-Stringtabelle und einen Eingriff im Kampf. Der angerichtete Schaden ist die Menge von
`OnDamage`, bei einem tödlichen Treffer also nur der Rest der Lebenspunkte des Opfers. Abschalten mit `-lifesteal_demo` in `load_order.txt`.

## Tastenbelegung der Beispiel-Mods

Die Mods teilen sich die Tasten, ohne sich zu überschneiden. **Nicht benutzt werden Tasten, die das Spiel selbst belegt:** Numblock 7, 8, 9 (Debug-Zähler), F1 bis F4, F6 bis F9 (Zeitlupe, Spieler zurücksetzen, Schnellladen) und F12,
dazu Strg+Numblock 1-6. Frei sind außerdem Pos1, Bild hoch/runter, Einfg, Entf und die Rollen-Taste (ohne Strg). Pause fehlt auf vielen Tastaturen; Ende und Strg+Ende kamen im Test nicht beim Skript an (im Protokoll stand stattdessen Tastencode 3, Ursache nicht geklärt) und werden nicht benutzt.

| Mod | Tasten |
|---|---|
| `quest_demo` | Numblock 1-6 (Quest), Bild hoch (Quests auflisten), Bild runter (Infos ansehen), Einfg (Quest-Dateien neu laden) |
| `klon_demo` | Numblock 0 (Klon-Fell ins Inventar) |
| `spawn_demo` | Numblock `+` (Vorlagen suchen), `,` (NPC erzeugen: am Zielpunkt des Fadenkreuzes, sonst neben dem Helden), `-` (letzten entfernen), `*` (alle dieser Sitzung), `/` (alle erzeugten Diegos, auch aus früheren Sitzungen) |
| `focus_demo` | Pos1 (Fokus/Fadenkreuz/Zielpunkt melden), Entf (erzeugten NPC unter dem Fadenkreuz entfernen) |
| `jens_quest` | Rollen-Taste (Wolfsfell abgeben; Mod ist standardmäßig abgeschaltet) |
| `property_inspect` | Strg + Ziffernblock / (Eigenschaften des Items in der Hand oder unter dem Fadenkreuz ins Protokoll) |
| `reference_dump` | Ziffernblock / (Referenz-Dump ins Protokoll) |
| `lifesteal_demo` | Ziffernblock \* (Lebensraub an der geführten Waffe umschalten) |
| `examples/scripts/main.as` | F10, F11 |
| `index.md` Schnellstart | Rollen-Taste |

## Tipps zum Entwickeln

- Zuerst mit `print()` herausfinden, welche Namen das Spiel verwendet, dann die Namen im Mod fest eintragen.
- Eigene Mods klein anfangen und ein Ereignis nach dem anderen hinzufügen. Hot-Reload macht das schnell: speichern, 1 Sekunde warten, testen.
- Ziffernblock-Tasten als Test-Auslöser verwenden, sie sind im Spiel unbelegt.
- Mod abschalten: `-Name` in `og3\mods\load_order.txt`.
