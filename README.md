# G3 AngelScript Modkit (G3ASM)

**Deutsch** | [English](README.en.md)

Mit G3ASM schreibst du Mods für das **Original-Gothic-3** in AngelScript. Eine schlanke Ersatz-`dbghelp.dll` (der Loader, `src/loader`) lädt
`g3asm.dll`. `g3asm.dll` startet AngelScript 2.38. Sie verbindet die Mod-Skripte mit Spielereignissen, Quests, dem Inventar, Dialogen und
Datei-Patches.

**Bevor du anfängst:** Gothic 3 muss den **Community Patch 1.75** und den **Parallel-Universe-Patch** (PU) haben. `tools/install_loader.ps1`
prüft die beiden Patches. Wenn ein Patch fehlt, bricht das Skript ab. Mit `-Force` baut das Skript G3ASM trotzdem ein, aber diesen Aufbau habe ich
nicht getestet. Das Modkit schreibt das Ergebnis der Prüfung ins Protokoll, in eine Zeile, die mit `Voraussetzungen: ...` beginnt.

Für Mitwirkende: [CONTRIBUTING.md](CONTRIBUTING.md) beschreibt den Aufbau, das Bauen, die Tests, die Continuous Integration (CI), das Release
und die Hausregeln.

## Eine Mod erstellen

1. **G3ASM einbauen.** Beende das Spiel. Gib im entpackten Release-Ordner diesen Befehl ein:

       powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_loader.ps1 -Game "<Spielordner>" -BuildDir bin

2. **Eine Mod anlegen.** Lege für jede Mod einen Ordner an. Lege darin eine Datei `main.as` an. Beispiel: `<Spielordner>\g3asm\mods\hallo\main.as`:

       void main() { print("Hallo aus meiner ersten Mod"); }
       void OnNPCKilled(Entity opfer) { PrintScreen("Getötet: " + opfer.GetName(), 3, 255, 120, 120); }

3. **Das Spiel starten.** Das Modkit schreibt Ausgaben und Fehler in `<Spielordner>\g3asm.log`. Jeder Fehler zeigt die Zeile und die Spalte.
   Wenn du `main.as` im laufenden Spiel speicherst, ist die neue Fassung nach etwa 1 Sekunde aktiv (Hot-Reload).

4. **Die Reihenfolge und die Schalter festlegen** in `g3asm\mods\load_order.txt`:
   - `-name` schaltet eine Mod ab.
   - `+name` schaltet eine Mod zu, die standardmäßig abgeschaltet ist. Alle Beispiel-Mods sind standardmäßig abgeschaltet.

   Die `mod.ini` einer Mod enthält ihre Version, ihre Abhängigkeiten und ihre Einstellungen.

