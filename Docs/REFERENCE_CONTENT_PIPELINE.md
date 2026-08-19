# DEADBRICK reference-content pipeline

The supplied LayOfTheLand build is a packaged UE5 build. Its package names are useful for architecture/behavior parity, while visual content lives in cooked `.pak` / `.utoc` / `.ucas` containers.

## Rule: never mount recovered cooked packages as editor assets

Cooked game `.uasset/.uexp/.ubulk` files are not treated as source/editor packages. They must **not** be copied directly under DEADBRICK `Content`. Doing so caused `Invalid value for PACKAGE_FILE_TAG`, broken material resolution and widespread Asset Registry errors.

`REBUILD_AND_OPEN_UE58.bat` now validates `Content` first. Untracked packages with invalid Unreal headers are moved into `ReferenceExtracted/QuarantinedContent/<timestamp>` rather than deleted. Tracked invalid packages are left untouched and reported.

## One-time local reference pipeline

1. Close Unreal Editor.
2. Pull `main`.
3. Run `IMPORT_LOTL_REFERENCE_UE58.bat`.
4. The script auto-detects the Steam Lay of the Land installation when possible, or asks for the game / `Content\Paks` path.
5. `retoc` converts IoStore/Zen data into an isolated legacy reference pak under `ReferenceExtracted/Legacy`.
6. Cooked files stay under `ReferenceExtracted` only.
7. `EXPORT_LOTL_EDITOR_ASSETS.ps1` uses CUE4Parse.CLI against the isolated pak and exports selected player/character/hands/weapons/props/material-related content to normal GLB/PNG files under `ReferenceExported`.
8. DEADBRICK is rebuilt.
9. Unreal's Python editor importer imports the normal files into `/Game/ReferenceImported`.
10. Runtime reference resolvers can then bind those **editor-valid** assets normally through Asset Registry.

Logs/markers are written under `ReferenceExtracted/Logs` and `Saved`.

## Runtime binding

`DeadbrickReferenceAssets` scans only valid `/Game` Asset Registry entries. It never attempts `StaticLoadObject` on arbitrary cooked files from disk.

The player tries to bind, in order:

- a compatible reference body skeletal mesh,
- a first-person arms/hands skeletal mesh,
- a firearm/weapon static mesh,
- matching animations when Unreal-valid animation assets exist.

Until a matching imported asset exists, the gameplay capsule remains authoritative and a visible first-person fallback viewmodel is used so the player is never an invisible camera.

The city generator can similarly prefer editor-valid door/window/container/prop assets. These visual shells do not replace the destructible voxel backing.

## Behavior parity

The packaged build does not expose original game source code. DEADBRICK therefore reproduces observed behaviors and architecture rather than pretending package names are source. `Docs/PACKAGE_MANIFEST_FINDINGS.md` remains the package-level parity map for world voxels, physics voxels, simulation, generation, interaction, managers and persistence.

## Important architecture rule

The structural city remains voxel-backed. Reference meshes are visual/interactive layers over DEADBRICK's own destructible world representation. Static chunk data, detached physics islands and transient fluid/gas/fire simulation remain separate systems, mirroring the separation visible in the Lay of the Land package stack.
