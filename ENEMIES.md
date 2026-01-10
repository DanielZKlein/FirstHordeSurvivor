# Enemy System

## Overview

Simple chase-and-attack enemies optimized for large hordes with RVO avoidance. Enemy stats are defined in a DataTable for easy bulk editing and balancing.

## Data Structure

### FEnemyTableRow (DataTable Row)
**File:** `Source/FirstHordeSurvivor/EnemyData.h`

```cpp
// Visuals
TSoftObjectPtr<UStaticMesh> EnemyMesh       // Visual mesh
TSoftObjectPtr<UMaterialInterface> EnemyMaterial  // Material
float MeshScale = 1.0f                      // Visual scale multiplier
FLinearColor EnemyColor                     // Material color parameter
float EmissiveStrength = 0.0f               // Glow intensity

// Stats
float BaseHealth = 100.0f                   // Max HP
float BaseDamage = 10.0f                    // Damage per attack
float MoveSpeed = 400.0f                    // Movement speed

// Rewards
int32 MinXP = 10                            // XP reward minimum
int32 MaxXP = 20                            // XP reward maximum
```

### ASurvivorEnemy (Character)
**File:** `Source/FirstHordeSurvivor/SurvivorEnemy.h/cpp`

**Components:**
- `UAttributeComponent* AttributeComp` - Health and stats
- `UStaticMeshComponent* EnemyMeshComp` - Visual from DataTable
- `USphereComponent* AttackOverlapComp` - Attack trigger (150 radius)
- `UWidgetComponent* HealthBarComp` - Health bar display

**Configuration:**
- `UDataTable* EnemyDataTable` - Reference to the enemy DataTable
- `FName EnemyRowName` - Row name to look up in the table

**Movement Settings:**
```cpp
bOrientRotationToMovement = true
RotationRate = FRotator(0, 1000, 0)
bUseRVOAvoidance = true
AvoidanceWeight = 0.5f
```

## DataTable Setup

1. Right-click Content Browser → Miscellaneous → **DataTable**
2. Select `FEnemyTableRow` as the row structure
3. Name it `DT_Enemies`
4. Add rows for each enemy type (row names like "Lvl1_Tetrahedron", "Lvl2_Cube", etc.)

## Initialization Flow

`BeginPlay()`:
1. Look up `EnemyRowName` in `EnemyDataTable`
2. Cache pointer to `FEnemyTableRow` as `EnemyData`
3. Call `InitializeFromData()`
4. Bind OnDeath and OnHealthChanged delegates
5. Disable health regen for enemies

`InitializeFromData()`:
1. Load mesh and material from row data
2. Create dynamic material instance with Color/Emissive params
3. Enable custom depth for outline rendering
4. Apply stats to AttributeComponent and CharacterMovement

## Visual Feedback

**Hit Flash:**
- On damage, material flashes white via `HitFlashIntensity` parameter
- Holds at full intensity for ~5 frames, then decays

**Health Bar:**
- Widget component displays health percentage
- Updates on `OnHealthChanged` delegate

**Outline:**
- Enemies render to custom depth buffer
- Post-process material draws red outlines around enemies only

## Creating New Enemy Types

1. Open `DT_Enemies` DataTable
2. Click **Add** to create a new row
3. Set row name (e.g., "Lvl3_Boss")
4. Configure all properties in the spreadsheet view
5. In enemy Blueprint, set `EnemyRowName` to match

**Bulk Editing:**
- All enemies visible in one spreadsheet
- Change column values to affect all enemies
- Export to CSV for external editing

## Spawning Enemies

```cpp
FActorSpawnParameters Params;
ASurvivorEnemy* Enemy = GetWorld()->SpawnActor<ASurvivorEnemy>(
    EnemyBlueprintClass,
    SpawnLocation,
    FRotator::ZeroRotator,
    Params
);
Enemy->EnemyDataTable = MyDataTable;
Enemy->EnemyRowName = FName("Lvl1_Tetrahedron");
```

## Content Assets

```
Content/Enemies/
├── DT_Enemies.uasset           # Enemy DataTable (create this!)
├── B_BaseEnemy.uasset          # Base enemy Blueprint
├── M_Enemy.uasset              # Parameterized enemy material
└── M_EnemyOutline.uasset       # Post-process outline material
```

