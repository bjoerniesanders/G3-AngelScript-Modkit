# Reference: values, names and constants

[Deutsch](../../modkit/reference.md) | **English**

All data on this page is **checked**. Each table gives its source:

- **SDK**: an enumeration from the Gothic 3 SDK (`GameEnum.h`), copied into G3ASM.
- **Measured**: seen in the game (logs of G3ASM).
- **Dump**: read from the running game with the mod `probe_reference` (section 9).

Data that is not on this page is not confirmed. Lists from the internet with names such as `Spell_Fireball`, `DamageType_Edge` or `Strength` do
not agree with the game. The correct names:

| You look for | The name in the game | Not |
|---|---|---|
| Spells | `Spl_*` (spell), `It_Spell_*` (spell item), `Mis_*` (projectile), for example `Spl_Fireball`, `Mis_Fireball` | `Spell_Fireball` |
| Skills | `Perk_*` | `It_Perk_*` |
| Amulets, rings, artifacts | `It_Am_*`, `It_Ring*`, `It_Artefact_*` | `It_Ri_*`, `It_Belt` |
| Ammunition | `Arrow`, `Bolt` | `It_Arrow`, `It_Bolt` |
| Attributes | `"STR"`, `"DEX"`, ... | `Strength` |
| Damage type "blade" | `type` 2 | `DamageType_Edge` |

## 1. Damage types (`type` in `Override_Damage` and `OnDamage`)

Source: SDK `gEDamageType`. The relation to weapons is measured. The modkit reads the type from the **item of the attacker** (`gCDamage_PS`),
not from the victim.

| Value | Name | Measured |
|---|---|---|
| 0 | None | Fallback: no item that the modkit can read (fall, damage over time, spell item `It_Spell_*`) |
| 1 | Impact | Club `It_1H_Club_01`, animal attacks (`Fist`) |
| 2 | Blade | Swords (`It_1H_OrcSlayer_01`, `It_1H_Broadsword_01`), halberd `It_Halberd_01`, spear `It_Staff_Spear_01` |
| 3 | Missile | Arrow `Arrow` (also the ammunition item in the quiver) |
| 4 | Fire | Fireball `Mis_Fireball` |
| 5 | Ice | Not measured |
| 6 | Lightning | Not measured |
| 7 | Physics | Not measured |

## 2. Status effects (`Entity.HasStatusEffect`, `OnStatusEffect`)

Source: SDK `gEStatusEffect` (bit mask on `gCNPC_PS`). Constants in the script: `STATUS_*`.

| Bit | Constant | Meaning | Measured |
|---|---|---|---|
| 1 | `STATUS_BURNING` | Burning | Not measured |
| 2 | `STATUS_FROZEN` | Frozen | Not measured |
| 4 | `STATUS_POISONED` | Poisoned | Not measured |
| 8 | `STATUS_DISEASED` | Diseased | Transmitted by the lizard (measured 2 times). The event came at the same time as the first bite. After this, the stamina `SP` stayed at 1 to 2 for 25 seconds (measured in combat, a test at rest is not done yet) |
| 16 | `STATUS_TRANSFORMED` | Transformed | Not measured |
| 64 | `STATUS_IN_FOG` | In fog | Not measured |
| 128 | `STATUS_GHOST` | Ghost | Not measured |
| 256 | `STATUS_IN_MAGIC_BARRIER` | In a magic barrier | Not measured |
| 512 | `STATUS_SOULTRAVEL` | Soul travel active | Not measured |

Conditions such as "knocked down", "defeated" or "dead" are **not** status effects. They are AI modes (section 6).

## 3. Item quality (`Entity.GetSlotQuality`, `SetSlotQuality`)

Source: SDK `gEItemQuality` (bit mask on the inventory stack, at offset `0x18` in `gCItem_PS`, at offset `0x40` in the stack). Constants in the
script: `QUALITY_*`.

| Bit | Constant | Display in the weapon description (text key) |
|---|---|---|
| 2 | `QUALITY_DISEASED` | None |
| 4 | `QUALITY_POISONED` | `HUD_Quality_Poison` (symbol 0) |
| 8 | `QUALITY_BURNING` | None |
| 16 | `QUALITY_FROZEN` | None |
| 32 | `QUALITY_SHARP` | `HUD_Quality_Sharp` (symbol 1) |
| 64 | `QUALITY_BLESSED` | `HUD_Quality_Blessed` (symbol 2) |
| 128 | `QUALITY_FORGED` | `HUD_Quality_Selfmade` (symbol 3) |
| 256 | `QUALITY_WORN` | `HUD_Quality_Worn` (symbol 4) |
| **512** | `QUALITY_LIFESTEAL` | A bit of this modkit. `HUD_Quality_Lifesteal` from the mod string table |

