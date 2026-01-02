# Enemy Spawner Design

## Design Goals

1. **Prevent death spirals**: Players who fall behind shouldn't face ever-increasing impossible odds
2. **Prevent runaway success**: Players who excel early shouldn't snowball infinitely
3. **Maintain pressure**: Game should always feel tense, not trivial
4. **Tunable**: Designer can adjust feel without code changes

## Core Problem

| Approach | Falling Behind | Getting Ahead |
|----------|----------------|---------------|
| Pure time-based | Death spiral (enemies accumulate) | Fair (constant rate) |
| Pure state-based | Fair (spawns slow down) | Snowball (more kills = more XP) |

**Solution**: Hybrid with caps.

---

## Spawn Rate Formula

```
EffectiveSpawnRate = min(BaseRate + ResponsiveBonus, MaxSpawnRate)
```

Where:
- **BaseRate** = Guaranteed spawns/min, scales with time
- **ResponsiveBonus** = Extra spawns when enemy count is low
- **MaxSpawnRate** = Hard ceiling (prevents snowball)

### Component 1: Base Rate (Time Pressure)

```cpp
float BaseRate = BaseSpawnRate + (ElapsedMinutes * SpawnRateGrowth);
```

| Parameter | Description | Example |
|-----------|-------------|---------|
| `BaseSpawnRate` | Starting spawns per minute | 30 |
| `SpawnRateGrowth` | Additional spawns/min per minute elapsed | 5 |

At minute 0: 30 spawns/min
At minute 5: 55 spawns/min
At minute 10: 80 spawns/min

This creates guaranteed escalation regardless of player performance.

### Component 2: Responsive Bonus (State Pressure)

```cpp
float EnemyDeficit = FMath::Max(0, TargetEnemyCount - CurrentEnemyCount) / TargetEnemyCount;
float ResponsiveBonus = MaxResponsiveBonus * EnemyDeficit * Responsiveness;
```

| Parameter | Description | Example |
|-----------|-------------|---------|
| `TargetEnemyCount` | "Full" enemy count for responsiveness calc | 50 |
| `MaxResponsiveBonus` | Max extra spawns/min when map is empty | 60 |
| `Responsiveness` | 0-1, how aggressively we respond | 0.5 |

**Example with 50 target, 60 max bonus, 0.5 responsiveness:**
- 0 enemies alive: Deficit = 1.0 → Bonus = 60 * 1.0 * 0.5 = 30 extra/min
- 25 enemies alive: Deficit = 0.5 → Bonus = 60 * 0.5 * 0.5 = 15 extra/min
- 50+ enemies alive: Deficit = 0 → Bonus = 0

### Component 3: Caps (Anti-Snowball)

```cpp
// Rate cap: prevents infinite XP generation
float FinalRate = FMath::Min(EffectiveSpawnRate, MaxSpawnRate);

// Count cap: prevents overwhelming + performance
if (CurrentEnemyCount >= MaxEnemiesOnMap) { skip spawn }
```

| Parameter | Description | Example |
|-----------|-------------|---------|
| `MaxSpawnRate` | Absolute ceiling on spawns/min | 120 |
| `MaxEnemiesOnMap` | Hard cap on concurrent enemies | 150 |

**Why both caps?**
- `MaxSpawnRate` limits XP/min potential (anti-snowball)
- `MaxEnemiesOnMap` limits performance cost and visual chaos

---

## Tuning Scenarios

### Scenario A: Player falling behind
- Enemies accumulating (count approaching MaxEnemiesOnMap)
- ResponsiveBonus → 0 (deficit is negative/zero)
- Only BaseRate applies
- Eventually hits MaxEnemiesOnMap cap → spawning pauses
- **Result**: Pressure plateaus, player has chance to recover

### Scenario B: Player doing well
- Enemies dying fast (count stays low)
- ResponsiveBonus is high
- But capped by MaxSpawnRate
- **Result**: Gets more enemies, but rate is bounded

