# Enemy System

## Overview

Simple chase-and-attack enemies optimized for large hordes with RVO avoidance.

## Classes

### UEnemyData (DataAsset)
**File:** `Source/FirstHordeSurvivor/EnemyData.h`

```cpp
TSoftObjectPtr<UStaticMesh> EnemyMesh       // Visual mesh (async loadable)
TSoftObjectPtr<UMaterialInterface> EnemyMaterial  // Material
float BaseHealth = 100.0f                   // Max HP
float BaseDamage = 10.0f                    // Damage per attack
float MoveSpeed = 400.0f                    // Movement speed
int32 MinXP = 10                            // XP reward minimum
int32 MaxXP = 20                            // XP reward maximum
```

### ASurvivorEnemy (Character)
**File:** `Source/FirstHordeSurvivor/SurvivorEnemy.h/cpp`

**Components:**
- `UAttributeComponent* AttributeComp` - Health and stats
- `UStaticMeshComponent* EnemyMeshComp` - Visual from EnemyData
- `USphereComponent* AttackOverlapComp` - Attack trigger (150 radius)

**Movement Settings:**
```cpp
bOrientRotationToMovement = true
RotationRate = FRotator(0, 1000, 0)
bUseRVOAvoidance = true
AvoidanceWeight = 0.5f
```

## Initialization Flow

`BeginPlay()` -> `InitializeFromData()`:
1. Find player via `GetPlayerCharacter()`
2. Validate EnemyData (logs error if missing)
3. Load mesh and material from EnemyData
4. Apply to EnemyMeshComp
5. Set AttributeComp->MaxHealth from EnemyData
6. Set CharacterMovement->MaxWalkSpeed from EnemyData
7. Bind OnDeath delegate

## AI Behavior

**Movement (Tick-based):**
```cpp
FVector Direction = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal();
AddMovementInput(Direction);
SetActorRotation(Direction.Rotation());
```

No AIController or behavior tree - direct pursuit for horde efficiency.

**Attack System:**
- Overlap sphere triggers `OnOverlapBegin()`
- Starts 1-second repeating timer
- `AttackPlayer()` applies `-EnemyData->BaseDamage` to player
- Timer cleared on `OnOverlapEnd()`

## Death & XP Drops

`OnDeath()`:
1. Stop attack timer
2. Disable collision/movement
3. Calculate XP: `FMath::RandRange(MinXP, MaxXP)`
4. Decompose into gem tiers using greedy algorithm (100, 50, 20, 5, 1)
5. Spawn each gem via `XPGemSubsystem->SpawnGem(Location, Value)`
6. Destroy enemy: `SetLifeSpan(0.1f)`

**Greedy XP Decomposition Example:**
- Enemy drops 73 XP
- Spawns: 1x 50-gem, 1x 20-gem, 3x 1-gem (5 gems total)

## Content Assets

```
Content/Enemies/
├── DA_FirstEnemy.uasset        # Primary enemy DataAsset
├── B_NewTestEnemy.uasset       # Blueprint (ASurvivorEnemy subclass)
└── AC_EnemyAIController.uasset # AI Controller (currently unused)
```

## Creating New Enemy Variants

1. Create new `UEnemyData` DataAsset
2. Assign mesh and material
3. Configure health, damage, speed, XP range
4. Either:
   - Place `B_NewTestEnemy` in level and override EnemyData property
   - Create new Blueprint subclass with different EnemyData default

## Spawning Enemies

Currently enemies are placed in levels manually. No wave spawner exists yet.

**To spawn via code:**
```cpp
FActorSpawnParameters Params;
ASurvivorEnemy* Enemy = GetWorld()->SpawnActor<ASurvivorEnemy>(
    EnemyBlueprintClass,
    SpawnLocation,
    FRotator::ZeroRotator,
    Params
);
Enemy->EnemyData = MyEnemyData;  // Set before BeginPlay if spawning deferred
```

## RVO Avoidance

Enemies use Reciprocal Velocity Obstacles to avoid clustering:
- `bUseRVOAvoidance = true`
- `AvoidanceWeight = 0.5f` (moderate priority)

This keeps hordes spread while still converging on player.