The display (measured in the assembler code, `Game.dll+0xA3DF0`) knows only the bits 4, 32, 64, 128 and 256. The prefixes of the lines are
`(+10)` for sharp, blessed and self-made, and empty for poisoned and worn.

## 4. Equipment slots (`Entity.GetSlotQuality(slot)`)

Source: SDK `gESlot`. Measured: a drawn weapon is in the right hand. A drawn bow is in the left hand. Weapons that are put away are on the back.
A bow that is put away is in slot 7. The stack (and thus the quality) moves with the weapon between the slots.

| Value | Name | Constant in the script |
|---|---|---|
| 0 | None | |
| 1 | RightHand | `SLOT_RIGHT_HAND` |
| 2 | LeftHand | `SLOT_LEFT_HAND` |
| 3 | EquipLeftHand | |
| 4 | Beard | |
| 5 | BackLeft | `SLOT_BACK_LEFT` |
| 6 | BackRight | `SLOT_BACK_RIGHT` |
| 7 | Bow | `SLOT_BOW` |
| 8 | Crossbow | |
| 9 | Armor | |
| 10 | Belt | |
| 11 | Amulet | |
| 12, 13 | Ring1, Ring2 | |
| 14 | Ammo | `SLOT_AMMO` |
| 15 | Hair | |
| 16 | Head | |
| 17 | Body | |
| 18 | Helmet | |

## 5. Item categories and use types

Source: SDK `gEItemCategory` and `gEUseType` (the part for weapons and equipment).

| Category | Value |
|---|---|
| None, Weapon, Armor, Artefact, Alchemy | 0, 1, 2, 3, 4 |
| Misc, Written, Spellbook, Skill | 6, 7, 9, 10 |

| Use type `gEUseType` | Value |
|---|---|
| 1H, 2H | 2, 3 |
| Arrow, Bow, CrossBow, Bolt | 4, 5, 6, 7 |
| Fist, Shield, Armor, Helmet, Staff | 8, 9, 10, 11, 12 |
| Amulet, Ring, Cast | 13, 14, 15 |
| Potion, Plant, Meat, Fruit, Bread | 16, 17, 18, 19, 20 |
| Halberd, Axe | 51, 52 |

## 6. AI modes

Source: SDK `gEAIMode` (`gCScriptRoutine_PS`). Conditions such as "defeated" and "dead" are here.

| Value | Mode |
|---|---|
| 0 | Routine |
| 1 | Reaction |
| 3 | Combat |
| 4 | Flee |
| 5 | Arena |
| 6 | Talk |
| 8 | Down (knocked down) |
| 9 | Dead |
| 10 | Abandoned |
| 11 | PlayerParty |

## 7. Attributes

Source: the documentation of the modkit (`api-reference.md`) and measurements. Tags are strings such as `"STR"`.

| Tag | Meaning | Measured |
|---|---|---|
| `HP` | Hit points | Hero and NPCs |
| `SP` | **Stamina** | Hero: maximum 150. It decreases in combat (150 → 78) and then regenerates. With a disease, it stayed at 1 to 2 in combat |
| `STR`, `DEX`, `INT`, `ALC`, `THF` | Attributes and abilities that quests give | Dump (section 9): quests give them as a reward |
| `SMT` | Smithing | 13 quests give it (dump). The tag `SMI`, which earlier documentation gave, reads 0 for the hero. It is not known if it is a valid tag |
| `MP`, `XP`, `LP` | Mana, experience, learn points | The hero gives values (`MP` 100/100, `XP` 0, `LP` 0 in the saved game of the dump). The meaning is assumed from the names |

Operations of `gCPlayerMemory_PS::ApplyMod` (SDK `EAttribModOperation`): 0 AddToVal, 1 AddToMax, 2 AddPercentageToVal, 3 AddPercentageToMax,
4 SetVal, 5 SetMax, 6 SetValToPercentage, 7 SetMaxToPercentage, 8 SetValToMax.

## 8. Example: one weapon in detail (`It_1H_OrcSlayer_01`)

Source: measured with `ListProperties` (mod `demo_properties`) in the running game, on the Orkschlächter (orc slayer).

| Property | Value | Meaning |
|---|---|---|
| `Item.GoldValue` | 600 | Trade value |
| `Item.Category` | 1 (`Weapon`) | Category |
| `Item.ReqAttrib1Tag`, `Op`, `Value` | `STR`, `ValGreaterEqual`, 100 | Requirement: strength 100 or more |
| `Item.RequiredSkill1` | `Perk_1H_1` | Requirement: the skill "Einhandkampf" (one-handed combat), level 1 |
| **`Item.ActivateSkill1`** | **`Perk_OrcSlayer`** | **"Bonus skill": while the hero uses the weapon, the skill applies ("Bonustalent Orktöter" in the description)** |
| `Item.ModAttrib1…6` | Empty | No attribute change |
| `Damage.DamageType` | 2 (`Blade`) | Blade damage |
| `Damage.DamageAmount` | 50 | Base damage |
| `Interaction.UseType` | 2 (`1H`) | One-handed weapon |

