# FirstHordeSurvivor - Claude Guidelines

## Hot Reload Notifications
- Always tell me if changes involved .h files (requires Unreal restart) vs .cpp only (can hot reload)

## Project Context
- UE5 C++ Vampire Survivors-style horde game
- Prefer code defaults with DataAsset override option for easy iteration
- XP gem visuals configured in XPGemSubsystem::InitializeDefaultVisuals()

## Code Style
- Keep gameplay values in code (not just DataAssets) so Claude can adjust them directly
