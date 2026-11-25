# Version rules of the script interface

[Deutsch](../../modkit/api-policy.md) | **English**

This page gives the changes that are permitted in the script interface between two versions. It applies to all items in the
[API reference](api-reference.md), in [Events](events.md) and in [Hooks](hooks.md).

## Version number

- The script interface has **one** sequential number: `GetApiVersion()` (`G3ASM_SCRIPT_API_VERSION` in the code). Each version that changes
  the interface increases the number by 1.
- The name of a release is `v0.<API>.<Fix>`, for example `v0.42.0-alpha.1`. The middle number is the script interface. The last number counts
  bug fixes that do not change the interface. `-alpha.N` and `-beta.N` are pre-release versions.
- While the first number is 0, the modkit is not complete. A complete modkit has version 1.0. `-alpha.N`: the scope increases with each version.
  `-beta.N`: the items that exist do not change (see below). New functions come only with a new number.

## Permitted changes in a new version

| Change | Permitted? |
|---|---|
| New functions, types, constants, events, overrides | Yes, at all times |
| A new parameter **at the end**, with a default value | Yes |
| Make the behavior agree with the description (bug fix) | Yes, with a note in the changes |
| Change or remove a function with the label **Experimental** (name, parameters, behavior) | Yes, with a note in the changes |
| Rename a function **without** a label, change its parameters or remove it | No. Use the **Deprecated** procedure (see below) |
| Change the signature of an event or an override | No. Add a **new form** next to the old form. The modkit continues to call the old form. Example: `OnDamage` with and without `owner` |
| Change the sequence or the meaning of override arguments (`SetOverrideArg(index, ...)`) | No |
| Change the values of constants (`QUEST_*`, `STATUS_*`, `QUALITY_*`, `SLOT_*`) | No |

## Beta: the items that exist stay (from version 46)

From **version 46**, the modkit is in the beta. This means:

- All items **without a label** in the [API reference](api-reference.md), in [Events](events.md) and in [Hooks](hooks.md) stay as their
  description tells. A mod that uses only these functions and runs with version 46 also runs with each later version. The table above applies
  to these functions without exceptions.
- New functions come only with a **new number** of the script interface (version 47, 48, ...). In one number, only bug fixes come (for
  example `v0.47.1-beta.1`).
- The only exceptions are the functions with the label **Experimental** (list below). They can still change or be removed. Each change has a
  note in the changes.
- These items are not part of the interface and can change at all times: the text and the quantity of the log lines, the measurement switches
  in the ini (`scriptlog`, `aitrace`, `anispeedlog`, `tradelog`, `memlog` and similar switches, and the dev pipe `[dev] pipe`), the measurement
  mods in `examples/mods` and the text output of `g3asm.exe` ([Command line](cli.md)). The fields of the output with `--json` are part of the
  interface.

## Deprecated functions

1. The function gets the label **Deprecated** in the reference, with the replacement. If there is no replacement yet, the reference tells this.
2. You can still call the function. At the first call in each mod, it writes a line `VERALTET: ...` (deprecated) to the log.
3. The function is removed **five versions** after the label, or later. It is never removed in a bug fix (`v0.N.x`). From version 1.0 of the
   modkit, it is removed only with a new first number (2.0).

| Function | Deprecated since | Replacement |
|---|---|---|
| `Entity::DoDamage` | Version 39, **removed with 46** | `Entity::Kill` (kill), `HurtHitPoints` (decrease only the hit points) |

## Experimental functions

**Experimental** means: the function runs, but I did not fully measure its effect in the game. If you use such a function, read the changes after
each new version. A function loses the label when I measured its behavior in the game and described it in the reference. This is not a change of
the interface.

Experimental in version 46: `HurtHitPoints`, `SetProperty`, `GenerateTradeGoods`, `Override_CombatMove`.

Removed with version 46, because they never worked: `GetPickPosition` (replacement: `PickCenter` and `GetPickPoint`), `ExecuteInfo`, `DumpQuest`.

## Check the version in a mod

```angelscript
void main() {
    if (GetApiVersion() < 37) {
        print("This mod needs script interface 37 (OnTouch). Please update G3ASM.");
        return;
    }
}
```

If a function does not exist in the running version, this is a **compile error**. The modkit then does not load the mod, and the log gives the
line. Thus the check above helps only for behavior that changed between versions. From version 40, the entry `api=37` in the `mod.ini` is
better. The modkit then does not load the mod and reports that the modkit is too old ([Mod package](mod-package.md)).
