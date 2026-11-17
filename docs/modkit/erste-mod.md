# Erste Mod Schritt für Schritt

**Deutsch** | [English](../en/modkit/erste-mod.md)

In dieser Anleitung baust du eine kleine, vollständige Mod. Jens in Kap Dun bekommt eine neue Gesprächsoption und gibt dir einen Auftrag: ein
Wolfsfell bringen. Die Quest erscheint im Questlog. Bei der Abgabe gibt es eine Belohnung. Jedes Kapitel baut auf dem vorigen auf. Am Ende steht die
ganze Mod in einem Stück.

## 1. Bevor du anfängst (bitte lesen)

1. Baue das Modkit ein (siehe [Mod-Ordner und Installation](mod-package.md#installation)). Das Spiel braucht den Community Patch 1.75 und den
   Parallel-Universe-Patch.
2. **Arbeite auf einer Kopie des Spielordners.** Arbeite nicht in deiner Hauptinstallation.
3. **Sichere deine Spielstände** (`Dokumente\gothic3`). Alle Kopien des Spiels benutzen denselben Ordner. Speichere Tests nur in neue, freie Plätze.
4. Halte das Protokoll offen: `<Spielordner>\g3asm.log`. Dort stehen deine Ausgaben und jeder Fehler mit Zeile und Spalte. Ein Editor, der
   Änderungen von selbst nachlädt, ist hier praktisch.
5. Hast du die Beispiel-Mods `jens_quest` und `jens_dialog` zugeschaltet, schalte sie ab. Sie arbeiten ebenfalls mit Jens.
6. **Speichere deine `.as`-Dateien als ANSI (Windows-1252)**, wenn du Umlaute in Texten benutzt. Das Spiel liest Texte in dieser Kodierung. Aus
   einer UTF-8-Datei erscheinen „ü“ und „ß“ als Zeichensalat. Wer sich nicht darum kümmern will, schreibt „ue“ und „ss“.

## 2. Die Mod anlegen

1. Lege den Ordner `<Spielordner>\g3asm\mods\wolfsfell\` an. Der Ordnername ist der Name deiner Mod.
2. Lege darin die Datei `main.as` an:

   ```angelscript
   void main() {
       print("Wolfsfell-Mod geladen");
   }
   ```

3. Starte das Spiel.

**Ergebnis:** Im Protokoll steht `[wolfsfell] Wolfsfell-Mod geladen`.

`main()` läuft einmal beim Start. Ändere den Text und speichere die Datei, während das Spiel läuft. Nach etwa 1 Sekunde steht die neue Zeile im
Protokoll. Das ist der **Hot-Reload**. Ab jetzt kannst du jede Änderung so ausprobieren. Du musst das Spiel nicht neu starten.

**Achtung:** Hat dein Skript einen Fehler, lädt die Mod nicht. Im Protokoll steht dann `Skript enthaelt Fehler`. Die Zeilen davor nennen Zeile,
Spalte und Grund.

## 3. Auf das Spiel reagieren

Meist tut eine Mod nichts von selbst. Sie reagiert auf **Ereignisse**. Ereignisse sind Funktionen mit festen Namen. Das Modkit ruft sie auf, wenn im
Spiel etwas passiert. Ergänze:

```angelscript
void OnNPCKilled(Entity opfer) {
    print("getötet: " + opfer.GetName());
}
```

**Ergebnis:** Erschlägst du einen Gegner, steht sein Name im Protokoll, zum Beispiel `[wolfsfell] getötet: Wolf`.

So findest du heraus, wie Dinge im Spiel heißen. Die Namen brauchst du gleich. Alle Ereignisse stehen unter [Ereignisse](events.md).

**Achtung:** Der Name und die Parameter müssen genau stimmen. `void OnNPCKilled(Entity victim)` funktioniert. `void OnNpcKilled(Entity victim)` ruft
das Modkit nie auf, und du bekommst keine Fehlermeldung.

## 4. Eine Quest anlegen

Das Spiel baut seine Quests bei jedem Spielstart und jedem Laden neu auf. Deine Quest muss dazugehören, damit ihr Stand im Spielstand landet. Deshalb
legst du sie genau in diesem Moment an, im Ereignis `OnQuestsLoaded`.

1. Schreibe ganz oben in die Datei die Namen, die die Mod benutzt:

   ```angelscript
   const bCString NPC   = "Jens";
   const bCString QUEST = "Wolfsfell_Quest";
   const bCString FELL  = "It_Wolf_Fur";
   ```

2. Ergänze:

   ```angelscript
   void OnQuestsLoaded() {
       if (GetQuestCount() < 0) return;               // Hauptmenü: noch keine Quests
       CreateQuest(QUEST);
       SetQuestTopic(QUEST, "Ein Fell fuer Jens");     // Titel im Questlog
       SetQuestDescription(QUEST, "Jens braucht ein Wolfsfell.");
       SetQuestXP(QUEST, 100);                         // Erfahrung bei Erfolg
   }
   ```

3. Speichere und lade einen Spielstand.

**Ergebnis:** Die Quest gibt es jetzt, aber sie ist noch nicht gestartet. Im Questlog erscheint sie erst, wenn sie läuft (Kapitel 5).

**Achtung:** Lege Quests nie in `OnFrame` oder in `main` an. Dann gehören sie nicht zum Spielstand, und nach dem Laden ist der Fortschritt weg. Den
Hintergrund beschreibt [Speichern und Laden](saving.md).

## 5. Ein Gespräch bei Jens

Gespräche heißen in Gothic 3 **Infos**. Eine Info gehört einem NPC und erscheint als Option im Gesprächsmenü. Sie besteht aus Zeilen, die nacheinander
gesprochen werden. Wie Quests baut das Spiel sie beim Laden neu auf. Deshalb legst du deine Infos in `OnInfosLoaded` an.

1. Ergänze bei den Namen oben:

   ```angelscript
   const bCString INFO_AUFTRAG = "Wolfsfell_Auftrag";
   ```

2. Ergänze:

   ```angelscript
   void OnInfosLoaded() {
       if (InfoExists(INFO_AUFTRAG)) return;
       CreateInfo(INFO_AUFTRAG, NPC);
       SetInfoQuest(INFO_AUFTRAG, QUEST);                // Zeilen kommen ins Questlog dieser Quest
       AddInfoLine(INFO_AUFTRAG, "Say", "player", "", "", "", "Hast du Arbeit fuer mich?");
       AddInfoLine(INFO_AUFTRAG, "Say", "npc", "", "", "", "Bring mir ein Wolfsfell, dann bezahle ich dich.");
       RebuildInfos();
   }

   void OnInfoFinished(const bCString &in info, const bCString &in owner, const bCString &in quest) {
       if (info == INFO_AUFTRAG) RunQuest(QUEST);
   }
   ```

3. Lade einen Spielstand und sprich mit Jens.

**Ergebnis:** Jens hat die Option „Hast du Arbeit für mich?“. Nach dem Gespräch meldet das Spiel „Quest angenommen“. Die Quest steht mit beiden
Zeilen im Questlog.

- Die erste Zeile des Spielers (`"player"`) ist der Text der Option im Menü.
- `OnInfoFinished` kommt, wenn ein Gespräch zu Ende gesprochen ist. Hier hängst du alles an, was danach passieren soll.
- Hat der Spieler die Option einmal gewählt, blendet das Spiel sie aus.

**Achtung:** Die Prüfung mit `InfoExists` am Anfang ist wichtig. Ohne sie hängt jedes erneute Anlegen dieselben Zeilen noch einmal an, und Jens
spricht alles doppelt. Vergiss auch `RebuildInfos()` nicht. Ohne diesen Aufruf erscheint die Option nicht.

## 6. Die Abgabe

Eine zweite Info erscheint nur, solange die Quest läuft. Hat der Spieler das Fell dabei, gibt er es ab und bekommt die Belohnung.

1. Ergänze bei den Namen oben:

   ```angelscript
   const bCString INFO_ABGABE = "Wolfsfell_Abgabe";
   ```

2. Ergänze in `OnInfosLoaded` vor `RebuildInfos();`:

   ```angelscript
       CreateInfo(INFO_ABGABE, NPC);
       SetInfoQuest(INFO_ABGABE, QUEST);
       SetInfoCondition(INFO_ABGABE, INFO_COND_RUNNING);   // nur solange die Quest läuft
       AddInfoLine(INFO_ABGABE, "Say", "player", "", "", "", "Ich habe das Wolfsfell.");
       AddInfoLine(INFO_ABGABE, "Say", "npc", "", "", "", "Gute Arbeit. Hier, nimm diese Pfeile.");
   ```

3. Ergänze in `OnInfoFinished`:

   ```angelscript
       if (info == INFO_ABGABE) {
           Entity held = GetHero();
           if (held.RemoveItems(FELL, 1) < 1) {
               PrintScreen("Du hast kein Wolfsfell dabei.");
               return;
           }
           SucceedQuest(QUEST);          // das Spiel zahlt die Quest-XP aus
           held.AddItems("Arrow", 15);   // eigene Belohnung
       }
   ```

**Ergebnis:** Mit einem Wolfsfell im Inventar wählst du „Ich habe das Wolfsfell.“. Das Fell verschwindet, die Quest ist erfolgreich, und du bekommst
Erfahrung und 15 Pfeile.

**Achtung:** Ob der Held das Fell hat, prüft hier das Skript, nicht das Spiel. Bedingungen wie „hat ein Item“ lassen sich für eigene Infos nicht
setzen. Die Option erscheint deshalb auch ohne Fell. Willst du sie dann ausblenden, nimm [`Override_InfoAvailable`](hooks.md).

## 7. Wenn Jens stirbt

```angelscript
void OnNPCKilled(Entity opfer) {
    if (opfer.GetName() == NPC && GetQuestStatus(QUEST) == QUEST_RUNNING)
        FailQuest(QUEST);
}
```

Diese Funktion ersetzt die aus Kapitel 3. Jede Ereignisfunktion darf es nur einmal geben.

**Ergebnis:** Stirbt Jens, während die Quest läuft, scheitert sie.

## 8. Speichern und Laden ausprobieren

1. Nimm den Auftrag an, speichere in einen freien Platz und beende das Spiel.
2. Starte das Spiel neu und lade den Spielstand.

**Ergebnis:** Die Quest läuft weiter. Das Questlog ist vollständig. Jens bietet nur noch die Abgabe an. Das Spiel stellt das selbst wieder her, weil
Quest und Infos in den Lade-Ereignissen entstehen.

Was das Spiel nicht selbst speichert, zum Beispiel eine eigene Zahl in deinem Skript, merkst du dir mit Schaltern im Spielstand (`SetGameEvent`).
Siehe [Speichern und Laden](saving.md#eigene-schalter-im-spielstand).

## 9. Version und Weitergeben

1. Lege neben `main.as` eine `mod.ini` an:

   ```ini
   [mod]
   version=1.0
   api=46
   title=Ein Fell fuer Jens
   ```

   Mit `api=46` lädt ein zu altes Modkit die Mod nicht und sagt das im Protokoll. Es bricht nicht mit Übersetzungsfehlern ab.

2. Baue das Paket für Spieler (im Ordner des Modkits):

   ```
   bin\g3asm\g3asm_packmod.exe release <Spielordner>\g3asm\mods\wolfsfell <Zielordner>
   ```

**Ergebnis:** Im Zielordner liegt der Ordner `wolfsfell` im Aufbau des Union Starters. Spieler legen ihn nach `<Spielordner>\MDS\Mods\` und
installieren ihn im Starter. Das Modkit brauchen sie nicht. Einzelheiten stehen unter [Mods weitergeben](mod-package.md#mods-weitergeben).

## Die ganze Mod

```angelscript
const bCString NPC          = "Jens";
const bCString QUEST        = "Wolfsfell_Quest";
const bCString FELL         = "It_Wolf_Fur";
const bCString INFO_AUFTRAG = "Wolfsfell_Auftrag";
const bCString INFO_ABGABE  = "Wolfsfell_Abgabe";

void main() {
    print("Wolfsfell-Mod geladen");
}

void OnQuestsLoaded() {
    if (GetQuestCount() < 0) return;               // Hauptmenü: noch keine Quests
    CreateQuest(QUEST);
    SetQuestTopic(QUEST, "Ein Fell fuer Jens");
    SetQuestDescription(QUEST, "Jens braucht ein Wolfsfell.");
    SetQuestXP(QUEST, 100);
}

void OnInfosLoaded() {
    if (InfoExists(INFO_AUFTRAG)) return;
    CreateInfo(INFO_AUFTRAG, NPC);
    SetInfoQuest(INFO_AUFTRAG, QUEST);
    AddInfoLine(INFO_AUFTRAG, "Say", "player", "", "", "", "Hast du Arbeit fuer mich?");
    AddInfoLine(INFO_AUFTRAG, "Say", "npc", "", "", "", "Bring mir ein Wolfsfell, dann bezahle ich dich.");

    CreateInfo(INFO_ABGABE, NPC);
    SetInfoQuest(INFO_ABGABE, QUEST);
    SetInfoCondition(INFO_ABGABE, INFO_COND_RUNNING);
    AddInfoLine(INFO_ABGABE, "Say", "player", "", "", "", "Ich habe das Wolfsfell.");
    AddInfoLine(INFO_ABGABE, "Say", "npc", "", "", "", "Gute Arbeit. Hier, nimm diese Pfeile.");
    RebuildInfos();
}

void OnInfoFinished(const bCString &in info, const bCString &in owner, const bCString &in quest) {
    if (info == INFO_AUFTRAG) RunQuest(QUEST);
    if (info == INFO_ABGABE) {
        Entity held = GetHero();
        if (held.RemoveItems(FELL, 1) < 1) {
            PrintScreen("Du hast kein Wolfsfell dabei.");
            return;
        }
        SucceedQuest(QUEST);
        held.AddItems("Arrow", 15);
    }
}

void OnNPCKilled(Entity opfer) {
    if (opfer.GetName() == NPC && GetQuestStatus(QUEST) == QUEST_RUNNING)
        FailQuest(QUEST);
}
```

## Wie es weitergeht

- [Beispiele](examples.md): fertige Mods zum Lernen. Darunter ist `jens_dialog`, die ausführliche Fassung dieser Mod (mit Quest aus einem Mod-Archiv
  und einem Rückfall für alte Spielstände).
- [Hooks](hooks.md): Spielfunktionen ändern, etwa Schaden oder ob eine Gesprächsoption erscheint.
- [Scripting-Guide](scripting-guide.md): Eigenheiten der Sprache und Fehlersuche.
- [API-Referenz](api-reference.md): alle Funktionen zum Nachschlagen.
