# WP-04: Vertical Physics and Terrain Support — First Integration

**Status:** Initial implementation; action-platformer collision coverage is not complete.

## Implemented

- Portable `AdventureTerrain` query callback: height, surface ID, layer and explicit success status, in canonical Q20.12 with positive Y up. A floor at Y=0 remains a valid collision.
- Legacy `.TER` adapter reuses the original spatial grid, `PointInPolygon` and `InterpolateGroundHeight`, but never calls race movement or mutates a `Player` during floor queries. The original polygon helper uses its own scratch globals.
- Support lookup filters candidates to those below the maximum step-up height; selection chooses the highest qualifying floor, so a lower bridge surface does not teleport a player onto an overhead platform.
- Player ground/support state lives in `AdventurePlayer`, not in the rigid legacy `Player` ABI. Only the rendering bridge transfers position, velocity, animation-facing and support-related presentation fields.
- Fixed 60-Hz gravity, terminal velocity, rising/falling state, jump impulse, *coyote time*, input edge detection and short buffered jump.
- A downward foot-crossing test checks whether a falling player's previous-to-next vertical position crosses an eligible supporting floor, preventing simple vertical tunneling through narrow platforms.
- Spawn snapping and grounded ramp movement, within limited step-height and snap thresholds.
- Asset-free synthetic fixture tests for a valid ground at height zero, ramp, upper/lower stacked surfaces, ledge coyote jump, jump/landing and deterministic replay.

## Collision boundaries and remaining risks

This is deliberately **not** the full collision solution specified in the master plan.

- **No walls or ceilings:** a player can currently pass through vertical scene geometry or jump up through a ceiling. Capsule sweeps and collision mesh support are a separate next step.
- **Only floor surfaces:** loop/ride surfaces, special dynamic geometry, moving platforms and destructible items need their own explicit providers.
- **Downward sweep is vertical only:** high-speed horizontal movement can still tunnel through thin obstacles; a capsule cast should be used before permitting high-speed gameplay.
- **No slope normals:** ramp elevation follows `.TER` geometry but slope-dependent friction, walkability thresholds and steep-slope sliding are not yet implemented.
- **No respawn checkpoint or death plane:** falling off the stage currently has no recoverable action-state exit.
- **No gameplay animation mapping:** jump and fall phases have pure simulation states but render animation still needs integration.
- **Visual verification outstanding:** the sign convention for legacy terrain vertex Y, the renderer-facing camera and collision layer semantics should be checked on real Resort Island assets, SDL and Dreamcast.
- The `.TER` adapter walks spatial polygon lists assuming valid bounds, consistent with the legacy loader. Malformed-asset hardening and surface normals remain to do.
- The original legacy race track bootstrap still initializes some race-related data before branching into the new game mode; simulation does not invoke race physics.

## Reproduce

```sh
make -C tests/adventure test
make -C source/sdl -j2
# With your legally supplied Sonic R data:
source/sdl/build/sonicr --adventure /path/to/SONICR-data
```

Dreamcast (KallistiOS required):

```sh
make -C source/dc clean
make -C source/dc ADVENTURE_BOOT=1
```

Controls: WASD/arrows or left stick move; Space / A jumps; ESC or START exits preview. Roll, camera controls and Homing Attack are not implemented.

## Next steps

1. Verify `.TER` height and floor identity on actual Resort Island geometry; capture signed coordinate traces on both platforms.
2. Implement walkable slope normals and surface-angle limits.
3. Add capsule sweeps for horizontal walls and ceilings, including safe iterative contact resolution and sliding.
4. Add moving platform support and reliable transitions between collision layers.
5. Add death-plane/respawn and action animations before claiming the vertical slice is playable.
