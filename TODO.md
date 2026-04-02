# Next Session TODO

## Bugs
- [ ] **Enemy stacking blocks movement** — enemies that stack too closely together stop each other from moving; a larger horde is less threatening than a smaller horde because it effectively moves at the speed of its slowest members

## Design Problems
- [ ] **Swarm damage not threatening enough** — only one enemy seems to damage the player at a time, and the per-enemy damage timer is very generous. Getting swarmed and overwhelmed should be a death warrant. Need to tune: multiple simultaneous damage sources, tighter damage intervals, or scaling damage with enemy count in contact
- [ ] **Upgrade RNG can shut out damage scaling** — fully random upgrade selection means if early levels only show defensive upgrades, player falls behind the power curve and can't earn XP. Need some mechanism to guarantee damage scaling options (weighted categories? pity timer for damage upgrades? guaranteed weapon upgrade every N levels?)

## Design Work Needed
- [ ] **Enemy spawning pacing** — spawning happens too quickly, scales too quickly, too uniform. Waves would be better; or waves on top of the current trickle rate. Design wave structure and pacing curve
- [ ] **Ramming / contact combat design** — this is a game about movement; ramming enemies should be situationally useful. Need to design stats and upgrades around this. At base level (no stats/upgrades): what happens when player runs into enemy at speed vs when enemy creeps up on player? Impact system exists but needs full design pass
- [ ] **Enemy attacks instead of touch damage** — enemies might trigger shaped attacks (circle/circle segment/line/rectangle) with very little delay once in range. Opportunities: "move fast enough to evade", "enemies can hurt other enemies so darting in to bait attacks becomes viable". Design attack shapes, timings, and friendly-fire rules
- [ ] **Movement feel overhaul** — current short linear acceleration curve is wrong. Goals:
  - Very immediate acceleration from standing start
  - Slow continued acceleration once at cruising speed
  - Decreased maneuverability at higher speeds
  - Harder to slow down and turn at high speed (momentum)
  - Feathering move button in move direction maintains speed (null inputs don't quickly return to standing still)
  - Keep highly tunable — not slaves to physical realism or plausibility
- [ ] **Pinball level elements** — bouncers, jump pads, channels, magnetic slingshots, attractors, repulsors, auto-triggered flippers. Figure out movement and enemy collision ruleset first before building these

## Upgrades
- [ ] Make upgrades real again and create a bunch more
- [ ] Create weapon upgrades for the throwing knife
- [ ] Need pickup radius and XP incoming upgrades
- [ ] Kiss/curse upgrade idea: one good effect + one bad effect in a single upgrade

## Weapons
- [ ] Create more weapons — rocket launcher next (model already downloaded)
- [ ] Impact VFX now configurable per-weapon via DataAsset (ImpactSound/ImpactVFX) — set these up for existing weapons

## UI
- [ ] Stat display window in upgrade screen
- [ ] Pause screen with same stat info
- [ ] Pause screen: damage done by each weapon (total / last minute / DPS)

## Polish / Assets
- [ ] Find better shoot VFX/SFX
- [ ] Find better missile mesh for projectiles
- [ ] Move actually-used assets from marketplace folders into dedicated Content/Assets folder (for cleaner git tracking)

## Completed
- [x] XP gem visual system with DataAsset + code defaults fallback
- [x] Emissive glowing gem material with per-tier colors and brightness
- [x] Git repo setup with .gitignore for large marketplace assets
- [x] Multi-shot mode configurable at ProjectileCount=1 for upgrade readiness
- [x] Impact sound/VFX added to weapon DataAsset (overrides BP defaults)
- [x] PickupRadius added as proper EPlayerStat + FGameplayAttribute (upgradeable)
- [x] AttackSpeed base value changed from multiplier to actual RPM (consistent with all other stats)
- [x] EWeaponStat tooltips fixed (explicit UMETA ToolTip instead of comments)
- [x] **Fixed projectile self-destruct bug** — `ASurvivorProjectile::SphereComp` had `SetGenerateOverlapEvents(true)` in the constructor, causing `OnOverlapBegin` to fire mid-construction and `SpawnActor` to return null when enemies overlapped the player. Fix: disable overlap events in constructor, re-enable via `SetTimerForNextTick` lambda in `BeginPlay`
- [x] Added null warning log to `SurvivorWeapon.cpp` for when `SpawnActor` returns null
- [x] Cleaned up ~21 noisy debug `UE_LOG` statements across `EnemySpawnSubsystem.cpp`, `SurvivorEnemy.cpp`, `SurvivorCharacter.cpp`, `UpgradeSubsystem.cpp`, `UpgradePanelWidget.cpp`
- [x] Added `set_asset_properties` MCP tool to the flopperam UnrealMCP plugin (allows writing DataAsset properties via MCP)

---
*Last updated: 2026-04-02*
