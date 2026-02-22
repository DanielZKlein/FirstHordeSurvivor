# FirstHordeSurvivor Architecture

UE5 C++ Vampire Survivors-style horde game with data-driven configuration.

## Project Structure

```
Source/FirstHordeSurvivor/
├── SurvivorGameMode.h/cpp       # Game initialization, subsystem setup
├── SurvivorCharacter.h/cpp      # Player: movement, XP, weapon spawning
├── SurvivorEnemy.h/cpp          # Enemy: chase AI, attacks, death/drops
├── SurvivorWeapon.h/cpp         # Auto-targeting weapon controller
├── SurvivorProjectile.h/cpp     # Projectile physics and hit detection
├── AttributeComponent.h/cpp     # Modular stat system (health, speed)
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
- Chase AI: direct pursuit toward player each tick
- RVO avoidance enabled for horde behavior
- Attack overlap sphere (150 radius), 1s attack interval
- Drops XP gems on death (greedy tier decomposition)
- Configured via `UEnemyData` DataAsset

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