## RVO Avoidance

Enemies use Reciprocal Velocity Obstacles to avoid clustering:
- `bUseRVOAvoidance = true`
- `AvoidanceWeight = 0.5f` (moderate priority)

This keeps hordes spread while still converging on player.

## Enemy Spawn System

### UEnemySpawnSubsystem (WorldSubsystem)
**File:** `Source/FirstHordeSurvivor/EnemySpawnSubsystem.h/cpp`

Manages enemy spawning with object pooling and time-based difficulty.

**Features:**
- Object pooling (enemies returned to pool on death, not destroyed)
- Pre-warming (spawns 20 inactive enemies at game start)
- Time-based spawn rate scaling
- Responsive spawning (faster when few enemies on map)
- Camera-aware spawn locations (outside visible area)
- Weighted enemy type selection with time-based unlocks

### Spawn Rate Formula

```
EffectiveRate = min(BaseRate + TimeBonus + ResponsiveBonus, MaxSpawnRate)

Where:
- BaseRate = 30 spawns/min
- TimeBonus = 5 spawns/min * ElapsedMinutes
- ResponsiveBonus = MaxBonus * (1 - CurrentEnemies/TargetCount) * Responsiveness
- MaxSpawnRate = 120 spawns/min
```

### Spawn Configuration DataTable (DT_EnemySpawns)

Row structure `FEnemySpawnEntry`:
```cpp
FName EnemyRowName;            // Reference to DT_Enemies row
float Weight = 1.0f;           // Relative spawn chance
float MinuteUnlock = 0.0f;     // When this enemy starts appearing
float MinuteDeprecate = 0.0f;  // When this enemy stops spawning (0 = never)
```

Example rows:
| Row Name | EnemyRowName | Weight | MinuteUnlock | MinuteDeprecate |
|----------|--------------|--------|--------------|-----------------|
| Basic | Lvl1_Tetrahedron | 10.0 | 0.0 | 4.0 |
| Medium | Lvl2_Pyramid | 5.0 | 2.0 | 8.0 |
| Hard | Lvl3_Cube | 2.0 | 5.0 | 0.0 |

### Subsystem Properties

```cpp
// Spawn Rate
float BaseSpawnRate = 30.0f;       // Spawns/min at start
float SpawnRateGrowth = 5.0f;      // Extra spawns/min per minute
float MaxSpawnRate = 120.0f;       // Hard cap

// Responsive System
float TargetEnemyCount = 50.0f;    // "Full" enemy count
float MaxResponsiveBonus = 60.0f;  // Max extra spawns/min when empty
float Responsiveness = 0.5f;       // 0-1, aggression when few enemies

// Caps
int32 MaxEnemiesOnMap = 150;       // Performance cap
int32 PreWarmCount = 20;           // Pre-spawned pool size

// Location
float SpawnRadius = 2500.0f;       // Distance from player
float SpawnMargin = 500.0f;        // Random variance
```

### Pooling Functions (ASurvivorEnemy)

```cpp
// Hide and disable enemy, return to pool
void Deactivate();

// Reset and activate enemy from pool
void Reinitialize(UDataTable* DataTable, FName RowName, FVector Location);
```

### Required Assets

```
Content/Enemies/
├── DT_Enemies.uasset           # Enemy stats DataTable
├── DT_EnemySpawns.uasset       # Spawn config DataTable (create this!)
└── B_BaseEnemy.uasset          # Base enemy Blueprint
```

### Auto-Start

The subsystem auto-starts spawning 0.5 seconds after world initialization. It attempts to load:
- `/Game/Enemies/B_BaseEnemy` (enemy Blueprint)
- `/Game/Enemies/DT_Enemies` (enemy stats)
- `/Game/Enemies/DT_EnemySpawns` (spawn weights, optional)

### Manual Configuration (Optional)

From GameMode or Level Blueprint:
```cpp
UEnemySpawnSubsystem* Spawner = GetWorld()->GetSubsystem<UEnemySpawnSubsystem>();
Spawner->Configure(EnemyClass, EnemyDataTable, SpawnConfigTable);
Spawner->StartSpawning();
```
