# FirstHordeSurvivor - Claude Guidelines

## Hot Reload Notifications
- Always tell me if changes involved .h files (requires Unreal restart) vs .cpp only (can hot reload)

## Project Context
- UE5 C++ Vampire Survivors-style horde game
- Prefer code defaults with DataAsset override option for easy iteration
- XP gem visuals configured in XPGemSubsystem::InitializeDefaultVisuals()

## Code Style
- Keep gameplay values in code (not just DataAssets) so Claude can adjust them directly

## Blueprint / MCP Usage
- Only use the Unreal MCP tools to **read** Blueprints or **write non-Blueprint assets** when specifically instructed
- For Blueprint changes, just tell the user what to wire up — the MCP is too slow for Blueprint writes
- Default: describe the Blueprint wiring in plain text (node chain format)

## Game TODO
- The project TODO lives in the Obsidian vault so it's accessible from any device:
  `C:\ObsidianVault\Primary Remote Vault\Tiny Habits\Pinball Survivors TODO.md`
- When completing tasks, mark them done in that file. When discovering new work, add it there.
- The TODO is organized by system (Upgrades, Weapons, Enemies, etc.) — keep that structure.

## Documentation
- When you make changes to how systems in the game work, update the corresponding .md file or create one.
- When updating an .md file, always put a "last changed" with the date into it.
- When reading an .md file that's badly out of date, make sure to double check if the underlying code has changed or not.
- If you have to run searches across the file system, using find or ls or any other tools, update a relevant .md file leaving reminders for yourself in future so you won't have to run the same search again.