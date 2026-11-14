# Referenz: Werte, Namen und Konstanten

Alle Angaben hier sind **geprüft**. Jede Tabelle nennt ihre Quelle:

- **SDK**: Aufzählung aus dem Gothic-3-SDK (`GameEnum.h`), von uns übernommen.
- **gemessen**: im Spiel beobachtet (Protokolle von G3ASM, Stand 2026-11-06).
- **Dump**: mit der Mod `probe_reference` aus dem laufenden Spiel gelesen (Abschnitt 9).

Was hier nicht steht, ist nicht belegt. Im Umlauf befindliche Listen mit Namen wie `Spell_Fireball`, `DamageType_Edge` oder `Strength` stimmen nicht mit dem Spiel überein
(gemessen sind `Spl_Fireball`, `Mis_Fireball`, Schadensart 2 = Klinge und die Attribut-Kennung `"STR"`). Amulette heißen `It_Am_*` (21 Vorlagen), Artefakte `It_Artefact_*`, gewöhnliche Ringe `It_Ring*`; `It_Belt` und `It_Ri_` gibt es nicht. Talente heißen im Spiel `Perk_*` (nicht `It_Perk_*`), Zauber gibt es als `Spl_*`, als Zauber-Item `It_Spell_*` und als Geschoss `Mis_*`; Munition heißt `Arrow` und `Bolt` (nicht `It_Arrow`/`It_Bolt`).

## 1. Schadensarten (`type` in `Override_Damage` und `OnDamage`)

Quelle: SDK `gEDamageType`; die Zuordnung zu Waffen ist gemessen. Die Art wird aus dem **angreifenden Item** gelesen (`gCDamage_PS`), nicht aus dem Opfer.

| Wert | Name | gemessen |
|---|---|---|
| 0 | None | Rückfall: kein Item lesbar (Sturz, Schaden über Zeit, Zauber-Item `It_Spell_*`) |
| 1 | Impact | Keule `It_1H_Club_01`, Tierangriffe (`Fist`) |
| 2 | Blade | Schwerter (`It_1H_OrcSlayer_01`, `It_1H_Broadsword_01`), Hellebarde `It_Halberd_01`, Speer `It_Staff_Spear_01` |
| 3 | Missile | Pfeil `Arrow` (auch das Munitions-Item im Köcher) |
| 4 | Fire | Feuerball `Mis_Fireball` |
| 5 | Ice | nicht gemessen |
| 6 | Lightning | nicht gemessen |
| 7 | Physics | nicht gemessen |

## 2. Statuseffekte (`Entity.HasStatusEffect`, `OnStatusEffect`)

Quelle: SDK `gEStatusEffect` (Bitmaske am `gCNPC_PS`). Konstanten im Skript: `STATUS_*`.

| Bit | Konstante | Bedeutung | gemessen |
|---|---|---|---|
| 1 | `STATUS_BURNING` | brennend | nicht gemessen |
| 2 | `STATUS_FROZEN` | gefroren | nicht gemessen |
| 4 | `STATUS_POISONED` | vergiftet | nicht gemessen |
| 8 | `STATUS_DISEASED` | krank | vom Waran übertragen (zweimal gemessen), das Ereignis kam im selben Moment wie der erste Biss. Danach blieb die Ausdauer `SP` 25 Sekunden lang bei 1 bis 2 (Messung im Kampf, ein Ruhetest fehlt noch) |
| 16 | `STATUS_TRANSFORMED` | verwandelt | nicht gemessen |
| 64 | `STATUS_IN_FOG` | im Nebel | nicht gemessen |
| 128 | `STATUS_GHOST` | Geist | nicht gemessen |
| 256 | `STATUS_IN_MAGIC_BARRIER` | in Magiebarriere | nicht gemessen |
| 512 | `STATUS_SOULTRAVEL` | Seelenreise aktiv | nicht gemessen |

Zustände wie „zu Boden geschlagen“, „besiegt“ oder „tot“ sind **keine** Statuseffekte, sie laufen über KI-Modi (Abschnitt 6).

## 3. Item-Qualität (`Entity.GetSlotQuality`, `SetSlotQuality`)

Quelle: SDK `gEItemQuality` (Bitmaske am Inventarstapel, bei `gCItem_PS` Offset `0x18`, beim Stapel `0x40`). Konstanten im Skript: `QUALITY_*`.

| Bit | Konstante | Anzeige in der Waffenbeschreibung (Textschlüssel) |
|---|---|---|
| 2 | `QUALITY_DISEASED` | keine |
| 4 | `QUALITY_POISONED` | `HUD_Quality_Poison` (Symbol 0) |
| 8 | `QUALITY_BURNING` | keine |
| 16 | `QUALITY_FROZEN` | keine |
| 32 | `QUALITY_SHARP` | `HUD_Quality_Sharp` (Symbol 1) |
| 64 | `QUALITY_BLESSED` | `HUD_Quality_Blessed` (Symbol 2) |
| 128 | `QUALITY_FORGED` | `HUD_Quality_Selfmade` (Symbol 3) |
| 256 | `QUALITY_WORN` | `HUD_Quality_Worn` (Symbol 4) |
| **512** | `QUALITY_LIFESTEAL` | eigenes Bit dieses Modkits; `HUD_Quality_Lifesteal` aus der Mod-Stringtabelle |