`RequiredSkill` is the requirement to use the weapon. `ActivateSkill` is the skill that the weapon adds. The value is the name of the skill
template and its ID. `Perk_OrcSlayer` is a separate skill (`gCSkill_PS`, tag `OrcSlayer`, requirement `STR`). There is a learn item
`It_Perk_Orcslayer` for it. In the game, the property data has the incorrect name `ReqAttrib1Op` for `ReqAttrib2…4Op`. This is an error of the
game, and the SDK also has it. Thus `ListProperties` shows `ReqAttrib1Op` more than 1 time.

## 9. Names from the game (dump)

**Important for the search:** `FindTemplateNames` finds names that **start** with the search text. Upper case and lower case have no effect. Thus
the group `Teleport` contains `Teleport_Ardea`, but not `Spl_Teleport_Ardea`. The group `Spl_` comes from an older dump with a maximum of 200
results. It can be incomplete.

The mod `probe_reference` (numeric keypad `/`) writes attributes and template groups to the log (`g3asm.log`, lines `[probe_reference] …`).
`tools/reference_from_log.py` makes the German version of this section from the log. This English copy has the same data.

Source: dump of the mod `probe_reference` from the running game. `FindTemplateNames` gives most names more than 1 time. The lists below give
unique names.

### Attributes

Tags that quests of the game give as a reward (`DescribeQuestRewards`):

| Tag | Quests | Example |
|---|---|---|
| `ALC` | 10 | `UrgGosh_BringHerb` (+1) |
| `DEX` | 28 | `Chris_KillDeer` (+1) |
| `INT` | 5 | `Rakus_RescueHim` (+1) |
| `SMT` | 13 | `Javier_BringSmith` (+1) |
| `STR` | 44 | `Curtis_Arena_Goran` (+1) |
| `THF` | 16 | `Ardea_Pocket` (+1) |

Values of the hero at the time of the dump (value, base value, maximum). This is **one saved game**, not a default value:

| Tag | Value | Base value | Maximum |
|---|---|---|---|
| `STR` | 120 | 120 | 120 |
| `DEX` | 102 | 102 | 102 |
| `INT` | 0 | 0 | 0 |
| `HP` | 176 | 176 | 200 |
| `MP` | 100 | 100 | 100 |
| `SP` | 150 | 150 | 150 |
| `XP` | 0 | 0 | 0 |
| `LP` | 0 | 0 | 0 |
| `ALC` | 13 | 13 | 13 |
| `SMI` | 0 | 0 | 0 |
| `THF` | 11 | 11 | 11 |

### Templates by name group

| Name part | Unique names | Note |
|---|---|---|
| `It_1H_` | 31 |  |
| `It_2H_` | 11 |  |
| `It_Staff_` | 21 |  |
| `It_Bow_` | 20 |  |
| `It_Crossbow` | 12 |  |
| `Arrow` | 1 |  |
| `Bolt` | 2 |  |
| `It_Armor_` | 26 |  |
| `It_Helmet` | 11 |  |
| `It_Shield` | 14 |  |
| `It_Spell` | 40 |  |
| `Mis_` | 4 |  |
| `Spl_` | 101 |  |
| `Perk` | 72 |  |
| `It_Scroll` | 43 |  |
| `Teleport` | 31 |  |
| `It_Potion` | 32 |  |
| `It_Plant_` | 54 |  |
| `It_Ring` | 13 |  |
| `It_Am_` | 21 |  |
| `It_Ri_` | 0 | No template |
| `It_Artefact` | 9 |  |
| `It_Belt` | 0 | No template |
| `It_Key` | 10 |  |
| `It_Book` | 9 |  |
| `It_Rune` | 0 | No template |
| `It_Recipe` | 61 |  |
| `It_Gold` | 7 |  |

**`It_1H_`** (31)

`It_1H_AncestorSword_01`, `It_1H_AssDagger_01`, `It_1H_AssSword_01`, `It_1H_BarbarianSword_01`, `It_1H_Bastardsword_01`, `It_1H_Broadsword_01`, `It_1H_Club_01`, `It_1H_Crescent_01`, `It_1H_ElBastardo_01`, `It_1H_Epee_01`, `It_1H_FingersRapier_01`, `It_1H_FlameSword_01`, `It_1H_GreatSword_01`, `It_1H_Iceblade_01`, `It_1H_Ingvar_Sword`, `It_1H_Inquisitor_01`, `It_1H_Katana_01`, `It_1H_Longsword_01`, `It_1H_MagicFlameSword_01`, `It_1H_MasterSword_01`, `It_1H_MiloksFirst_Sword_01`, `It_1H_Moonblade_01`, `It_1H_OrcSlayer_01`, `It_1H_PaladinSword_01`, `It_1H_Rubyblade_01`, `It_1H_Runesword_01`, `It_1H_Scimitar_01`, `It_1H_Sword_01`, `It_1H_Sword_Rhobar_01`, `It_1H_Sword_Rust_01`, `It_1H_Wenzel_Sword_01`

