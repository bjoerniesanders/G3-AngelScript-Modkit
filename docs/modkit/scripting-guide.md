# Scripting-Guide

**Deutsch** | [English](../en/modkit/scripting-guide.md)

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

Texte sind `bCString`, die Zeichenketten des Spiels. Ein Text in Anführungszeichen wird automatisch zu einem `bCString`.

**Achtung, Umlaute:** Das Spiel liest Texte als Windows-1252. Das Modkit übernimmt die Bytes deiner `.as`-Datei unverändert. Enthalten Texte
Umlaute, speichere die Datei als ANSI (Windows-1252). Aus einer UTF-8-Datei erscheinen „ü“ und „ß“ als Zeichensalat. Kommentare sind davon nicht
betroffen. Die Beispiel-Mods schreiben in Texten „ue“ und „ss“.

```angelscript
bCString s = "Hallo";
s += " Welt";
s = s + " " + 5 + " " + 2.5f + " " + true;      // Zahlen und bool lassen sich anhängen
if (s == "Hallo Welt 5 2.5 true") { }
int n = s.GetLength();
bCString links = s.Left(5);
bool drin = s.Contains("Welt");
```

Einen Typ `string` gibt es nicht. Alle Texte sind `bCString`. Auch `Format`-Funktionen fehlen. Für eine Zahl in einem Text nimm `ToString(5)` oder
`"" + 5`.

## Objekte prüfen

Objekte wie `Entity` haben keinen impliziten bool-Operator.

```angelscript
Entity jens = FindEntity("Jens");
if (jens.IsValid()) { ... }          // richtig
if (jens) { ... }                    // Fehler beim Übersetzen
```

`Entity` ist ein Wert mit einem Zeiger auf das Spielobjekt. Jeder Zugriff prüft den Zeiger und läuft unter Schutz. Eine ungültige Entity liefert
Standardwerte: leeren Namen, Position 0, -1 bei Zahlen. Das Spiel stürzt nicht ab.

**Entities nicht über Weltwechsel aufbewahren.** Hol sie neu, wenn du sie brauchst (`GetHero()`, `FindEntity(name)`), zum Beispiel in `OnFrame`.

## Referenzen

`&in` für Eingabeparameter (`const bCString &in`) und `&out` für Ausgabeparameter (`void Teile(int a, int &out q, int &out r)`) sind erlaubt.
`&inout` geht nur bei Objekttypen mit Handles, also nicht bei `int`, `float`, `bCString` oder `Entity`. Der Übersetzer meldet dann „Only object
types that support object handles can use &inout“. Für einen Wert, der hinein- und wieder herausgeht, nimm einen `&in`- und einen `&out`-Parameter
oder den Rückgabewert.

## Zeit und Einheiten

- `OnFrame(float dt)`: `dt` in Sekunden (Spielzeit, skaliert).
- Entfernungen (`GetDistanceTo`, Positionen) sind in der Einheit der Spielwelt. Diese Einheit ist vermutlich **Zentimeter** (400 ≈ 4 m). Die
  Jens-Mod nutzt 400 als Reichweite.
- `GetTimeStamp()` liefert den Zeitstempel des Spiels (`uint`). Die Einheit habe ich nicht geprüft.

## Tasten

`OnKey(int vk, bool down)` bekommt den Windows-Tastencode. `KEY_F1` bis `KEY_F12` sind vordefiniert. Alle anderen Codes schreibst du als Zahl:

| Taste | Code | Taste | Code |
|---|---|---|---|
| Ziffernblock 0-9 | 0x60-0x69 | Umschalt (Shift) | 0x10 |
| A-Z | 0x41-0x5A | Strg | 0x11 |
| 0-9 (Reihe) | 0x30-0x39 | Leertaste | 0x20 |

`IsKeyDown(code)` fragt den Zustand einer Taste ab. Das geht nur, wenn das Spiel im Vordergrund ist. Das Spiel belegt viele Tasten, darunter die
meisten F-Tasten. Siehe `ini\controls.ini` und die Liste unter [Beispiele](examples.md#tastenbelegung). Der Ziffernblock ist frei, außer 7, 8, 9 und
Strg + 1 bis 6. Die Nachricht geht immer auch an das Spiel.

## Fehlersuche

| Symptom | Ursache und Lösung |
|---|---|
| Die Mod startet nicht. Im Protokoll steht `Skript enthaelt Fehler` | Lies die Zeilen davor. Sie nennen Zeile, Spalte und Grund |
| `Skript-Ausnahme in ... (Zeile n)` | Ein Laufzeitfehler: Division durch 0 oder Null-Zugriff in deinem Code |
| `Skript abgebrochen (laenger als 5 s ...)` | Eine Endlosschleife oder zu viel Arbeit in einem Aufruf |
| Eine Ereignisfunktion wird nicht aufgerufen | Die Signatur stimmt nicht genau (siehe [Ereignisse](events.md)). Schreibe die Parametertypen und `const bCString &in` genau so |
| `Entity` liefert einen leeren Namen oder -1 | Die Entity ist ungültig: nicht geladen, oder der Name ist anders. Prüfe mit `IsValid()` |
| `GetItemCount` liefert -1 | Die Entity hat kein Inventar, das das Modkit lesen kann |
| Nach einer Änderung passiert nichts | Hot-Reload prüft einmal je Sekunde. Bei einem Syntaxfehler bleibt die Mod entladen |
| Namen (NPC, Item) stimmen nicht | Die Namen stammen aus dem Spiel. Lass sie in `OnNPCKilled` oder `OnReceiveItem` ins Protokoll schreiben (siehe [Beispiele](examples.md)) |

Zum Ausprobieren von Namen und Werten ist `print(...)` das wichtigste Werkzeug. Die Ausgabe steht im Protokoll.
