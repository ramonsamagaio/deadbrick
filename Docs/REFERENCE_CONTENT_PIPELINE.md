# DEADBRICK reference-content pipeline

The supplied LayOfTheLand build is a packaged UE5 build. Its gameplay package names are useful for architecture parity, while the visual content itself lives in cooked `.pak` / `.utoc` / `.ucas` containers.

## One-time local import

1. Close Unreal Editor.
2. Pull `main`.
3. Run `IMPORT_LOTL_REFERENCE_UE58.bat`.
4. Paste or drag the local `LayOfTheLand` folder that contains `Content\Paks`.
5. The importer uses UE 5.8 `UnrealPak` to inspect/extract the containers, preserves original `Content` paths, locally excludes imported reference folders from Git, and rebuilds DEADBRICK.

Logs are written under `ReferenceExtracted/Logs`.

## Runtime binding

DEADBRICK scans `/Game` through the Asset Registry. If compatible cooked content is present it automatically prefers supplied skeletal meshes and matching animation sequences for player/enemy placeholders. The city generator also searches for supplied door, window, container/crate/chest static meshes.

Reference doors/windows/containers are wrapped in `AReferenceDestructibleProp`: the original cooked mesh is used visually until it is broken, then its volume is converted into DEADBRICK voxel material and damaged into voxel debris.

## Important architecture rule

The structural city remains voxel-backed. Reused meshes are decoration/interactive prop shells, not replacements for the destructible wall/floor/road voxel field.

## Source build parity

Keep `Docs/PACKAGE_MANIFEST_FINDINGS.md` as the package-level parity map. Systems observed in the reference build must be tracked as KEEP, ADAPT, or REPLACE+ rather than silently omitted.