**`It_2H_`** (11)

`It_2H_AncestorMight_01`, `It_2H_Axe_Lumberjack`, `It_2H_Bak-Arash_Staff`, `It_2H_DemonSword_01`, `It_2H_Executioner_01`, `It_2H_GreatInquisitor_01`, `It_2H_InnosWrath_01`, `It_2H_OrderSword_01`, `It_2H_Sword_01`, `It_2H_Sword_Ore_01`, `It_2H_Sword_Rust_01`

**`It_Staff_`** (21)

`It_Staff_BladeStaff_01`, `It_Staff_Darkmage_01`, `It_Staff_Darkmage_02`, `It_Staff_Darkmage_03`, `It_Staff_Druid_01`, `It_Staff_Firemage_01`, `It_Staff_Firemage_02`, `It_Staff_Firemage_03`, `It_Staff_JudgementStaff_01`, `It_Staff_Mana_01`, `It_Staff_Mana_02`, `It_Staff_Mana_03`, `It_Staff_Prot_01`, `It_Staff_Prot_02`, `It_Staff_Prot_03`, `It_Staff_Quarterstaff_01`, `It_Staff_Scythe_01`, `It_Staff_Spear_01`, `It_Staff_Watermage_01`, `It_Staff_Watermage_02`, `It_Staff_Watermage_03`

**`It_Bow_`** (20)

`It_Bow_01`, `It_Bow_Army`, `It_Bow_Ass`, `It_Bow_Bone`, `It_Bow_Composite`, `It_Bow_Deathbringer`, `It_Bow_Demon`, `It_Bow_Fight`, `It_Bow_Horn`, `It_Bow_Hunting`, `It_Bow_Long`, `It_Bow_Masterbow`, `It_Bow_Nimroder`, `It_Bow_Oak`, `It_Bow_OrcRipper`, `It_Bow_Ranger`, `It_Bow_Rune`, `It_Bow_SilentDeath`, `It_Bow_War`, `It_Bow_WolfRipper`

**`It_Crossbow`** (12)

`It_Crossbow_01`, `It_Crossbow_BloodHarvest`, `It_Crossbow_Bounty`, `It_Crossbow_Heavy`, `It_Crossbow_KrashMorra`, `It_Crossbow_Merc`, `It_Crossbow_Nrd`, `It_Crossbow_Orc`, `It_Crossbow_Penetration`, `It_Crossbow_Rhino`, `It_Crossbow_SlaveKiller`, `It_Crossbow_War`

**`Arrow`** (1)

`Arrow`

**`Bolt`** (2)

`Bolt`, `Bolt_Sharp`

**`It_Armor_`** (26)

`It_Armor_Ass_H`, `It_Armor_Ass_L`, `It_Armor_Ass_M`, `It_Armor_Druid`, `It_Armor_KDF`, `It_Armor_KDW`, `It_Armor_Leather`, `It_Armor_Merc_H`, `It_Armor_Merc_L`, `It_Armor_Merc_M`, `It_Armor_Nom_H`, `It_Armor_Nom_L`, `It_Armor_Nom_M`, `It_Armor_Nrd_H`, `It_Armor_Nrd_L`, `It_Armor_Nrd_M`, `It_Armor_Paladin`, `It_Armor_Peasant`, `It_Armor_Ranger`, `It_Armor_Reb_H`, `It_Armor_Reb_L`, `It_Armor_Reb_M`, `It_Armor_Rhobar`, `It_Armor_RoyalGuard`, `It_Armor_Xardas`, `It_Armor_Zuben`

**`It_Helmet`** (11)

`It_Helmet_Crown_01`, `It_Helmet_Guard_01`, `It_Helmet_HeavyTurban_01`, `It_Helmet_Horn_01`, `It_Helmet_Leather_01`, `It_Helmet_Leather_02`, `It_Helmet_LightHorn_01`, `It_Helmet_LightLeather_01`, `It_Helmet_LightMerc_01`, `It_Helmet_Merc_01`, `It_Helmet_Turban_01`

**`It_Shield`** (14)

`It_Shield_Knight_01`, `It_Shield_Leather_01`, `It_Shield_Paladin_01`, `It_Shield_Rhobar_01`, `It_Shield_Round_01`, `It_Shield_Rune_01`, `It_Shield_Skull_01`, `It_Shield_Wood_01`, `It_Shield_Wood_02`, `It_Shield_Wood_03`, `It_Shield_Wood_04`, `It_Shield_Wood_05`, `It_Shield_Wood_06`, `It_Shield_Wood_07`

