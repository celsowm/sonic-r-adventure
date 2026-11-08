# WP-04.6 — Explicitly authored visual roof triangles and head-sphere contacts

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

The portable `adventure_roof.c` now performs a **continuous upward swept head-sphere / finite triangle** test. For upward movement, it computes analytic contact against the plane offset by the head sphere's radius, tests the projected point inside the roof footprint, then checks the sphere's earliest approach to each of the triangle's three finite edges, including their endpoints (vertices). Edge contacts use a bounded deterministic convex minimum and first-crossing search in Q12 time, rather than frame-based positional samples.

The capsule height is the **total body height**. The head sphere center is at `footY + capsule_height - capsule_radius`, preserving the old planar roof-contact height while adding lateral radius coverage. The player retains the earlier of this hit and the existing TER underside collision, with a small separation skin. A successful upward hit stops vertical motion; the player then falls under gravity.

- Accepts reversed triangle winding and slopes up to a conservative limit.
- Uses integer arithmetic, no `sqrt`, `atan2` or floating-point physics; suitable for deterministic C tests and SH-4.
- Unit tests exercise plane-time intersection, start-touched roof, rejected back-side hit, sloped/diagonal impact, opt-in-only behavior, **sphere-only edge and vertex contacts**, grazing without sticky escape, multiple roof ordering, variable sphere radius and full-game head bonks.
- All new code is isolated under `source/sdl/src/adventure`. Racing physics and authored game data are unchanged.

### Important limits

**This is not a full continuous swept capsule vs arbitrary triangle algorithm.** It resolves the **head sphere** against selected roof faces/edges/vertices, but does not sweep the remaining body shaft, the lower endcap, complex steep vertical walls, dynamic geometry or arbitrary scenery transformations. Edge/vertex time of impact uses a bounded Q12 integer convex distance search rather than a closed-form quadratic root. The pre-existing static-wall swept disc and TER floor/underside sweep remain in effect. Full capsule side contacts and collision materials require another pass. Collision faces must still be opt-in; the default visual-roof manifest remains intentionally empty.

Actual visual play-testing, determining individual solid roofs and Dreamcast timing are still outstanding; an empty opt-in manifest is intentional until that review.

## Validation

```sh
make -C tests/adventure test
make -C source/sdl -j2
```

The portable tests run without commercial assets. The uploaded original game resources are never stored or committed in the GitHub repository.
