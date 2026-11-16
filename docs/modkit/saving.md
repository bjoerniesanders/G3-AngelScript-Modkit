# Speichern und Laden

Quests, Dialoge und Texte, die eine Mod anlegt, kennt das Spiel nicht von sich aus. Damit sie Speichern und Laden überstehen, musst du wissen, wie das
Spiel lädt:

1. Es baut alle Quests aus seinen Dateien neu auf.
2. Es baut alle Dialoge (Infos) neu auf.
3. Erst danach liest es aus dem Spielstand, was sich geändert hat: Status der Quests, Questlog, welche Gesprächsoptionen schon gewählt wurden.

Legst du deine Quests und Infos **nach Schritt 1 und 2, aber vor Schritt 3** an, gehören sie dazu, und das Spiel stellt ihren Stand selbst wieder her.
Genau dafür gibt es zwei Ereignisse.

## Lade-Ereignisse

| Funktion | Wann |
|---|---|
| `void OnQuestsLoaded()` | Nach Schritt 1, beim Spielstart und bei jedem Laden: hier eigene Quests anlegen |
| `void OnInfosLoaded()` | Nach Schritt 2: hier eigene Infos anlegen |

```angelscript
const bCString QUEST = "MeineMod_Wolfsfell";

void OnQuestsLoaded() {
    if (GetQuestCount() < 0) return;               // Hauptmenü: noch keine Quests
    CreateQuest(QUEST);
    SetQuestTopic(QUEST, "Wolfsfell fuer Jens");
    SetQuestDescription(QUEST, "Jens braucht ein Wolfsfell.");
    SetQuestXP(QUEST, 100);
}

void OnInfosLoaded() {
    // eigene Infos anlegen, siehe Dialoge
}
```

**Ergebnis:** Nach dem Laden hat die Quest wieder ihren Status, das Questlog ist vollständig, und gewählte Gesprächsoptionen bleiben ausgeblendet.

**Achtung:**
- In `main()` oder `OnFrame` angelegte Quests und Infos verlieren beim Laden ihren Stand.
- Im Hauptmenü läuft `OnQuestsLoaded` schon einmal, ohne Quests (`GetQuestCount()` liefert -1). Lege dann nichts an; der richtige Aufbau folgt beim
  Laden eines Spielstands.

Eine vollständige Mod nach diesem Muster steht in [Erste Mod Schritt für Schritt](erste-mod.md).

## Speicher-Ereignisse

| Funktion | Wann |
|---|---|
| `void OnBeforeSave(const bCString &in name)` (ab Version 31) | Direkt bevor ein Spielstand geschrieben wird |
| `void OnAfterSave(const bCString &in name, int ok)` (ab Version 31) | Direkt danach; `ok` 1 = gelungen, 0 = fehlgeschlagen |

Beide laufen genau einmal je Speichern, auch beim Schnellspeichern.

Wozu: Was eine Mod dauerhaft ins Spiel bringt, zum Beispiel ein Item aus einer geklonten Vorlage oder ein eigenes Talent, steht danach im Spielstand.
Lädt jemand diesen Spielstand ohne die Mod, zeigt der Eintrag ins Leere, und das Spiel kann abstürzen. Die Mod nimmt solche Dinge deshalb vor dem
Speichern heraus und legt sie danach wieder an:

```angelscript
void OnBeforeSave(const bCString &in name) { /* Talent ausschalten, Stapel entfernen */ }
void OnAfterSave(const bCString &in name, int ok) { /* Stapel wieder anlegen, Talent einschalten */ }
```

**Ergebnis:** Der Spielstand enthält nichts von der Mod und lädt auch ohne sie. Vollständiges Beispiel: `examples/mods/perk_lifesteal`.

**Achtung:**
- Nach dem **Laden** fehlt das Entfernte ebenfalls; lege es auch bei jedem Laden wieder an (z. B. in `OnWorldActivate`).
- Während der beiden Ereignisse wartet das Spiel. Nichts Langes tun.
- `Entity`-Griffe nicht über das Speichern hinweg aufbewahren, sondern neu holen (`GetHero()`).

## Eigene Schalter im Spielstand

`SetGameEvent(name)`, `ClearGameEvent(name)` und `IsGameEventSet(name)` speichern Ja/Nein-Werte im Gedächtnis des Helden. Das Spiel schreibt sie in jeden
Spielstand; sie gelten für den jeweiligen Spielstand, auch wenn die Mod zwischendurch abgeschaltet war.

Benutze sie für alles, was die Mod nach dem Laden wissen muss und das das Spiel nicht selbst speichert, etwa „Belohnung schon bekommen“. Gib den Namen
ein Präfix (`MeineMod_...`), damit sie nicht mit Schaltern des Spiels zusammenstoßen.

## Notfall-Wiederherstellung

Kam die Mod erst nach einem Spielstand dazu, kennt dieser Spielstand ihre Quest und Infos nicht. Dann hilft folgender Rückfall:

1. Den Fortschritt zusätzlich in Schaltern merken (`SetGameEvent("MeineMod_Begonnen")` usw.).
2. In `OnFrame` etwa einmal je Sekunde prüfen, ob `QuestExists` und `InfoExists` stimmen; fehlt etwas, neu anlegen.
3. Den Stand aus den Schaltern herstellen.

**Achtung:**
- Den Status mit `SetQuestStatus(quest, QUEST_RUNNING)` setzen, **nicht** mit `RunQuest`; sonst meldet das Spiel noch einmal „Quest angenommen“.
- Bei abgeschlossenen Quests vorher XP und Belohnung auf 0 setzen, sonst zahlt das Spiel sie beim Setzen des Status noch einmal aus.
- Das Questlog lässt sich so nicht wiederherstellen.

`examples/mods/jens_dialog/main.as` macht beides: Lade-Ereignisse im Normalfall, Schalter als Rückfall.
