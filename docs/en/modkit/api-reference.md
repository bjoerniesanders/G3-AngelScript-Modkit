# API reference (script interface version 46)

[Deutsch](../../modkit/api-reference.md) | **English**

Use this page for reference. It gives all functions and types that a script can call. For an introduction and examples, see the
[Scripting guide](scripting-guide.md) and [Examples](examples.md).

- All functions are global, but not functions with `Entity::` before them or functions in a section about the methods of `Entity`
  (`hero.GetAttribute("STR")`).
- Without a running game (tests), the game functions give default values (`false`, `-1`, empty text).
- "From version N" gives the script interface that added the function (`GetApiVersion()`). The [Version rules](api-policy.md) tell what can
  change between versions.

**Labels** are at the start of the description:

| Label | Meaning |
|---|---|
| None | You can use it as described |
| **Experimental** | It runs, but I did not fully check the effect. The name, the parameters and the behavior can still change |
| **Not checked in the game** | Tested without the game. Never tried in the game |
| **Does not work** | You can call it, but in the game it gives no useful result. The replacement is given |
| **Deprecated** | You can call it. At the first call in each mod, it writes a warning. The replacement is given |

## General

| Function | Description |
|---|---|
| `void print(const bCString &in)` | Writes `[mod name] text` to the log `g3asm.log` (and to the log of the game) |
| `bCString ToString(int / uint / float / double / bool)` | A number or a bool as text |
| `float GetFrameTime()` | The duration of the last frame in seconds (game time) |
| `uint GetTimeStamp()` | The time stamp of the game |
| `int GetApiVersion()` | The version of the script interface (46) |
| `bCString GetGameDir()` | The game folder, with `\` at the end |
| `void check(bool, const bCString &in)` | For tests: writes to the log if the condition is false |

From the AngelScript add-ons: `array<T>` (`insertLast`, `length`, `[]`, ...) and `scriptmath` (`sin`, `cos`, `sqrt`, `abs`, `pow`, `floor`, ...
with `float`).

## Other mods and settings

| Function | Description |
|---|---|
| `bool IsModActive(const bCString &in name)` (from version 40) | `true` if the mod is loaded (installed, on, conditions of the `mod.ini` met), also without a script |
| `bCString GetModVersion(const bCString &in name)` (from version 40) | `version` from the `mod.ini` of an active mod. Empty if there is no entry |
| `bCString GetModSetting(const bCString &in key, const bCString &in def = "")` (from version 44) | A setting of **your own** mod as text: the user value, or if there is none, the default from the `mod.ini`, or if there is none, `def` |
| `int GetModSettingInt(key, int def = 0)`, `float GetModSettingFloat(key, float def = 0)`, `bool GetModSettingBool(key, bool def = false)` (from version 44) | The same as a number or a switch. If the value cannot be read, you get `def` |
| `bool HasModSetting(const bCString &in key)` (from version 44) | Is there a user value or a default? |
| `bool SetModSetting(const bCString &in key, const bCString &in value)` (from version 44) | Saves a user value immediately in `g3asm\settings\<mod>.ini` |
| `bool ResetModSetting(const bCString &in key)` (from version 44) | Removes the user value. Then the default applies again |
| `bCString CallMod(const bCString &in mod, const bCString &in func, const bCString &in arg)` (from version 44) | Calls `bCString <func>(const bCString &in)` in a different mod and gives its return value. Empty if the mod or the function is missing |
| `bool ModFunctionExists(const bCString &in mod, const bCString &in func)` (from version 44) | Does the mod have this function? |
| `int SendModMessage(const bCString &in topic, const bCString &in data)` (from version 44) | Immediately calls `OnModMessage` in all **other** mods. Return value: the number of receivers |

- Upper case and lower case have no effect for mod names and setting keys.
- `IsModActive` is for optional cooperation. If your mod must have another mod, put it into `requires` in the `mod.ini`
  ([Mod folder](mod-package.md#version-and-dependencies-modini-from-version-40)).
- Settings: integers can also be hexadecimal (`0x10`). Decimal numbers can use `.` or `,`. Switches: `1`/`0`, `true`/`false`, `ja`/`nein`,
  `on`/`off`. Keys without `= [ ] ; #` and with a maximum of 100 characters. Values without line breaks and with a maximum of 2000 characters.
  `SetModSetting` never changes the `mod.ini`. Thus an update of the mod keeps the user values.
