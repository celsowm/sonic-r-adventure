# Offline geometry audit: Sonic R original Windows .TER files

**Date:** 2026-11-08  
**Input:** User-provided archive `Sonic-R_Win_EN_RIP-Version(1).zip`, containing original game resources.  
**Privacy/IP boundary:** The binary files and their extracted vertex/polygon tables were **not** committed to GitHub. This note contains only derived counts, invariants and engineering findings.

## All five .TER files verified

| Track | Bytes | Floor surfaces | Wall meshes | Wall polyline vertices | Height-gated wall meshes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Resort Island | 80,492 | 561 | 151 | 1,175 | 32 |
| Radical City | 46,396 | 508 | 143 | 1,423 | 19 |
| Regal Ruin | 64,164 | 486 | 288 | 1,883 | 114 |
| Reactive Factory | 63,864 | 452 | 335 | 2,365 | 15 |
| Radiant Emerald | 30,112 | 313 | 112 | 860 | 1 |
| **Total** | **285,028** | **2,320** | **1,029** | **7,706** | **181** |

In every level, the 10 pointers in the .TER header were consistent with the loader's 92-byte header and the 6-byte vertex / 14-byte face / 16-byte surface / 16-byte collision mesh / 6-byte edge record formats. The 32×32 grid index array contained 1,024 entries. Every ground and wall list entry decoded to a valid object index; every list reached a terminating bit or the empty-cell marker. All 1,029 wall meshes were referenced by at least one grid cell. Every wall mesh's vertex range was in bounds, and its configured horizontal radius covered all referenced vertices.

## Proven vertical-coordinate mismatch in WP-04 wall adapter

All height-gated wall vertices in the supplied original .TER files have **legacy Y <= 0**. In particular, all 225 Resort Island gated-wall vertices have legacy Y <= 0 and their **canonical Adventure** heights, after negation, range from **+135 to +774** world units.

The previous wall adapter compared `AdventurePlayer.pos.y` (positive-up) against the *unnegated* `TerVertex.y` (legacy negative-up), so it could skip finite-height walls at valid playable heights. This mismatch is independent of screen rendering or the original racing physics.

**Correction:** For each swept-disc contact, project the horizontal impact position onto the finite wall segment, interpolate the two original signed vertex heights and negate the result before evaluating whether Sonic's feet have cleared the wall top. Apply the gate only to meshes that set `heightGate`. Keep all other wall behavior read-only and separate from racing.

## Validation scopes

- **Executed locally against the provided archive:** Header/table sizes, source vertex and face counts, wall/grid references, per-wall bounding radii and height signs.
- **Automated repository tests:** Signed Y conversion, variable wall-top interpolation, collision at/below wall top, passage above the top, and heights around 1,000 units.
- **Not yet verified:** Rendering and gameplay with original resources, staircases/ceilings and moving geometry, Dreamcast KOS compilation or physical hardware frame time.

No proprietary assets or extracted geometry payloads are included in this report.
