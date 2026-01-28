# Weapons System

## Overview

Timer-based auto-fire weapons with intelligent targeting, data-driven configuration, and support for penetration, AoE explosions, knockback, and multi-shot modes.

## Architecture

```
UWeaponDataBase (abstract DataAsset)
├── UProjectileWeaponData    // Missiles, bullets, arrows
├── UAuraWeaponData          // PBAoE like Garlic (planned)
└── UChainWeaponData         // Chain lightning (planned)
```

Each archetype declares which stats it uses and provides descriptions for UI.

## Stat System

Stats are defined in `UpgradeTypes.h`:

```cpp
enum class EWeaponStat : uint8
{
    Damage,           // Per hit/pulse/jump
    AttackSpeed,      // RPM multiplier (runtime only, starts at 1.0)
    Area,             // Explosion radius, search radius, etc.
    Penetration,      // Pierce count, jumps, bounces
    ProjectileSpeed,  // Projectile travel speed
    ProjectileCount,  // Multi-shot
    Duration,         // Aura duration, DoT length
    Range,            // Max range
    Knockback,        // Push force
};
```

**Base values** are stored in the DataAsset (plain floats).
**Runtime modifiers** from upgrades are tracked in the weapon actor using `FGameplayAttribute` (Additive + Multiplicative).

Final value: `(BaseValue + Additive) * Multiplicative`

## Classes

### UWeaponDataBase (Abstract DataAsset)
**File:** `Source/FirstHordeSurvivor/WeaponDataBase.h`

Base class for all weapon data. Contains identity and universal stats:

```cpp
FName WeaponID                    // Unique identifier
FText DisplayName                 // UI display name
UTexture2D* Icon                  // Optional UI icon
float BaseDamage = 10.0f          // Damage per hit
USoundBase* AttackSound           // Fire audio
UNiagaraSystem* AttackVFX         // Fire particles
```

Virtual interface:
- `GetApplicableStats()` - Which stats this weapon uses
- `GetStatDescription(Stat)` - UI description for a stat
- `GetBaseStatValue(Stat)` - Base value before modifiers
- `GetBaseRPM()` - Base fire rate

### UProjectileWeaponData (DataAsset)
**File:** `Source/FirstHordeSurvivor/ProjectileWeaponData.h`

```cpp
// Projectile Config
TSubclassOf<ASurvivorProjectile> ProjectileClass
float BaseRPM = 60.0f             // Rounds per minute

// Stats
float ProjectileSpeed = 1000.0f   // Travel speed (units/sec)
float Range = 2000.0f             // Max travel distance
int32 Penetration = 0             // Enemies pierced (0 = stop on first)
float Area = 0.0f                 // Explosion radius (0 = single target)
int32 ProjectileCount = 1         // Projectiles per shot
float Knockback = 0.0f            // Push force on hit

// Multi-Shot (when ProjectileCount > 1)
EMultiShotMode MultiShotMode      // Volley or Barrage
float SpreadAngle = 30.0f         // Fan spread in degrees
float BarrageRPM = 120.0f         // Fire rate within burst

// Targeting
float Precision = 5.0f            // Random aim deviation (degrees)
float RangeWeight = -1.0f         // Distance preference (negative = closer)
float InFrontWeight = 1000.0f     // Direction preference

// Explosion Visuals
USoundBase* ExplosionSound
UNiagaraSystem* ExplosionVFX
```

### EMultiShotMode

```cpp
enum class EMultiShotMode : uint8
{
    Volley,   // All projectiles fire simultaneously in a fan
    Barrage,  // Projectiles fire sequentially, then cooldown starts
};
```

- **Volley**: All projectiles spawn at once, spread across SpreadAngle
- **Barrage**: Projectiles fire one at a time at BarrageRPM, then main cooldown starts

### ASurvivorWeapon (Actor)
**File:** `Source/FirstHordeSurvivor/SurvivorWeapon.h/cpp`

- Attached to player character on BeginPlay
- Uses timer-based firing (no tick)
- Auto-targets enemies using weighted scoring
- Tracks runtime stat modifiers from upgrades

**Public API:**
```cpp
void StartShooting()
void StopShooting()
float GetStat(EWeaponStat Stat) const
void ApplyStatUpgrade(EWeaponStat Stat, float Additive, float Multiplicative)
bool UsesStat(EWeaponStat Stat) const
TArray<EWeaponStat> GetApplicableStats() const
```