### Scenario C: Player doing extremely well
- Killing faster than MaxSpawnRate can provide
- Hits the cap ceiling
- XP/min is capped
- **Result**: Still progresses, but can't infinitely snowball

---

## Spawn Location

Spawn outside camera frustum, offset from player:

```cpp
float SpawnDistance = CameraViewRadius + SpawnMargin; // e.g., 2500 units
float Angle = FMath::RandRange(0, 360);
FVector SpawnOffset = FVector(
    FMath::Cos(FMath::DegreesToRadians(Angle)) * SpawnDistance,
    FMath::Sin(FMath::DegreesToRadians(Angle)) * SpawnDistance,
    0
);
FVector SpawnLocation = PlayerLocation + SpawnOffset;
```

| Parameter | Description | Example |
|-----------|-------------|---------|
| `SpawnMargin` | Buffer beyond camera edge | 500 |
| `SpawnHeightOffset` | Z offset if needed | 0 |

Optional: Weight spawn direction toward player's movement (enemies "ahead" of player).

---

## Proposed Class: `AEnemySpawner` or `UEnemySpawnSubsystem`

### Properties (All EditDefaultsOnly for tuning)

```cpp
// Base Rate
float BaseSpawnRate = 30.0f;           // Spawns per minute at start
float SpawnRateGrowth = 5.0f;          // Additional spawns/min per minute elapsed

// Responsive Rate
float TargetEnemyCount = 50.0f;        // "Full" for responsiveness calculation
float MaxResponsiveBonus = 60.0f;      // Max extra spawns/min when empty
float Responsiveness = 0.5f;           // 0-1, aggressiveness of response

// Caps
float MaxSpawnRate = 120.0f;           // Absolute max spawns per minute
int32 MaxEnemiesOnMap = 150;           // Hard cap on concurrent enemies

// Location
float SpawnDistance = 2500.0f;         // Distance from player
float SpawnMargin = 500.0f;            // Buffer beyond camera

// Enemy Selection
TArray<FEnemySpawnEntry> EnemyTypes;   // Weighted list of enemy types
```

### FEnemySpawnEntry

```cpp
USTRUCT()
struct FEnemySpawnEntry
{
    TSubclassOf<ASurvivorEnemy> EnemyClass;
    UEnemyData* EnemyData;
    float Weight = 1.0f;                // Relative spawn chance
    float MinuteUnlock = 0.0f;          // When this enemy starts appearing
};
```

---

## Open Questions

1. **Subsystem vs Actor?**
   - Subsystem: Cleaner, auto-lifecycle, no placement needed
   - Actor: Visible in level, easier debug visualization

2. **Multiple spawn "waves" vs continuous trickle?**
   - Current design assumes continuous trickle
   - Could add burst spawns at intervals

3. **Enemy difficulty scaling over time?**
   - Spawn tougher enemies later (via MinuteUnlock)
   - Or scale enemy stats with elapsed time?

4. **Spatial distribution?**
   - Pure random circle around player?
   - Weight toward movement direction?
   - Spawn in groups/clusters?

---

## Example Tuning Session

**Goal**: First 2 minutes feel manageable, minute 5 feels hectic, minute 10 is survival mode.

```
BaseSpawnRate = 20
SpawnRateGrowth = 8
TargetEnemyCount = 40
MaxResponsiveBonus = 40
Responsiveness = 0.6
MaxSpawnRate = 100
MaxEnemiesOnMap = 120
```

| Minute | Base Rate | Max w/ Responsive | Effective Max |
|--------|-----------|-------------------|---------------|
| 0 | 20 | 20 + 24 = 44 | 44 |
| 2 | 36 | 36 + 24 = 60 | 60 |
| 5 | 60 | 60 + 24 = 84 | 84 |
| 10 | 100 | 100 + 24 = 124 | 100 (capped) |

At minute 10+, rate is capped at 100/min regardless of performance.
