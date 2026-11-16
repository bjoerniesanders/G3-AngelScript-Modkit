# Quests

Die 571 Quests des Spiels sind Textdateien im Archiv `Data\Quests.pak` (`G3_World_01/<Name>_quest_G3_World_01.quest`). Eigene Quests legt deine Mod
beim Laden an; das Spiel behandelt sie dann wie seine eigenen.

## Status

| Konstante | Wert | Bedeutung |
|---|---|---|
| `QUEST_OPEN` | 0 | angelegt, nicht gestartet |
| `QUEST_RUNNING` | 1 | läuft |
| `QUEST_SUCCESS` | 2 | erfolgreich beendet |
| `QUEST_FAILED` | 3 | gescheitert |
| `QUEST_OBSOLETE` | 4 | hinfällig |
| `QUEST_CANCELLED` | 5 | abgebrochen |
| `QUEST_LOST` | 6 | verloren |
| `QUEST_WON` | 7 | gewonnen |

`GetQuestStatus(name)` liefert `-1`, wenn die Quest nicht existiert.

## Ablauf einer eigenen Quest

```angelscript
const bCString QUEST = "MeinMod_Wolfsfell";

void Starten() {
    CreateQuest(QUEST);                                        // Name frei wählbar, eindeutig
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

- `CreateQuest` ist harmlos, wenn es die Quest schon gibt (liefert `true`).
- `SucceedQuest` auf eine schon erfolgreiche Quest und `FailQuest` auf eine beendete Quest werden abgelehnt (`false`).
- **XP:** Die Quest-XP zahlt das Spiel beim Abschluss selbst aus, mit einer Meldung "Quest abgeschlossen". Setze `SetQuestXP` vor `SucceedQuest`.
  Mit 100 gesetzt zahlte das Spiel im Test 500 aus (vermutlich ein Faktor); steht 0 darin, gibt es nichts.
- **Quests im Spielstand:** Eine Quest, die die Mod in `OnQuestsLoaded` anlegt, bekommt nach dem Laden Status und Questlog vom Spiel zurück
  (im Spiel geprüft). Wird sie erst später angelegt (zum Beispiel in `OnFrame`), geht der Stand verloren. Siehe [Speichern und Laden](saving.md).

## Texte

Die Namen im Questlog sind Text-IDs, die das Spiel in seiner Stringtabelle nachschlägt. Eine unbekannte ID liefert einen leeren Text.
Das Modkit fängt die Nachschlagefunktionen ab:

- `RegisterText(id, text)` merkt sich einen Text für eine ID. Überall, wo das Spiel diese ID anzeigt, erscheint dein Text.
- `SetQuestTopic(name, text)` und `SetQuestDescription(name, text)` legen dafür die IDs `G3ASM_QTOPIC_<Quest>` und `G3ASM_QDESC_<Quest>` an und tragen sie in die Quest ein.

Der Titel erscheint im Questlog. Umlaute gehen, wenn die `.as`-Datei als ANSI (Windows-1252) gespeichert ist ([Scripting-Guide](scripting-guide.md#strings-bcstring)).

## Questlog und Dialoge

Das Questlog von Gothic 3 besteht aus **Dialogen** (Infos): Die Einträge entstehen, wenn Gespräche geführt werden, nicht aus einer Liste, die ein Skript füllt.
Eine per Skript angelegte Quest hat deshalb zunächst nur Titel und Beschreibung. Logeinträge entstehen über eigene Dialogzeilen, die mit der Quest
verbunden sind (`SetInfoQuest`, siehe [Dialoge](dialogs.md)); direkt ins Log schreiben geht nicht.

## Alle Quests des Spiels

```angelscript
int n = GetQuestCount();                       // z. B. 571
for (int i = 0; i < n; i++) {
    bCString name = GetQuestNameAt(i);
    print(name + ": " + GetQuestStatus(name));
}
```

Bekannte Einschränkung: Die Aufzählung ist noch nicht vollständig geprüft. Im Spieltest meldete die Statusabfrage für die Namen der Liste keine laufenden Quests,
obwohl eine lief; die Mod `demo_quest` (Bild hoch) gibt dafür die Einzelheiten ins Protokoll.
