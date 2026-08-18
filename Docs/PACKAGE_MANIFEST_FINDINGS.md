# Supplied build: package-manifest findings

This note records architecture clues visible in the supplied packaged Unreal build. These are asset/package names from the `.utoc` index, not extracted source code.

## Voxel stack observed

- `BP_MainVoxelWorld`
- `BP_PhysicsVoxelWorld`
- `BP_PropVoxelWorld`
- `BP_EmptyVoxelWorld`
- `BP_MainVoxelGenerator`
- `BP_EmptyVoxelGenerator`
- `BP_VoxelManager`
- `BP_VoxelItemComponent`
- `NS_VoxelPhysics`
- `NS_SimulationVoxelEffects`
- `M_VoxelSolid`
- `M_VoxelNonSolid`
- `M_FluidVoxel`
- `M_GasVoxel`
- `M_FireVoxel`
- `M_InsideVoxel`

### Inference

The reference appears to separate ordinary world voxels, physics/simulated voxels, prop voxel worlds and generation/management. DEADBRICK should mirror that separation conceptually: persistent static chunk data, detached physics islands/rubble, and transient simulation cells should not all live in one expensive representation.

## Procedural world stack observed

- `BP_RoadGenerator`
- `BP_UndergroundStrataGenerator`
- `BP_CaveGenerator`
- many cave-detail generators such as rock, column, crystal, mushroom, vine and biome-specific generators
- `BiomeMeshSpawners_Struct`
- `BiomeWeatherStates_Struct`
- `BP_StructureManager`
- `BP_StructureEditor`
- `BP_StructureMeshSpawnerEditor`

### DEADBRICK translation

Use layered specialized passes instead of one giant generator. Proposed order:

1. region seed and macro terrain
2. district zoning
3. arterial/secondary roads
4. block subdivision
5. lots and building archetypes
6. building shells and floor plans
7. underground utilities, basements, sewers and subway/service tunnels
8. structural damage/abandonment pass
9. props and parked/wrecked vehicles
10. loot economy
11. zombie population and survivor-event history
12. weather, utilities and simulation state

## Building stack observed

The manifest exposes a dedicated `BP_BuildingManager`, a voxel building preview generator, custom prefab UI, transform gizmos and procedural preview shapes including box, sphere, cylinder, cone, capsule and hexagonal variants.

DEADBRICK should keep the same strength: fast macro tools plus fine voxel editing. Urban additions should include wall/floor/room drawing, stair/elevator shafts, road/path tools, barricade painting, blueprint copy/paste and reusable player prefabs.

## Crafting / interaction stack observed

The build contains dedicated crafting recipe widgets and data, physical item components, smelteries, anvils, cooking effects/stations, chests and item managers. The Steam description also confirms physical placement-based crafting.

DEADBRICK adaptation:

- field crafting from nearby loose items
- workbench recipes
- dismantling/salvage
- firearm repair and gunsmithing
- manual ammo loading/repacking
- electrical/electronic assembly
- cooking and water purification
- medicine/first aid
- vehicle repair

## Simulation clues observed

The package contains flowing-water assets, water nav filters, underwater post process, gas voxels, fire voxels, on-fire status effects and water-extinguish effects. Voxel physics has dedicated effects and query systems.

DEADBRICK urban simulation targets:

- pipes and hydrants flooding interiors/basements
- smoke and toxic-gas accumulation
- natural-gas leaks and ignition
- spreading structural/furniture/vehicle fires
- loose rubble and detached structural islands
- electrical hazards in wet areas
- alarms/noise propagating into the zombie director

## Manager-level systems observed

- `BP_SaveManager`
- `BP_AIManager`
- `BP_ItemManager`
- `BP_AmbienceManager`
- `BP_MusicManager`
- `BP_TutorialManager`
- `BP_MainGameMode`
- `BP_MainGameInstance`
- `BP_MainPlayerController`

This supports a subsystem-oriented architecture for DEADBRICK instead of allowing generation, persistence, AI and simulation to become tightly coupled.
