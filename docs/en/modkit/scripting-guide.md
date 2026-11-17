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

There is no type `string`. All texts are `bCString`. There are also no `Format` functions. To put a number into a text, use `ToString(5)` or
`"" + 5`.

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
