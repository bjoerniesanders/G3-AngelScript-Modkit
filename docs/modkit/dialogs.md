# Dialoge (Infos)

Gespräche in Gothic 3 bestehen aus **Infos**: Eine Info ist eine Option im Gesprächsmenü eines NPCs samt den Zeilen, die danach gesprochen werden.
Das Modkit kann Infos lesen und eigene anlegen. Eine Schritt-für-Schritt-Anleitung steht in [Erste Mod](erste-mod.md#5-ein-gespräch-bei-jens).
Im Spiel gibt es 4593 Infos. Jede gehört einem NPC und läuft als kleines Skript aus Befehlszeilen.

## Aufbau einer Info

| Teil | Bedeutung |
|---|---|
| Name | eindeutige Kennung (`SKALVERAM31`, bei eigenen z. B. `G3ASM_Jens_Job`) |
| Owner | der NPC, bei dem die Option erscheint (`Jens`) |
| Parent | Eltern-Info: macht daraus eine Folgefrage im Gesprächsbaum |
| Quest | verknüpft die Info mit einer Quest: ihre Gesprächszeilen kommen automatisch ins Questlog dieser Quest |
| Bedingungsart | wann die Option erscheint (siehe unten) |
| Typ | `INFO_TYPE_INFO` (3, normale Option), `INFO_TYPE_NEWS` (2), `INFO_TYPE_IMPORTANT` (1), `INFO_TYPE_COMMENT` (5), ... |
| Skriptzeilen | Befehle, die ablaufen, wenn der Spieler die Option wählt |

Bedingungsarten (`INFO_COND_*`): `GENERAL` (3, immer wählbar), `OPEN` (5, Quest noch nicht gestartet), `ACTIVATOR` (6, Quest anbieten),
`RUNNING` (7, solange die Quest läuft), `DELIVERY` (8) und `PARTDELIVERY` (9, Abgabe), `SUCCESS` (10), `FAILED` (12), außerdem `CRIME`, `FIGHT`, `HELLO`, `OVERTIME`, `DOCANCEL`.

Die erste Zeile `Say` des Spielers ist der Text, der im Gesprächsmenü steht.

## Befehle in Skriptzeilen

`AddInfoLine(info, befehl, entity1, entity2, id1, id2, text)` hängt eine Zeile an. Häufige Befehle im Spiel (Anzahl der Vorkommen):

| Befehl | Parameter | Wirkung |
|---|---|---|
| `Say` (12553) | entity1 = `player` oder `npc`, id1 = Sprechstimmung (z. B. `SD_Thoughtful`), text | spricht eine Zeile |
| `SaySVM` (467) | entity1, id1 = Standardsatz (`LetMePass`) | Standardsatz des NPC |
| `Description` (623) | entity1 = `player`, text | Beschriftung einer Antwort (z. B. `ENDE`) |
| `End` (271) | | beendet das Gespräch |
| `SetGameEvent` / `ClearGameEvent` (1354 / 147) | id1 = Ereignisname | Schalter im Spielstand setzen/löschen |
| `RunQuest` (57), `SucceedQuest` (20), `CloseQuest` (76) | id1 = Questname | steuert eine Quest |
| `AddQuestLog` (2) | id1 = Quest, id2 = Text-ID | schreibt einen Eintrag ins Questlog |
| `Give` (584) | entity1 = `npc`, entity2 = `player`, id1 = Item, id2 = Menge | NPC gibt etwas |
| `Erase` (4) | entity1 = `player`, id1 = Item, id2 = Menge | nimmt etwas weg |
| `GiveXP` (346) | id1 = Menge | Erfahrungspunkte |
| `SetTradeEnabled`, `SetTeachEnabled` (139, 143) | entity1 = `npc`, id1 = `true`/`false` | Handel bzw. Lehrer ein/aus |
| `SetRoutine` (247) | entity1 = NPC, id1 = Tagesablauf (`Work`, `Free`) | Tagesablauf wechseln |
| `Attack` (83) | entity1 = `npc`, entity2 = `player` | NPC greift an |
| `InfoWait` (51) | id1 = Millisekunden | Pause |
| `ShowGameMessage` (2) | id2 = Art, text | Meldung auf dem Bildschirm |

`DescribeCommands()` listet alle 60 Befehle des Spiels mit einem Beispiel.

## Funktionen

| Funktion | Beschreibung |
|---|---|
| `int GetInfoCount()`, `GetInfoName(i)`, `GetInfoOwner(i)` | Infos auflisten |
| `bCString DescribeInfo(name)` | alles zu einer Info als Text (Besitzer, Quest, Bedingung, alle Zeilen) |
| `bCString DescribeCommands()` | Befehlsvokabular aller Infos |
| `bool InfoExists(name)` | gibt es die Info? |
| `bool CreateInfo(name, owner)` | Legt eine Info an (Typ `INFO`, Bedingung `GENERAL`); siehe unten |
| `bool SetInfoQuest(name, quest)`, `SetInfoParent(name, parent)`, `SetInfoType(name, typ)`, `SetInfoCondition(name, art)` | Eigenschaften setzen |
| `bool AddInfoLine(info, befehl, entity1, entity2, id1, id2, text)` | Skriptzeile anhängen. Ein nicht leerer `text` wird als eigene Text-ID registriert |
| `bool RebuildInfos()` | Besitzerverzeichnis des Spiels neu aufbauen: **nach dem Anlegen aufrufen**, sonst erscheinen die Optionen nicht |
| `bool RemoveInfo(name)` | Info entfernen |
| `bool SetInfoGiven(name, bool)`, `bool IsInfoGiven(name)` | "Info wurde schon gegeben": blendet die Option aus |

**`CreateInfo`, bitte genau lesen:**
- Gibt es die Info schon, ändert `CreateInfo` nichts und liefert `true`. Ein folgendes `AddInfoLine` hängt die Zeilen dann ein zweites Mal an, und
  der NPC spricht alles doppelt. Prüfe deshalb vorher mit `InfoExists`, vor allem wenn du Infos in `OnInfosLoaded` und später noch einmal anlegst.
- `false` heißt: Das Spiel ist noch nicht bereit, oder es hält den Namen intern noch belegt und hätte die neue Info umbenannt. Dann legt die
  Funktion nichts an und meldet das einmal im Protokoll (`CreateInfo('...'): Name im Info-Manager noch belegt`).
- Nach dem Anlegen `RebuildInfos()` aufrufen, sonst erscheinen die Optionen nicht.

Ereignis: `void OnInfoFinished(const bCString &in info, const bCString &in owner, const bCString &in quest)` kommt, wenn ein Gespräch zu Ende gesprochen ist.
Daran hängst du Folgen, die das Spiel nicht selbst kann (Inventar prüfen, Quest beenden, Belohnungen).

## Beispiel: Quest über ein Gespräch

```angelscript
const bCString NPC = "Jens", QUEST = "MeinMod_Wolfsfell", INFO_JOB = "MeinMod_Job";

bool Anlegen() {
    if (!CreateInfo(INFO_JOB, NPC)) return false;
    SetInfoQuest(INFO_JOB, QUEST);                         // Zeilen kommen ins Questlog
    SetInfoCondition(INFO_JOB, INFO_COND_GENERAL);
    AddInfoLine(INFO_JOB, "Say", "player", "", "", "", "Hast du Arbeit für mich?");
    AddInfoLine(INFO_JOB, "Say", "npc", "", "", "", "Ja, ich brauche ein Wolfsfell.");
    RebuildInfos();
    return true;
}

void OnInfosLoaded() { Anlegen(); }                        // siehe "Speichern und Laden"

void OnInfoFinished(const bCString &in info, const bCString &in owner, const bCString &in quest) {
    if (info == INFO_JOB) RunQuest(QUEST);
}
```

Die vollständige Mod steht in `examples/mods/jens_dialog/main.as`.

## Eigene Texte

Texte in `AddInfoLine` und Quests sind Text-IDs der Stringtabelle des Spiels. Das Modkit fängt die Textsuche ab: ein Text, den du übergibst, wird unter einer eigenen ID
(`G3ASM_INFO_<Info>_<n>`, `G3ASM_QTOPIC_<Quest>`) registriert und überall angezeigt, wo das Spiel diese ID auflöst. Umlaute gehen, wenn die `.as`-Datei als ANSI (Windows-1252) gespeichert ist ([Scripting-Guide](scripting-guide.md#strings-bcstring)).
