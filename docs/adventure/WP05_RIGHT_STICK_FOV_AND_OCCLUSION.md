# WP-05.2 — Right-stick orbit, vertical camera, speed FOV, obstruction refinement

**Status:** Initial implementation with asset-free deterministic tests and SDL build checks. The camera is still integrated only with the Adventure preview; it does not change Sonic R's original race camera.

## Inputs and control mapping

The portable `AdventureRawInput` now has two additional independent signed axes:

- `look_x`: positive rotates the camera clockwise around Sonic; `look_y`: positive raises the camera eye relative to its look-at target.
- **SDL mapped controller:** actual right analog stick X/Y, with right-stick Y sign inverted so pushing up raises the view. The stick's raw range is signed 16-bit.
- **Generic SDL joystick:** axes 2/3 when present, as a best-effort fallback; some unconventional devices may map these differently. Use SDL game-controller mappings for consistent control.
- **Keyboard:** J/L rotate; I/K raise/lower. Existing Q/E rotates as before. Right-stick rotation takes precedence over Q/E when both are present.
- **Stock Dreamcast controller:** it has no second analog stick. L/R triggers continue camera rotation; Y raises and X lowers the viewpoint. Dreamcast keyboard also has J/L/I/K bindings.

Axes do not enter `AdventureInput_Sample` or the left-stick movement solver. Camera reads the raw axes once per fixed gameplay tick. A raw dead zone of ±6,000 suppresses joystick drift. Horizontal manual orbit is bounded to ±1,280 in 4,096-angle units and automatically recenters after release. Vertical pitch is bounded to ±256 angle units (about ±22.5°); the user-chosen elevation **persists** until adjusted again, avoiding an unwanted spring to default pitch.

## Dynamic framing / projection

`AdventureCamera.zoom_q12` is an integer inverse perspective-scale multiplier. Zero horizontal speed targets 4,096 (the original perspective); 5 world units per fixed tick and faster target 3,584 (a modest **12.5% wider** effective field of view). Intermediate speeds interpolate linearly and a bounded 64 Q12-per-tick rate prevents a sudden snap while accelerating/braking. Vertical jump velocity alone does not widen the FOV.

The renderer does not use OpenGL's perspective matrix: it CPU-projects 3D vertices into screen-space using `g_projScaleXCurrent` and `g_projScaleY`. The scoped `AdventureCameraLegacy_BeginProjection/EndProjection` bridge multiplies those **actual integer perspective factors** by `zoom_q12/4096` only around Adventure's render call, then restores the old factors immediately. No modifications persist to the original race or next frame. Rendering code outside this bridge remains unchanged. Because the original `RenderHUD` function also draws backgrounds and overlays, those draws may see the same projection factor; separate 3D-only projection scoping is a possible follow-up if screen-space decorations exhibit drift during visual testing.

## Camera movement and obstacle avoidance

- The camera continues to track Sonic's world position and look ahead horizontally based on velocity, and now also looks up/down up to two world units while he rises or falls. The head center and camera position are independently smoothed.
- Pitch changes the desired eye elevation using fixed integer arithmetic, leaving movement basis horizontal and retaining the 12-unit default follow distance. Teleport resets both orbit and pitch.
- `AdventureCameraLegacy_Trace` now considers the earliest of native TER wall sweeps, selected static BIN capsule faces (still opt-in), and **48 bounded heightfield/overhead samples**, compared with 16 before. Within a reported terrain obstruction interval, eight fixed binary refinements improve the estimated first collision. The camera shortens its distance immediately to avoid an obstruction and regains distance gradually when clear.
- This still is **not** a triangle-accurate mesh raycast through arbitrary decorative `.BIN` objects, and it never makes decorative polygons solid by guesswork.

## Validation

The portable regressions cover stick dead zone, right-stick precedence over shoulder buttons, signed orbit, persistently adjusted vertical pitch, pitch limits, height after camera smoothing, FOV scaling vs horizontal speed and resting recovery, airborne lookahead, and a deterministic replay with alternating orbit axes and vertical look. Existing collision, controller and action-camera tests remain in place.

```sh
make -C tests/adventure test
make -C source/sdl -j2
```

The stock Dreamcast controls have been implemented at the Maple input bridge, but cross-compilation, hardware FPS and visual framing remain **unverified**. Original commercial game assets are never committed to this repository.

## Next validation

1. Launch with the original Resort Island files in an SDL build and inspect yaw/pitch signs, occlusion, perspective and HUD layout visually.
2. Evaluate renderer-only field-of-view changes without 2D overlay leakage if a mismatch is found.
3. Test the optional Dreamcast KOS build and profile the 48-step occlusion ray under actual frame budgets; cache or amortize queries if necessary.
