# FirstHordeSurvivor Architecture

UE5 C++ Vampire Survivors-style horde game with data-driven configuration.

_Last updated: 2026-05-17 (flow field pathfinding added)_

## Project Structure

```
Source/FirstHordeSurvivor/
├── SurvivorGameMode.h/cpp       # Game initialization, subsystem setup
├── SurvivorCharacter.h/cpp      # Player: movement, XP, weapon spawning
├── SurvivorEnemy.h/cpp          # Enemy: flow-field chase, attacks, death/drops
├── SurvivorWeapon.h/cpp         # Auto-targeting weapon controller
├── SurvivorProjectile.h/cpp     # Projectile physics and hit detection
├── AttributeComponent.h/cpp     # Modular stat system (health, speed)
├── EnemySpawnSubsystem.h/cpp    # Enemy pooling, weighted spawning, floor bounds
├── FlowFieldSubsystem.h/cpp     # Dijkstra-flood-fill pathfinding (10Hz BFS)
├── XPGemSubsystem.h/cpp         # Gem pooling and spawning
├── XPGem.h/cpp                  # Gem actor with state machine
├── WeaponData.h                 # Weapon configuration DataAsset
├── EnemyData.h                  # Enemy configuration DataAsset
├── XPGemVisualConfig.h/cpp      # Gem tier visual DataAsset
├── UpgradeSubsystem.h/cpp       # Upgrade pool, selection, and application
├── UpgradeDataAsset.h/cpp       # Individual upgrade definition (DataAsset)
├── UpgradeEffect.h/cpp          # Single stat modifier within an upgrade
├── UpgradeTypes.h               # Enums (EUpgradeType, EPlayerStat, EWeaponStat)
├── UpgradeTableRow.h            # DataTable row for upgrade registry
└── UpgradePanelWidget.h/cpp     # C++ base for upgrade selection UI widget
```

## Core Classes

### ASurvivorGameMode
- Registers XP gem configuration with subsystem on BeginPlay
- Properties: `XPGemVisualConfig`, `XPGemClass`

### ASurvivorCharacter
- Top-down controlled via Enhanced Input
- Components: AttributeComponent, SpringArm/Camera, RollingAudio
- Spawns weapon from `StartingWeaponData` on BeginPlay
- XP collection with `PickupRange` (default 500)
- Invulnerability system (0.5s after hit)

### ASurvivorEnemy
- Chase AI: samples `UFlowFieldSubsystem::SampleDirection` (bilinear flow field) and falls back to direct ToPlayer steering when off-grid or pre-init
- Layered movement systems on top of flow field: separation push (`SeparationSettings`), crowd push from trailing enemies (`CrowdPushSettings`), knockback with momentum chains (`KnockbackSettings`), and a 0.5s self-stun after dealing damage
- RVO avoidance is disabled (`bUseRVOAvoidance = false`); enemy capsules ECR_Ignore each other, separation forces handle spacing
- Outside orbit radius faces velocity; inside orbit faces player
- Attack overlap sphere (150 radius), 1s attack interval
- Drops XP gems on death (greedy tier decomposition)
- Configured via `UEnemyData` DataAsset, pooled by `UEnemySpawnSubsystem`

### UAttributeComponent
- Attached to player and enemies
- Modular attributes: `BaseValue + Additive * Multiplicative`
- Tracks: MaxHealth, HealthRegen, MaxSpeed, MaxAcceleration
- Delegates: OnAttributeChanged, OnHealthChanged, OnDeath

## Data Assets

| Asset | Purpose |
|-------|---------|
| `UWeaponData` | Projectile class, RPM, damage, speed, targeting weights |
| `UEnemyData` | Mesh, material, health, damage, speed, XP range |
| `UXPGemVisualConfig` | Map of XP values to visual configs (optional override) |
| `UUpgradeDataAsset` | Upgrade identity, effects, targeting, selection rules |

## Subsystems

### UXPGemSubsystem (WorldSubsystem)
- Manages gem object pooling
- Two-tier visual configuration:
  1. DataAsset override (if GameMode provides one)
  2. Hardcoded defaults in `InitializeDefaultVisuals()`
- Public API: `SpawnGem(Location, Value)`, `ReturnGemToPool(Gem)`

### UUpgradeSubsystem (WorldSubsystem)
- Manages upgrade pool, selection, and application (see [UPGRADES.md](UPGRADES.md))
- Registered by GameMode (DataTable) and Character (player ref, weapons)
- Weighted random selection of 3 upgrades on level-up
- Applies effects to player attributes and weapon stats
- Tracks owned upgrades (stacks) and weapon states (levels)

### UEnemySpawnSubsystem (WorldSubsystem)
- Pools `ASurvivorEnemy` actors (`PreWarmCount=20`, `MaxEnemiesOnMap=350`)
- Weighted random spawn type selection from `DT_EnemySpawns` (time-gated unlock/deprecation)
- Time-based + responsive spawn rate (faster when below target count)
- Caches floor bounds from a `LevelFloor` brush to clamp spawn locations; exposes them via `GetFloorBounds()` / `HasFloorBounds()` for the flow field subsystem to reuse

### UFlowFieldSubsystem (WorldSubsystem)
- Path-aware enemy navigation. One BFS per update covers every enemy on the map (cost independent of horde size).
- Dijkstra flood-fill from player cell (8-direction, integer costs 10/14) at 10Hz, sized from floor bounds with cell auto-upscaling.
- Capsule-sweep obstacle pass at init (`ECC_WorldStatic` + `ECC_WorldDynamic`); call `RefreshBlockedCells` (or `ff.refresh` console) after spawning walls at runtime.
- Bilinear-sampled `SampleDirection` returns `ZeroVector` when off-grid / on blocked / pre-init — callers fall back to direct steering.
- Debug: `ff.debug 1` (arrows), `ff.debug 2` (+ blocked boxes), `ff.enable 0` (kill switch).
- Full spec: `memory/flowfield-pathfinding.md`

## Key Patterns

**Data-Driven Design**: Code defaults with DataAsset override option. Values in C++ for Claude to adjust directly, DataAssets for designer iteration.

**Component-Based**: Reusable AttributeComponent for any actor needing stats.

**Object Pooling**: XP gems recycled instead of destroyed/created.

**Event-Driven**: Delegates for health changes, death, XP gained.

## Default Gameplay Values

| System | Parameter | Default |
|--------|-----------|---------|
| Player | Pickup range | 500 |
| Player | Invulnerability | 0.5s |
| Weapon | RPM | 60 |
| Weapon | Damage | 10 |
| Weapon | Range | 1000 |
| Enemy | Health | 100 |
| Enemy | Damage | 10 |
| Enemy | Speed | 400 |
| Gem | Flee duration | 0.35s |
| Gem | Magnetize accel | 2000 |
