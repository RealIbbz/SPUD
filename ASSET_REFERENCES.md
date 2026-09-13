# Asset reference support (local patch)

Save format version 3 adds asset references to SPUD's native property serializer.
Hard references to assets store `SPUDDATA_CLASSID_ASSET` followed by the full object
path. Null references and owned runtime objects retain their version 2 class-ID
encoding. The nested-object visitors never store or restore properties on shared
assets. Asset loads validate the resolved object against the property's class;
missing or incompatible assets are logged and the reference is cleared.

Soft object and soft class properties use `ESST_SoftObjectPath`, containing a path
string. Restoring them does not load the target. Arrays of soft references use the
same representation per element. Existing opaque collection serialization still
uses Unreal's property serializer; its object-path archive now handles nulls.

Version 2 runtime-object saves remain readable. Saves containing the new encoding
require this patched reader; do not open them with an unpatched SPUD build. Asset
paths still depend on the assets being available in the build and on redirects
being retained after renames. Missing references are reported by logging, not a
new transactional load-failure API.

Regression tests: `SPUDTest.AssetReferences`, `SPUDTest.LegacyV2References`, and
`SPUDTest.InvalidAssetReferences`, plus the existing SPUDTest suite. The tests cover
both restore paths, generic UObject asset references, nested structs, data-table
handles, arrays, maps, nulls, unresolved soft references, invalid hard references,
and owned runtime objects. The project mission regression is
`Scripts/validate_tactics_spud.py`.

## Validation in Tactics

Development Editor build and all 14 selected automation tests passed (10 SPUD,
3 difficulty, 1 spatial-bounds regression). A mission with 22 units saved and
restored after restarting the editor. Unit IDs, schema/data-table references,
definition/spec snapshots, inventory, equipment, and stats matched exactly,
including reduced health, ammunition consumed in 20 magazines, and 7 skill points.

The separate puppet lifecycle failure is handled in Tactics by
`UUnitRestoreSubsystem`: native finalization runs after SPUD's level restore,
reconstructs missing puppets and repairs awareness links before AI can plan.
Blueprint per-object callbacks no longer own core unit readiness. The plugin
exposes `IsRestoringGameState()` for native callers, including streamed levels.
Puppet serialization and the save format are unchanged by this lifecycle fix.
See `DataSource/AI/Restore.md` in the project for implementation and validation.
