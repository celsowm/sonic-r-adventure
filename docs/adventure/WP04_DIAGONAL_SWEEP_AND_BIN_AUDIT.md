# WP-04.4 — Diagonal foot/head sweep and visual-geometry audit

**Source:** User-provided original Windows game ZIP, inspected locally. The archive, original meshes and raw polygon records are **not committed**.

## What the actual stage BINs contain

The track loader `track_geometry.c` distinguishes *track parts* (quad strips) from *decorations* (triangles and quads with separate positions, pivots and rotations). The data contain no general, per-polygon "solid roof" collision flag. Forcing every roof-looking visual face to be solid would also turn decorative canopies, transparent scenery and rotating ornaments into unintended obstacles.

| Stage | Track parts | Track quads | Horiz.-oriented track quads¹ | Double-sided track quads² | Decoration objects | Decoration tri / quad |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Resort Island | 390 | 3,879 | 2,622 | 394 | 218 | 747 / 6,976 |
| Radical City | 692 | 6,110 | 4,649 | 139 | 324 | 90 / 8,182 |
| Regal Ruin | 660 | 4,803 | 2,928 | 125 | 347 | 198 / 8,277 |
| Reactive Factory | 635 | 6,037 | 4,412 | 51 | 352 | 139 / 8,190 |
| Radiant Emerald | 811 | 8,968 | 5,185 | 1,019 | 81 | 978 / 3,839 |

¹ Derived for tri of first three quad-strip vertices with `normalY² / ||normal||² >= 0.7`. These counts are **orientation candidates**, not actual physical ceilings or disjoint roof meshes. Degenerate triangles are excluded.

² Based on the first bit of the original face flags as read by the existing track loader, **not** a "solid" designation.

**Decision:** Do not generate collision by indiscriminately copying visual `.BIN` quads. Until a distinct authorable collision asset or verified mapping from visual roof geometry exists, retain the original `.TER` as authoritative for supporting floors and underside contacts. A future explicit per-stage collider set can add selected solid roof triangles while ignoring decorations.

## What changed in gameplay

Before this increment, a rising/falling Sonic queried the `.TER` only at the **end** of a 60-Hz tick. A narrow ledge between the old and new horizontal positions could be missed despite crossing its Y level. In particular, diagonally moving through an elevated roof could put Sonic on its far side without stopping the jump.

`AdventureVertical_Sweep()` is a portable, deterministic integer-only trajectory routine. It:

1. Samples the full X/Z path at nominal quarter-world-unit spacing, capped at 32 samples per tick.
2. Interpolates the true vertical height at each sample; asks either for the highest floor below the old feet (fall) or lowest platform above the old head (rise).
3. Detects the first crossing and refines the contact fraction with a bounded nine-step binary search.
4. Returns the resolved position at impact. Rising stops below the platform with a skin; falling lands on it. Both stop the horizontal displacement **at contact**, so narrow platforms cannot be skipped.
5. Injects collision lookups through `AdventureTerrain`: same algorithm in tests and with the original `.TER` adapter, independent of race physics or commercial assets.

Tests include thin ledges crossed mid-tick, diagonal head impact against a narrow underside, an end-to-end gameplay tick crossing a half-unit roof, misses outside the platform footprint and bit-for-bit deterministic contact fractions.

## Remaining limitations

- This is a *sampled heightfield sweep*, **not** an exact 3D capsule-vs-triangle continuous collision solver. Sub-quarter-unit features, extreme velocities and complex slopes can still tunnel. In particular, the 32-sample cap means unusually large displacements can exceed quarter-unit spacing.
- The `.TER` foot/head probes have no capsule radial footprint: horizontal collision uses a separate disc. Full sphere/capsule edge contacts and stepping/sliding across angled surfaces remain future work.
- The bin art lacks collision semantics, so **visual-only ceilings remain non-solid**, intentionally. Decoration mesh transforms and selected physical-roof authoring require a separate pipeline.
- Landing/bonking stop residual horizontal movement for the remainder of the tick rather than physically redistributing it, conservative until full TOI response.
- Real gameplay rendering with original assets and Dreamcast KOS hardware performance have not yet been verified.