**`It_Spell`** (40)

`It_Spell_AnimalForm`, `It_Spell_ArmyOfDarkness`, `It_Spell_BlessWeapon`, `It_Spell_Bloodlust`, `It_Spell_CallAnimals`, `It_Spell_Charm`, `It_Spell_CureDisease`, `It_Spell_CurePoison`, `It_Spell_DestroyEvil`, `It_Spell_Disease`, `It_Spell_DuskToDawn`, `It_Spell_Fear`, `It_Spell_Fireball`, `It_Spell_Firerain`, `It_Spell_Flamesword`, `It_Spell_FrostWave`, `It_Spell_Hailstorm`, `It_Spell_HealOther`, `It_Spell_Healing`, `It_Spell_HeatWave`, `It_Spell_IceBomb`, `It_Spell_Icelance`, `It_Spell_Light`, `It_Spell_Lightningbolt`, `It_Spell_Meteor`, `It_Spell_Mist`, `It_Spell_OpenLocks`, `It_Spell_Poison`, `It_Spell_Sleep`, `It_Spell_SoulTravel`, `It_Spell_SummonDemon`, `It_Spell_SummonGoblin`, `It_Spell_SummonGolem`, `It_Spell_SummonLightning`, `It_Spell_SummonMonster`, `It_Spell_SummonSkeleton`, `It_Spell_TameAnimal`, `It_Spell_Telekinesis`, `It_Spell_TimeBubble`, `It_Spell_WordOfControl`

**`Mis_`** (4)

`Mis_DestroyEvil`, `Mis_Fireball`, `Mis_Icelance`, `Mis_Lightningbolt`

**`Spl_`** (101)

`Spl_AnimalForm`, `Spl_BanishVermin`, `Spl_BlessWeapon`, `Spl_Bloodlust`, `Spl_CallAnimals`, `Spl_Charm`, `Spl_Cure`, `Spl_CureDisease`, `Spl_CurePoison`, `Spl_DestroyEvil`, `Spl_Disease`, `Spl_DuskToDawn`, `Spl_Fear`, `Spl_Fireball`, `Spl_Firerain`, `Spl_Flamesword`, `Spl_FrostWave`, `Spl_Hailstorm`, `Spl_HealOther`, `Spl_HeatWave`, `Spl_Hypnosis`, `Spl_IceBomb`, `Spl_Icelance`, `Spl_Light`, `Spl_Lightningbolt`, `Spl_Meteor`, `Spl_OpenLocks`, `Spl_Poison`, `Spl_Refresh`, `Spl_Resurrect`, `Spl_ShockWave`, `Spl_Sleep`, `Spl_SoulTravel`, `Spl_SpiritLink`, `Spl_SummonArmyOfCuteness`, `Spl_SummonArmyOfDarkness`, `Spl_SummonDemon`, `Spl_SummonGoblin`, `Spl_SummonGolem`, `Spl_SummonImp`, `Spl_SummonLightning`, `Spl_SummonMist`, `Spl_SummonMonster`, `Spl_SummonSkeleton`, `Spl_TameAnimal`, `Spl_Telekinesis`, `Spl_Teleport_AlShedim`, `Spl_Teleport_Ardea`, `Spl_Teleport_Bakaresh`, `Spl_Teleport_Beliar`, `Spl_Teleport_BenErai`, `Spl_Teleport_BenSala`, `Spl_Teleport_Braga`, `Spl_Teleport_Faring`, `Spl_Teleport_FireClan`, `Spl_Teleport_Geldern`, `Spl_Teleport_Gotha`, `Spl_Teleport_Guru`, `Spl_Teleport_HammerClan`, `Spl_Teleport_Innos`, `Spl_Teleport_Ishtar`, `Spl_Teleport_KapDun`, `Spl_Teleport_King`, `Spl_Teleport_Lago`, `Spl_Teleport_LeaveVangard`, `Spl_Teleport_Monastery`, `Spl_Teleport_Montera`, `Spl_Teleport_MoraSul`, `Spl_Teleport_Nemora`, `Spl_Teleport_Okara`, `Spl_Teleport_Reddock`, `Spl_Teleport_Silden`, `Spl_Teleport_Trelis`, `Spl_Teleport_VangardTempel`, `Spl_Teleport_WolfClan`, `Spl_Teleport_Xardas`, `Spl_Teleport_Zuben`, `Spl_Template`, `Spl_Template_Instant`, `Spl_Template_Projectile`, `Spl_Template_Summon`, `Spl_TimeBubble`, `Spl_Transform_Alligator`, `Spl_Transform_Bison`, `Spl_Transform_Bloodfly`, `Spl_Transform_Boar`, `Spl_Transform_Firewaran`, `Spl_Transform_Lion`, `Spl_Transform_Lurker`, `Spl_Transform_Meatbug`, `Spl_Transform_Minecrawler`, `Spl_Transform_Rhino`, `Spl_Transform_Ripper`, `Spl_Transform_Sabertooth`, `Spl_Transform_Scavenger`, `Spl_Transform_Shadowbeast`, `Spl_Transform_Snapper`, `Spl_Transform_SwampLurker`, `Spl_Transform_Waran`, `Spl_Transform_Wolf`, `Spl_WordOfControl`

