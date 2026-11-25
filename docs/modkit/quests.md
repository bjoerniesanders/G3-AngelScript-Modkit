# Quests

**Deutsch** | [English](../en/modkit/quests.md)

Das Spiel hat 571 Quests. Sie sind Textdateien im Archiv `Data\Quests.pak` (`G3_World_01/<Name>_quest_G3_World_01.quest`). Eigene Quests legt
deine Mod beim Laden an. Das Spiel behandelt sie dann wie seine eigenen.

## Status

| Konstante | Wert | Bedeutung |
|---|---|---|
| `QUEST_OPEN` | 0 | Angelegt, nicht gestartet |
| `QUEST_RUNNING` | 1 | Läuft |
| `QUEST_SUCCESS` | 2 | Erfolgreich beendet |
| `QUEST_FAILED` | 3 | Gescheitert |
| `QUEST_OBSOLETE` | 4 | Hinfällig |
| `QUEST_CANCELLED` | 5 | Abgebrochen |
| `QUEST_LOST` | 6 | Verloren |
| `QUEST_WON` | 7 | Gewonnen |

`GetQuestStatus(name)` liefert `-1`, wenn die Quest nicht existiert.

## Ablauf einer eigenen Quest

```angelscript
const bCString QUEST = "MeinMod_Wolfsfell";

void Starten() {
    CreateQuest(QUEST);                                        // Name frei wählbar, aber eindeutig
    SetQuestTopic(QUEST, "Wolfsfell für Jens");                // Titel im Questlog
    SetQuestDescription(QUEST, "Jens braucht ein Wolfsfell.");
    RunQuest(QUEST);                                           // Status 1
}

void Abschliessen() {
    SetQuestXP(QUEST, 100);                                    // vor dem Abschluss setzen
    SucceedQuest(QUEST);                                       // Status 2
}
```

Hinweise:

- `CreateQuest` schadet nicht, wenn es die Quest schon gibt. Es liefert dann `true`.
- `SucceedQuest` auf eine schon erfolgreiche Quest und `FailQuest` auf eine beendete Quest lehnt das Modkit ab. Beide liefern dann `false`.
- **XP:** Die Quest-XP zahlt das Spiel beim Abschluss selbst aus, mit der Meldung „Quest abgeschlossen“. Setze `SetQuestXP` vor `SucceedQuest`.
  Im Test zahlte das Spiel bei 100 genau 500 XP aus. Das ist vermutlich ein Faktor. Bei 0 gibt es nichts.
- **Quests im Spielstand:** Legt die Mod eine Quest in `OnQuestsLoaded` an, bekommt sie nach dem Laden Status und Questlog vom Spiel zurück. Das habe
  ich im Spiel geprüft. Legt die Mod die Quest erst später an (zum Beispiel in `OnFrame`), geht der Stand verloren. Siehe
  [Speichern und Laden](saving.md).

## Texte

Die Namen im Questlog sind Text-IDs. Das Spiel schlägt den Text zu jeder ID in seiner Stringtabelle nach. Eine unbekannte ID liefert einen leeren
Text. Das Modkit fängt die Nachschlagefunktionen ab:

- `RegisterText(id, text)` merkt sich einen Text für eine ID. Überall, wo das Spiel diese ID anzeigt, erscheint dein Text.
- `SetQuestTopic(name, text)` und `SetQuestDescription(name, text)` legen dafür die IDs `G3ASM_QTOPIC_<Quest>` und `G3ASM_QDESC_<Quest>` an. Danach
  tragen sie die IDs in die Quest ein.

Der Titel erscheint im Questlog. Umlaute gehen, wenn die `.as`-Datei als ANSI (Windows-1252) gespeichert ist
([Scripting-Guide](scripting-guide.md#strings-bcstring)).

## Questlog und Dialoge

Im Questlog von Gothic 3 stehen unter dem Titel die **Gesprächszeilen**, die der Spieler zu dieser Quest gehört hat: jede Zeile mit dem Namen des
Sprechers davor. Eine per Skript angelegte Quest zeigt im Log deshalb zunächst nur ihren Titel. Einträge entstehen auf zwei Wegen:

- über eigene Dialogzeilen, die du mit der Quest verbindest (`SetInfoQuest`, siehe [Dialoge](dialogs.md)),
- ab v0.47.1 ohne Gespräch mit `AddQuestLogEntry` (siehe unten).

**Achtung:** Die Beschreibung aus `SetQuestDescription` zeigt das Questlog nicht an, auch nicht, wenn sie sich später ändert. Für Fortschritt im Log
nimm `AddQuestLogEntry` oder Dialogzeilen.

### Einträge ohne Gespräch (ab v0.47.1)

`AddQuestLogEntry(quest, heading, text)` hängt einen Eintrag an das Log der Quest an. `heading` steht davor wie der Name eines Sprechers. Ist
`heading` leer, steht nichts davor.

```angelscript
float timer = 0;
void OnFrame(float dt) {
    timer += dt;
    if (timer < 1.0f) return;
    timer = 0;
    // erlegt, gefunden, gekauft oder geschenkt: entscheidend ist, dass der Held ein Fell hat
    if (GetQuestStatus(QUEST) != QUEST_RUNNING || IsGameEventSet("MeinMod_FellLog") || GetHero().GetItemCount("It_Wolf_Fur") < 1) return;
    if (AddQuestLogEntry(QUEST, "", "Ich habe ein Wolfsfell. Jetzt muss ich es nur noch zu Jens bringen.") > 0)
        SetGameEvent("MeinMod_FellLog");   // nur einmal
}
```

Ergebnis im Questlog: unter „Wolfsfell für Jens“ steht die neue Zeile. Rückgabe ist die Zahl der Einträge der Quest, `-1` bei einem Fehler (Quest
unbekannt, ohne Spiel).

- Das Spiel speichert die Einträge mit der Quest. Sie kommen nach Speichern, Laden und einem Neustart des Spiels zurück. Die Texte stehen im Eintrag
  selbst, die Mod muss nichts erneut anlegen.
- Damit die Quest nach dem Laden noch da ist, muss die Mod sie in `OnQuestsLoaded` anlegen (siehe [Speichern und Laden](saving.md)). Eine Quest, die
  nur zur Laufzeit entsteht, fehlt nach dem Laden mitsamt ihren Einträgen.
- Jeder Aufruf hängt einen neuen Eintrag an. Merke dir mit einem Schalter im Spielstand (`SetGameEvent`), welche Einträge schon geschrieben sind.
- Ein Eintrag bleibt so, wie er geschrieben wurde, auch wenn die Mod später einen anderen Text verwendet.
- Umlaute gehen, wenn die `.as`-Datei als ANSI (Windows-1252) gespeichert ist.
- Beispiel: `jens_dialog` (der Held hat ein Wolfsfell).

## Alle Quests des Spiels

```angelscript
int n = GetQuestCount();                       // z. B. 571
for (int i = 0; i < n; i++) {
    bCString name = GetQuestNameAt(i);
    print(name + ": " + GetQuestStatus(name));
}
```

Bekannte Einschränkung: Diese Liste habe ich nicht vollständig geprüft. Im Spieltest meldete die Statusabfrage für die Namen der Liste keine laufenden
Quests, obwohl eine Quest lief. Die Mod `demo_quest` (Bild hoch) schreibt die Einzelheiten ins Protokoll.