- `CallMod`: numbers and more than one value go as text (`ToString`, your own separator). During the call, the target mod runs with its own
  settings. A maximum of 8 nested calls. After that, you get an empty text and a log line.

## Window, keys, screen text

| Function | Description |
|---|---|
| `bool IsKeyDown(int vk)` | The key is down at this time (only when the game is in the foreground) |
| Constants `KEY_F1` ... `KEY_F12` | Key codes of the function keys |
| `void SetWindowTitle(const bCString &in)` | Sets the window title |
| `void PlaySystemSound()` | Windows system sound |
| `bool PrintScreen(const bCString &in text, float seconds = 3, int r = 255, int g = 255, int b = 255)` | Text on the screen |
| `bool PrintGameMessage(const bCString &in text, int type = 0)` | A message in the style of the game |
| `int GetActivePage()` | The number of the open game page (menu, inventory, ...). `-1` = none, thus the game itself. Other known values: 0 to 6 and 17. Their meaning is only partially known |

## Draw (only in `OnDraw`)

| Function | Description |
|---|---|
| `int CreateFont(const bCString &in face, int height, bool bold = false, bool italic = false)` (from version 34) | Makes a font and gives a number. The same data always gives the same number |
| `bool DrawText(int font, int x, int y, const bCString &in text, uint argb = 0xFFFFFFFF, bool shadow = true)` (from version 34) | Draws text. (`x`, `y`) is the top left corner. The color is `0xAARRGGBB` |
| `int TextWidth(int font, const bCString &in text)` (from version 34) | The width of a text in this font |
| `int LoadImage(const bCString &in name)` (from version 38) | Loads an image from the GUI data of the game. Number > 0. `0` = cannot be loaded (yet) |
| `bool DrawImage(int image, int x, int y, int w, int h, float angle = 0)` (from version 38) | Draws the image into the rectangle. `angle` turns it (radians) |
| `int GetScreenWidth()`, `int GetScreenHeight()` (from version 34) | The size of the drawing area |
| `bool WorldToScreen(const bCVector &in pos, float &out x, float &out y)` (from version 34) | Converts a world point into screen coordinates. `false` if the point is behind the camera |

- All drawings are above the HUD and the inventory.
- The coordinates apply to the drawing area of the game GUI, not to window pixels. The GUI scales. For a window with 2560 × 1440,
  `GetScreenWidth()` × `GetScreenHeight()` gives 1365 × 768. The size and `WorldToScreen` are correct only from the first `OnDraw`.
