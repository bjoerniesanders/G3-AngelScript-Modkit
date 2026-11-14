# Scripting-Guide

Die Skripte sind AngelScript 2.38. Die Grundlagen der Sprache stehen in der offiziellen Dokumentation (https://www.angelcode.com/angelscript/).
Diese Seite beschreibt, was in G3ASM anders oder besonders ist.

## Eine Datei, eine Mod

```angelscript
// Globale Variablen und Konstanten
const bCString MEIN_NPC = "Jens";
int zaehler = 0;

// Ereignisse: Funktionen mit festen Namen und Signaturen
void main() { print("Start"); }
void OnFrame(float dt) { zaehler++; }

// Eigene Funktionen und Klassen
bool IstNah(Entity a, Entity b) {
    float d = a.GetDistanceTo(b);
    return d >= 0 && d < 400.0f;
}
```

Alle Ereignisfunktionen sind optional. Die vollständige Liste steht auf der Seite [Ereignisse](events.md).

## Strings: `bCString`

Texte sind `bCString` (das Zeichenkettenobjekt des Spiels, Windows-1252). Literale wandeln sich automatisch um.

```angelscript
bCString s = "Hallo";
s += " Welt";
s = s + " " + 5 + " " + 2.5f + " " + true;      // Zahlen und bool lassen sich anhängen
if (s == "Hallo Welt 5 2.5 true") { }
int n = s.GetLength();
bCString links = s.Left(5);
bool drin = s.Contains("Welt");
```

Einen Typ `string` gibt es nicht, alle Texte sind `bCString`; `Format`-Funktionen fehlen ebenfalls. Zahlen in Text: `ToString(5)` oder `"" + 5`.

## Objekte prüfen

Objekte wie `Entity` haben keinen impliziten bool-Operator.

```angelscript
Entity jens = FindEntity("Jens");
if (jens.IsValid()) { ... }          // richtig
if (jens) { ... }                    // Fehler beim Übersetzen
```

`Entity` ist ein Wert mit einem Zeiger auf das Spielobjekt. Jeder Zugriff prüft den Zeiger und läuft unter Schutz: eine ungültige Entity liefert
Standardwerte (leerer Name, Position 0, -1 bei Zahlen), das Spiel stürzt nicht ab.

**Entities nicht über Weltwechsel aufbewahren.** Hol sie bei Bedarf neu (`GetHero()`, `FindEntity(name)`), zum Beispiel in `OnFrame`.

## Referenzen

`&in` für Eingabeparameter (`const bCString &in`) ist erlaubt. Referenzen auf einfache Typen als Ausgabeparameter (`int &out`, `int &inout`) sind gesperrt.
Rückgaben gehen über den Rückgabewert, notfalls über ein `array<T>` oder eine Klasse.

## Zeit und Einheiten

- `OnFrame(float dt)`: `dt` in Sekunden (Spielzeit, skaliert).
- Entfernungen (`GetDistanceTo`, Positionen) sind in der Einheit der Spielwelt, vermutlich **Zentimeter** (400 ≈ 4 m; die Jens-Mod nutzt 400 als Reichweite).
- `GetTimeStamp()` liefert den Zeitstempel des Spiels (`uint`, `bCTimer`), Einheit nicht eigens geprüft.

## Tasten

`OnKey(int vk, bool down)` bekommt den Windows-Tastencode. `KEY_F1` bis `KEY_F12` sind vordefiniert, alle anderen Codes schreibst du als Zahl:

| Taste | Code | Taste | Code |
|---|---|---|---|
| Numblock 0-9 | 0x60-0x69 | Umschalt (Shift) | 0x10 |
| A-Z | 0x41-0x5A | Strg | 0x11 |
| 0-9 (Reihe) | 0x30-0x39 | Leertaste | 0x20 |

`IsKeyDown(code)` fragt den Zustand ab (nur wenn das Spiel im Vordergrund ist). Viele Tasten sind im Spiel belegt, darunter die meisten F-Tasten
(siehe `ini\controls.ini` und die Liste unter [Beispiele](examples.md#tastenbelegung-der-beispiel-mods)). Der Ziffernblock ist bis auf 7, 8, 9 und
Strg + 1 bis 6 frei. Die Nachricht geht immer zusätzlich an das Spiel.

## Fehlersuche

| Symptom | Ursache und Lösung |
|---|---|
| Mod startet nicht, im Protokoll `Skript enthält Fehler` | Zeilen davor lesen: sie nennen Zeile, Spalte und Grund |
| `Skript-Ausnahme in ... (Zeile n)` | Laufzeitfehler (Division durch 0, Null-Zugriff in deinem Code) |
| `Skript abgebrochen (länger als 5 s ...)` | Endlosschleife oder zu viel Arbeit in einem Aufruf |
| Ereignisfunktion wird nicht aufgerufen | Die Signatur stimmt nicht exakt (siehe [Ereignisse](events.md)): Parametertypen und `const bCString &in` genau so schreiben |
| `Entity` liefert leeren Namen / -1 | Die Entity ist ungültig (nicht geladen, anderer Name). Mit `IsValid()` prüfen |
| `GetItemCount` liefert -1 | Entity hat kein lesbares Inventar |
| Nach einer Änderung passiert nichts | Hot-Reload prüft einmal je Sekunde. Bei Syntaxfehlern bleibt die Mod entladen |
| Namen (NPC, Item) stimmen nicht | Namen stammen aus dem Spiel: im Protokoll von `OnNPCKilled`, `OnReceiveItem` ausgeben lassen (siehe [Beispiele](examples.md)) |

Zum Ausprobieren von Namen und Werten ist `print(...)` das wichtigste Werkzeug: die Ausgabe steht im Protokoll.
