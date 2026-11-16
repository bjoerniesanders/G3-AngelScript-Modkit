# Beispiele

Die fertigen Mods liegen unter `examples/mods/` (im Release-Zip `examples\mods\`). Wer noch keine Mod geschrieben hat, beginnt besser mit
[Erste Mod Schritt für Schritt](erste-mod.md).

## So benutzt du ein Beispiel

1. Kopiere den Ordner der Mod nach `<Spielordner>\g3asm\mods\`, z. B. `examples\mods\demo_events` nach `<Spielordner>\g3asm\mods\demo_events`.
2. Alle Beispiele sind **standardmäßig aus**. Schalte die Mod zu: Trage in `<Spielordner>\g3asm\mods\load_order.txt` eine Zeile `+demo_events` ein.
3. Starte das Spiel. Was die Mod tut, steht in `g3asm.log` und oft auch auf dem Bildschirm.

**Achtung:** Einige Mods schreiben dauerhafte Dinge in den Spielstand (`perk_lifesteal`, `probe_time`). Lies den Abschnitt der Mod, bevor du mit ihr
speicherst. Mods, die sich eine Taste teilen, nicht gleichzeitig zuschalten (siehe [Tastenbelegung](#tastenbelegung)).

Die Namen sagen, was eine Mod ist:

| Name | Art |
|---|---|
| `demo_*` | Lern-Beispiel: zeigt eine Funktion des Modkits, klein und zum Kopieren gedacht |
| `probe_*` | Messmod: misst oder protokolliert etwas im Spiel, für die Entwicklung des Modkits |
| andere | kleine fertige Mods mit Spielinhalt: `jens_quest`, `jens_dialog`, `perk_lifesteal`, `beliars_klaue`, `teleport_party` |

## Kleine Skripte zum Abschreiben

Drei Stücke ohne eigenen Mod-Ordner. Jedes passt in eine `main.as`.

**Ereignisse mitschreiben** (die ausführliche Fassung ist `demo_events`). Gut, um Namen von NPCs, Items und Zaubern herauszufinden:

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

**Position merken und zurückkehren** (Ziffernblock 1 merkt, Ziffernblock 2 kehrt zurück):

```angelscript
bCVector merk;
bool gemerkt = false;

void OnKey(int vk, bool down) {
    if (!down) return;
    Entity held = GetHero();
    if (vk == 0x61 && held.IsValid()) {            // Ziffernblock 1: Position merken
        merk = held.GetPosition();
        gemerkt = true;
        PrintScreen("Position gemerkt: " + merk.ToString());
    } else if (vk == 0x62 && gemerkt) {            // Ziffernblock 2: zurück
        held.SetPosition(merk);
    }
}
```

**Attribute und Uhrzeit** (Ziffernblock 3: +1 Stärke, Ziffernblock 4: Mittag):

```angelscript
void OnKey(int vk, bool down) {
    if (!down) return;
    Entity held = GetHero();
    if (vk == 0x63) {                              // Ziffernblock 3: +1 Stärke
        held.AddAttributeBase("STR", 1);
        PrintScreen("Stärke: " + held.GetAttribute("STR"));
    } else if (vk == 0x64) {                       // Ziffernblock 4: Mittag
        SetGameTime(-1, 12, 0);
    }
}
```

## Mods mit Spielinhalt

### `jens_quest`: Quest ohne Dialog

**Zeigt:** eine Quest, die nur aus Ereignissen besteht. Nähert sich der Held Jens in Kap Dun, bittet Jens um ein Wolfsfell, und die Quest startet.
Abgeben kannst du das Fell mit der Rollen-Taste in Jens' Nähe oder, indem du es ihm im Spiel gibst (`OnReceiveItem`). Stirbt Jens, scheitert die Quest.

```angelscript
// Ausschnitt
void OnKey(int vk, bool down) {
    if (!down || vk != 0x91) return;               // Rollen-Taste
    Entity held = GetHero();
    if (held.GetItemCount(ITEM) < 1) return;
    if (held.RemoveItems(ITEM, 1) < 1) return;
    SetQuestXP(QUEST, 100);
    SucceedQuest(QUEST);
}
```

**Achtung:** Nicht zusammen mit `jens_dialog` zuschalten; beide benutzen dieselbe Quest.

### `jens_dialog`: Quest mit Gesprächen

**Zeigt:** die ausführliche Fassung von [Erste Mod Schritt für Schritt](erste-mod.md). Jens bekommt zwei Gesprächsoptionen („Hast du Arbeit für
mich?“ und „Ich habe das Wolfsfell.“), die Quest steht mit den Dialogzeilen im Questlog, die Belohnung ist ein Bogen, Pfeile und ein Punkt
Geschick. Dazu zwei Dinge, die die Anleitung weglässt:
- Die Quest kommt als echte Quest-Datei aus einem Mod-Archiv (`ModFiles\Quests.m0x`, siehe [Mod-Archive](archives.md)). Ist das Archiv nicht
  installiert, legt das Skript sie selbst an.
- Ein Rückfall für Spielstände, die vor dem Zuschalten der Mod entstanden sind: Der Fortschritt steht zusätzlich in Schaltern im Spielstand
  (`SetGameEvent`), siehe [Speichern und Laden](saving.md#notfall-wiederherstellung).

### `perk_lifesteal`: eigenes Talent mit Kachel

**Zeigt:** ein Bonustalent „Lebensraub“ für den Orkschlächter, mit Kachel im Charakterbogen. Wer den Orkschlächter führt, heilt sich bei jedem Treffer.
Dafür klont die Mod eine Talent-Vorlage und ein Lern-Item, verbindet beide mit `SetTemplateRef`, gibt dem Helden das Lern-Item und lässt die Waffe
das Talent einschalten. Die Heilung kommt aus `OnDamage`.

**Achtung:** Das Talent steht im Spielstand. Die Mod nimmt es deshalb vor jedem Speichern heraus und legt es danach wieder an. Willst du die Mod
abschalten und hast ältere Spielstände mit dem Talent, setze vorher in `main.as` `ENTFERNEN = true`, lade, speichere und schalte erst dann ab.
Sonst stürzt das Spiel beim Öffnen des Charakterbogens ab.

### `beliars_klaue`: Blitz bei starken Treffern

**Zeigt:** Schaden mit Täter über eine Beschwörung. Trifft der Held mit dem Orkschlächter einen Gegner stark (mindestens 55 Schaden), schlägt mit 30 %
Chance ein Blitz ein. Die Mod erzeugt dazu die Beschwörung `Smn_Lightning` am Opfer, trägt den Helden mit `SetEntityRef` als Besitzer ein und
startet den Blitz-Effekt.

- Als Gegner zählen NPCs, die dem Helden feindlich gesinnt sind, Raubtiere und verärgerte Tiere.
- Zwischen zwei Blitzen liegen mindestens 4 Sekunden. Der Blitz macht zwischen 17 und über 500 Schaden und kann Umstehende treffen.
- Werte zum Anpassen stehen oben in `main.as` (`MIN_TREFFER`, `CHANCE_PROZENT`).

### `teleport_party`: Begleiter teleportieren mit

**Zeigt:** ein Spielskript abfangen. Teleportiert sich der Held, holt die Mod seine Begleiter, Beschwörungen und Sklaven nach und stellt sie im Kreis
1,5 m neben ihn. Dazu fängt sie das Spielskript `MagicTeleport` ab (`WatchScript` und `Override_Script`).

- Begleiter, denen der Held „Warte hier“ gesagt hat, bleiben (`WARTENDE_MIT` in `main.as`).
- Wer nach 1,5 Sekunden noch weiter als 15 m weg ist, wird noch einmal geholt.
- **Strg + Ziffernblock ,** schreibt die Gruppe ins Protokoll.

## Lern-Beispiele

| Mod | Zeigt | Bedienung |
|---|---|---|
| `demo_events` | Alle Ereignisse des Questsystems im Protokoll und auf dem Bildschirm | – |
| `demo_hooks` | Spielfunktionen mit `Override_...` und `Super()` ändern ([Hooks](hooks.md)) | – |
| `demo_quest` | Alle Quest-Funktionen an einer Demo-Quest | Ziffernblock 1–6: Status, anlegen und starten, Erfolg, Fehlschlag, entfernen, XP; Bild hoch: Quests auflisten; Bild runter: Infos ansehen; Einfg: Quest-Dateien neu laden |
| `demo_properties` | Eigenschaften eines Items lesen | Strg + Ziffernblock /: Item in der Hand (sonst unter dem Fadenkreuz) ins Protokoll |
| `demo_lifesteal` | Qualität einer Waffe setzen, eigener Text in der Beschreibung, Heilung bei Treffern | Ziffernblock \*: „Lebensraub“ an der Waffe ein/aus |
| `demo_clone` | Item-Vorlage klonen, mit eigenem Namen und Text | Ziffernblock 0: geklontes Fell ins Inventar |
| `demo_spawn` | NPCs erzeugen und entfernen | Ziffernblock +: Vorlagen suchen; `,`: NPC am Fadenkreuz erzeugen; `-`: letzten entfernen; `*`: alle dieser Sitzung; `/`: alle erzeugten Diegos |
| `demo_focus` | Fokus und Fadenkreuz lesen | Pos1: anvisiertes Ziel melden; Entf: erzeugten NPC unter dem Fadenkreuz entfernen |
| `demo_text` | Eigene Texte nur über `stringtable.ini`, ohne Skript ([Mod-Ordner](mod-package.md#eigene-texte-stringtableini)) | – |
| `demo_patch` | Datei-Patch auf eine Quest-Datei ([Datei-Patches](patches.md)) | – |
| `demo_packmod` | Mod-Archiv schreiben und prüfen, ob das Spiel es liest ([Mod-Archive](archives.md)) | – |

**`demo_lifesteal`:** „Lebensraub“ ist eine Eigenschaft der Waffe wie „geschärft“ und steht als Zeile `(20 %) Lebensraub` in ihrer Beschreibung.
Schlägt der Held damit zu, bekommt er 20 % des Schadens als Lebenspunkte zurück. Bei einem tödlichen Treffer zählt nur der Rest der Lebenspunkte des
Opfers.

## Messmods

Diese Mods dienen der Entwicklung des Modkits. Sie schreiben vor allem ins Protokoll; zum Lernen taugen sie, wenn du eine bestimmte Funktion suchst.

| Mod | Misst | Bedienung |
|---|---|---|
| `probe_draw` | Zeichnen über HUD und Inventar: Text, Bilder, sich drehender Kompass, `WorldToScreen` | – |
| `probe_script` | Spielskripte nach Namen: `ScriptExists`, `Override_Script`, `CallScript` | Strg + Ziffernblock +: lesende Skripte für Held und Ziel aufrufen |
| `probe_time` | Zeitrate und Spieluhr | Umschalt + Ziffernblock + / -: Zeitrate doppelt/halb; Umschalt + Ziffernblock \*: Uhr ×10 |
| `probe_ai` | KI-Skripte (`Override_AI`) und gesprochene Sätze als Untertitel (`OnAIOutput`) | – |
| `probe_monster_damage` | `Override_MonsterDamage`: Monster richten an NPCs doppelt so viel Schaden an | – |
| `probe_learn_cost` | `Override_LearnCost`: Lehrer verlangen die Hälfte | – |
| `probe_schema` | `DumpPropertySchema`: schreibt `g3asm\property_schema_1.json` und `_2.json` | – |
| `probe_attack_speed` | `Override_CombatMove`: Schläge des Helden schneller (Faktor in der `mod.ini`) | – |
| `probe_freeze` | Funktionen, die mit Version 46 kamen | Strg + Umschalt + Ziffernblock \*: `Kill`; -: `GenerateTradeGoods`; +: Fadenkreuz und Uhr |
| `probe_events` | Konsole, Quest-Status, Händlerware | Strg + Ziffernblock \*: Konsole öffnen; Umschalt + Ziffernblock /: Ware des Händlers neu erzeugen |
| `probe_lightning` | Blitz auf ein Ziel | Strg + Ziffernblock -: Blitz auf den anvisierten NPC |
| `probe_reference` | Listen geprüfter Namen fürs Protokoll ([Referenz](reference.md)) | Ziffernblock /: Liste schreiben |

**Achtung bei `probe_time`:** Der Uhrfaktor steht im Spielstand. Die Mod stellt ihn vor dem Speichern zurück und setzt ihn danach wieder.

## Tastenbelegung

Freie Tasten für eigene Mods: der Ziffernblock außer 7, 8, 9 und Strg + 1 bis 6, dazu Pos1, Bild hoch, Bild runter, Einfg, Entf und die Rollen-Taste
(ohne Strg).

Das Spiel belegt selbst:
- Ziffernblock 7, 8, 9 und Strg + Ziffernblock 1 bis 6,
- F1 bis F4, F6 bis F9 (Zeitlupe, Spieler zurücksetzen, Schnellladen) und F12.

Nicht benutzen: Ende und Strg + Ende kommen nicht beim Skript an; Pause fehlt auf vielen Tastaturen.

Diese Beispiele teilen sich Tasten und gehören nicht gleichzeitig zugeschaltet:
- `demo_spawn` und `demo_lifesteal` (Ziffernblock \*),
- `demo_spawn` und `probe_reference` (Ziffernblock /),
- `jens_quest` und die Mini-Mod aus dem [Schnellstart](index.md#schnellstart) (Rollen-Taste).

Die Mini-Mod `examples/scripts/main.as`, die der Installer anlegt, benutzt F10 und F11.

## Tipps

- Finde Namen zuerst mit `print()` heraus und trage sie dann fest in die Mod ein.
- Fang klein an und füge ein Ereignis nach dem anderen hinzu. Mit Hot-Reload heißt das: speichern, eine Sekunde warten, ausprobieren.
- Eine Mod abschalten: `-Name` in `g3asm\mods\load_order.txt`; ein Beispiel zuschalten: `+Name`.