**`Perk`** (72)

`Perk_1H1H_1`, `Perk_1H1H_2`, `Perk_1H1H_SecretBonus`, `Perk_1H_1`, `Perk_1H_2`, `Perk_1H_3`, `Perk_Acrobatic`, `Perk_Assassin`, `Perk_Axe_1`, `Perk_Axe_2`, `Perk_Axe_3`, `Perk_Barter`, `Perk_Bow_1`, `Perk_Bow_2`, `Perk_Bow_3`, `Perk_Crossbow_1`, `Perk_Crossbow_2`, `Perk_Crossbow_3`, `Perk_Darkmage`, `Perk_Druid`, `Perk_Excuse`, `Perk_ExplosiveArrow`, `Perk_FireArrow`, `Perk_Firemage`, `Perk_HeavyArmor`, `Perk_Hunter_1`, `Perk_Hunter_2`, `Perk_ImmuneToDisease`, `Perk_ImmuneToPoison`, `Perk_KnockOut`, `Perk_Learn`, `Perk_LightArmor`, `Perk_LockPick_1`, `Perk_LockPick_2`, `Perk_LockPick_3`, `Perk_MasterGladiator`, `Perk_MasterMage`, `Perk_MasterThief`, `Perk_Mining`, `Perk_OrcHunter`, `Perk_OrcSlayer`, `Perk_Paladin`, `Perk_PickPocket_1`, `Perk_PickPocket_2`, `Perk_PickPocket_3`, `Perk_PoisonArrow`, `Perk_PoisonBlade`, `Perk_PotionHP`, `Perk_PotionMP`, `Perk_PotionPerm`, `Perk_PotionPoison`, `Perk_PotionTransform`, `Perk_ResistCold`, `Perk_ResistHeat`, `Perk_SharpArrow`, `Perk_SharpenBlade`, `Perk_Shield_1`, `Perk_Shield_2`, `Perk_Smith_1`, `Perk_Smith_2`, `Perk_Smith_3`, `Perk_Sneak_1`, `Perk_Sneak_2`, `Perk_Sprinter`, `Perk_StaffMagic`, `Perk_Staff_1`, `Perk_Staff_2`, `Perk_Staff_3`, `Perk_TrophyFur`, `Perk_TrophySkin`, `Perk_TrophyTeeth`, `Perk_Watermage`

**`It_Scroll`** (43)

`It_Scroll`, `It_Scroll_ALC_01`, `It_Scroll_AnimalForm`, `It_Scroll_ArmyOfCuteness`, `It_Scroll_ArmyOfDarkness`, `It_Scroll_BlessWeapon`, `It_Scroll_Bloodlust`, `It_Scroll_CallAnimals`, `It_Scroll_Charm`, `It_Scroll_CureDisease`, `It_Scroll_CurePoison`, `It_Scroll_DestroyEvil`, `It_Scroll_Disease`, `It_Scroll_DuskToDawn`, `It_Scroll_Fear`, `It_Scroll_Fireball`, `It_Scroll_Firerain`, `It_Scroll_Flamesword`, `It_Scroll_FrostWave`, `It_Scroll_Hailstorm`, `It_Scroll_HealOther`, `It_Scroll_Healing`, `It_Scroll_HeatWave`, `It_Scroll_Hypnosis`, `It_Scroll_INT_01`, `It_Scroll_Icelance`, `It_Scroll_Light`, `It_Scroll_Lightningbolt`, `It_Scroll_Meteor`, `It_Scroll_Mist`, `It_Scroll_OpenLocks`, `It_Scroll_Poison`, `It_Scroll_Sleep`, `It_Scroll_SoulTravel`, `It_Scroll_SummonDemon`, `It_Scroll_SummonGoblin`, `It_Scroll_SummonGolem`, `It_Scroll_SummonLightning`, `It_Scroll_SummonSkeleton`, `It_Scroll_TameAnimal`, `It_Scroll_Telekinesis`, `It_Scroll_TimeBubble`, `It_Scroll_WordOfControl`

**`Teleport`** (31)

