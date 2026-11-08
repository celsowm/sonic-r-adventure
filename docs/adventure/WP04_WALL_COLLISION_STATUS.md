# WP-04.2 — Swept Wall Collision / Slide

**State:** implementation committed separately from the original racing collision path; CI and real-stage validation required.

## Architecture

- The portable `AdventureCollision_SweepDiscSegment` detects the earliest overlap of a moving Q20.12 horizontal disc and a finite wall segment using bounded deterministic substeps plus binary refinement of the time-of-impact.
- The body is represented by a vertical capsule **footprint** of one world unit radius and four units height. The height of the capsule is reserved in the provider API; only horizontal sweeps are implemented in this increment.
- `AdventureCollision_MoveAndSlide` resolves up to three consecutive contacts per 60-Hz simulation tick, stops at the nearest obstacle, adds a separation skin, then projects the remaining displacement along the wall tangent.
- `AdventureTerrain_SweepLegacyWall` reads the native .TER grid, `TerCollisionMesh` mask/layer, center/radius broad phase and `g_terEdgeList` wall segments.
- A 4096-bit per-query visited set prevents duplicate wall processing, while a separate 1024-bit visited-cell mask prevents rescanning grid entries. Sweep sampling adapts to half the local grid-cell span and checks adjacent cells for boundary collisions.
- Finite-height wall gates are conservatively ignored only when the player's feet are above both endpoint heights. Walls without gates are treated as vertically unbounded.
- All wall collision queries are **read-only**. In particular, the legacy `WallCollision`, `WallBounce`, `WallPushback` and soft-wall mask mutation paths are never invoked.
- The normalized input, vertical terrain support, coyote jumping and original chase-camera preview remain separate from the racing simulation.

## Tests

Portable, asset-free tests cover opposing wall normals, high-speed perpendicular wall crossing, near-endpoint misses, moving away from a wall, tangential slide, two-wall corners, and horizontal gameplay blocked by a synthetic wall.

```sh
make -C tests/adventure test
make -C source/sdl -j2
```

GitHub workflow: [Adventure gameplay and desktop build](../../.github/workflows/adventure-tests.yml).

## Incomplete by design

- **Ceilings are not yet handled.** Legacy .TER wall meshes are polylines with optional height gates, not arbitrary 3D capsule sweep geometry. Overhead triangles and capsule head sweeps need a separate vertical query provider.
- The capsule's declared height is not yet used for full Y-interval wall intersection, slope-walkability nor climbing/steps.
- Collision uses conservative advancement with bounded integer sampling, not a mathematical analytic time-of-impact solution. Very tiny or pathological geometry at extreme velocities requires additional fixtures and empirical tuning.
- Radius / capsule height / acceleration constants are provisional; not tuned to Sonic R's camera scale.
- Moving geometry, destructibles, water, loop surfaces, stage exits, checkpoints and respawn are out of scope.
- World coordinate signs, mesh masks, stage perimeter wall coverage and performance require validation using the original game data on SDL and actual Dreamcast hardware.

**Do not describe WP-04 as complete or the stage as fully playable until ceiling/obstacle sweeps and platform runtime verification are done.**
