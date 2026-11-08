# WP-04.9 — Unified collision ordering and iterative response

**State:** implementation with portable regression tests and Linux SDL CI. No original commercial game assets are added to GitHub.

## Problem

The previous Adventure player had three separate movement responses: `AdventureCollision_MoveAndSlide` for TER walls, `AdventureSlide_Resolve` for authored BIN triangles and a final `AdventureVertical_Sweep` for TER ground and overhead. That ordering could stop one contact before checking an earlier contact in another collider category, lose the remaining tangential movement, and fail to slide along a wall while airborne.

## Architecture

`AdventureSolver_Sweep` is a single **read-only world dispatcher** with a uniform `AdventureCapsuleHit` output:

| Provider | Narrow phase | Normal direction | Contact classification |
| --- | --- | --- | --- |
| TER static wall | Swept horizontal disc with height gate and layer mask | Away from wall in X/Z | `ADV_CONTACT_TER_WALL` |
| Authored BIN triangles | Continuous vertically oriented capsule vs finite triangle | Away from triangle in 3D | `ADV_CONTACT_BIN_TRIANGLE` |
| TER floor / authored BIN roof foot support | Sampled diagonal foot-height crossing | Up (+Y) | `ADV_CONTACT_TER_FLOOR` |
| Elevated TER platform underside | Sampled diagonal head crossing | Down (-Y) | `ADV_CONTACT_TER_CEILING` |

All providers report Q12 impact fractions. The dispatcher chooses the earliest, then `AdventureSlide_Resolve` handles the same set of providers on every remaining-contact iteration, removing inward motion but keeping tangential movement (up to four contacts per tick). The heightfield sweep is called with the optional BIN capsule provider disabled internally, avoiding duplicate queries. The BIN support provider remains explicitly authored; no decorative meshes become solid by default.

`AdventurePlayer_TickWorld` now performs one integrated displacement per fixed tick, after sampling horizontal input, jump buffering, coyote time and gravity. It uses **one** `AdventureSolver_Move` call. The solver reports floor contacts and ceiling contacts in the result; the player uses a real downward floor contact to allow landing. A previously grounded player may also follow the current terrain/ramp within the legacy step-up and ground-snap budget. A passing airborne player cannot be grounded just because a floor is close. Support identity is read from the merged TER/approved-BIN ground provider after response.

## Added regression fixtures

- BIN obstacle at X=8 versus TER wall at X=10: choose BIN first; moving BIN to X=14 chooses TER wall first.
- Native TER floor crossing and elevated-TER underside crossing use opposite vertical normals.
- A diagonal fall collides with the native TER wall and then its ground, preserving the unblocked Z displacement.
- Airborne collision with a TER wall allows vertical ascent and movement along its tangent.
- Repeated combined wall/floor/BIN queries return identical contact ordering, positions and projected velocities.
- Existing head/shaft edge grazing, narrow landing, ledge coyote, upper/lower floors and authored-BIN roof support tests remain in place.

## Remaining work

- The **world dispatcher unifies time ordering and response**, but its native TER narrow phases are still specialized swept-disc/heightfield functions, not a single exact analytic capsule–triangle solver. It does not convert original TER wall loops or decoration geometry into general 3D static triangles.
- TER heightfields have sampled X/Z contacts and no radial foot support. Grounded ramps use height snaps; slope normal/friction and steep-slope policy are not yet implemented.
- Moving geometry, destructibles, full-body moving contacts, pathologically thin geometry and post-teleport depenetration remain incomplete.
- Real visual/runtime checks with the original stage data and Dreamcast hardware performance measurements are pending. Passing CI alone is not proof of on-device frame rate.

## Build

```sh
make -C tests/adventure test
make -C source/sdl -j2
```