`Teleport_AlShedim`, `Teleport_Ardea`, `Teleport_Bakaresh`, `Teleport_Beliar`, `Teleport_BenErai`, `Teleport_BenSala`, `Teleport_Braga`, `Teleport_Faring`, `Teleport_FireClan`, `Teleport_Geldern`, `Teleport_Gotha`, `Teleport_Guru`, `Teleport_HammerClan`, `Teleport_Innos`, `Teleport_Ishtar`, `Teleport_KapDun`, `Teleport_King`, `Teleport_Lago`, `Teleport_LeaveVangard`, `Teleport_Monastery`, `Teleport_Montera`, `Teleport_MoraSul`, `Teleport_Nemora`, `Teleport_Okara`, `Teleport_Reddock`, `Teleport_Silden`, `Teleport_Trelis`, `Teleport_VangardTempel`, `Teleport_WolfClan`, `Teleport_Xardas`, `Teleport_Zuben`

**`It_Potion`** (32)

`It_Potion`, `It_Potion_Alligator`, `It_Potion_Bison`, `It_Potion_Bloodfly`, `It_Potion_Boar`, `It_Potion_CureDisease`, `It_Potion_CurePoison`, `It_Potion_DEX`, `It_Potion_Deadly`, `It_Potion_Empty`, `It_Potion_Firewaran`, `It_Potion_Godspower`, `It_Potion_HPMax`, `It_Potion_Health`, `It_Potion_Lion`, `It_Potion_Lurker`, `It_Potion_MC`, `It_Potion_MPMax`, `It_Potion_Mana`, `It_Potion_Meatbug`, `It_Potion_Poison`, `It_Potion_Rhino`, `It_Potion_SPMax`, `It_Potion_STR`, `It_Potion_Sabertooth`, `It_Potion_Scavenger`, `It_Potion_Shadowbeast`, `It_Potion_Snapper`, `It_Potion_Stamina`, `It_Potion_SwampLurker`, `It_Potion_Waran`, `It_Potion_Wolf`

**`It_Plant_`** (54)

`It_Plant_Adanosboost`, `It_Plant_Alligator`, `It_Plant_Beliarboost`, `It_Plant_Booze`, `It_Plant_Booze_Snow`, `It_Plant_Cactus`, `It_Plant_DEX`, `It_Plant_DEX_Snow`, `It_Plant_DemonMush`, `It_Plant_DemonMush_Snow`, `It_Plant_DruidStone`, `It_Plant_Health`, `It_Plant_HealthPerm`, `It_Plant_HealthPerm_Snow`, `It_Plant_Health_02`, `It_Plant_Health_02_Snow`, `It_Plant_Health_03`, `It_Plant_Health_03_Snow`, `It_Plant_Health_04`, `It_Plant_Health_04_Snow`, `It_Plant_Health_Snow`, `It_Plant_Innosboost`, `It_Plant_Lion`, `It_Plant_Mana`, `It_Plant_ManaPerm`, `It_Plant_ManaPerm_Snow`, `It_Plant_Mana_02`, `It_Plant_Mana_02_Snow`, `It_Plant_Mana_03`, `It_Plant_Mana_03_Snow`, `It_Plant_Mana_04`, `It_Plant_Mana_04_Snow`, `It_Plant_Mana_Snow`, `It_Plant_Perm_01`, `It_Plant_Perm_01_Snow`, `It_Plant_STAPerm`, `It_Plant_STAPerm_Snow`, `It_Plant_STR`, `It_Plant_STR_Snow`, `It_Plant_ShapeMush`, `It_Plant_ShapeMush_Snow`, `It_Plant_Snapper`, `It_Plant_Stamina_01`, `It_Plant_Stamina_01_Snow`, `It_Plant_Stamina_02`, `It_Plant_Stamina_02_Snow`, `It_Plant_StoneNettle`, `It_Plant_StoneNettle_Snow`, `It_Plant_SwampWeed`, `It_Plant_ToadMush`, `It_Plant_ToadMush_Snow`, `It_Plant_Trelott`, `It_Plant_Wolf`, `It_Plant_Wolf_Snow`

**`It_Ring`** (13)

`It_Ring_DEX_01`, `It_Ring_HP_01`, `It_Ring_MP_01`, `It_Ring_Prot_Combat_01`, `It_Ring_Prot_Edge_01`, `It_Ring_Prot_Edge_02`, `It_Ring_Prot_Fire_01`, `It_Ring_Prot_Ice_01`, `It_Ring_Prot_Lightning_01`, `It_Ring_Prot_Magic_01`, `It_Ring_Prot_Missile_01`, `It_Ring_SP_01`, `It_Ring_STR_01`

**`It_Am_`** (21)

`It_Am_Assassin_01`, `It_Am_DEX_01`, `It_Am_Druid_01`, `It_Am_HPBoost_01`, `It_Am_HP_01`, `It_Am_Hunter_01`, `It_Am_MP_01`, `It_Am_Mage_01`, `It_Am_Paladin_01`, `It_Am_Prot_Edge_01`, `It_Am_Prot_Edge_02`, `It_Am_Prot_Edge_03`, `It_Am_Prot_Edge_04`, `It_Am_Prot_Fire_01`, `It_Am_Prot_Ice_01`, `It_Am_Prot_Lightning_01`, `It_Am_Prot_Missile_01`, `It_Am_SP_01`, `It_Am_STR_01`, `It_Am_Thief_01`, `It_Am_Warrior_01`

