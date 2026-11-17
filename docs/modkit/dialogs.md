# Dialoge (Infos)

**Deutsch** | [English](../en/modkit/dialogs.md)

Gespräche in Gothic 3 bestehen aus **Infos**. Eine Info ist eine Option im Gesprächsmenü eines NPCs, zusammen mit den Zeilen, die danach gesprochen
werden. Das Modkit kann Infos lesen und eigene anlegen. Eine Schritt-für-Schritt-Anleitung steht in [Erste Mod](erste-mod.md#5-ein-gespräch-bei-jens).
Im Spiel gibt es 4593 Infos. Jede gehört einem NPC und läuft als kleines Skript aus Befehlszeilen.

## Aufbau einer Info

| Teil | Bedeutung |
|---|---|
| Name | Eindeutige Kennung (`SKALVERAM31`; bei eigenen zum Beispiel `G3ASM_Jens_Job`) |
| Owner | Der NPC, bei dem die Option erscheint (`Jens`) |
| Parent | Eltern-Info. Sie macht aus der Info eine Folgefrage im Gesprächsbaum |
| Quest | Verknüpft die Info mit einer Quest. Ihre Gesprächszeilen kommen automatisch ins Questlog dieser Quest |
| Bedingungsart | Wann die Option erscheint (siehe unten) |
| Typ | `INFO_TYPE_INFO` (3, normale Option), `INFO_TYPE_NEWS` (2), `INFO_TYPE_IMPORTANT` (1), `INFO_TYPE_COMMENT` (5), ... |
| Skriptzeilen | Befehle, die ablaufen, wenn der Spieler die Option wählt |

Bedingungsarten (`INFO_COND_*`):

| Art | Wert | Wann die Option erscheint |
|---|---|---|
| `GENERAL` | 3 | Immer |
| `OPEN` | 5 | Die Quest ist noch nicht gestartet |
| `ACTIVATOR` | 6 | Der NPC bietet die Quest an |
| `RUNNING` | 7 | Solange die Quest läuft |
| `DELIVERY`, `PARTDELIVERY` | 8, 9 | Der Spieler gibt etwas ab |
| `SUCCESS` | 10 | Die Quest war erfolgreich |
| `FAILED` | 12 | Die Quest ist gescheitert |

Außerdem gibt es `CRIME`, `FIGHT`, `HELLO`, `OVERTIME` und `DOCANCEL`.

Die erste Zeile `Say` des Spielers ist der Text im Gesprächsmenü.

## Befehle in Skriptzeilen

`AddInfoLine(info, befehl, entity1, entity2, id1, id2, text)` hängt eine Zeile an. Häufige Befehle im Spiel (Anzahl der Vorkommen):

| Befehl | Parameter | Wirkung |
|---|---|---|
| `Say` (12553) | entity1 = `player` oder `npc`, id1 = Sprechstimmung (z. B. `SD_Thoughtful`), text | Spricht eine Zeile |
| `SaySVM` (467) | entity1, id1 = Standardsatz (`LetMePass`) | Standardsatz des NPCs |
| `Description` (623) | entity1 = `player`, text | Beschriftung einer Antwort (z. B. `ENDE`) |
| `End` (271) | | Beendet das Gespräch |
| `SetGameEvent`, `ClearGameEvent` (1354, 147) | id1 = Ereignisname | Setzt oder löscht einen Schalter im Spielstand |
| `RunQuest` (57), `SucceedQuest` (20), `CloseQuest` (76) | id1 = Questname | Steuert eine Quest |
| `AddQuestLog` (2) | id1 = Quest, id2 = Text-ID | Schreibt einen Eintrag ins Questlog |
| `Give` (584) | entity1 = `npc`, entity2 = `player`, id1 = Item, id2 = Menge | Der NPC gibt etwas |
| `Erase` (4) | entity1 = `player`, id1 = Item, id2 = Menge | Nimmt etwas weg |
| `GiveXP` (346) | id1 = Menge | Erfahrungspunkte |
| `SetTradeEnabled`, `SetTeachEnabled` (139, 143) | entity1 = `npc`, id1 = `true` oder `false` | Schaltet Handel oder Lehrer ein oder aus |
| `SetRoutine` (247) | entity1 = NPC, id1 = Tagesablauf (`Work`, `Free`) | Wechselt den Tagesablauf |
| `Attack` (83) | entity1 = `npc`, entity2 = `player` | Der NPC greift an |
| `InfoWait` (51) | id1 = Millisekunden | Pause |
| `ShowGameMessage` (2) | id2 = Art, text | Meldung auf dem Bildschirm |

`DescribeCommands()` listet alle 60 Befehle des Spiels, jeden mit einem Beispiel.

## Funktionen

| Funktion | Beschreibung |
|---|---|
| `int GetInfoCount()`, `GetInfoName(i)`, `GetInfoOwner(i)` | Listet die Infos auf |
| `bCString DescribeInfo(name)` | Alles zu einer Info als Text: Besitzer, Quest, Bedingung, alle Zeilen |
| `bCString DescribeCommands()` | Das Befehlsvokabular aller Infos |
| `bool InfoExists(name)` | Gibt es die Info? |
| `bool CreateInfo(name, owner)` | Legt eine Info an (Typ `INFO`, Bedingung `GENERAL`). Siehe unten |
| `bool SetInfoQuest(name, quest)`, `SetInfoParent(name, parent)`, `SetInfoType(name, typ)`, `SetInfoCondition(name, art)` | Setzen Eigenschaften |
| `bool AddInfoLine(info, befehl, entity1, entity2, id1, id2, text)` | Hängt eine Skriptzeile an. Ist `text` nicht leer, registriert das Modkit ihn als eigene Text-ID |
| `bool RebuildInfos()` | Baut das Besitzerverzeichnis des Spiels neu auf. **Rufe es nach dem Anlegen auf.** Sonst erscheinen die Optionen nicht |
| `bool RemoveInfo(name)` | Entfernt eine Info |
| `bool SetInfoGiven(name, bool)`, `bool IsInfoGiven(name)` | „Info wurde schon gegeben“. Blendet die Option aus |

**`CreateInfo`, bitte genau lesen:**
- Gibt es die Info schon, ändert `CreateInfo` nichts und liefert `true`. Ein folgendes `AddInfoLine` hängt die Zeilen dann ein zweites Mal an, und
  der NPC spricht alles doppelt. Prüfe deshalb vorher mit `InfoExists`. Das ist besonders wichtig, wenn du Infos in `OnInfosLoaded` und später noch
  einmal anlegst.
- `false` heißt: Das Spiel ist noch nicht bereit, oder es hält den Namen intern noch belegt und hätte die neue Info umbenannt. Dann legt die Funktion
  nichts an und meldet das einmal im Protokoll (`CreateInfo('...'): Name im Info-Manager noch belegt`).
- Rufe nach dem Anlegen `RebuildInfos()` auf. Sonst erscheinen die Optionen nicht.

Ereignis: `void OnInfoFinished(const bCString &in info, const bCString &in owner, const bCString &in quest)` kommt, wenn ein Gespräch zu Ende
gesprochen ist. Daran hängst du Folgen, die das Spiel nicht selbst kann: Inventar prüfen, Quest beenden, Belohnungen.

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

Texte in `AddInfoLine` und in Quests sind Text-IDs der Stringtabelle des Spiels. Das Modkit fängt die Textsuche ab. Einen Text, den du übergibst,
registriert es unter einer eigenen ID (`G3ASM_INFO_<Info>_<n>`, `G3ASM_QTOPIC_<Quest>`). Der Text erscheint dann überall, wo das Spiel diese ID
auflöst. Umlaute gehen, wenn die `.as`-Datei als ANSI (Windows-1252) gespeichert ist ([Scripting-Guide](scripting-guide.md#strings-bcstring)).
