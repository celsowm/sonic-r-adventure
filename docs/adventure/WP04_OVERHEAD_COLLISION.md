# WP-04.3 — Upward head collision against elevated .TER floors

**Scope:** portable upward head sweep, shared read-only .TER queries and synthetic tests; no proprietary data committed.

## Real-data findings

Inspected the five .TER files provided in the original Windows archive. Their surface/face records primarily describe floors rather than a separate ceiling mesh. Across the five files there are 5,998 face entries; 5,997 have negative legacy Y normals and the single positive-normal entry is an anomalous record at the end of EMERALD's face table. Therefore the Adventure controller deliberately treats the **underside of elevated walkable terrain polygons** as a platformer ceiling, rather than requiring a nonexistent dedicated ceiling table.

All five grids contain 1,024 cells. The number of cells referencing at least two ground-candidate polygons (not necessarily at exactly the same X/Z) is:

| Stage | Cells with 2+ floor candidates | Largest candidate list |
| --- | ---: | ---: |
| Resort Island | 304 | 22 |
| Radical City | 364 | 15 |
| Regal Ruin | 412 | 24 |
| Reactive Factory | 292 | 27 |
| Radiant Emerald | 308 | 12 |

These are **spatial-index candidate counts**, not verified overlapping collision surfaces. An actual height hit still requires the original polygon containment test and interpolated Y.

## Implementation

- `AdventureTerrain.query_overhead` takes the current head height and returns the **lowest qualifying** floor Y at or above it. It is independent of the `query_ground` callback that returns the **highest qualifying** floor Y below the feet.
- `adventure_terrain_legacy.c` shares one grid traversal between support and overhead. It rejects negative grid offsets before integer division (which otherwise rounds toward zero), guards missing data and returns explicit success/failure independent of height zero.
- On an ascending tick, if the head crosses an overhead surface, vertical position is capped below that platform with a small Q12 separation skin, vertical velocity becomes zero, and gravity takes over on subsequent ticks. Previous foot height is a lower bound so a touching head does not push Sonic underground.
- The overhead check uses canonical **positive-up** coordinates, while the original `.TER` helper is still solely responsible for converting source legacy negative-up vertex Y.
- The collision query ignores the currently supporting floor because Sonic's head starts above his own feet.
- Synthetic fixtures test nearest overhead selection in 0/20/30-unit stacked floors, head impacts from below and from a second-level platform, subsequent landing on the correct lower floor, jumping with no overhead, and deterministic replay.

## Not solved

- This implementation queries the overhead floor at the **new horizontal position** for the vertical head sweep. A full 3D swept capsule/triangle narrow-phase is still required for diagonal high-speed impacts and complex mesh ceilings.
- Original walls with height gates still use a separate horizontal disc provider, and do not define roof geometry.
- The original stage art `.BIN` can include scenery with visually apparent roofs that do **not** exist as solid `.TER` floors. Such scenery is not automatically collidable.
- Moving platforms, two-sided step support, roof normals, climb/sliding and camera occlusion are not implemented.
- The original supplied archive was inspected offline; visual gameplay and Dreamcast hardware remain unverified.

## Build and validation

```sh
make -C tests/adventure test
make -C source/sdl -j2
```

The GitHub Action for both jobs is `adventure-tests.yml`; no game data is required in CI.
