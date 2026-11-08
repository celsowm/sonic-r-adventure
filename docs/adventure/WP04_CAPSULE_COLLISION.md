# WP-04.7 — Full-height vertical capsule vs authored static roof triangles

**Scope:** fixed-point continuous collision *detection* for the whole vertical capsule (shaft, rounded top and rounded bottom) against individually selected `.BIN` roof triangles. This increment is **not** a complete character controller against arbitrary meshes. No proprietary game data or extracted polygons are committed.

## Geometry / narrow phase

A capsule of total height `height` and radius `radius` consists of the vertical segment from `footY + radius` to `footY + height - radius`, expanded uniformly by a sphere of radius `radius`. At each trial time `t` along a translation:

- Project both segment endpoints to the triangle plane; when their projection lies inside the finite face, the plane separation is a closest-feature candidate.
- Test whether the vertical capsule shaft itself pierces the triangle face (zero segment-triangle distance), including the case where the two rounded endcaps never touch.
- Compute the closest points between the vertical segment and each of the triangle's three finite edges. A finite set of quadratic-region candidates — endcaps, XZ shaft projection, endpoints and Y clamp thresholds — gives the minimum over each edge. This includes all three triangle vertices.
- Take the minimum segment-triangle distance and compare it with the squared radius to decide overlap.
- The separation distance between two convex objects under translation is convex in time. A bounded integer ternary search brackets its minimum; first-contact binary refinement returns the earliest intersecting Q12 tick fraction. Ties preserve the middle interval to accommodate fixed-point plateaus.
- The contact carries a Q12 unit normal from the roof toward the capsule. No `float`, heap allocation, racing physics or animation ABI is used.

Distance evaluation uses Q6 local coordinates to bound 64-bit products. Authored triangle edges must fit within 8,192 whole world units, and capsule height/radius are bounded (256/32 units). The implementation rejects invalid body dimensions, zero-area triangles and overly steep roof faces. It is deterministic at Q12 tick resolution, **not** mathematically exact sub-Q12 time of impact.

## Gameplay integration

- `AdventureTerrain.sweep_capsule` is a read-only optional callback, replacing the previous head-only roof callback.
- During a **rising** jump, the controller compares the earliest capsule/triangle hit with the existing TER floor-underside sweep. The earlier collision wins. The body is positioned at impact plus a small skin along the hit normal, and upward velocity is zeroed.
- During **grounded horizontal movement**, a shaft or lower-endcap encounter with a selected roof triangle stops the movement at the earliest hit (even without jumping). This prevents walking through a low static roof or obstacle edge.
- The old explicit `AdventureRoof_SweepTriangle` head-sphere primitive is retained as a small reusable geometric test, but gameplay uses `AdventureCapsule_SweepMesh`, not head-only collision.
- No visual collider is automatically selected. The `adventure_roof_legacy.c` manifest remains empty until a visual, static-solid review.
- The original `.TER` still supplies ground/ramp/ledge support and walls; the horizontal legacy wall sweep is unchanged.

## Validation

Portable fixtures cover: rounded top impact from below, rounded bottom impact from above, full shaft hitting the side of a triangle while both endcaps miss the face, edge/vertex grazing, high-speed full-width tunnel crossing, separation when leaving a touching edge, earliest hit from multiple roofs, deterministic time/contact normal, and grounded game tick blocked by a low roof.

```sh
make -C tests/adventure test
make -C source/sdl -j2
```

## Remaining limitations

- **Contact response** currently stops remaining motion after the hit rather than implementing fully iterative three-dimensional sliding and stepping.
- **Falling/landing on authored BIN roofs:** the new general capsule primitive can detect hits from above, but the gameplay integration is initially limited to upward flight and grounded sideways movement. Support on visual-only roofs needs a roof-aware ground/support provider, otherwise Sonic would immediately fall in later ticks.
- A selected `.BIN` face is considered physical only when explicitly authored. Arbitrary decoration geometry remains visual-only.
- Complex dynamic objects, mesh rotations, overhang walking, capsule interactions with TER wall vertical extents, slopes/normal-dependent friction, zero-velocity spawn depenetration, and physics performance profiling on real Dreamcast hardware remain unverified.
- This is convex-per-triangle narrow phase using fixed integer precision; degenerate and extremely large inputs are rejected rather than silently overflowing.

Do not claim a complete 3D character controller or verified on-hardware framerate from this work.
