# WP-04.8 — Iterative three-dimensional sliding and authored-roof support

**Status:** Implemented as a portable deterministic increment. The original racing physics and visual asset files are untouched. The stage's BIN solid-roof manifest is still **empty by default**, pending visual review.

## Collision response: keep the unused part of the tick

`AdventureSlide_Resolve` takes a foot position, a desired three-dimensional movement vector and an optional capsule-triangle query. It uses fixed Q20.12 arithmetic throughout:

1. Sweep the full capsule toward the requested endpoint, choose the earliest selected static-triangle hit.
2. Advance to the contact fraction, separate along the contact normal by a small collision skin.
3. Remove only the component of the unspent displacement that points *into* the contact normal, retaining the tangential component.
4. Retain constraints from prior hits when navigating multiple adjacent surfaces, reprojecting the remainder against them to handle corners.
5. Continue for up to four contacts within the same 60-Hz tick; stop if no unspent motion remains. No heap allocations or floating-point math.

The response also reports a tangential velocity (the requested motion with blocked components removed), used to prevent a rising Sonic from retaining upward speed after head-butting a ceiling. The original .TER wall solver remains separate, and its horizontal sliding behavior is unchanged.

**Gameplay integration:**

- **Grounded horizontal translation** against approved BIN triangles uses the sliding resolver instead of stopping at the first side/body hit. This covers shaft, head, lower cap, edges and corners.
- **Rising jump** chooses the first collision between existing TER platform undersides and the full capsule's selected-triangle hits. When a BIN triangle wins, the player resolves the attempted 3D displacement through the sliding resolver. TER-only collision still uses its existing conservative stop-at-contact behavior.
- **Falling and staying on selected BIN rooftops:** `AdventureRoof_QueryGround` tests the X/Z footprint of explicitly selected walkable BIN triangles, interpolates positive-up roof height and returns the highest approved surface at or below the foot threshold. `AdventureRoofLegacy_QueryGround` merges that support with original TER floors. Authored roof surface IDs use negative values below -1, retaining 0 as the collision layer so legacy TER wall masks remain active. The falling foot sweep and grounded support query use this merged provider.

## New tests

- A synthetic X wall allows sliding along Z without discarding the remainder.
- A two-wall corner accumulates constraints and stops only the blocked X/Z components.
- A horizontal ceiling blocks upward motion but retains X/Z travel.
- A selected roof triangle supplies physical side contacts with tangential movement; repeated runs are identical.
- Selected roof height queries reject points outside the footprint and honor the maximum height and explicit success flag.
- A falling Sonic lands on a roof above the original TER floor, keeps its distinct surface ID, and remains grounded across subsequent ticks.
- Legacy adventure tests for walking, jumping, walls, roof head/shaft collision and deterministic replay remain in the same suite.

## Design boundaries

- **Static, authored geometry only.** A `.BIN` render face is never made collidable by orientation heuristics. The opt-in manifest is intentionally empty until its stage/part/face IDs can be visually approved.
- There is not yet a *single unified* swept character solver for native TER walls, TER floors, authored BIN roofs, and moving objects. Grounded/ascending BIN contacts are iteratively resolved; the original heightfield / TER wall solvers retain their independent paths.
- Falling against the *side* of selected BIN geometry is not yet projected into a full 3D slide; falling **onto** its walkable top uses the merged foot support provider.
- Roof support is a point-on-triangle X/Z query; the bottom cap's radial footprint and steep transitions need an explicit walkability/step-height policy. Slopes are included subject to the author's conservative roof-angle gate.
- Multi-contact constraint projection is bounded and approximate in Q12. More complete complementarity / moving-platform contact manifolds are out of scope.
- The CI compiles and tests the Linux SDL application, not the Dreamcast KOS cross-build or the commercial-stage runtime. No frame-time claims on Dreamcast or visual equivalence claims are made here.

## Verification

```sh
make -C tests/adventure test
make -C source/sdl -j2
```

See `docs/adventure/WP04_CAPSULE_COLLISION.md` for the narrow-phase, and `docs/adventure/WP04_EXPLICIT_BIN_ROOFS.md` for the opt-in authoring workflow.