Die Anzeige (gemessen im Assembler, `Game.dll+0xA3DF0`) kennt nur die Bits 4, 32, 64, 128 und 256. Die Präfixe der Zeilen sind `(+10)` bei Scharf, Gesegnet und Selbstgeschmiedet, leer bei Vergiftet und Abgenutzt.

## 4. Ausrüstungs-Slots (`Entity.GetSlotQuality(slot)`)

Quelle: SDK `gESlot`. Gemessen: Eine gezogene Waffe liegt in der rechten Hand, ein gezogener Bogen in der linken, weggesteckte Waffen auf dem Rücken, ein weggesteckter Bogen im Slot 7. Der Stapel (und damit die Qualität) wandert mit der Waffe zwischen den Slots.

| Wert | Name | Konstante im Skript |
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
| 12 / 13 | Ring1 / Ring2 | |
| 14 | Ammo | `SLOT_AMMO` |
| 15 | Hair | |
| 16 | Head | |
| 17 | Body | |
| 18 | Helmet | |

## 5. Item-Kategorien und Nutzungsarten

Quelle: SDK `gEItemCategory` und `gEUseType` (Auszug zu Waffen und Ausrüstung).

| Kategorie | Wert |
|---|---|
| None / Weapon / Armor / Artefact / Alchemy | 0 / 1 / 2 / 3 / 4 |
| Misc / Written / Spellbook / Skill | 6 / 7 / 9 / 10 |

| Nutzungsart `gEUseType` | Wert |
|---|---|
| 1H / 2H | 2 / 3 |
| Arrow / Bow / CrossBow / Bolt | 4 / 5 / 6 / 7 |
| Fist / Shield / Armor / Helmet / Staff | 8 / 9 / 10 / 11 / 12 |
| Amulet / Ring / Cast | 13 / 14 / 15 |
| Potion / Plant / Meat / Fruit / Bread | 16 / 17 / 18 / 19 / 20 |
| Halberd / Axe | 51 / 52 |

## 6. KI-Modi

Quelle: SDK `gEAIMode` (`gCScriptRoutine_PS`). Zustände wie „besiegt“ und „tot“ stehen hier.

| Wert | Modus |
|---|---|
| 0 | Routine |
| 1 | Reaction |
| 3 | Combat |
| 4 | Flee |
| 5 | Arena |
| 6 | Talk |
| 8 | Down (zu Boden geschlagen) |
| 9 | Dead |
| 10 | Abandoned |
| 11 | PlayerParty |

## 7. Attribute

Quelle: Doku des Modkits (`api-reference.md`) und Messung. Kennungen sind Zeichenketten wie `"STR"`.

| Kennung | Bedeutung | gemessen |
|---|---|---|
| `HP` | Lebenspunkte | Held und NPCs |
| `SP` | **Ausdauer** | Held: Maximum 150, sinkt im Kampf (150 → 78) und regeneriert danach wieder; bei Krankheit blieb sie im Kampf bei 1 bis 2 |
| `STR`, `DEX`, `INT`, `ALC`, `THF` | Attribute und Fertigkeiten, die Quests vergeben | Dump (Abschnitt 9): von Quests als Belohnung vergeben |
| `SMT` | Schmiedekunst | wird von 13 Quests vergeben (Dump). Das früher dokumentierte `SMI` liefert beim Helden 0; ob es eine gültige Kennung ist, ist nicht geklärt |
| `MP`, `XP`, `LP` | Mana, Erfahrung, Lernpunkte | Held liefert Werte (`MP` 100/100, `XP` 0, `LP` 0 im Dump-Spielstand); Bedeutung nach den Namen angenommen |

Operationen von `gCPlayerMemory_PS::ApplyMod` (SDK `EAttribModOperation`): 0 AddToVal, 1 AddToMax, 2 AddPercentageToVal, 3 AddPercentageToMax, 4 SetVal, 5 SetMax, 6 SetValToPercentage, 7 SetMaxToPercentage, 8 SetValToMax.

## 8. Beispiel: eine Waffe im Detail (`It_1H_OrcSlayer_01`)

Quelle: gemessen mit `ListProperties` (Mod `demo_properties`) im laufenden Spiel am Orkschlächter.

