# WP-05 — Fixed-step 3D action camera and renderer bridge

**WP-05.2 implemented:** [right-stick and vertical orbit, dynamic perspective, improved occlusion](WP05_RIGHT_STICK_FOV_AND_OCCLUSION.md). This document records the WP-05.1 baseline.

**Status:** Implemented in `source/sdl/src/adventure/`, with portable tests and SDL compilation. This is a first-playable camera, **not** verified visually with the original stages or on Dreamcast hardware.

## What changes

The Adventure preview previously called `BuildChaseCamera` and `BuildCameraViewForViewport` from Sonic R's **racing camera** every frame. That pipeline recalculates its own pursuit distance and yaw based on the original racing `Player`, so merely adjusting `CamStateEntry` could never give an independent action-platformer camera.

`AdventureCamera` now owns a dedicated, portable Q20.12 state:

- `position` and `look_at` are positive-up Q20.12 world coordinates, advanced on the same 60 Hz fixed ticks as physics.
- `yaw` is a 12-bit heading (0 = world +X, 1024 = world +Z). A 33-entry quarter-wave sine table supplies deterministic, integer-interpolated forward/right vectors without using runtime floating-point trigonometry.
- Turning follows Sonic's facing with shortest-arc wraparound and a maximum angular change of 64/4096 revolution per tick. The left/right shoulder buttons (Q/E on SDL keyboard) apply a bounded player-relative orbit offset; releasing them recenters the camera gradually.
- Horizontal look-at prediction uses the actual player velocity with a bounded lead, and horizontal/vertical tracking use separate smoothing factors. Teleports larger than 100 world units immediately reset the camera rather than leaving it behind.
- Distance is normally 12 world units and the camera looks approximately two units above Sonic. When obstruction is detected, camera distance shortens immediately to avoid clipping; when clear, it recovers at one-third world unit per tick.

The camera's movement basis is passed **into the gameplay tick before moving Sonic**. The camera then follows the resulting player position, keeping camera-relative controls and camera update in the same deterministic simulation loop even when a render frame executes multiple ticks.

## Renderer integration

`adventure_camera_legacy.c` is the sole renderer-specific adapter. It fills the existing `RenderCamera` viewport output with the action camera's world positions, derived yaw/pitch, integer and float presentation fields, then calls the established `SetCameraStructPtr`, `BuildViewMatrix`, `ComputeCameraBasis`, `BuildD3DViewMatrix` and `SetViewportClipRect` pipeline. The float conversion is intentionally confined to the rendering bridge; physics and camera simulation remain integer-only.

**No Adventure frame calls `BuildChaseCamera` or `BuildCameraViewForViewport` anymore.** Those functions and the original racing game remain unchanged, and the new camera is enabled only inside `AdventureMode_RunPreview`.

The adapter respects the original render camera's Q12 world output convention and the historical opposite-X/Z convention of `CamStateEntry` (Q4). Reusing the original viewport's matrix-building path avoids reimplementing projection, parallax and track draw setup.

## Terrain and wall obstruction

The camera simulation accepts a read-only `AdventureCameraTrace` callback. `AdventureCameraLegacy_Trace` combines:

1. The current legacy `.TER` static-wall sweep using a small camera probe radius and the active layer mask.
2. Sixteen fixed ground/approved-BIN-support height samples between look-at point and proposed camera eye, with a half-world-unit clearance.
3. The earliest Q12 obstruction fraction, letting the camera pull forward before intersecting the obstruction.

The camera checks the *actual smoothed eye* as well as the ideal eye, avoiding clipping caused by interpolation lag. This is a **bounded conservative occlusion approximation**, not a continuous triangle raycast for arbitrary visual scenery. The authored BIN roof collision registry remains opt-in and empty by default; decorative polygons do not block the camera automatically.

## Tests

The asset-free test suite covers:

- Spawn camera position, +X/+Z heading and orthogonal movement basis.
- Shortest-arc yaw smoothing across 0/4095.
- Manual camera orbit, release-to-recenter and bounded rotation.
- Smoothing toward moving and airborne Sonic with velocity-based lookahead.
- Immediate compression by a synthetic blocking wall at X = -6 and gradual recovery after the obstruction clears.
- Far-distance teleport reset.
- A 350-tick replay with moving, airborne and orbiting input that must produce identical Q12 camera state and heading.

```sh
make -C tests/adventure test
make -C source/sdl -j2
```

## Baseline limitations and further visual validation

- Visual validation of **camera X/Z sign, yaw/pitch and framing** against the original Resort Island renderer (SDL), followed by the Dreamcast KOS build. CI currently establishes compilation and synthetic determinism only.
- Smooth collision avoidance around visual-only obstacles and tighter near-camera roof/wall geometry (e.g. an explicit per-stage camera-only collision hull).
- Right-stick orbit, vertical pitch, speed-dependent perspective and jump/fall lookahead are now implemented in WP-05.2. Acceleration-dependent distance and loop/banked-camera behavior remain outstanding.
- Camera occlusion cache and performance profiling on Dreamcast; ground ray probes are bounded but call the original legacy polygon query.

No commercial game resources are committed to the repository.
