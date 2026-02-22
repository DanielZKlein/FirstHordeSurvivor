# Next Session TODO

## Upgrades
- [ ] Make upgrades real again and create a bunch more
- [ ] Create weapon upgrades for the throwing knife
- [ ] Need pickup radius and XP incoming upgrades
- [ ] Kiss/curse upgrade idea: one good effect + one bad effect in a single upgrade

## Weapons
- [ ] Create more weapons — rocket launcher next (model already downloaded)
- [ ] Impact VFX now configurable per-weapon via DataAsset (ImpactSound/ImpactVFX) — set these up for existing weapons

## Enemy Movement & Collision
- [ ] Enemies blocking each other — faster enemies get stuck behind slower ones. Solutions: push through? passthrough? avoidance?
- [ ] Enemy speeds feel low; maybe player starting speed is too high? Review movement settings
- [ ] Player touching enemies feels floaty — should slow down or stop, or add collision (but player must always be able to push through)

## Impact System (New)
- [ ] Implement impact mechanic: player moves into enemy → knocks them back + damages them; player takes some damage too
- [ ] Tie impact to landing / bumping into things?

## Movement System
- [ ] Movement control doesn't exist yet — figure out what it should be
- [ ] Think about unique movement system and cross-pollination:
  - Weapon damage / rate of fire scaling with current movement speed?
  - Armor scaling with current move speed?
  - Armor/dodge scaling with agility?
  - Dodge chance from agility?

## Level Design
- [ ] Start thinking about level geo and obstacles: bumpers, flippers, attractors, grooves, rails, ramps

## UI
- [ ] Stat display window in upgrade screen
- [ ] Pause screen with same stat info
- [ ] Pause screen: damage done by each weapon (total / last minute / DPS)

## Polish / Assets
- [ ] Find better shoot VFX/SFX
- [ ] Find better missile mesh for projectiles
- [ ] Move actually-used assets from marketplace folders into dedicated Content/Assets folder (for cleaner git tracking)

## Bug Fixes
- [x] ~~Investigate why XP gems are so large~~ (Fixed: Scale controlled via DataAsset/code defaults)
- [x] ~~Investigate why XP isn't being added to the player~~ (Fixed: XP collection and leveling working correctly)

## Completed
- [x] XP gem visual system with DataAsset + code defaults fallback
- [x] Emissive glowing gem material with per-tier colors and brightness
- [x] Git repo setup with .gitignore for large marketplace assets
- [x] Multi-shot mode configurable at ProjectileCount=1 for upgrade readiness
- [x] Impact sound/VFX added to weapon DataAsset (overrides BP defaults)

---
*Last updated: 2026-02-21*
