# WP-04.11 — Slope acceleration, ground traction and airborne momentum

**Status:** Implemented as portable fixed-60-Hz Q20.12 physics with synthetic regression tests. All values below are tunable *displacements per simulation tick*, not frame-dependent real-time units. The original racing physics and commercial stage binaries remain untouched.

## Movement model

The terrain-support normal added in WP-04.10 is now an input to a standalone `adventure_traction.c` module. Horizontal motion has three explicit regimes:

| State | Steering / friction | Horizontal speed cap | Incline acceleration |
| --- | --- | --- | --- |
| Flat grounded or isolated controller pilot | Existing 900 Q12 steering and 1,250 Q12 brake | 3 world units / tick | None |
| Walkable incline | Momentum-preserving steering; 160 Q12 rolling friction without input | 5 world units / tick | Gravity projected onto the surface tangent |
| Airborne after a ramp/ledge launch | Reduced 340 Q12 air steering or 96 Q12 drag without input | 5 world units / tick | None |

On a walkable incline, the acceleration applied to the horizontal displacement is:

```text
ax = (normal.x * normal.y * SLOPE_PULL) / (4096 * 4096)
az = (normal.z * normal.y * SLOPE_PULL) / (4096 * 4096)
SLOPE_PULL = 4096 / 3  (Q12 per fixed tick)
```

Normals are oriented positive-up. Their horizontal components point downhill, so this vector accelerates downhill and opposes uphill steering. The implementation validates the normal range and uses 64-bit intermediates. Integer square roots cap horizontal speed without a floating-point dependency.

### Preserving earned speed

Conventional `Approach(current, run_target, steering_acceleration)` would immediately brake a faster, downhill-moving Sonic back to the ordinary 3-unit run cap, defeating the incline acceleration. If input is aligned with existing velocity and the character exceeds normal run speed, the desired *direction* is preserved but its target magnitude is raised to the current earned speed. The player can therefore steer a downhill boost rather than losing it solely by continuing to hold forward. Reversing direction still requires gradual braking/steering.

When Sonic jumps off a walkable slope or leaves its ledge, `AdventurePlayer.slope_airborne` remembers the origin until the next grounded contact. Airborne updates retain horizontal inertia using limited drag/air steering; **ground slope gravity is not applied in the air**. Landing clears this state. Jumps from flat ground keep the previous flat/air behavior to avoid a broad unrelated control change.

The legacy `AdventurePlayer_TickGroundPilot` uses the same flat regime as before; no changes to its public API or the original racing `Player` ABI were required.

## Regression coverage

- Flat running and braking remain equivalent to the original WP-02/03 controller tuning.
- Neutral input rolls down a gentle slope without requiring a horizontal stick input.
- Uphill steering opposes the incline, downhill steering reinforces it; downhill can build speed above the flat run cap but never exceed the slope cap.
- Neutral uphill momentum decays, while neutral downhill momentum increases if gravity exceeds rolling friction.
- Airborne steering gradually changes direction without resetting a downhill-earned boost.
- A world-level game test follows a sloping fixture, runs out over a ledge and verifies the next airborne tick retains nearly all horizontal displacement.
- Repeated 400-tick mixed steering/rolling/airborne simulations produce identical fixed-point state.

## Boundaries and further work

This is a **playable first pass**, not a final Sonic Adventure speed curve. Real-stage tuning is needed for slope force, rolling friction, speed cap, downhill turns, camera following and high-speed ledge handling. It does not add loop adhesion, banked gravity, per-material friction, slopes above the walkability limit, moving platforms or physics-tick runtime profiling on Dreamcast.

When on a steep *unwalkable* face, the small one-tick downhill bias from WP-04.10 remains; this module handles only walkable inclines or airborne motion after leaving them.

```sh
make -C tests/adventure test
make -C source/sdl -j2
```

No original game ROMs, BIN/TER payloads or other copyrighted game assets are committed.
