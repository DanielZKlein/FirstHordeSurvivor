# Weapons System

## Overview

Timer-based auto-fire weapons with intelligent targeting and data-driven configuration.

## Classes

### UWeaponData (DataAsset)
**File:** `Source/FirstHordeSurvivor/WeaponData.h`

```cpp
TSubclassOf<ASurvivorProjectile> ProjectileClass  // Projectile to spawn
float RPM = 60.0f                                 // Rounds per minute
float MaxRange = 1000.0f                          // Targeting range
float Damage = 10.0f                              // Per-hit damage
float ProjectileSpeed = 1000.0f                   // Projectile velocity
float Precision = 5.0f                            // Spread cone (degrees)
float RangeWeight = -1.0f                         // Distance preference (negative = closer)
float InFrontWeight = 1000.0f                     // Direction preference
USoundBase* ShootSound                            // Fire audio
UNiagaraSystem* ShootVFX                          // Fire particles
```

### ASurvivorWeapon (Actor)
**File:** `Source/FirstHordeSurvivor/SurvivorWeapon.h/cpp`

- Attached to player character on BeginPlay
- Uses timer-based firing (no tick)
- Auto-targets enemies using weighted scoring

**Public API:**
- `StartShooting()` - Begin fire loop
- `StopShooting()` - Pause firing
- `Fire()` - Single shot (called by timer)

### ASurvivorProjectile (Actor)
**File:** `Source/FirstHordeSurvivor/SurvivorProjectile.h/cpp`

**Components:**
- `USphereComponent` - Overlap collision
- `UProjectileMovementComponent` - Zero gravity, rotation follows velocity
- `UStaticMeshComponent` - Visual
- `UNiagaraComponent` - Trail VFX

**Initialization:** `Initialize(Speed, Damage, Range)`

**Behavior:**
- Destroys when exceeding MaxRange from start
- Damages actors with AttributeComponent on overlap
- Spawns hit VFX/sound on successful hit

## Targeting System

**FindBestTarget() Algorithm:**
1. Get all `ASurvivorEnemy` actors in world
2. Filter by MaxRange distance
3. Score each target:
   - `DistanceScore = Distance * RangeWeight` (negative weight = prefer closer)
   - `DirectionScore = DotProduct(PlayerVelocity, DirectionToEnemy) * InFrontWeight`
   - `TotalScore = DistanceScore + DirectionScore`
4. Return highest scoring target

## Fire Pipeline

1. Timer triggers `Fire()` at interval `60.0 / RPM`
2. `FindBestTarget()` returns best enemy or nullptr
3. Calculate direction with precision spread (random cone offset)
4. Spawn projectile at owner location
5. Call `Initialize()` with speed/damage/range from WeaponData
6. Play sound and VFX at location

## Content Assets

```
Content/Weapons/
├── DA_WD_TestMissile.uasset    # Test weapon DataAsset
├── B_ProjectileTest.uasset     # Test projectile Blueprint
├── B_LaserBolt.uasset          # Laser projectile Blueprint
├── NS_LaserBullet.uasset       # Laser trail Niagara system
├── M_LaserBolt_Add.uasset      # Additive laser material
└── MI_LaserBolt.uasset         # Laser material instance
```

## Integration with Player

In `ASurvivorCharacter::BeginPlay()`:
```cpp
// Spawn weapon actor
Weapon = GetWorld()->SpawnActor<ASurvivorWeapon>(WeaponClass);
Weapon->WeaponData = StartingWeaponData;
Weapon->AttachToActor(this, ...);
Weapon->StartShooting();
```

## Adding New Weapons

1. Create new `UWeaponData` DataAsset in Content Browser
2. Set ProjectileClass (use existing or create Blueprint subclass of ASurvivorProjectile)
3. Configure RPM, damage, speed, precision
4. Assign to character's `StartingWeaponData` or weapon pickup system