**`It_Artefact`** (9)

`It_Artefact_Abbas`, `It_Artefact_Amulet`, `It_Artefact_Crown`, `It_Artefact_GeldernShadowBeastRuin`, `It_Artefact_Helmet_Geldern`, `It_Artefact_Ring_01`, `It_Artefact_Ring_02`, `It_Artefact_Ring_Geldern`, `It_Artefact_Robe`

**`It_Key`** (10)

`It_Key`, `It_Key_Ali`, `It_Key_CPT_01`, `It_Key_CPT_02`, `It_Key_CPT_03`, `It_Key_Fabio`, `It_Key_Geldern`, `It_Key_Lockpick`, `It_Key_Sigmor`, `It_Key_Tjalf`

**`It_Book`** (9)

`It_Book`, `It_Book_ALC_01`, `It_Book_Arrow`, `It_Book_CPT_01`, `It_Book_CPT_02`, `It_Book_CPT_03`, `It_Book_CPT_04`, `It_Book_CPT_Teleport`, `It_Book_INT_01`

**`It_Recipe`** (61)

`It_Recipe_2Handed`, `It_Recipe_2HandedOre`, `It_Recipe_Alligator`, `It_Recipe_AncestorMight`, `It_Recipe_AncestorSword`, `It_Recipe_Bastardsword`, `It_Recipe_Bison`, `It_Recipe_Bloodfly`, `It_Recipe_BluntArrow`, `It_Recipe_Boar`, `It_Recipe_Booze_1`, `It_Recipe_Booze_2`, `It_Recipe_Booze_Nrd`, `It_Recipe_Broadsword`, `It_Recipe_CureDisease`, `It_Recipe_CurePoison`, `It_Recipe_DEX`, `It_Recipe_Deadly`, `It_Recipe_ElBastardo`, `It_Recipe_ExplosiveArrow`, `It_Recipe_FireArrow`, `It_Recipe_Firewaran`, `It_Recipe_HPMax`, `It_Recipe_Health_1`, `It_Recipe_Health_2`, `It_Recipe_Health_3`, `It_Recipe_Health_4`, `It_Recipe_Lion`, `It_Recipe_Longsword`, `It_Recipe_Lurker`, `It_Recipe_MC`, `It_Recipe_MPMax`, `It_Recipe_Mana_1`, `It_Recipe_Mana_2`, `It_Recipe_Mana_3`, `It_Recipe_Mana_4`, `It_Recipe_MasterSword`, `It_Recipe_Meatbug`, `It_Recipe_Meatbugragout`, `It_Recipe_NrdSword`, `It_Recipe_PoisonArrow`, `It_Recipe_Poison_1`, `It_Recipe_Poison_2`, `It_Recipe_Poison_3`, `It_Recipe_Rhino`, `It_Recipe_Rubyblade`, `It_Recipe_Runesword`, `It_Recipe_SPMax`, `It_Recipe_STR`, `It_Recipe_Sabertooth`, `It_Recipe_Scavenger`, `It_Recipe_Shadowbeast`, `It_Recipe_SharpArrow`, `It_Recipe_Snapper`, `It_Recipe_Stamina_1`, `It_Recipe_Stamina_2`, `It_Recipe_Stew`, `It_Recipe_Stollengrollen`, `It_Recipe_SwampLurker`, `It_Recipe_Waran`, `It_Recipe_Wolf`

**`It_Gold`** (7)

`It_Gold`, `It_GoldChalice_Lares`, `It_GoldNugget`, `It_GoldRing`, `It_Gold_Stack_01`, `It_Gold_Stack_02`, `It_Gold_Stack_03`

### Measured names (from logs)

| Group | Names |
|---|---|
| Spells (`OnSpellCast`) | `Spl_Fireball`, `Spl_Icelance`, `Spl_HealOther`, `Spl_Teleport_Ardea`, `Spl_Teleport_KapDun` |
| Projectiles (`inflictor` for NPC victims) | `Arrow`, `Mis_Fireball` |
| Spell item in the hand of a mage | `It_Spell_Fireball` |
| Weapons | `It_1H_Club_01`, `It_1H_OrcSlayer_01`, `It_1H_Broadsword_01`, `It_1H_Longsword_01`, `It_Halberd_01`, `It_Staff_Spear_01`, `It_Bow_Ranger` |
| Animal attacks (`inflictor`) | `Fist` |
| Other items | `It_Wolf_Fur`, `It_Gold`, `It_Plant_ShapeMush`, `It_Plant_Health_02` |