| Eigenschaft | Wert | Bedeutung |
|---|---|---|
| `Item.GoldValue` | 600 | Handelswert |
| `Item.Category` | 1 (`Weapon`) | Kategorie |
| `Item.ReqAttrib1Tag` / `Op` / `Value` | `STR`, `ValGreaterEqual`, 100 | Voraussetzung: Stärke mindestens 100 |
| `Item.RequiredSkill1` | `Perk_1H_1` | Voraussetzung: Talent „Einhandkampf“ der Stufe 1 |
| **`Item.ActivateSkill1`** | **`Perk_OrcSlayer`** | **„Bonustalent“: Solange die Waffe geführt wird, gilt das Talent (in der Beschreibung „Bonustalent Orktöter“)** |
| `Item.ModAttrib1…6` | leer | keine Attributänderung |
| `Damage.DamageType` | 2 (`Blade`) | Klingenschaden |
| `Damage.DamageAmount` | 50 | Grundschaden |
| `Interaction.UseType` | 2 (`1H`) | Einhandwaffe |

`RequiredSkill` ist die Voraussetzung zum Führen, `ActivateSkill` das Talent, das die Waffe dazugibt. Der Wert ist der Name der Talent-Vorlage und ihre ID, `Perk_OrcSlayer` ist ein eigenes Talent (`gCSkill_PS`, Tag `OrcSlayer`, Voraussetzung `STR`), zu dem es das Lern-Item `It_Perk_Orcslayer` gibt.
Im Spiel sind `ReqAttrib2…4Op` in den Eigenschaftsdaten fälschlich als `ReqAttrib1Op` benannt (Fehler des Spiels, steht auch im SDK); `ListProperties` zeigt deshalb mehrfach `ReqAttrib1Op`.

## 9. Namen aus dem Spiel (Dump)

**Wichtig zur Suche:** `FindTemplateNames` findet Namen, die mit dem Suchtext **beginnen**. Die Suche ignoriert außerdem die Groß-/Kleinschreibung. Eine Gruppe wie `Teleport` enthält deshalb `Teleport_Ardea`, aber nicht `Spl_Teleport_Ardea`. Bis Version 28 war die Trefferzahl auf 200 begrenzt (die Gruppe `Spl_` hatte genau 200 Einträge und kann abgeschnitten sein, bis zum nächsten Dump).

Die Mod `probe_reference` (Ziffernblock `/`) schreibt Attribute und Vorlagen-Gruppen ins Protokoll (`g3asm.log`, Zeilen `[probe_reference] …`). Der Text zwischen den Markierungen
wird von `tools/reference_from_log.py` aus dem Protokoll erzeugt und nicht von Hand gepflegt.

<!-- DUMP-ANFANG -->
Quelle: Dump der Mod `probe_reference` aus dem laufenden Spiel, erzeugt mit `tools/reference_from_log.py`. `FindTemplateNames` liefert die meisten Namen mehrfach; unten stehen eindeutige Namen.

### Attribute

Kennungen, die Quests des Spiels als Belohnung vergeben (`DescribeQuestRewards`):

| Kennung | Quests | Beispiel |
|---|---|---|
| `ALC` | 10 | `UrgGosh_BringHerb` (+1) |
| `DEX` | 28 | `Chris_KillDeer` (+1) |
| `INT` | 5 | `Rakus_RescueHim` (+1) |
| `SMT` | 13 | `Javier_BringSmith` (+1) |
| `STR` | 44 | `Curtis_Arena_Goran` (+1) |
| `THF` | 16 | `Ardea_Pocket` (+1) |

Werte des Helden zum Zeitpunkt des Dumps (Wert / Grundwert / Maximum). Das ist **ein Spielstand**, kein Standardwert:

| Kennung | Wert | Grundwert | Maximum |
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

### Vorlagen nach Namensgruppen

| Namensteil | eindeutige Namen | Hinweis |
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
| `It_Ri_` | 0 | keine Vorlage |
| `It_Artefact` | 9 |  |
| `It_Belt` | 0 | keine Vorlage |
| `It_Key` | 10 |  |
| `It_Book` | 9 |  |
| `It_Rune` | 0 | keine Vorlage |
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
<!-- DUMP-ENDE -->

### Gemessene Namen (Protokolle bis 2026-11-06)

| Gruppe | Namen |
|---|---|
| Zauber (`OnSpellCast`) | `Spl_Fireball`, `Spl_Icelance`, `Spl_HealOther`, `Spl_Teleport_Ardea`, `Spl_Teleport_KapDun` |
| Geschosse (`inflictor` bei NPC-Opfern) | `Arrow`, `Mis_Fireball` |
| Zauber-Item in der Hand eines Magiers | `It_Spell_Fireball` |
| Waffen | `It_1H_Club_01`, `It_1H_OrcSlayer_01`, `It_1H_Broadsword_01`, `It_1H_Longsword_01`, `It_Halberd_01`, `It_Staff_Spear_01`, `It_Bow_Ranger` |
| Tierangriffe (`inflictor`) | `Fist` |
| Sonstige Items | `It_Wolf_Fur`, `It_Gold`, `It_Plant_ShapeMush`, `It_Plant_Health_02` |
