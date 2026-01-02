# Blender Quick Reference

Quick reminders for creating geometric enemy meshes.

## Creating Basic Polyhedra

### Tetrahedron (d4)
1. Delete default cube (`X`)
2. `Shift+A` → Mesh → Cone
3. **Immediately** (don't click elsewhere) set in bottom-left panel:
   - Vertices: `3`
   - Radius 1: `1`
   - Depth: `1`
4. Panel disappeared? Press `F9` to reopen it

### Other Shapes via Cone
| Shape | Vertices |
|-------|----------|
| Tetrahedron | 3 |
| Square pyramid | 4 |
| Pentagonal pyramid | 5 |
| Hexagonal pyramid | 6 |

### Platonic Solids (Add-on)
1. Edit → Preferences → Add-ons
2. Search "Extra Objects"
3. Enable "Add Mesh: Extra Objects"
4. Now: `Shift+A` → Mesh → Math Function → Regular Solid
   - Tetrahedron, Octahedron, Dodecahedron, Icosahedron

## Export to UE5

1. Select mesh
2. File → Export → FBX
3. In export settings:
   - Scale: `1.0` (adjust in UE5 via EnemyData.MeshScale instead)
   - Forward: `-Y Forward`
   - Up: `Z Up`
4. Import FBX into `Content/Enemies/Meshes/`

## Tips
- UE5's EnemyData has `MeshScale` property - use that for sizing, not Blender scale
- Keep meshes at origin in Blender
- Apply transforms before export: `Ctrl+A` → All Transforms
