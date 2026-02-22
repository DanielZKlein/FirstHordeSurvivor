# Upgrade System

## Overview

Pick-one-of-three upgrade selection on level-up. The system supports player stat boosts, global/per-weapon stat boosts, new weapon grants, prerequisites, level-gating, weighted random selection, and stackable upgrades.

**Status:** Core logic implemented, UI panel scaffolded (needs Blueprint subclass for visuals).

## Architecture

```
UpgradeTypes.h            # Enums: EUpgradeType, EPlayerStat, EWeaponStat, legacy FUpgradeDefinition
UpgradeEffect.h/cpp       # FUpgradeEffect struct - single stat modifier (additive + multiplicative)
UpgradeDataAsset.h/cpp    # UUpgradeDataAsset - defines one upgrade (identity, effects, rules)
UpgradeTableRow.h         # FUpgradeTableRow - DataTable row referencing an UpgradeDataAsset
UpgradeSubsystem.h/cpp    # UUpgradeSubsystem - WorldSubsystem managing pool, selection, application
UpgradePanelWidget.h/cpp  # UUpgradePanelWidget - C++ base for upgrade selection UI
```

## Flow: Level-Up to Upgrade Applied

```
1. Enemy dies → drops XP gem
2. Player collects gem → ASurvivorCharacter::AddXP()
3. XP >= threshold → AddXP calls UUpgradeSubsystem::TriggerUpgradeSelection()
4. TriggerUpgradeSelection:
   a. Increments CurrentPlayerLevel
   b. Calls GetRandomUpgradeChoices(3) to pick 3 weighted random upgrades
   c. Broadcasts OnShowUpgradeSelection delegate with the choices
5. UI (UUpgradePanelWidget or Blueprint listener) shows the 3 options
6. Player clicks one → UUpgradePanelWidget::OnOptionSelected(index)
7. Widget broadcasts OnUpgradeSelected delegate
8. Listener calls UUpgradeSubsystem::ApplyUpgrade(SelectedUpgrade)
9. ApplyUpgrade:
   a. Increments stack count for this UpgradeID
   b. If NewWeapon: tells character to spawn it via AddWeapon()
   c. Applies each FUpgradeEffect (PlayerStat → AttributeComponent, WeaponStat → weapon actor)
   d. If per-weapon upgrade: increments that weapon's level
   e. Broadcasts OnUpgradeApplied
```

## Key Classes

### FUpgradeEffect (Struct)
**File:** `UpgradeEffect.h/cpp`

A single stat modification. Upgrades can have multiple effects, enabling mixed bonuses/penalties (e.g. Glass Cannon: +30% Damage, -20% MaxHealth).

| Field | Type | Purpose |
|-------|------|---------|
| Type | EUpgradeType | PlayerStat or WeaponStat |
| PlayerStat | EPlayerStat | Which player attribute (if Type == PlayerStat) |
| WeaponStat | EWeaponStat | Which weapon stat (if Type == WeaponStat) |
| AdditiveBonus | float | Flat bonus (can be negative) |
| MultiplicativeBonus | float | Multiplier (0.8 = -20%, 1.1 = +10%) |

`GetEffectDescription()` returns formatted UI text like "+30% Damage" or "-20 Max Health".

### UUpgradeDataAsset (DataAsset)
**File:** `UpgradeDataAsset.h/cpp`

Defines a single upgrade that can appear in the selection pool.

**Identity:**
- `UpgradeID` (FName) - unique key for tracking stacks and prerequisites
- `DisplayName`, `Description`, `Icon` - UI display

**Effects:**
- `Effects` (TArray\<FUpgradeEffect\>) - all stat modifications this upgrade applies

**Weapon Targeting:**
- `WeaponToGrant` - for NewWeapon upgrades, which weapon to give the player
- `TargetWeapon` - if set, weapon stat effects only apply to this weapon; if null, they apply globally to all weapons that use the stat
- `bRequiresWeaponOwnership` - only show this upgrade if the player already owns TargetWeapon

**Selection Rules:**
- `Weight` (int32, default 100) - higher = more likely to appear in selection
- `MaxStacks` (int32, default 5) - how many times this upgrade can be picked per run
- `MinLevel` (int32, default 1) - player must be at least this level
- `RequiredUpgrades` (TArray\<FName\>) - prerequisite upgrade IDs

**Helpers:**
- `IsNewWeaponUpgrade()` - true if WeaponToGrant is set
- `IsPerWeaponUpgrade()` - true if TargetWeapon is set
- `GetEffectiveIcon()` - falls back to weapon icon if no upgrade icon
- `GetCombinedEffectsDescription()` - joins all effect descriptions with newlines
- `GetAffectedWeaponStats()` - list of weapon stats this upgrade touches

### FUpgradeTableRow (DataTable Row)
**File:** `UpgradeTableRow.h`

Simple registry row: references a `UUpgradeDataAsset` plus a `bEnabled` toggle for quick enable/disable without removing the row.

### UUpgradeSubsystem (WorldSubsystem)
**File:** `UpgradeSubsystem.h/cpp`

Central manager for the upgrade system. Owns the upgrade pool, tracks state, handles selection and application.

**Registration (called on BeginPlay):**
- `RegisterUpgradeTable(DataTable)` - called by GameMode, caches all enabled upgrades
- `RegisterPlayer(Character)` - called by SurvivorCharacter
- `RegisterWeapon(WeaponData, WeaponActor)` - called when player gets a new weapon

