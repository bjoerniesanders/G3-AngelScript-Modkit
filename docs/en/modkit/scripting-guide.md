# Scripting guide

[Deutsch](../../modkit/scripting-guide.md) | **English**

The scripts are AngelScript 2.38. The official documentation gives the basics of the language (https://www.angelcode.com/angelscript/).
This page tells you what is different or special in G3ASM.

## One file, one mod

```angelscript
// Global variables and constants
const bCString MY_NPC = "Jens";
int counter = 0;

// Events: functions with fixed names and signatures
void main() { print("Start"); }
void OnFrame(float dt) { counter++; }

// Your own functions and classes
bool IsNear(Entity a, Entity b) {
    float d = a.GetDistanceTo(b);
    return d >= 0 && d < 400.0f;
}
```

All event functions are optional. The page [Events](events.md) gives the full list.

## Strings: `bCString`

Texts are `bCString`, the strings of the game. A text in quotation marks becomes a `bCString` automatically.

**Caution, special characters:** The game reads texts as Windows-1252. The modkit uses the bytes of your `.as` file without changes. If texts
contain umlauts or other special characters, save the file as ANSI (Windows-1252). If you save it as UTF-8, characters such as "ü" and "ß" show
as incorrect characters. Comments are not affected. The example mods write "ue" and "ss" in texts.

```angelscript
bCString s = "Hello";
s += " World";
s = s + " " + 5 + " " + 2.5f + " " + true;      // you can append numbers and bool
if (s == "Hello World 5 2.5 true") { }
int n = s.GetLength();
bCString left = s.Left(5);
bool found = s.Contains("World");
```

To put a number into a text, use `ToString(5)` or `"" + 5`.

## Strings: `string` (from version 47)

`string` is the text type of AngelScript (add-on `scriptstdstring`). You need it for `dictionary` and for functions such as `split`, `join`,
`parseInt` and `formatInt`. For all other texts, `bCString` is sufficient.

The two types change into each other automatically. You can give a `string` to each function that uses a `bCString`, and the opposite.

```angelscript
string s = "Gothic";                          // the literal is bCString and changes into string
bCString b = s;                               // string changes into bCString
int n = GetHero().GetItemCount(s);            // string given to an engine function
array<string> parts = string("a,b,c").split(",");
print(join(parts, "-"));                      // result: a-b-c
int number = parseInt("42");
```

**Caution:** A text in quotation marks is always `bCString`. Thus you cannot use the methods of `string` directly on a literal. Write
`string("a,b,c").split(",")`, not `"a,b,c".split(",")`.

## Tables: `dictionary` (from version 47)

A `dictionary` keeps values under a key (text). Values can be numbers, `bool`, `bCString`, `string`, `Entity` and other types.

```angelscript
dictionary d;
d.set("gold", 100);
d["name"] = bCString("Diego");
int gold = 0;
if (d.get("gold", gold)) print("Gold: " + gold);   // result: Gold: 100
int g = int(d["gold"]);                            // read a value with []
if (d.exists("name")) d.delete("name");
array<string>@ keys = d.getKeys();
dictionary prices = {{"It_Apple", 5}, {"It_Bread", 10}};
```

| Function | Description |
|---|---|
| `set(key, value)`, `d[key] = value` | Keeps a value. A value that is already there is replaced |
| `bool get(key, value &out)` | Reads the value. `false` if the key is not there |
| `d[key]` | Reads a value, for example `int(d["gold"])` or `bCString(d["name"])` |
| `bool exists(key)`, `bool delete(key)`, `deleteAll()` | Checks, removes one entry or all entries |
| `uint getSize()`, `bool isEmpty()` | The number of entries |
| `array<string>@ getKeys()` | All keys, in no fixed sequence |

**Caution:** If the key is not there, `get` gives `false` and writes a random value into the variable. A start value such as `int n = 0` is
not kept. Thus set the default value after the call:

```angelscript
int n;
if (!d.get(name, n)) n = 0;   // result: n is 0 if name is not there
d.set(name, n + 1);
```

**Caution:** A `dictionary` in a global variable is lost when the mod loads again (hot reload). The save game does not keep it. Entities in a
`dictionary` follow the same rule as all entities: do not keep them through a change of the world (see [Check objects](#check-objects)).

## Coroutines (from version 47)

A coroutine is a function that can wait in the middle of its sequence. With `Wait(seconds)`, it stops and the game continues. After the wait
time, it continues at the same location. Thus you can write sequences across many seconds without counters in `OnFrame`.

```angelscript
void Countdown() {
    for (int i = 3; i > 0; i--) {
        PrintScreen("" + i, 1);
        Wait(1.0f);                  // wait 1 second of game time
    }
    PrintScreen("Go!", 2);
}

void main() {
    StartCoroutine(Countdown);       // result: 3, 2, 1, Go! with 1 second between them
}
```

To give values to the coroutine, use a `dictionary`:

```angelscript
void Greeting(dictionary@ args) {
    bCString name;
    if (!args.get("name", name)) name = "stranger";
    Wait(2.0f);
    PrintScreen("Hello " + name, 3);
}

void main() {
    dictionary args = {{"name", "Diego"}};
    StartCoroutine(Greeting, args);
}
```

| Function | Description |
|---|---|
| `int StartCoroutine(Coroutine@ f)` | Starts `void f()` as a coroutine. Return value: the ID (> 0), `-1` if there is an error |
| `int StartCoroutine(CoroutineArgs@ f, dictionary@ args)` | Starts `void f(dictionary@ args)` with values |
| `void Wait(float seconds)` | Waits for `seconds` of game time. `Wait(0)` waits until the next frame |
| `void Yield()` | Waits until the next frame |
| `bool StopCoroutine(int id)` | Stops a coroutine of your mod. `false` if it does not run anymore |
| `int StopAllCoroutines()` | Stops all coroutines of your mod. Return value: their number |
| `bool IsCoroutineRunning(int id)` | `true` while the coroutine runs or waits |

- A coroutine starts in the **next frame**, not immediately when you call `StartCoroutine`.
- `Wait` counts **game time**. In the ESC menu and during a load, the time stops. In the inventory, on the map and on other screens, the time
  continues, as the game does.
- A coroutine runs a maximum of 1 time in each frame. If the wait time is shorter than a frame, it continues in the next frame.
- Each mod can have a maximum of 256 coroutines at the same time. After that, `StartCoroutine` gives `-1`.

**Caution:**
- You can use `Wait` and `Yield` only in a coroutine. In `main`, `OnFrame` or an event, they stop the call with a script exception.
- Between two `Wait` calls, the same 5-second limit applies as for each script call. The modkit stops a loop without `Wait` after this time.
- When the mod loads again (hot reload) or the game ends, all its coroutines stop. The save game does not keep coroutines.
- When a save game loads, the coroutines continue. For entities, the rule in [Check objects](#check-objects) applies: get them again after a
  change of the world.

## Check objects

Objects such as `Entity` have no implicit bool operator.

```angelscript
Entity jens = FindEntity("Jens");
if (jens.IsValid()) { ... }          // correct
if (jens) { ... }                    // compile error
```

`Entity` is a value with a pointer to the game object. Each access checks the pointer and runs with protection. An entity that is not valid
gives default values: an empty name, position 0, -1 for numbers. The game does not crash.

**Do not keep entities across a world change.** Get them again when you need them (`GetHero()`, `FindEntity(name)`), for example in `OnFrame`.

## References

You can use `&in` for input parameters (`const bCString &in`) and `&out` for output parameters (`void Divide(int a, int &out q, int &out r)`).
`&inout` works only for object types with handles. Thus it does not work for `int`, `float`, `bCString` or `Entity`. The compiler gives the
error "Only object types that support object handles can use &inout". For a value that goes in and comes out again, use one `&in` parameter and
one `&out` parameter, or use the return value.

## Time and units

- `OnFrame(float dt)`: `dt` is in seconds (game time, scaled).
- Distances (`GetDistanceTo`, positions) are in the unit of the game world. This unit is probably **centimeters** (400 ≈ 4 m). The Jens mod
  uses 400 as its range.
- `GetTimeStamp()` gives the time stamp of the game (`uint`). I did not check the unit.

## Keys

`OnKey(int vk, bool down)` gets the Windows key code. `KEY_F1` to `KEY_F12` are predefined. Write all other codes as numbers:

| Key | Code | Key | Code |
|---|---|---|---|
| Numeric keypad 0-9 | 0x60-0x69 | Shift | 0x10 |
| A-Z | 0x41-0x5A | Ctrl | 0x11 |
| 0-9 (top row) | 0x30-0x39 | Space bar | 0x20 |

`IsKeyDown(code)` gets the state of a key. This works only when the game is in the foreground. The game uses many keys, for example most F keys.
See `ini\controls.ini` and the list in [Examples](examples.md#key-assignments). The numeric keypad is free, but not 7, 8, 9 and Ctrl + 1 to 6.
The game always gets the message too.

## Troubleshooting

| Symptom | Cause and solution |
|---|---|
| The mod does not start. The log shows `Skript enthaelt Fehler` (script contains errors) | Read the lines before it. They give the line, the column and the reason |
| `Skript-Ausnahme in ... (Zeile n)` (script exception in ..., line n) | A run-time error: a division by 0 or a null access in your code |
| `Skript abgebrochen (laenger als 5 s ...)` (script stopped, longer than 5 s) | An endless loop or too much work in one call |
| An event function is not called | The signature is not exactly correct (see [Events](events.md)). Write the parameter types and `const bCString &in` exactly as shown |
| `Entity` gives an empty name or -1 | The entity is not valid: it is not loaded, or the name is different. Do a check with `IsValid()` |
| `GetItemCount` gives -1 | The entity has no inventory that the modkit can read |
| Nothing occurs after a change | Hot reload does a check 1 time each second. If there is a syntax error, the mod stays unloaded |
| Names (NPC, item) are not correct | Names come from the game. Write them to the log from `OnNPCKilled` or `OnReceiveItem` (see [Examples](examples.md)) |

To try names and values, `print(...)` is the most important tool. The output goes to the log.
