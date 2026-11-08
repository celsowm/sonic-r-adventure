# WP-06.1 — Conditional Stage Overlays (first playable vertical slice)

**Status:** Implemented for asset-free testing and the Adventure preview. Original Resort Island visual validation and Dreamcast/KOS validation are outstanding.

## Design

The original Island BIN and TER remain unchanged. AdventureWorld wraps the existing read-only terrain callbacks and adds authored static triangles. One activation bit controls BOTH drawing and collision, preventing invisible floors or visible nonsolid ice.

- AdventureWorld_Init attaches a legacy terrain provider, track, Act, weather and time.
- AdventureWorld_SetEnvironment selects Act (1–8), weather (clear/rain/snow) and time (sunrise/day/sunset/night) and updates every zone.
- AdventureWorld_LoadPackBytes validates a versioned compact pack **transactionally**; invalid input must leave the live world unchanged.
- AdventureWorld_LoadPackFile reads an optional user-generated asset with no heap, JSON or OBJ parser at runtime.
- AdventureWorld_MakeTerrain merges native TER and original approved BIN callbacks with active overlay contact, highest foot support, normals and capsule casts.
- AdventureZoneRender_Draw draws the same approved overlay triangles after the original scenery and before Sonic, using the existing depth-tested R_Draw backend and near-plane clipping.

## AZN1 v1 file contract

All numbers are little-endian, and all world points are signed Q20.12 positive-Y-up.

| Header offset | Type | Meaning |
| --- | --- | --- |
| 0x00 | 4 bytes | magic AZN1 |
| 0x04 | u16 | version 1 |
| 0x06 | u16 | legacy track ID (1–5) |
| 0x08 | u16 | zone count, maximum 4 |
| 0x0A | u16 | triangle count, maximum 64 |
| 0x0C | u32 | zone descriptors offset |
| 0x10 | u32 | triangle array offset |

A zone descriptor is 16 bytes: stable ID (u32, 1–255), Act mask (u8), weather mask (u8), time mask (u8), anchor flags (u8), first triangle (u16), triangle count (u16), reserved zero (u32). Anchor flag 1 means coordinates relative to the original spawn; 0 means absolute world coordinates. Each triangle has three vertices with three signed 32-bit Q12 coordinates = 36 bytes.

The initial limit of 4 zones / 64 triangles is **only for a vertical slice**, not a sufficient capacity for an archipelago. Later formats need a spatial grid/BVH, draw meshes/materials distinct from simplified collision, streaming and versioned asset migration. Do not silently interpret future pack versions as v1.

## Resort Island demonstration

The source file is [stages/resort_island/zones.json](../../stages/resort_island/zones.json). It authors a two-triangle **ice-blue test bridge** 6–22 world units ahead of Sonic, 6 units wide and 2 units below the original spawn. The bridge is available only in Act 1 and snow, at any time of day.

**This is intentionally synthetic demo geometry, not yet a visually surveyed location for Frozen Archipelago.** It does not claim to reach an actual new island. Without a generated asset file, the Adventure preview instantiates the same bridge programmatically as a visible physics test.

### Compile content

Run from the repository root:

    python3 tools/pack_adventure_zones.py stages/resort_island/zones.json --output /your/SonicR/ADVENTURE/ISLAND_ZONES.AZN

Then launch with the user's original files:

    ./source/sdl/build/sonicr --adventure-snow /your/SonicR

Use --adventure-rain to test disabled ice while loading the original rainy visual assets. The weather option is applied **before** the legacy stage textures are loaded. A future UI can choose environment and Act without restarting. On Dreamcast, the optional pack path uses the same /cd filesystem conventions; KOS build/runtime tests are still pending.

## Regression tests

Running make -C tests/adventure test now exercises:

- original deterministic game, collision and camera regressions;
- active zone support, normal lookup, capsule hits and player standing on ice;
- dry/rain/other-Act collision and render disablement;
- invalid pack offsets/counts/magic/truncation and rollback;
- the fallback demo zone and versioned Python packer output.

## Next increments

First inspect actual Resort Island on SDL and align the new bridge to the original BIN/TER (the supplied OBJ is an editor reference, not an exact collider). Next separate render mesh and collision proxy, integrate a spatial index and real island/cliff zones, add per-Act goals and checkpoints and test Dreamcast cost. No proprietary game resources are added to this repository.