- **There are no rectangles with only one color.** The rectangle functions of the game draw nothing. Use an image in place of a rectangle.
- Call `LoadImage` 1 time for each name. In `main()`, the GUI data is frequently not available yet. The modkit tries a name that failed again
  after 2 seconds. Names as in `gui.pak`: the full path (`\data\gui\icons\hud\v3\icons\misc\g3_hud_icon_misc_compass.tga`; in the script, write
  each `\` two times) or only the file name (`g3_hud_icon_misc_compass.tga`).
- `GetPosition()` of an NPC is the point at its feet. `+ bCVector(0, 100, 0)` is approximately the center of the body.
- Example: `probe_draw`.

## Time

| Function | Description |
|---|---|
| `int GetGameDay()`, `GetGameHour()`, `GetGameMinute()` | Game time. `-1` without a world |
| `bool SetGameTime(int day, int hour, int minute)` | Sets the game time. The clock and the daylight change immediately. `day = -1` keeps the day |
| `float GetTimeScale()`, `bool SetTimeScale(float)` (from version 36) | The speed of the full simulation (1 = normal, 0.05 to 10). The game does not save it |
| `float GetClockFactor()`, `bool SetClockFactor(float)` (from version 36) | The speed of the game clock (game minutes for each real minute, normal 12, 0 to 10000). **It is in the saved game**, see below |

Set the clock factor back in `OnBeforeSave`, before the save, and set it again in `OnAfterSave`. If you do not, it stays in the saved game, also
without the mod (example `probe_time`).

## Find, make and remove entities

An `Entity` is a handle to a game object (NPC, hero, item, building, template). An `Entity` that is not valid is permitted. Its methods then
give default values.

| Function | Description |
|---|---|
| `Entity GetHero()` | The hero |
| `Entity GetPlayer()` | The player |
| `Entity FindEntity(const bCString &in name)` | An entity by name (`"Jens"`, `"PC_Hero"`). Not valid if not found |
| `int CountEntities(const bCString &in part)`, `Entity GetEntityByPart(const bCString &in part, int index)` | Find entities by a part of the name: the number, and the entry with the number `index` |
| `Entity SpawnNPC(const bCString &in templateName, const bCVector &in position)` | Makes an NPC or an object from a template at the position. Not valid if there is an error |
| `bool DestroyNPC(const Entity &in)` | Removes an NPC that `SpawnNPC` made |
| `bool IsNPC(const Entity &in)` | Is the entity an NPC? |
| `bool IsDynamicSpawn(const Entity &in)` | Is it a spawned NPC (not the hero, not the player)? |
| `int CountTemplateNames(const bCString &in prefix)` (from version 28) | The number of templates with a name that starts with this text, each template one time. `-1` without the game |
| `bCString FindTemplateNames(const bCString &in prefix, int max = 20)` | The names of the templates that start with this text, as `"Name1;Name2;..."`. Empty if nothing is found |
| `Entity GetTemplateEntity(const bCString &in name)` (from version 29) | The template with exactly this name (`"It_1H_OrcSlayer_01"`). Not valid if it does not exist |
| `int CloneTemplate(const bCString &in base, const bCString &in newName)` | Copies an item template with a new name. `0` made, `1` already existed, negative = error |

- **`SpawnNPC`:** The template name must be exactly correct (`FindTemplateNames` helps). Error codes in the log: `-2` unknown template, `-5` no
  world, `-6` and `-7` the spawn failed. With `PickCenter()` and `GetPickPoint()`, you can make an NPC at the point of the crosshair.
- **`DestroyNPC`** removes only spawned NPCs, also from previous sessions. The function refuses NPCs of the game world, the hero, the player and
  objects of the world (`false`, reason in the log). Do not use the `Entity` after this.
- **`GetEntityByPart`:** If you remove entities during the loop, go backward.
- **Run time:** `CountEntities`, `GetEntityByPart` and `FindTemplateNames` search the full world or all templates. One call can take up to
  0.5 s, and the game stops for this time. Do not call them in each frame. Call them one time and keep the result. An empty part (`""`)
  agrees with all entities, also with landscape parts without a name.
- **`FindTemplateNames`:** The start must agree (`Teleport` finds `Teleport_Ardea`, but not `Spl_Teleport_Ardea`). Upper case and lower case have
  no effect. Each name occurs one time (from v0.46.1; before, frequently two times). Each result costs approximately 3 ms. Divide long lists (`max` up to 20000) across many frames. If
  you do not, the limit of 5 seconds for each call stops the script (example `probe_reference`).
- **`CloneTemplate`:** call it in `OnQuestsLoaded`. Then items from the clone stay in saved games, because the clone gets the same ID at each game
  start. In `main()`, the game is not ready yet (code `-4`). Other codes: `-2` unknown base, `-6` and `-7` the copy failed. Set the name and the
  description of the item with `RegisterText("FO_<newName>", ...)` and `RegisterText("ITEMDESC_<newName>", ...)`. Then use
  `AddItems(newName, n)`. Example `demo_clone`.

## Entity: position and type

| Function | Description |
|---|---|
| `bool IsValid() const` | The entity exists and the modkit can read it |
| `bCString GetName() const` | The name |
| `bCString GetTemplateName() const` | The name of the template from which the entity came. Empty if not known |
| `bCString GetCategory() const` | The general type: `"NPC"`, `"Building"`, `"Object"`, `"Landscape"`, `"Border"`, `"Unnamed"`, `"Other"` |
| `bool IsBuilding() const` | A building? |
| `bCVector GetPosition() const` | The position in the world |
| `bCQuaternion GetRotation() const` | The rotation in the world |
| `bCMatrix4 GetWorldMatrix() const` | The world matrix (identity matrix for an entity that is not valid) |
| `bCVector GetBoundsMin() const`, `bCVector GetBoundsMax() const` | The bounding box with the children, in world coordinates |
| `Entity GetParent() const` | The parent object. Not valid if there is none |
| `float GetDistanceTo(const Entity &in) const` | The distance in centimeters. `-1` for entities that are not valid |
| `bool SetPosition(const bCVector &in)` | Moves the entity |
| `bool SetRotation(const bCQuaternion &in)` | Sets the rotation (Y is up) |
| `bool LookAt(const Entity &in target, float yawOffsetDegrees = 0)` | Turns the entity around the vertical axis to the target. If it looks away, use the offset 180 |
| `bool TeleportTo(const Entity &in target)` (from version 33) | Teleports to the target, as the game does with companions |
| `==` | The same game object? |

- `GetCategory` uses name patterns of the game (`G3_Myrtana_House_*` = building, `G3_Object_*` = usable objects). Unnamed objects, such as trees
  and stones, are `"Unnamed"`. `GetTemplateName` then frequently gives more data.
- The crosshair frequently hits a part of a building. `GetParent()` is then the building.
- After `TeleportTo`, the entity is exactly on the target. A companion that is exactly on the hero speaks to the hero immediately. Thus, after
  this, move it a small distance with `SetPosition` (example `teleport_party`).

## Entity: attributes and inventory

| Function | Description |
|---|---|
| `int GetAttribute(const bCString &in tag) const` | The current value |
| `int GetAttributeBase(tag) const`, `int GetAttributeMax(tag) const` | The base value and the maximum value |
| `bool SetAttributeBase(tag, int)`, `bool AddAttributeBase(tag, int)` | Sets or increases the base value (as when the hero learns) |
| `bool SetAttribute(tag, int)`, `bool AddAttribute(tag, int)` | Sets or increases the current value |
| `int GetItemCount(const bCString &in itemName) const` | The quantity of this item across all stacks. `-1` = no inventory that the modkit can read |
| `int AddItems(const bCString &in itemName, int count)` | Puts items into the inventory. `>= 0` success, `-1` no inventory, `-2` unknown item |
| `int RemoveItems(const bCString &in itemName, int count)` | Removes up to `count` items. Return value: the number removed. `-1` = error |
| `int DeleteItemStack(const bCString &in name)` (from version 30) | Fully removes all stacks of this item. Return value: the number of stacks |
| `int GetInventoryStackCount() const` (from version 37) | The number of stacks in the inventory. `-1` without an inventory |
| `Entity GetSlotItem(int slot) const` (from version 28) | The item in the equipment slot. Not valid if the slot is empty |
| `uint GetSlotQuality(int slot) const`, `bool SetSlotQuality(int slot, uint bits)` (from version 28) | The quality (bits `QUALITY_*`) of the item in the equipment slot |

- Attribute tags: `"STR"`, `"DEX"`, `"INT"`, `"HP"`, `"MP"`, `"SP"` (stamina), `"LP"`, `"ALC"`, `"SMT"`, `"THF"` and more. The checked list is in
  the [Reference](reference.md#7-attributes). `"XP"` reads 0.
- **Only the hero:** The attribute functions read and write through the player memory (`PlayerMemory`). Only the hero has it. For NPCs, they
  give `-1` or `false`.
- **Experience and learning points:** read them through the properties of the player memory:

      bCString xp = GetProperty(GetHero(), "PlayerMemory", "XP");   // text, for example "13050"

  A small helper function in the script changes the text into a number (example `jens_quest`, function `Zahl`). More properties: `"LPAttribs"` (learning points for attributes), `"LPPerks"`. The level is in `GetProperty(hero, "NPC", "Level")`.
- Item names are the names of item templates (`It_Wolf_Fur`, `It_Water`, `It_1H_Longsword_01`). `OnReceiveItem` shows them in the log.
- `RemoveItems` decreases only the quantity. Permanent items (learn items of skills) then leave an empty stack. `DeleteItemStack` removes these
  stacks.
- Equipment slots: `SLOT_RIGHT_HAND`, `SLOT_LEFT_HAND`, `SLOT_BACK_LEFT`, `SLOT_BACK_RIGHT`, `SLOT_BOW`, `SLOT_AMMO`. Qualities:
  `QUALITY_DISEASED`, `QUALITY_POISONED`, `QUALITY_BURNING`, `QUALITY_FROZEN`, `QUALITY_SHARP`, `QUALITY_BLESSED`, `QUALITY_FORGED`, `QUALITY_WORN`
  and `QUALITY_LIFESTEAL` (a bit of the modkit). The [Reference](reference.md) gives the values.
- The description of the weapon shows the qualities that are set as a line with a symbol. For `QUALITY_LIFESTEAL`, the text comes from the key
  `HUD_Quality_Lifesteal` of the mod string table (example `demo_lifesteal`). Items with different qualities are different stacks and go into the
  saved game. The inventory can show a change only when you open it the next time.

## Entity: combat and status effects

| Function | Description |
|---|---|
| `Entity GetCurrentAttacker() const` (from version 26) | The current opponent of an NPC or of the hero. Not valid if there is none |
| `uint GetStatusEffects() const` (from version 27) | All status effects as a bit mask (`STATUS_*`) |
| `bool HasStatusEffect(int) const`, `bool SetStatusEffect(int, bool)` (from version 27) | Gets or sets one status effect |
| `bool Kill(const Entity &in killer)`, `bool Kill()` (from version 46) | Kills an NPC or an animal. `OnNPCKilled` comes immediately. The killer gets experience. The player can loot the victim |
| `int HurtHitPoints(int amount, int minHp = 0)` (from version 30) | **Experimental.** Decreases the hit points by `amount`, but not below `minHp`. Return value: the new hit points. **It does not kill** |
| `bool EffectExists(const bCString &in name)`, `bool StartEffect(const bCString &in name, const Entity &in at)` (from version 30, global) | Does a check for an effect of the game (visual only), or starts it at the entity |

- `GetCurrentAttacker` follows the target selection of the AI. With more than one opponent, it is not always the opponent that hit last.
- Status effects: `STATUS_BURNING`, `STATUS_FROZEN`, `STATUS_POISONED`, `STATUS_DISEASED`, `STATUS_TRANSFORMED`, `STATUS_IN_FOG`, `STATUS_GHOST`,
  `STATUS_IN_MAGIC_BARRIER`, `STATUS_SOULTRAVEL` ([Reference](reference.md)). `SetStatusEffect` causes `OnStatusEffect`. The game decides the
  results. For disease and poison, the game also uses its own timer, which this function does not set.
- `HurtHitPoints` shows in `OnDamage` without an attacker and without a weapon. At 0 hit points, the NPC stays standing. To kill, use `Kill`.
  Damage with a cause comes from a summon with an owner (example `beliars_klaue`).
- Effect names are in the spell templates, for example `"eff_magic_summonlightning_target_01"` (the lightning of "Blitz beschwören", summon
  lightning).

## Entity: skills

| Function | Description |
|---|---|
| `bool LearnSkill(name)` (from version 30) | Learns a skill |
| `bool ActivateSkill(name)`, `bool DeactivateSkill(name)` (from version 30) | Turns a skill on or off (counts as a weapon with a skill) |
| `bool HasLearnedSkill(name)`, `bool IsSkillActive(name)`, `int GetSkillActivationCount(name)` (from version 30) | Queries |

`name` is the skill template, for example `"Perk_1H_1"`. For an unknown skill or no inventory: `false` or `-1`. For a cloned skill, `LearnSkill`
gives `false`. The mod must make the effect of a custom skill itself (example `perk_lifesteal`).

## Read and write properties

Each entity and each template has property sets (`Item`, `Damage`, `NPC`, `Inventory`, ...) with named values (`GoldValue`, `DamageAmount`, ...).

| Function | Description |
|---|---|
| `bCString GetProperty(const Entity &in e, const bCString &in set, const bCString &in prop)` (from version 29) | Reads a property as text. Empty if it does not exist |
| `bCString ListProperties(const Entity &in e, const bCString &in set, int max = 200)` (from version 29) | All properties of the set as `"Name=Value;Name=Value;..."` (up to 600) |
| `bool SetProperty(const Entity &in e, const bCString &in set, const bCString &in prop, const bCString &in value)` (from version 29) | **Experimental.** Writes a property from text. Only text values |
| `bool SetTemplateRef(const Entity &in e, const bCString &in cls, const bCString &in prop, const bCString &in target)` (from version 30) | Sets a reference to a template |
| `bool SetEntityRef(const Entity &in e, const bCString &in cls, const bCString &in prop, const Entity &in target)` (from version 30) | Sets a reference to an entity |
| `int DumpPropertySchema(const bCString &in file)` (from version 43) | Writes all property classes with their properties as JSON to `g3asm\<file>` |

- `set` is the name of the set (`"Item"`, `"Damage"`, `"NPC"`; upper case and lower case have no effect) or the class name (`"gCItem_PS"`).
  `prop` is the name of the property as in the [Gothic 3 SDK](https://github.com/georgeto/gothic3sdk) (`"GoldValue"`, `"RequiredSkill1"`).
- `ListProperties` shows what an item really contains. The mod `demo_properties` writes it to the log when you push a key.
- For references and lists, the text that you read does not always give useful data. **You cannot write references with `SetProperty`** (it gives
  `true`, but has no effect). Use `SetTemplateRef` and `SetEntityRef` for this. To see if `SetProperty` worked, use `GetProperty` after it.
- Use `SetProperty` and `SetTemplateRef` only on templates and entities that the mod made itself (`CloneTemplate`).
- Examples: `SetTemplateRef(weapon, "gCItem_PS", "ActivateSkill2", "Perk_Lifesteal")` (the weapon turns on a skill, `perk_lifesteal`) and
  `SetEntityRef(summon, "gCInteraction_PS", "Owner", GetHero())` (the owner of a summon, who also gets the credit for its damage,
  `beliars_klaue`). `cls` is the class name of the set. `prop` is the name of the property.
- `DumpPropertySchema`: `file` is a simple file name without a path. Return value: the number of classes (273 in the game), `-1` without the game,
  `-2` file name not valid, `-3` cannot write. For each class, the file contains the base class and the properties with type, category,
  description and all enum values. The names for `GetProperty` come from this file. Duration approximately 0.1 s. Example `probe_schema`.

## Group (party)

| Function | Description |
|---|---|
| `int GetPartyMemberCount()`, `Entity GetPartyMember(int)` (from version 33) | The members of the group of a leader (for the hero: companions, summons, slaves) |
| `Entity GetPartyLeader()` (from version 33) | The leader of the group of the entity |
| `bool IsPartyWaiting()`, `bool SetPartyWaiting(bool)` (from version 33) | Did the hero say "Wait here"? |

Without a group: `-1` or not valid. `GetProperty(m, "Party", "PartyMemberType")` gives the type of a member: 1 companion, 2 mob, 3 slave,
4 controlled, 5 summoned. Example `teleport_party`.

## Traders

| Function | Description |
|---|---|
| `bool GenerateTradeGoods()` (from version 37) | **Experimental.** Makes the goods of a trader again, as the game does. Gives `true`, but the goods do not change at this time |

Causes `Override_TradeGenerate` and `OnTradeGenerated`.

## Focus and crosshair

| Function | Description |
|---|---|
| `Entity GetFocusedEntity()` | The target of the hero at this time (the name above the target). Not valid if there is nothing |
| `Entity GetCrosshairEntity()` | The object below the crosshair. Not valid if there is nothing |
| `Entity PickCenter(float maxDistance = 100000)` | The object exactly in the center of the screen, with the hit point. The hero does not count |
| `Entity PickScreen(int x, int y, float maxDistance = 100000)` | The same for one pixel of the game window, for example the mouse position |
| `bCVector GetPickPoint()`, `bool HasPickPoint()` | The hit point of the last `PickCenter` or `PickScreen`, and if there was one |
| `bool SetCrosshairVisible(bool on)` | Shows or hides the crosshair |

Example `demo_focus` (Home key). `SpawnNPC("Diego", GetPickPoint())` makes an NPC at the target.

## Game scripts

Gothic 3 has game scripts in its `Script_*.dll` files: named functions such as `MagicTeleport`, `GetAttitude` or `IsHumanoid`. A script can call
them and intercept some of them.

| Function | Description |
|---|---|
| `int CallScript(const bCString &in name, const Entity &in self, const Entity &in other, int arg = 0)` (from version 32) | Calls a game script. The return value is its result, or 0 if it is missing |
| `bool ScriptExists(const bCString &in name)`, `bCString GetScriptDll(const bCString &in name)` (from version 32) | Does the script exist, and which DLL contains it? |
| `void WatchScript(const bCString &in name)` (from version 32) | Registers a name for `Override_Script` and `OnScript` (from version 41 also AI scripts for `Override_AI` and `OnAI`) |
| `void SetScriptResult(int value)` (from version 32) | The result for the game if `Override_Script` does not call the original |
| Constants `AI_STATE` (1), `AI_FUNCTION` (2), `AI_CALLBACK` (3), `AI_ROUTINE` (4) (from version 41) | The type of the AI script in `Override_AI` and `OnAI` |
| `bCString GetLocText(const bCString &in id)` (from version 41) | The text for an ID of the string table in the game language, also your own texts |
| `bCString GetSpeechText(const bCString &in id)` (from version 41) | The text of a spoken sentence, from the ID of `OnAIOutput`. Empty if not known |

- `CallScript`: an `Entity` that is not valid means "nobody". Examples: `CallScript("IsHumanoid", npc, Entity())` gives 1 for humans.
  `CallScript("GetAttitude", npc, GetHero())` gives the attitude (2 neutral, 4 hostile). Scripts with an effect change the game immediately.
- **You cannot get to** scripts that the game calls directly in its internal code and not through the name, for example `AssessHit`,
  `IsDeadlyDamage` and `CanBurn`. For hits, use the game script `OnHit` (through `WatchScript("OnHit")` and `Override_Script`), and
  `Override_Damage`, `OnDamage` and `Override_MonsterDamage` ([Hooks](hooks.md)).
- The game loads the script DLLs only after `main()`. Thus use `ScriptExists` from `OnWorldActivate`.
- `WatchScript` applies to all mods. One call is sufficient. Upper case and lower case have no effect.
- `GetSpeechText` gives dialog lines and commands of the hero (`SVM_Player_CmdPartyJoin` = "Komm mit!", come with me). There is no text for
  ambient talk.

## Console

| Function | Description |
|---|---|
| `void SetConsoleReply(const bCString &in text)` (from version 35) | The answer to the command that runs at this time (only in `Override_ConsoleCommand`) |
| `bool OpenConsole()` (from version 35) | Opens the console. You can see it only in Marvin mode |

## Quests

The page [Quests](quests.md) gives the instructions.

| Function | Description |
|---|---|
| `bool QuestExists(name)`, `bool CreateQuest(name)` | Check, make |
| `bool RunQuest(name)`, `SucceedQuest(name)`, `FailQuest(name)`, `CloseQuest(name)`, `RemoveQuest(name)` | Start, complete with success, fail, close, remove |
| `int GetQuestStatus(name)`, `bool SetQuestStatus(name, int)` | Reads or sets the status (`QUEST_*`). `-1` = the quest does not exist |
| `bool IsQuestFinished(name)`, `bool IsQuestSucceeded(name)` | Queries |
| `int GetQuestXP(name)`, `bool SetQuestXP(name, int)` | The experience points of the quest |
| `bool SetQuestReward(name, attribute, quantity)`, `bCString GetQuestReward(name)` | Attribute reward (for example `"DEX"`, 1). The game gives it and shows it in the success message |
| `bool SetQuestTopic(name, text)`, `bool SetQuestDescription(name, text)` | The title and the description in the quest log |
| `void RegisterText(id, text)` | Sets your own text for a text ID of the game |
| `int GetQuestCount()`, `bCString GetQuestNameAt(int)` | Lists all quests |
| `bool ReloadQuestFiles()` | Reads the quest files of the game again |
| `bCString GetQuestDir()`, `bCString GetQuestFilePath(name)` | The folder and the file of a quest |
| `bool LoadQuestFile(name, path)` | Reads the properties of a quest from a `.quest` file |
| `bCString DescribeQuestRewards()` | All attributes that quests of the game give as a reward |
| `void QuestDebug()` | Troubleshooting: writes the raw data of the quest list to the log |
| Constants | `QUEST_OPEN` 0, `QUEST_RUNNING` 1, `QUEST_SUCCESS` 2, `QUEST_FAILED` 3, `QUEST_OBSOLETE` 4, `QUEST_CANCELLED` 5, `QUEST_LOST` 6, `QUEST_WON` 7 |

`LoadQuestFile` needs the path as the game uses it: `Data/Quests/<world>/<quest>_quest_<world>.quest`, with `/` and relative to the game folder.
The file can also be in an archive. `GetQuestFilePath(name)` gives exactly this path. You cannot write quest files.

## Hooks

In an `Override_...` function, `bool Super()` calls the original (or the next mod with the same override). `bool SetOverrideArg(int index, uint value)`
changes a number argument during this call. `uint GetOverrideArg(int index)` reads it. The page [Hooks](hooks.md) gives all other data.

## Switches in the saved game

| Function | Description |
|---|---|
| `bool SetGameEvent(name)`, `ClearGameEvent(name)`, `IsGameEventSet(name)` | Yes-or-no switches that the game saves with the saved game ([Save and load](saving.md)) |

## Dialogs

The page [Dialogs](dialogs.md) gives the instructions and the functions: `GetInfoCount`, `GetInfoName`, `GetInfoOwner`, `DescribeInfo`,
`DescribeCommands`, `InfoExists`, `CreateInfo`, `RemoveInfo`, `SetInfoQuest`, `SetInfoParent`, `SetInfoType`, `SetInfoCondition`, `AddInfoLine`,
`RebuildInfos`, `SetInfoGiven`, `IsInfoGiven`, and the constants `INFO_COND_*` and `INFO_TYPE_*`.

## Mod archives

`PackMod`, `PackFolder`, `NextModFile`, `GetPackError`: pack a folder as a mod archive. See
[Mod archives](archives.md#script-interface-from-version-24).

## Types

The math functions of the types use the library of the game. Thus they calculate exactly as the game does.

### `bCString`
Text (Windows-1252). `+`, `+=`, `==`, `<`, `+ int/uint/float/double/bool`, `GetLength()`, `IsEmpty()`, `Compare(s)`, `CompareNoCase(s)`,
`Find(s, start = 0)`, `Contains(s)`, `Left(n)`, `Right(n)`, `Mid(start, n)`, `ToUpper()`, `ToLower()`, `GetUpper()`, `GetLower()`.

### `bCVector` (`x`, `y`, `z`)
A point or a direction. Y is up. The unit is centimeters. `GetMagnitude()`, `GetSquareMagnitude()`, `Normalize()`, `GetNormalized()`,
`GetDotProduct(v)`, `GetCrossProduct(v)`, `+`, `-`, `* float`, `/ float`, `-v`, `==`, `ToString()`.

### `bCVector2` (`x`, `y`), `bCVector4` (`x`, `y`, `z`, `w`)
Fields, `GetMagnitude()`, `Normalize()`. `bCVector2` also has `+`, `-`, `*`.

### `bCQuaternion` (`x`, `y`, `z`, `w`)
A rotation. The default value is "no rotation". `Clear()`, `Normalize()`, `Invert()`, `GetMagnitude()`, `*`, `SetQuaternion(bCMatrix3)`.

### `bCMatrix3`
A 3 × 3 rotation matrix. The default value is the identity matrix. `GetElement(row, col)`, `SetElement(row, col, f)`, `SetIdentity()`,
`Transpose()`, `Invert()`, `* matrix`, `* vector`, `SetToRotationAxisX/Y/Z(radians)`, `SetToRotation(bCQuaternion)`.

### `bCMatrix4` (from version 19)
A 4 × 4 matrix. The default value is the identity matrix. Row vectors (`v' = v * M`): rows 0 to 2 are the axes X, Y, Z. Row 3 is the translation.
`GetElement(row, col)`, `SetElement(row, col, f)`, `SetIdentity()`, `Transpose()`, `Invert()`, `GetInverted()`, `GetDeterminant()`,
`* bCMatrix4`, `* bCVector` (point with translation), `GetTranslation()`, `SetTranslation(v)`, `GetXAxis()`, `GetYAxis()`, `GetZAxis()`,
`SetToTranslation(v)`, `SetToScaling(v)`, `SetToRotation(bCQuaternion)`, `SetToRotationAxisX/Y/Z(radians)`, `GetRotationMatrix()` (as
`bCMatrix3`), `GetRotation()` (as `bCQuaternion`). `GetWorldMatrix()` gives the world matrix of an entity.
