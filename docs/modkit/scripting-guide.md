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

Für eine Zahl in einem Text nimm `ToString(5)` oder `"" + 5`.

## Strings: `string` (ab Version 47)

`string` ist der Text-Typ von AngelScript (Add-on `scriptstdstring`). Du brauchst ihn für `dictionary` und für Funktionen wie `split`, `join`,
`parseInt` und `formatInt`. Sonst reicht `bCString`.

Beide Typen wandeln sich selbst ineinander um. Du kannst einen `string` an jede Funktion geben, die einen `bCString` erwartet, und umgekehrt.

```angelscript
string s = "Gothic";                          // das Literal ist bCString und wird zu string
bCString b = s;                               // string wird zu bCString
int n = GetHero().GetItemCount(s);            // string an eine Engine-Funktion
array<string> teile = string("a,b,c").split(",");
print(join(teile, "-"));                      // Ergebnis: a-b-c
int zahl = parseInt("42");
```

**Achtung:** Ein Text in Anführungszeichen ist immer `bCString`. Methoden von `string` gehen deshalb nicht direkt am Literal. Schreibe
`string("a,b,c").split(",")`, nicht `"a,b,c".split(",")`.

## Tabellen: `dictionary` (ab Version 47)

Ein `dictionary` speichert Werte unter einem Schlüssel (Text). Werte können Zahlen, `bool`, `bCString`, `string`, `Entity` und andere Typen
sein.

```angelscript
dictionary d;
d.set("gold", 100);
d["name"] = bCString("Diego");
int gold = 0;
if (d.get("gold", gold)) print("Gold: " + gold);   // Ergebnis: Gold: 100
int g = int(d["gold"]);                            // Wert über [] lesen
if (d.exists("name")) d.delete("name");
array<string>@ schluessel = d.getKeys();
dictionary preise = {{"It_Apple", 5}, {"It_Bread", 10}};
```

| Funktion | Beschreibung |
|---|---|
| `set(key, value)`, `d[key] = value` | Speichert einen Wert. Ein vorhandener Wert wird ersetzt |
| `bool get(key, value &out)` | Liest den Wert. `false`, wenn der Schlüssel fehlt |
| `d[key]` | Wert lesen, zum Beispiel `int(d["gold"])` oder `bCString(d["name"])` |
| `bool exists(key)`, `bool delete(key)`, `deleteAll()` | Prüfen, einen Eintrag oder alle entfernen |
| `uint getSize()`, `bool isEmpty()` | Zahl der Einträge |
| `array<string>@ getKeys()` | Alle Schlüssel, ohne feste Reihenfolge |

**Achtung:** Fehlt der Schlüssel, gibt `get` `false` zurück und schreibt trotzdem einen zufälligen Wert in die Variable. Ein Startwert wie
`int n = 0` bleibt nicht erhalten. Setze den Standardwert deshalb nach dem Aufruf:

```angelscript
int n;
if (!d.get(name, n)) n = 0;   // Ergebnis: n ist 0, wenn name fehlt
d.set(name, n + 1);
```

**Achtung:** Ein `dictionary` in einer globalen Variable geht beim Neuladen der Mod (Hot-Reload) verloren. Es wird auch nicht im Spielstand
gespeichert. Für Entities im `dictionary` gilt dieselbe Regel wie sonst: nicht über einen Weltwechsel aufbewahren
(siehe [Objekte prüfen](#objekte-prüfen)).

## Koroutinen (ab Version 47)

Eine Koroutine ist eine Funktion, die mitten im Ablauf warten kann. Mit `Wait(sekunden)` hält sie an, und das Spiel läuft weiter. Nach der
Wartezeit macht sie an derselben Stelle weiter. So schreibst du Abläufe über mehrere Sekunden ohne Zähler in `OnFrame`.

```angelscript
void Countdown() {
    for (int i = 3; i > 0; i--) {
        PrintScreen("" + i, 1);
        Wait(1.0f);                  // 1 Sekunde Spielzeit warten
    }
    PrintScreen("Los!", 2);
}

void main() {
    StartCoroutine(Countdown);       // Ergebnis: 3, 2, 1, Los! im Sekundentakt
}
```

Werte gibst du über ein `dictionary` mit:

```angelscript
void Gruss(dictionary@ args) {
    bCString name;
    if (!args.get("name", name)) name = "Fremder";
    Wait(2.0f);
    PrintScreen("Hallo " + name, 3);
}

void main() {
    dictionary args = {{"name", "Diego"}};
    StartCoroutine(Gruss, args);
}
```

| Funktion | Beschreibung |
|---|---|
| `int StartCoroutine(Coroutine@ f)` | Startet `void f()` als Koroutine. Rückgabe: ID (> 0), `-1` bei einem Fehler |
| `int StartCoroutine(CoroutineArgs@ f, dictionary@ args)` | Startet `void f(dictionary@ args)` mit Werten |
| `void Wait(float seconds)` | Wartet die Spielzeit `seconds`. `Wait(0)` wartet bis zum nächsten Bild |
| `void Yield()` | Wartet bis zum nächsten Bild |
| `bool StopCoroutine(int id)` | Beendet eine Koroutine der eigenen Mod. `false`, wenn sie nicht mehr läuft |
| `int StopAllCoroutines()` | Beendet alle Koroutinen der eigenen Mod. Rückgabe: ihre Zahl |
| `bool IsCoroutineRunning(int id)` | `true`, solange die Koroutine läuft oder wartet |

- Eine Koroutine beginnt im **nächsten Bild**, nicht sofort beim Aufruf von `StartCoroutine`.
- `Wait` zählt die **Spielzeit**. Im ESC-Menü und beim Laden steht sie still. Im Inventar, auf der Karte und in anderen Bildschirmen läuft sie
  weiter, wie das Spiel selbst.
- Eine Koroutine läuft höchstens einmal je Bild. Ist die Wartezeit kürzer als ein Bild, geht es im nächsten Bild weiter.
- Jede Mod kann höchstens 256 Koroutinen zugleich haben. Danach gibt `StartCoroutine` `-1` zurück.

**Achtung:**
- `Wait` und `Yield` gehen nur in einer Koroutine. In `main`, `OnFrame` oder einem Ereignis brechen sie den Aufruf mit einer Skript-Ausnahme ab.
- Zwischen zwei `Wait` gilt dieselbe Grenze von 5 Sekunden wie bei jedem Skriptaufruf. Eine Schleife ohne `Wait` bricht das Modkit danach ab.
- Lädt die Mod neu (Hot-Reload) oder endet das Spiel, enden alle ihre Koroutinen. Der Spielstand speichert keine Koroutinen.
- Beim Laden eines Spielstands laufen Koroutinen weiter. Für Entities gilt die Regel aus [Objekte prüfen](#objekte-prüfen): nach einem
  Weltwechsel neu holen.

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
