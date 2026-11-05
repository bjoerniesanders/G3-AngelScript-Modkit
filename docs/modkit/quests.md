# Quests

Das Questsystem des Spiels steuerst du über `gCQuestManager_PS` und `gCQuest_PS`. Alle Quests des Spiels (571 in der Grundversion) sind ini-ähnliche Dateien
unter `Data/Quests/G3_World_01/<Name>_quest_G3_World_01.quest` (in `Data\Quests.pak`). Eigene Quests legst du zur Laufzeit an.

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
- **Quests merken:** Ob eine per Skript angelegte Quest in Spielständen gespeichert wird, ist nicht geprüft. Sicherer ist, den Zustand aus
  `GetQuestStatus` abzuleiten und die Quest bei Bedarf neu anzulegen (so macht es der Jens-Mod).

## Texte

Die Namen im Questlog sind Text-IDs (`eCLocString`), die das Spiel in seiner Stringtabelle nachschlägt. Eine unbekannte ID liefert einen leeren Text.
Der Patch fängt die Nachschlagefunktionen ab:

- `RegisterText(id, text)` merkt sich einen Text für eine ID. Überall, wo das Spiel diese ID anzeigt, erscheint dein Text.
- `SetQuestTopic(name, text)` und `SetQuestDescription(name, text)` legen dafür die IDs `OG3_QTOPIC_<Quest>` und `OG3_QDESC_<Quest>` an und tragen sie in die Quest ein.

Der Text wird als Windows-1252 gelesen (Umlaute funktionieren). Der Titel erscheint im Questlog (im Spieltest bestätigt).

## Questlog und Dialoge

Das Questlog von Gothic 3 besteht aus **Dialogen** (Infos): Die Einträge entstehen, wenn Gespräche geführt werden, nicht aus einer Liste, die ein Skript füllt.
Eine per Skript angelegte Quest hat deshalb einen Titel, aber keine Logeinträge. Dafür fehlt noch eine Dialog-Schnittstelle (siehe [Grenzen und Stand](limitations.md)).
Bis dahin zeigst du dem Spieler Hinweise mit `PrintScreen`.

## Alle Quests des Spiels

```angelscript
int n = GetQuestCount();                       // z. B. 571
for (int i = 0; i < n; i++) {
    bCString name = GetQuestNameAt(i);
    print(name + ": " + GetQuestStatus(name));
}
```

Bekannte Einschränkung: Die Aufzählung ist noch nicht vollständig geprüft. Im Spieltest meldete die Statusabfrage für die Namen der Liste keine laufenden Quests,
obwohl eine lief; der Mod `quest_demo` (Bild hoch) gibt dafür die Einzelheiten ins Protokoll.
