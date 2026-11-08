# WP-04: Action Platformer Collision — Iterative Integration

**Status:** Floor/roof support, static-wall sweeps, capsule contact detection and bounded 3D sliding for authored static roofs are integrated. A unified geometry solver, stage-runtime verification and KOS validation remain outstanding. See [wall sweeps](WP04_WALL_COLLISION_STATUS.md), [overhead collisions](WP04_OVERHEAD_COLLISION.md), [diagonal sweeps](WP04_DIAGONAL_SWEEP_AND_BIN_AUDIT.md), [opt-in BIN roofs](WP04_EXPLICIT_BIN_ROOFS.md), [capsule contacts](WP04_CAPSULE_COLLISION.md), [3D sliding and BIN roof support](WP04_SLIDE_AND_ROOF_SUPPORT.md), and [original TER audit](REAL_TER_AUDIT_2026-11-08.md).

## Implemented

- Portable `AdventureTerrain` query callback: height, surface ID, layer and explicit success status, in canonical Q20.12 with positive Y up. A floor at Y=0 remains a valid collision.
- Legacy `.TER` adapter reuses the original spatial grid, `PointInPolygon` and `InterpolateGroundHeight`, but never calls race movement or mutates a `Player` during floor queries. The original polygon helper uses its own scratch globals.
- Support lookup filters candidates to those below the maximum step-up height; selection chooses the highest qualifying floor, so a lower bridge surface does not teleport a player onto an overhead platform.
- Player ground/support state lives in `AdventurePlayer`, not in the rigid legacy `Player` ABI. Only the rendering bridge transfers position, velocity, animation-facing and support-related presentation fields.
- Fixed 60-Hz gravity, terminal velocity, rising/falling state, jump impulse, *coyote time*, input edge detection and short buffered jump.
- A bounded, sampled 3D trajectory sweep for the head/feet checks intermediate X/Z positions during a fixed tick and refines the first floor/underside contact; this avoids skipping many narrow platforms during a diagonal jump. It is **not** an analytic full-capsule/triangle sweep.
- Spawn snapping and grounded ramp movement, within limited step-height and snap thresholds.
- Asset-free synthetic fixture tests for a valid ground at height zero, ramp, upper/lower stacked surfaces, ledge coyote jump, jump/landing and deterministic replay.

## Collision boundaries and remaining risks

This is deliberately **not** the full collision solution specified in the master plan.

- **Partial integration:** Static .TER walls use swept discs; TER floors use sampled head/foot trajectories; and explicitly authorized BIN roof faces use full-height swept capsules with shaft/cap/edge/vertex contacts. Only BIN-static contacts have bounded 3D slide response. No visual roofs are approved by default, and arbitrary scenery/dynamic geometry are excluded.
- **Special meshes excluded:** loop/ride surfaces, special dynamic geometry, moving platforms and destructible items still need explicit collision providers.
- **Solvers are not unified:** Selected BIN triangles can support landing and subsequent grounded movement, and rising/grounded BIN collisions use iterative 3D sliding. TER walls, TER floors and airborne falling-side contacts still use separate resolution paths. Radial TER floor-edge and thin dynamic geometry contacts remain incomplete.
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
3. Unify the TER wall/floor and authored BIN capsule contact responses, validate sliding during descent, and author solid roof entries only after visual review.
4. Add moving platform support and reliable transitions between collision layers.
5. Add death-plane/respawn and action animations before claiming the vertical slice is playable.
