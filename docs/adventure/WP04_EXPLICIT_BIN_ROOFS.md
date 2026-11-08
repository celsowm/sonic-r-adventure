# WP-04.5 — Explicitly authored visual roof triangles

## Why there is no automatic BIN→collision conversion

The original game's **track BIN** contains world-space render geometry and decorations. Its face flags do **not** contain a reliable "solid roof" indicator. Promoting every visually horizontal polygon into a collider would create invisible gameplay restrictions from decorative scenery, transparent overlays and other non-solid faces.

The user-provided original Resort Island BIN and TER were compared offline. Of 1,686 matching X/Z vertex positions, **1,662** had exactly opposite Y coordinates (with the remainder largely at Y=0 or otherwise ambiguous). This demonstrates that render BIN vertex Y is **positive-up**, while the matching TER vertex Y is **negative-up**. `AdventureRoofLegacy_LoadSelected` converts BIN vertices to Q20.12 without inverting Y. TER still inverts Y in its independent adapter.

## Authoring: opt in by stage / part / face

`adventure_roof_legacy.c` contains the fixed selection array:

```c
static const AdventureRoofSelection s_selected_roofs[] = {
    /* Example (not enabled): { TRACK_RESORT_ISLAND, part_index, face_index }, */
    {0, 0, 0} /* sentinel */
};
```

Only after visual inspection confirms that a particular **track-part quad** is a solid, static roof should its stage ID, track-part index and zero-based quad face index be added to this array. The loader validates the part / face / vertex indices against existing in-memory resource counts. It triangulates each selected quad using the same four indexed vertices as the renderer. Generic scenery and moving decorations are deliberately excluded. The total selected roof cache is capped at **64 triangles** (32 quads), fixed storage with no allocation per tick.

**Current authored roof count: zero.** No unreviewed visual meshes are promoted to physical geometry by default.

## Narrow-phase and gameplay integration

The new portable `adventure_roof.c` performs a **continuous upward head-point / finite triangle face** test. For upward movement, it computes an analytic segment/plane crossing in Q20.12, verifies the point lies inside the chosen face using 2D barycentric/cross-product tests, and returns its fraction of the fixed tick. The player retains the earlier of this hit and the existing TER underside collision, with a small separation skin. A successful upward hit stops vertical motion; the player then falls under gravity.

- Accepts reversed triangle winding and slopes up to a conservative limit.
- Uses integer arithmetic, no `sqrt`, `atan2` or floating-point physics; suitable for deterministic C tests and SH-4.
- Unit tests exercise plane-time intersection, start-touched roof, rejected back-side hit, finite face bounds, sloped/diagonal impact, opt-in-only behavior, multiple roof ordering and full game tick head bonk.
- All new code is isolated under `source/sdl/src/adventure`. Racing physics and authored game data are unchanged.

### Important limits

**This is not a full continuous swept capsule vs arbitrary triangle algorithm.** The analytic test uses Sonic's head **point**, not his head's sphere radius, and does not yet resolve roof edges/vertices, the capsule side, complex steep vertical walls, dynamic geometry or arbitrary scenery transformations. The pre-existing static-wall swept disc and TER floor/underside sweep remain in effect. Full capsule contact and collision materials require another pass.

Actual visual play-testing, determining individual solid roofs and Dreamcast timing are still outstanding; an empty opt-in manifest is intentional until that review.

## Validation

```sh
make -C tests/adventure test
make -C source/sdl -j2
```

The portable tests run without commercial assets. The uploaded original game resources are never stored or committed in the GitHub repository.
