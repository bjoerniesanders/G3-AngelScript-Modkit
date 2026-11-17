# Speichern und Laden

**Deutsch** | [English](../en/modkit/saving.md)

Quests, Dialoge und Texte, die eine Mod anlegt, kennt das Spiel nicht. Damit sie Speichern und Laden überstehen, musst du wissen, wie das Spiel lädt:

1. Das Spiel baut alle Quests aus seinen Dateien neu auf.
2. Das Spiel baut alle Dialoge (Infos) neu auf.
3. Danach liest das Spiel die Änderungen aus dem Spielstand: Status der Quests, Questlog und die Gesprächsoptionen, die der Spieler schon gewählt
   hat.

Legst du deine Quests und Infos **nach Schritt 1 und 2, aber vor Schritt 3** an, gehören sie zum Spiel. Das Spiel stellt ihren Stand dann selbst
wieder her. Genau dafür gibt es zwei Ereignisse.

## Lade-Ereignisse

| Funktion | Wann |
|---|---|
| `void OnQuestsLoaded()` | Nach Schritt 1, beim Spielstart und bei jedem Laden. Lege hier eigene Quests an |
| `void OnInfosLoaded()` | Nach Schritt 2. Lege hier eigene Infos an |

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

**Ergebnis:** Nach dem Laden hat die Quest wieder ihren Status. Das Questlog ist vollständig. Gewählte Gesprächsoptionen bleiben ausgeblendet.

**Achtung:**
- Quests und Infos, die du in `main()` oder in `OnFrame` anlegst, verlieren beim Laden ihren Stand.
- Im Hauptmenü läuft `OnQuestsLoaded` einmal ohne Quests (`GetQuestCount()` liefert -1). Lege dann nichts an. Der richtige Aufbau folgt, wenn der
  Spieler einen Spielstand lädt.

Eine vollständige Mod nach diesem Muster steht in [Erste Mod Schritt für Schritt](erste-mod.md).

## Speicher-Ereignisse

| Funktion | Wann |
|---|---|
| `void OnBeforeSave(const bCString &in name)` (ab Version 31) | Direkt bevor das Spiel einen Spielstand schreibt |
| `void OnAfterSave(const bCString &in name, int ok)` (ab Version 31) | Direkt danach. `ok` 1 = gelungen, 0 = fehlgeschlagen |

Beide laufen genau einmal je Speichern. Das gilt auch beim Schnellspeichern.

Wozu: Der Spielstand enthält die dauerhaften Änderungen einer Mod, zum Beispiel ein Item aus einer geklonten Vorlage oder ein eigenes Talent. Lädt
jemand diesen Spielstand ohne die Mod, zeigt der Eintrag ins Leere, und das Spiel kann abstürzen. Die Mod nimmt solche Dinge deshalb vor dem
Speichern heraus und legt sie danach wieder an:

```angelscript
void OnBeforeSave(const bCString &in name) { /* Talent ausschalten, Stapel entfernen */ }
void OnAfterSave(const bCString &in name, int ok) { /* Stapel wieder anlegen, Talent einschalten */ }
```

**Ergebnis:** Der Spielstand enthält nichts von der Mod. Er lädt auch ohne sie. Vollständiges Beispiel: `examples/mods/perk_lifesteal`.

**Achtung:**
- Nach dem **Laden** fehlt das Entfernte ebenfalls. Lege es auch bei jedem Laden wieder an (zum Beispiel in `OnWorldActivate`).
- Während der beiden Ereignisse wartet das Spiel. Tu darin nichts Langes.
- Bewahre `Entity`-Griffe nicht über das Speichern hinweg auf. Hol sie neu (`GetHero()`).

## Eigene Schalter im Spielstand

`SetGameEvent(name)`, `ClearGameEvent(name)` und `IsGameEventSet(name)` speichern Ja/Nein-Werte im Gedächtnis des Helden. Das Spiel schreibt sie in
jeden Spielstand. Sie gelten für diesen Spielstand, auch wenn die Mod zwischendurch abgeschaltet war.

Benutze sie für alles, was die Mod nach dem Laden wissen muss und was das Spiel nicht selbst speichert, zum Beispiel „Belohnung schon bekommen“. Gib
den Namen ein Präfix (`MeineMod_...`), damit sie nicht mit Schaltern des Spiels zusammenstoßen.

## Notfall-Wiederherstellung

Kam die Mod erst nach einem Spielstand dazu, kennt dieser Spielstand ihre Quest und ihre Infos nicht. Dann hilft dieser Rückfall:

1. Merke den Fortschritt zusätzlich in Schaltern (`SetGameEvent("MeineMod_Begonnen")` und so weiter).
2. Prüfe in `OnFrame` etwa einmal je Sekunde `QuestExists` und `InfoExists`. Fehlt etwas, lege es neu an.
3. Stelle den Stand aus den Schaltern her.

**Achtung:**
- Setze den Status mit `SetQuestStatus(quest, QUEST_RUNNING)`, **nicht** mit `RunQuest`. Sonst meldet das Spiel noch einmal „Quest angenommen“.
- Setze bei abgeschlossenen Quests vorher XP und Belohnung auf 0. Sonst zahlt das Spiel sie beim Setzen des Status noch einmal aus.
- Das Questlog lässt sich so nicht wiederherstellen.

`examples/mods/jens_dialog/main.as` macht beides: Lade-Ereignisse im Normalfall, Schalter als Rückfall.
