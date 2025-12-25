# XP Gem System

## Overview

Pooled XP gems with dramatic flee-then-magnetize behavior and tiered visuals.

## Classes

### FXPGemData (Struct)
**File:** `Source/FirstHordeSurvivor/XPGem.h`

```cpp
UStaticMesh* Mesh                    // 3D model
UMaterialInterface* Material         // Base material (dynamic instance created)
float Scale = 1.0f                   // Actor scale
FLinearColor Color = White           // Base color (supports HDR > 1.0)
float EmissiveStrength = 20.0f       // Glow intensity
float LightIntensity = 1000.0f       // Point light brightness
float LightRadius = 200.0f           // Light attenuation radius
UNiagaraSystem* TrailEffect          // Optional particle trail
```

### UXPGemVisualConfig (DataAsset, optional)
**File:** `Source/FirstHordeSurvivor/XPGemVisualConfig.h`

- `TMap<int32, FXPGemData> GemVisuals` - Map of XP value to visuals
- `FXPGemData DefaultVisual` - Fallback for unmapped values
- Override for code defaults when assigned to GameMode

### UXPGemSubsystem (WorldSubsystem)
**File:** `Source/FirstHordeSurvivor/XPGemSubsystem.h/cpp`

**Public API:**
- `SpawnGem(Location, XPValue)` - Get pooled or spawn new gem
- `ReturnGemToPool(Gem)` - Return gem for reuse
- `RegisterVisualConfig(Config)` - Set DataAsset override
- `RegisterGemClass(Class)` - Set custom gem Blueprint class

### AXPGem (Actor)
**File:** `Source/FirstHordeSurvivor/XPGem.h/cpp`

**Components:**
- `UStaticMeshComponent` - Visual gem mesh
- `UNiagaraComponent` - Trail particles
- `UPointLightComponent` - Dynamic light (no shadows)

## Gem State Machine

```
Inactive → Spawning (0.8s) → Idle → Fleeing (0.35s) → Magnetizing → Collected
   ↑                                                                    │
   └────────────────────── ReturnToPool ←───────────────────────────────┘
```

**States:**
1. **Inactive** - In pool, hidden, no tick
2. **Spawning** - Flies upward/outward with drag, decelerating
3. **Idle** - Waiting for player to enter pickup range
4. **Fleeing** - Dramatic escape when player gets close (upward bias)
5. **Magnetizing** - Accelerates toward player
6. **Collected** - Adds XP, returns to pool

## Movement Parameters

```cpp
float FlyAwayForce = 800.0f        // Initial spawn velocity
float MagnetAcceleration = 2000.0f // Magnetize acceleration
float MaxSpeed = 3000.0f           // Speed cap during magnetize
float FleeForce = 600.0f           // Flee velocity
float CollectDistance = 50.0f      // Distance to trigger collection
float SpawningDuration = 0.8f      // Time in spawning state
float FleeDuration = 0.35f         // Time in fleeing state
```

## Default Visuals (Code)

Configured in `XPGemSubsystem::InitializeDefaultVisuals()`:

| XP | Color | Mesh | Scale | Emissive | Light |
|----|-------|------|-------|----------|-------|
| 1 | Gray (0.16, 0.16, 0.16) | SM_Derbis_C | 3.0 | 15 | 500/200 |
| 5 | White (2.0, 2.0, 2.0) | SM_Derbis_B | 4.5 | 20 | 800/260 |
| 20 | Yellow (1.0, 0.96, 0.0) | SM_Derbis_B | 6.0 | 30 | 1200/325 |
| 50 | Blue (0.1, 0.4, 1.0) | SM_Derbis_A | 8.0 | 40 | 2000/455 |
| 100 | Red (1.0, 0.03, 0.0) | SM_Derbis_A | 10.0 | 50 | 3000/650 |
| Default | Green (0.2, 1.0, 0.3) | SM_Derbis_C | 5.0 | 20 | 1000/260 |

**Assets Used:**
- `/Game/VFX_Destruction/Meshes/SM_Derbis_A/B/C`
- `/Game/Materials/M_XPGem` (with Color and EmissiveStrength parameters)

## Visual Configuration Priority

1. **DataAsset** - If `UXPGemVisualConfig` registered via GameMode
2. **Code Defaults** - `InitializeDefaultVisuals()` hardcoded values
3. **Fallback** - Green gem for any unmapped XP value

## Visual Application

`AXPGem::SetVisuals(FXPGemData)`:
1. Set mesh from data
2. Create dynamic material instance
3. Set `Color` vector parameter
4. Set `EmissiveStrength` scalar parameter
5. Apply to all material slots
6. Set actor scale
7. Configure point light (color, intensity, radius)
8. Activate trail if TrailEffect assigned

## Content Assets

```
Content/XPGems/
├── M_XPGem.uasset           # Gem material (Color, EmissiveStrength params)
└── DA_XPGemVisuals.uasset   # Optional visual config DataAsset
```

## Spawning Flow (from Enemy Death)

1. Enemy calculates random XP in `[MinXP, MaxXP]`
2. Greedy decomposition into tiers: 100, 50, 20, 5, 1
3. For each tier gem needed:
   - Call `XPGemSubsystem->SpawnGem(Location + RandomOffset, TierValue)`
   - Subsystem returns pooled gem or creates new
   - Gem calls `Initialize()` with upward velocity bias
   - Gem calls `SetVisuals()` with appropriate tier data

## Collection Flow

1. Player's `PickupRange` (default 500) checked in gem's Idle tick
2. When in range, gem enters **Fleeing** state (0.35s)
3. After flee, enters **Magnetizing** state
4. Accelerates toward player at 2000 units/s^2, capped at 3000 units/s
5. When within `CollectDistance` (50), calls `Player->AddXP(Value)`
6. Returns to pool via `ReturnToPool()`

## Modifying Gem Visuals

**Option 1: Edit Code Defaults**
Modify `XPGemSubsystem::InitializeDefaultVisuals()` directly.

**Option 2: DataAsset Override**
1. Create `UXPGemVisualConfig` DataAsset
2. Add entries to `GemVisuals` map for each tier
3. Set `DefaultVisual` for fallback
4. Assign to GameMode's `XPGemVisualConfig` property