**Selection:**
- `GetRandomUpgradeChoices(Count=3)` - weighted random selection without replacement from the available pool
- `IsUpgradeAvailable(Upgrade)` - checks all conditions: not maxed, meets level, meets prerequisites, meets weapon requirements, has weapon slot (for NewWeapon)

**Availability Checks:**
| Check | Method | Logic |
|-------|--------|-------|
| Stack cap | `IsNotMaxedOut()` | CurrentStacks < MaxStacks |
| Level gate | `MeetsLevelRequirement()` | CurrentPlayerLevel >= MinLevel |
| Prerequisites | `MeetsPrerequisites()` | All RequiredUpgrades owned |
| Weapon ownership | `MeetsWeaponRequirement()` | If bRequiresWeaponOwnership, must own TargetWeapon; if NewWeapon, must NOT already own it |
| Weapon slots | `HasWeaponSlotAvailable()` | WeaponCount < 4 (for NewWeapon upgrades only) |

**Application:**
- `ApplyUpgrade(Upgrade)` - increments stacks, applies all effects, broadcasts
- `ApplyPlayerStatEffect()` - maps EPlayerStat to FGameplayAttribute on AttributeComponent, calls ApplyAdditive/ApplyMultiplicative
- `ApplyWeaponStatEffect()` - if TargetWeapon set, applies to that weapon only; otherwise applies to all weapons using that stat
- `ApplyNewWeaponUpgrade()` - calls `ASurvivorCharacter::AddWeapon()`

**State Tracking:**
- `OwnedUpgradeStacks` (TMap\<FName, int32\>) - how many times each upgrade was taken
- `WeaponStates` (TMap\<FName, FWeaponUpgradeState\>) - owned weapons and their levels
- `CurrentPlayerLevel` - incremented each time TriggerUpgradeSelection is called

**Delegates:**
- `OnShowUpgradeSelection` - fired with the array of choices, for UI to listen to
- `OnUpgradeApplied` - fired after an upgrade is applied

### UUpgradePanelWidget (UMG Widget)
**File:** `UpgradePanelWidget.h/cpp`

C++ base class for the upgrade selection UI. Starts collapsed, shows on `ShowUpgradeChoices()`.

**C++ handles:**
- Storing choices, showing/hiding, dispatching selection

**Blueprint subclass (WBP_UpgradePanel) must implement:**
- `BP_PopulateOptions(Choices)` - create visual option widgets (buttons, text, icons)
- `BP_OnPanelShown()` / `BP_OnPanelHidden()` - animations, input mode changes, pause

**Blueprint calls back into C++:**
- `OnOptionSelected(Index)` - triggers OnUpgradeSelected delegate and closes panel

## Stat Enums

### EPlayerStat
MaxHealth, HealthRegen, MaxSpeed, MaxAcceleration, MovementControl, Armor, Impact

### EWeaponStat
Damage, AttackSpeed, Area, Penetration, ProjectileSpeed, ProjectileCount, Duration, Range, Knockback

### EUpgradeType
PlayerStat, WeaponStat, NewWeapon, WeaponEvolution (future)

## Modifier Math

Both player attributes and weapon stats use the same `FGameplayAttribute` formula:

```
FinalValue = (BaseValue + Additive) * Multiplicative
```

Additive bonuses stack by summing. Multiplicative bonuses stack by multiplying the Multiplicative field (e.g., two +10% bonuses: 1.1 * 1.1 = 1.21).

## Content Assets

```
Content/Player/
├── DA_Upgrade_MaxHealth.uasset      # Player stat upgrade
├── DA_Upgrade_MoveSpeed.uasset      # Player stat upgrade
└── DT_Upgrades.uasset               # DataTable registry (FUpgradeTableRow)
```

## Integration Points

| System | How it connects |
|--------|----------------|
| GameMode | `BeginPlay` registers UpgradeDataTable with UUpgradeSubsystem |
| Character | `BeginPlay` registers self with subsystem; `AddXP` triggers upgrade selection on level-up |
| Weapons | `AddWeapon` registers each weapon with subsystem for stat tracking |
| AttributeComponent | Upgrade effects call `ApplyAdditive`/`ApplyMultiplicative` on player attributes |
| Weapon Actors | `ApplyStatUpgrade()` and `UsesStat()` for per-weapon and global weapon upgrades |

## Adding New Upgrades

1. Create a new `UUpgradeDataAsset` in Content Browser (right-click > Miscellaneous > Data Asset > UpgradeDataAsset)
2. Set UpgradeID, DisplayName, Description
3. Add effects to the Effects array (pick PlayerStat or WeaponStat, set bonuses)
4. For weapon-specific: set TargetWeapon and enable bRequiresWeaponOwnership
5. For new weapon grants: set WeaponToGrant
6. Configure Weight, MaxStacks, MinLevel, RequiredUpgrades as needed
7. Add a row to DT_Upgrades DataTable pointing to the new asset

## Legacy Note

`UpgradeTypes.h` contains both the current enums and a legacy `FUpgradeDefinition` struct that predates the DataAsset approach. The struct is unused by the current system but still compiled. The active system uses `UUpgradeDataAsset` + `FUpgradeEffect` instead.