5. **Die Mod weitergeben.** Dieser Befehl baut ein Paket für Spieler:

       bin\g3asm\g3asm_packmod.exe release <Spielordner>\g3asm\mods\meine_mod <Zielordner>

   Das Paket hat den Aufbau des Union Starters und enthält die Laufzeit. Es enthält auch `G3ASM-LICENSE.txt` für die Laufzeit, weil die Laufzeit
   unter der GNU General Public License (GPL) steht. Einzelheiten stehen unter [Mods weitergeben](docs/modkit/mod-package.md#mods-weitergeben).

## Mods nutzen (für Spieler)

Spieler brauchen das Modkit nicht. Jedes Mod-Paket enthält die Laufzeit (`Script_G3ASM_<Version>.dll`). Das Spiel lädt die Laufzeit selbst.

- **Mit dem Union Starter:** Lege den Paketordner nach `<Spielordner>\MDS\Mods\`. Hake die Mod im Starter an und installiere. Um die Mod zu
  entfernen, nimm den Haken weg und installiere erneut.
- **Ohne Starter:**
  1. Kopiere `ScriptFiles\scripts\*.dll` nach `<Spielordner>\scripts\`.
  2. Kopiere alle Dateien aus `ModFiles\` nach `<Spielordner>\Data\`. Gib jeder Datei die nächste freie Generation. Beispiel: Aus `G3ASM.m0x`
     wird `G3ASM.mod`. Bei einer zweiten Mod wird daraus `G3ASM.m00` und so weiter. Verfahre ebenso mit anderen Archiven, zum Beispiel
     `Quests.m0x`.

Wenn das Spiel startet, entpackt die Laufzeit jede Mod aus `Data\G3ASM.*` nach `g3asm\mods\<Mod>\`. Die Laufzeit schreibt Ausgaben und Fehler
in `g3asm.log` im Spielordner. Auch Spieler brauchen den Community Patch 1.75 und den Parallel-Universe-Patch.

Eine Mod kann diese Aufgaben erfüllen:
- Skriptfunktionen ausführen, wenn Spielereignisse eintreten: Kills, Items, Zauber, Schaden, Bereiche, Quest-Status, Speichern und Laden,
  gesprochene Sätze, Tasten und jedes Bild.
- Ausgewählte Spielfunktionen mit `Override_...` und `Super()` ändern oder abbrechen. Dazu gehören Schaden, Dialogoptionen, Lernkosten,
  Monsterschaden, Spielskripte und KI-Zustände nach Namen, die Konsole und Händlerware.
- Attribute, das Inventar, die Item-Qualität, Talente, Eigenschaften, Quests und Dialoge lesen und ändern.
- Vorlagen klonen und Nicht-Spieler-Charaktere (NPCs) erzeugen.
- Text und Bilder über dem HUD zeichnen.
- Texte des Spiels mit einer `stringtable.ini` ersetzen.
- Quest-Dateien und Info-Dateien mit Unified-Diffs ändern (`ScriptPatches`).
- Eigene Dateien in ein Mod-Archiv packen (`PackMod`).
- Mit anderen Mods zusammenarbeiten (`IsModActive`, `CallMod`, `SendModMessage`).

Anleitung und Referenz: [Erste Mod Schritt für Schritt](docs/modkit/erste-mod.md), [docs/modkit/index.md](docs/modkit/index.md) (Überblick),
[Scripting-Guide](docs/modkit/scripting-guide.md), [Ereignisse](docs/modkit/events.md), [API-Referenz](docs/modkit/api-reference.md),
[Beispiele](docs/modkit/examples.md).

Die Beispiel-Mods unter `examples/mods` haben diese Namen:
- `demo_*`: Beispiele zum Lernen.
- `probe_*`: Mods, die das Spiel messen.
- Mods mit eigenem Namen: kleine Mods mit Spielinhalt.

## Grenzen

**Grundsätzlich:** Die Spiellogik von Gothic 3 liegt in nativen DLLs. Eine Mod greift nur an den Stellen ein, die das Modkit freilegt. Die
vollständige Liste steht unter [Grenzen](docs/modkit/limitations.md).

**Diese Funktionen gehen nicht.** Ich habe sie im Spiel gemessen oder im Spielcode untersucht:
- Eine Mod kann keine einfarbigen Flächen zeichnen. Eine Mod kann nur Text und Bilder zeichnen.
- Das Spiel kann keine eigenen Talente lehren. Die Mod muss die Wirkung des Talents selbst herstellen.
- Es gibt keinen Treffer-Hook, bevor das Spiel den Schaden berechnet. Um einen Treffer zu ändern, nutze das Spielskript `OnHit`,
  `Override_MonsterDamage` oder `Override_Damage`.
- Eine Mod kann nicht direkt ins Questlog schreiben. Die Einträge entstehen über Dialogzeilen.
- Eine Mod kann die Bedingungen eigener Infos nicht setzen. Das Skript muss die Bedingungen selbst prüfen.
- Spieler können keine Datei-Patches (`ScriptPatches`) nutzen. Mod-Pakete enthalten geänderte Spieldateien als Archive (`ModFiles\<Volume>.m0x`).

**Diese Funktionen gibt es nicht.** Ich habe sie noch nicht gebaut oder untersucht:
- Animationen, Änderungen an der Welt, eigene Modelle.
- Eigene Bedienelemente mit Eingabe, ein Einstellungsmenü im Spiel.
- Datei-Patches für komprimierte Archiveinträge (`Strings.pak`) und für Binärdateien.

**Experimentell:** `HurtHitPoints`, `SetProperty`, `GenerateTradeGoods`, `Override_CombatMove`.

**Nicht getestet.** Einzelheiten stehen unter [Kompatibilität](docs/compatibility.md):
- GOG-Versionen, CD-Versionen und DVD-Versionen. G3ASM unterstützt keine Installationen ohne den Community Patch 1.75 oder ohne den
  Parallel-Universe-Patch.
- Der Einsatz mit anderen Script-Plugins, die eigene Hooks setzen, zum Beispiel CombatFix.
- Wenn `Script_MonsterDamage.dll` installiert ist, bleibt `Override_MonsterDamage` aus. Wenn `Script_LearningThresholdTweaks.dll` installiert
  ist, bleibt `Override_LearnCost` aus.
- Zwei Mods mit verschiedenen Laufzeit-Versionen auf dem Rechner eines Spielers.

**ACHTUNG:** Wenn deine Mod dauerhafte Änderungen macht, entferne die Änderungen in `OnBeforeSave` und stelle sie nach dem Speichern wieder her.
Dauerhafte Änderungen sind Items aus geklonten Vorlagen und Talente. Ein Spielstand mit solchen Änderungen stürzte ab, als die Mod nicht
installiert war. Ein Beispiel ist `perk_lifesteal`.

## Lizenz

G3ASM steht unter der Lizenz GPL-3.0-or-later. Siehe `LICENSE`. `LICENSE-EXCEPTIONS.md` gibt zwei Zusatzerlaubnisse nach Abschnitt 7:
- Du darfst das Modkit zusammen mit den Programmdateien von Gothic 3 weitergeben.
- Mods, die nur die Skript-Schnittstelle nutzen, dürfen unter jeder Lizenz stehen.

Die Beispiel-Mods in `examples/` sind gemeinfrei (CC0 1.0, `examples/LICENSE`). Vor dem ersten Release stand das Repo unter der MIT-Lizenz.

Beim Bauen werden AngelScript (zlib-Lizenz) und MinHook (BSD-2-Clause) geladen. Sie gehören nicht zum Repo. Das Projekt enthält keine
Spieldaten. Die Speicher-Layouts der Engine stammen aus dem [Gothic 3 SDK](https://github.com/georgeto/gothic3sdk) (georgeto,
GPL-3.0-or-later). SDK steht für Software Development Kit. Das Repo enthält keinen Code des SDK.

## Danksagung

Ich danke diesen Personen und Gruppen:
- Den Autoren des Gothic-3-SDK (gothic3sdk). Ihre Header haben die Klassen, Signaturen und Eigenschaften der Engine erschlossen.
- Dem Team des Community Patch.
- Den Machern des Gothic 3 Mod Starters. Ihre Arbeit hat das Format der Mod-Archive offengelegt.
- Der Gothic-Community.

Das Modkit nutzt AngelScript (Andreas Jönsson, AngelCode, zlib-Lizenz) und MinHook (Tsuda Kageyu, BSD-2-Lizenz). Vorbild für den Aufbau von
G3ASM war das Modkit des Gothic-1-Remakes.

Gothic 3 ist eine Marke der jeweiligen Rechteinhaber. G3ASM ist ein inoffizielles Fan-Projekt. Es enthält keine Dateien des Spiels.
