# WP-04.10 — Slope normals, walkability and stable terrain transitions

**Implementation status:** Normal extraction, slope policy and portable regressions. This work uses no original copyrighted geometry payloads; only the already-loaded `.TER` records at runtime or selected `.BIN` roof triangles.

## Normal sources

- **Original TER:** Each 14-byte `TerFace` has three signed normal components. `AdventureTerrain_QueryLegacyNormal` redoes the polygon footprint query at the exact world X/Z before reading `s_hitEdgeIdx`. The legacy Y axis is negative-up, so the Y normal component is inverted before normalized Q20.12 representation.
- **Authored BIN:** `AdventureRoof_QueryNormal` computes the 3D cross product of a selected triangle's two edges, orients it positive-up and normalizes it. Faces not explicitly included in the roof manifest cannot supply normal/support.
- **Merged support:** `AdventureRoofLegacy_QueryNormal` dispatches by surface identity: nonnegative IDs are TER surfaces; roof IDs are the negative encoded value `-2 - triangle_id`. This preserves terrain support and the identity of authored roof surfaces without adding legacy racing-physics dependencies.
- **Compatibility:** `AdventureTerrain.query_normal` is an *optional*, read-only callback. Existing fixtures and integrations without it still behave as level ground; missing or invalid normals fall back to canonical up `(0, 4096, 0)`.

## Walkability policy

`AdventureSlope_Walkable` uses a fixed Q12 normal threshold, `normal.y >= 3072`, equivalent to approximately **41.4°** from vertical. This policy is intentionally simpler than Sonic Adventure's eventual high-speed wall/loop movement. A normal below that threshold does **not** ground Sonic.

The bounded `AdventureSlope_QuerySupport` checks the highest surface at or below the maximum support height and, if that surface is too steep, requests a lower candidate. Up to eight candidates are checked per query. This is essential for stacked surfaces: a steep overhang can exist above a walkable lower floor without incorrectly selecting the overhang as support.

The normal of the current support is stored in `AdventurePlayer.ground_normal`, independently of the original binary `Player` structure. This allows later surface-relative animations, friction and camera alignment to consult the same authoritative value. When airborne, it resets to positive-up. Floor collision remains physical for steep surfaces; only the *grounded* decision rejects them.

## Transitions and steep-slide behavior

During a grounded tick, the controller queries support at the intended horizontal destination. A transition to an unwalkable slope or a large unsupported drop ends grounded state while preserving coyote-jump behavior. If the obstacle is a too-steep physical surface, it adds a small bounded, deterministic downhill horizontal bias, directed along the positive horizontal projection of the surface normal. This prevents indefinite grounded attachment but is **not** a full Sonic Adventure slope-acceleration model.

Grounded traversal of walkable ramps continues to use the existing step-up and ground-snap thresholds, with the final Y sampled at the resolved X/Z. Vertical impulses and collisions still use the WP-04.9 unified first-contact dispatcher.

## Tests and limitations

Portable fixtures cover normalized source normals (including negative-Y winding), shallow vs steep classification, near-horizontal defaults for missing callbacks, stacked steep-over-walkable support, walking from level ground onto a gentle ramp and then losing grounded state on a steep ramp, rejection of steep respawn, and slope normals from explicitly selected BIN triangles.

**Still outstanding:** Per-material friction, adhesion to loops, gravity relative to banked/vertical surfaces, angular damping, a single analytic capsule-triangle narrow phase for TER and BIN, moving platforms and actual Dreamcast hardware timing. The data-driven 41.4° threshold is an initial conservative platformer value and must be tuned through real stage playtests.

## Reproduction

```sh
make -C tests/adventure test
make -C source/sdl -j2
```

The original archive supplied by the user remains outside the repository.