### ASurvivorProjectile (Actor)
**File:** `Source/FirstHordeSurvivor/SurvivorProjectile.h/cpp`

**Components:**
- `USphereComponent` - Overlap collision
- `UProjectileMovementComponent` - Zero gravity, rotation follows velocity
- `UStaticMeshComponent` - Visual mesh
- `UNiagaraComponent` - Trail VFX

**Initialization:**
```cpp
void Initialize(
    float Speed,
    float DamageAmount,
    float Range,
    int32 PierceCount = 0,        // 0 = destroy on first hit
    float ExplosionRadius = 0.0f, // 0 = no explosion
    float KnockbackForce = 0.0f,
    USoundBase* InExplosionSound = nullptr,
    UNiagaraSystem* InExplosionVFX = nullptr
);
```

**Behavior:**
- Destroys when exceeding MaxRange from start
- Damages actors with AttributeComponent on overlap
- Tracks `HitEnemies` TSet to avoid double-hits during pierce
- Explodes on impact if Area > 0 (damages all in radius except direct hit)
- Applies knockback force on hit
- Continues through enemies if RemainingPierces > 0

## Targeting System

**FindBestTarget() Algorithm:**
1. Get all `ASurvivorEnemy` actors in world
2. Filter by Range distance
3. Score each target:
   - `DistanceScore = Distance * RangeWeight` (negative = prefer closer)
   - `DirectionScore = Dot(PlayerVelocity, DirToEnemy) * InFrontWeight`
   - `TotalScore = DistanceScore + DirectionScore`
4. Return highest scoring target

## Fire Pipeline

### Volley Mode (ProjectileCount > 1)
1. Timer triggers `Fire()` at interval `60.0 / EffectiveRPM`
2. Find best target, calculate base direction with Precision spread
3. Fire all projectiles at once, spread across SpreadAngle
4. Play sound/VFX once

### Barrage Mode (ProjectileCount > 1)
1. Timer triggers `Fire()`
2. Find best target, cache direction
3. Fire first projectile, play sound/VFX
4. Schedule `ContinueBurst()` at BarrageRPM interval
5. Each burst fires next projectile with sound/VFX
6. After all projectiles fired, schedule next attack at main RPM

### Single Projectile
Same as Volley with count = 1.

## Content Assets

```
Content/Weapons/
├── DA_WD_TestMissile.uasset          # Basic test missile
├── DA_SimpleFirstHitMissile.uasset   # Single-target missile
├── DA_SimplePiercingMissile.uasset   # Piercing missile
├── DA_SimpleExplosionMissile.uasset  # AoE explosion missile
├── DA_MultiMissile.uasset            # Multi-shot missile
├── BP_SimpleFirstHitMissile.uasset   # Projectile blueprint
├── BP_SimplePiercingMissile.uasset   # Projectile blueprint
├── BP_SimpleExplosionMissile.uasset  # Projectile blueprint
├── B_LaserBolt.uasset                # Laser projectile
├── M_Arrow.uasset                    # Arrow material
├── MI_LaserBolt.uasset               # Laser material instance
└── VFX/                              # Visual effects
```

## Integration with Player

In `ASurvivorCharacter::BeginPlay()`:
```cpp
Weapon = GetWorld()->SpawnActor<ASurvivorWeapon>(WeaponClass);
Weapon->WeaponData = StartingWeaponData;
Weapon->AttachToActor(this, ...);
Weapon->StartShooting();
```

## Adding New Projectile Weapons

1. Create `UProjectileWeaponData` DataAsset in Content Browser
2. Set ProjectileClass (use existing or create Blueprint subclass)
3. Configure stats: RPM, damage, speed, range, penetration, area, knockback
4. For multi-shot: set ProjectileCount, MultiShotMode, SpreadAngle
5. Assign explosion sound/VFX if using Area > 0
6. Assign to character or weapon pickup system

## Stat Interactions

| Stat | Projectile Meaning |
|------|-------------------|
| Damage | Per-hit damage |
| AttackSpeed | Multiplier on BaseRPM |
| Area | Explosion radius (0 = none) |
| Penetration | Pierce count (0 = stop on first) |
| ProjectileSpeed | Travel velocity |
| ProjectileCount | Projectiles per attack |
| Range | Max travel distance + targeting range |
| Knockback | Push force on hit |
