# FirstHordeSurvivor - Claude Guidelines

## Hot Reload Notifications
- Always tell me if changes involved .h files (requires Unreal restart) vs .cpp only (can hot reload)

## Project Context
- UE5 C++ Vampire Survivors-style horde game
- Prefer code defaults with DataAsset override option for easy iteration
- XP gem visuals configured in XPGemSubsystem::InitializeDefaultVisuals()

## Code Style
- Keep gameplay values in code (not just DataAssets) so Claude can adjust them directly

## Documentation
- When you make changes to how systems in the game work, update the corresponding .md file or create one.
- When updating an .md file, always put a "last changed" with the date into it.
- When reading an .md file that's badly out of date, make sure to double check if the underlying code has changed or not.