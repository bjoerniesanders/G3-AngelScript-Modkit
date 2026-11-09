# Speichern und Laden

Quests, Dialoge und Texte, die ein Mod zur Laufzeit anlegt, kennt das Spiel nicht von sich aus. Beim Laden eines Spielstands baut es Quests und Infos neu aus seinen
Datendateien auf und liest danach die gespeicherten Laufzeitdaten dazu ein (Status, Questlog, "Info gegeben"). Eigene Objekte müssen deshalb **während des Aufbaus**
angelegt werden, bevor das Spiel die Laufzeitdaten einliest. Dann stellt das Spiel Status und Questlog selbst wieder her.

## Lade-Ereignisse

| Funktion | Wann |
|---|---|
| `void OnQuestsLoaded()` | direkt nach dem Neuaufbau der Quests (`LoadQuestsFromInis`), beim Spielstart und bei jedem Laden |
| `void OnInfosLoaded()` | direkt nach dem Neuaufbau der Dialoge (`ReadCompiledInfoManager`) |

Während dieser Ereignisse arbeiten die Quest- und Dialogfunktionen auf dem Manager, der gerade aufgebaut wird (das Spiel kennt ihn zu dem Zeitpunkt noch nicht als
"aktuellen" Manager). Lege deine Quest in `OnQuestsLoaded` und deine Infos in `OnInfosLoaded` an:

```angelscript
void OnQuestsLoaded() {
    CreateQuest(QUEST);
    SetQuestTopic(QUEST, "Wolfsfell für Jens");
    SetQuestDescription(QUEST, "Jens braucht ein Wolfsfell.");
    SetQuestXP(QUEST, 100);
}
void OnInfosLoaded() { ErstelleInfos(); }
```

Die Reihenfolge beim Laden (im Protokoll sichtbar): `LoadQuestsFromInis` → `OnQuestsLoaded` → Quest-Laufzeitdaten werden gelesen → Info-Manager `ReadSaveGame`.

## Speicher-Ereignisse

| Funktion | Wann |
|---|---|
| `void OnBeforeSave(const bCString &in name)` (ab Version 31) | direkt vor dem Schreiben eines Spielstands |
| `void OnAfterSave(const bCString &in name, int ok)` (ab Version 31) | direkt danach; `ok` 1 = Erfolg, 0 = Fehlschlag |

Beide laufen genau einmal je Speichern (gemessen: Menü und Schnellspeichern, `gCWorld::SaveGameWorld`, derselbe Thread wie beim Laden). Sie dienen dazu, **dauerhafte
Zustände aus dem Spielstand herauszuhalten**: Was ein Mod dem Helden dauerhaft hinzufügt (zum Beispiel ein eigenes Item), steht danach im Spielstand und fehlt, wenn der
Mod nicht läuft oder sich ein anderer Mod daran stört. Der Mod nimmt es in `OnBeforeSave` heraus und legt es in `OnAfterSave` wieder an.

```angelscript
void OnBeforeSave(const bCString &in name) { /* Talent deaktivieren, Stapel loeschen */ }
void OnAfterSave(const bCString &in name, int ok) { /* Stapel wieder anlegen, Talent neu aktivieren */ }
```

Ein Beispiel ist `examples/mods/perk_lifesteal`. Entity-Handles nicht über das Speichern hinweg aufbewahren. Während der Ereignisse nichts Langes tun: das Spiel wartet.
## Eigene Schalter im Spielstand

`SetGameEvent(name)`, `ClearGameEvent(name)`, `IsGameEventSet(name)` speichern kleine Ja/Nein-Werte im Gedächtnis des Helden. Das Spiel schreibt sie in jeden Spielstand.
Sie überstehen Speichern und Laden und gelten für den jeweiligen Spielstand, auch wenn der Mod nicht lief.

Benutze sie für alles, was der Mod nach dem Laden wissen muss und das das Spiel nicht selbst speichert (zum Beispiel "Quest gescheitert"). Wähle Namen mit Präfix
(`MeinMod_...`), damit sie nicht mit Spielereignissen kollidieren.

## Notfall-Wiederherstellung

Falls das Spiel Quest oder Dialoge nicht zurückbringt (zum Beispiel wenn der Mod erst nach einem Laden dazukam), prüft ein Mod am besten einmal pro Sekunde in
`OnFrame`, ob `QuestExists` und `InfoExists` stimmen, legt sie sonst neu an und stellt den Stand aus den Schaltern her. Dafür gilt:

- Den Status setzt du mit `SetQuestStatus(quest, QUEST_RUNNING)`, **nicht** mit `RunQuest`: Letzteres zeigt die Meldung "Quest angenommen" erneut.
- Für abgeschlossene Quests setzt du Belohnung und XP vorher auf 0, damit das Setzen des Status nichts doppelt auszahlt.
- Das Questlog lässt sich in diesem Notfall nicht wiederherstellen, es ist Teil der Laufzeitdaten der Quest.

Das Beispiel `examples/mods/jens_dialog/main.as` macht beides: Lade-Ereignisse im Normalfall, Schalter als Rückfall.
