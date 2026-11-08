# Sonic R Adventure — Master Development Plan

**Status:** Proposed design, not implemented  
**Baseline:** repository main at 3da98f2e134eac82e45cd69660216fa0e4563ac5 (2026-11-08 planning snapshot)  
**Target platforms:** Windows, macOS, Linux (SDL2/OpenGL), Sega Dreamcast (KallistiOS/PVR)  
**Language:** portable C for game logic; platform-specific integrations only at the existing backends  
**Primary reference:** Sonic Adventure's high-speed 3D action stages, not its story or exact proprietary assets  
**Project principle:** Convert Sonic R's engine into a maintainable 3D action-platforming game; do not merely disable lap counting.

> This documentation is a technical implementation proposal. File paths, function names and architectural observations below were checked against the source tree. Suggested interfaces, numerical constants, file formats, stage layouts, performance budgets and behaviors are DESIGN PROPOSALS, not existing implemented features or verified measurements.

## Documentation map

1. **This file** — goals, scope, principles, architectural decisions, dependency map, phases and definition of done.
2. [ENGINE_ARCHITECTURE.md](adventure/ENGINE_ARCHITECTURE.md) — precise integration points, data structures, update order, decoupling, fixed-point math, game-state lifecycle and save compatibility.
3. [MOVEMENT_CAMERA_COMBAT.md](adventure/MOVEMENT_CAMERA_COMBAT.md) — controls, movement physics, camera, surfaces, rings, homing attacks, damage and character abilities.
4. [STAGES_CONTENT_PIPELINE.md](adventure/STAGES_CONTENT_PIPELINE.md) — collision/runtime stage format, world objects, Resort Island Act 1, asset pipeline and Dreamcast-aware authoring.
5. [IMPLEMENTATION_AND_VALIDATION.md](adventure/IMPLEMENTATION_AND_VALIDATION.md) — work packages, detailed acceptance tests, benchmark plan, risks, dependencies and delivery gates.

---

## 1. Vision, elevator pitch and non-goals

**Vision:** Sonic R Adventure is a full 3D platform-action game built on Sonic R's reimplemented engine and its unique visual identity. Players explore large interconnected environments, build momentum across slopes and loops, leap between platforms, collect rings, defeat enemies, find alternative routes and complete goals. The moment-to-moment feel should evoke the Sonic/Adventure style: accessible low-speed directional control combined with inertia and spectacle at high speed.

**Immediate release:** a complete Sonic-only vertical slice, provisionally called **Resort Island — Act 1**, using the legacy environment with new traversal and objectives.

**Long-term:** transform existing Sonic R environments into substantial action stages; optionally introduce other character campaigns and hub areas after the core action loop is proven.

### In-scope for the first vertical slice

- Single human-controlled Sonic; full analog stick support on Dreamcast and modern desktop controllers.
- Reversible movement and free exploration in X/Y/Z, without holding an acceleration button.
- Variable-speed running, momentum-preserving turns, jump/air control, rolling, Spin Dash and Homing Attack.
- Camera-relative movement at low speed and movement-relative smoothing at high speed.
- Ground, air, wall, ceiling, slope, ledge, moving platform and hazard interactions.
- Ground springs, dash panels, rings, monitors, basic enemy, death/fall, damage, checkpoint, goal.
- Player-centric dynamic chase camera, camera obstruction handling, optional manual yaw and scripted zones.
- One real end-to-end action stage with start, authored paths, branching, verticality, checkpoint and finish.
- HUD for rings, lives/attempts (decision pending), timer as secondary/optional element and stage state.
- Same action logic on SDL and Dreamcast; platform-specific rendering and input adapters.
- Build, validation and performance measurements on both targets.

### Explicit non-goals for the vertical slice

- Porting Sonic Adventure, importing its source/code or reproducing its complete level/story.
- Maintaining a racing mode inside the new action-game executable indefinitely.
- Rebuilding the whole renderer or moving to C++/a heavyweight ECS.
- Story, hub world, cutscenes, Chao, boss encounters, online multiplayer, 4-player splitscreen or full roster.
- Physically perfect slopes, full collision on arbitrary animated mesh topology or fully arbitrary gravity from day one.
- Shipping proprietary Sonic R/Sonic Adventure assets as part of this code repository.

### Quality bar

- The game is fun as a platformer even at walking speed, not only when Sonic is moving fast.
- Player control always wins over cinematic presentation unless an authored, explicit camera/rail segment is active.
- Player actions feel consistent, predictable and forgiving, with transparent collision and reproducible tests.
- One authoritative gameplay simulation shared between desktop and Dreamcast.
- Well-defined APIs and ownership, no growing pile of flags scattered across decompiled legacy functions.
- No speculative compatibility code: old racing data are consumed through well-defined adapters only while useful.

## 2. Repository findings (verified)

### Current platform/build layers

- **source/sdl/Makefile** builds desktop C + SDL2 + OpenGL.
- **source/dc/Makefile** compiles shared logic under **source/sdl/src** and substitutes Dreamcast platform/render/audio sources from **source/dc/src**.
- **source/sdl/src/main.c** contains the large decompiled WinMain-style application flow and the race update loop.
- **source/sdl/src/platform/**, **source/sdl/src/input/input.c** and **source/dc/src/platform_dc.c** cover platform/inputs.
- Existing rendering routes include **render_scene.c**, **render_track_d3d.c**, **render_gl.c** and Dreamcast's **r_pvr_backend.c**, **render_pvr.c** and **render_track_dc.c**.
- **source/sdl/src/sound/** and **source/dc/src/** implement platform audio/music, with ADP/ADX support documented for Dreamcast.

### Existing reusable gameplay systems

- **player_struct.h**: **Player** is a 0x71C-byte legacy structure with position, velocity, ring count, ground contact, normals, collision layer, loop mode, animation and many race fields.
- **player_camera.c**: **UpdateHumanPlayerPhysics(Player *, unsigned short)** contains legacy player acceleration, jump, ability and race-conditional physics. The name is misleading: this is not merely a camera file.
- **physics_main.c**: **PlayerPhysicsMain** contains the opponent/AI-oriented physics path, including race-specific conditions; do not blindly call it for a new controller.
- **player_movement.c**: **UpdatePlayerMovement** includes ground/wall collision, collectibles, slope/loop movement and position update.
- **ground_collision.c**: **GroundCollision**, **PointInPolygon**, **InterpolateGroundHeight** and **QueryTerrainHeight** use the .TER terrain grid and surface layers. **ter_types.h** declares typed file layouts.
- **camera.c**: **BuildChaseCamera** is built around race-stage chasing and terrain Y sampling.
- **track_geometry.c**, **track_load.c**, **track_per_level.c** load stage display geometry, .TER surfaces, path/intro/waypoint data, rings and AI files.
- **race_update.c**: **Update5PlayerRacePhysics**, **UpdateRaceObjects**, **UpdateRaceRings** and other race-centric updates.
- **animation.c**, **anim_data.c**, **render_character_d3d.c** and **render_char_sprites.c** provide an existing character animation/render pipeline.
- **hud_full.c**, **save.c**, **source/dc/src/save_vmu.c** are platform resources to audit/adapt, not necessarily copy directly.

### Architectural consequence

The **render pipeline and level data loaders are reusable**, but the **simulation orchestrator, controller, camera, score/progression flow and object lifecycle require genuine decoupling**. Reusing player rendering does not imply reusing legacy racing physics verbatim.

## 3. Foundational architecture decisions (ADRs)

### ADR-001: One game simulation, two platform backends

New gameplay sources must live under **source/sdl/src/adventure/** (or a similarly shared neutral location) and be compiled by both Makefiles. No SDL includes, PVR types, Win32 types or KOS calls appear in pure gameplay headers. Specific draw, input, audio, save and file-reading operations route through existing platform facades or small adapters.

### ADR-002: Keep original game working only as a temporary reference

Use the current race to compare render, collision and animation regressions during migration. Add an explicit **Adventure** launch mode or path; do **not** implement each feature via a collection of ad hoc tests such as "if race type == adventure" scattered across the entire old code. A clean centralized mode dispatch at the simulation boundary is acceptable for the transition; a distinct action-stage loop is the goal. Keep preservation work outside this scope.

### ADR-003: Do not resize Player or casually reinterpret its fields

Player's 0x71C layout and many field offsets are shared among old modules. Add a sidecar **AdventurePlayerState** for movement states, target information, cooldowns, checkpoint and controller context. Use explicit mapping functions to copy position, yaw, velocity, animation and rings into legacy Player fields needed by the renderer. Do not use unsafe overlay unions or repurpose racing fields without documenting every reader.

### ADR-004: One coordinate/mathematics contract

The old code uses signed fixed-point data, 12-bit angle conventions and non-obvious sign inversions between world, track and camera coordinates. Define **AdventureVec3**, **AdventureWorldPos**, and conversion helpers in exactly one module. Record what "up", "forward", yaw zero and gravity direction mean. Unit-test conversions before adding movement. Gameplay state can use a portable, explicitly selected numeric format; prefer deterministic 16.16 or existing 20.12 arithmetic in runtime integration where appropriate. Avoid mixing float and fixed units by inference.

### ADR-005: Collision should support two complementary sources

Retain .TER for legacy static track ground/walls while adding a dedicated adventure collider query API that also addresses stage-authored platforms, moving objects, ceilings, hazards, narrow ledges and stacked floors. A universal query result records position, contact normal, surface ID, platform ID, material and flags. The exact internal acceleration structure is replaceable. Preserve legacy loop data as an initial surface/rail provider with controlled entry/exit semantics.

### ADR-006: Data-driven objects and stage composition

Stage objects have stable IDs, class/type, transforms, optional properties and trigger volumes. Author them in desktop tools and compile to a compact endian-defined runtime format. Avoid hardcoding new Resort Island-specific conditions in core physics or game loop. Default to a small type table, arrays and deterministic object updates rather than implementing a full ECS.

### ADR-007: Deterministic fixed-step simulation

Introduce an authoritative fixed simulation step and a clear schedule (poll input → player controller → collision → interactions → entities → animation events → camera → snapshot → render). Do not bind game speed to render timing. Confirm how legacy simulation timing behaves, then select a common step size (initial proposal 1/60 s, to be benchmarked rather than assumed feasible at 60 fps on Dreamcast). Rendering may run at a different rate, interpolating snapshots where viable.

### ADR-008: Asset provenance and boundaries

Engine-only repository; original commercial assets are user-supplied as currently documented. New authored content must have its own clear provenance and license. No ripped Sonic Adventure binaries, textures, maps, dialogue or music in a public commit. Existing repository license explicitly prohibits selling the code; preserve that notice and obtain legal clarification before distribution/licensing changes.

### ADR-009: Dreamcast is a first-class target from day one

Prototype fast on desktop, but enforce daily/phase-level Dreamcast build checks. Never let the desktop path depend on an unportable rendering API, dynamically sized memory growth or an asset format the Dreamcast cannot load. Preserve PVR-friendly geometry/material batches and measurable resource usage.

### ADR-010: Vertical slice before content multiplication

Do not make five stages, all characters or extensive HUDs until **Resort Island Act 1** hits the acceptance gate. The expensive risk is physics/camera/collision architecture, not the number of environmental themes.

## 4. Game-mode design

The modern adventure mode should expose the following sequence:

~~~text
BOOT
  -> TITLE
  -> CHARACTER_SELECT (initially Sonic only)
  -> STAGE_SELECT (initially Resort Island Act 1)
  -> LOADING
  -> STAGE_INTRO (skippable; no forced race countdown)
  -> ACTIVE
       -> PAUSED
       -> DAMAGED / RESPAWN
       -> GOAL_REACHED
  -> STAGE_RESULTS
  -> STAGE_SELECT
~~~

The result screen reports completion, rings, optional elapsed time, checkpoint/collectible achievements, and a future grade if desired. This flow must not read lap counters, race ranks, opponent state, ghost/AI state or race medals.

### Stage objective model

A stage declares its objective and optional sub-objectives. Vertical slice primary objective: **reach the goal capsule**. Optional goals: ring thresholds, enemy defeat, hidden emblem, time challenge. Objectives are stage metadata, not logic scattered among screen handlers.

### Ring/life policy

Recommended baseline: collecting rings increments a stage-local count; one hit with rings releases a bounded number of recoverable rings and grants short invulnerability, while a hit without rings triggers death/respawn. Exact loss count, bounce distribution and retries are tuneable; avoid assuming the original racing ring mechanism already implements this.

### Character policy

Sonic first. Common traversal/animation/combat interfaces allow Tails and Knuckles later. Do not build large polymorphic infrastructure in anticipation of hypothetical characters; extract shared logic when a second playable character is implemented.

## 5. Systems that must stay untouched as long as possible

- File I/O, platform bootstrap and SDL/KOS separation.
- Existing Sonic R geometry and texture loading for the first test stage.
- Basic 3D render submission, viewports, sky, ambient/music and sound dispatch.
- Character model loading and rendering until the new animation state map is tested.
- Existing .TER parsing and its binary layout, except bug fixes that are isolated and regression tested.

## 6. Systems requiring clean replacement or targeted adaptation

| System | First action | Final direction |
| --- | --- | --- |
| Race game loop in main.c | Extract narrow callsite for adventure tick | Separate module owns stage update |
| UpdateHumanPlayerPhysics | Do not call in action mode; reuse confirmed formulas selectively | New controller |
| PlayerPhysicsMain | Leave for legacy AI until unused | Delete race-only callpath in action build |
| UpdatePlayerMovement | Wrap/test collision primitives | Explicit collision/motion query separation |
| BuildChaseCamera | Use reference behavior, no direct dependency | Adventure third-person camera |
| Lap progress/results | Exclude action loop | Goals/checkpoints/results |
| Track objects | Reuse renderer and static scenery | Data-driven action entities |
| Old HUD | Gate/replace | Minimal modular adventure HUD |
| Ghost/network | Exclude from first target | Evaluate only after stable solo game |
| Legacy save | Separate namespace/version | Adventure progress and checkpoints |

## 7. Delivery phases with hard gates

**P0 — Baseline and safety**
- Freeze baseline commit, capture desktop/Dreamcast boot/performance evidence, asset/setup docs, compiler warnings, memory baselines and visual screenshots.
- Add new mode without altering legacy sim results.
- Gate: both old builds remain valid; new mode builds a blank/harness scene on both targets.

**P1 — Simulation extraction**
- New AdventureGame state, input adapter, fixed-step driver, Player sidecar and level start/end lifecycle.
- Gate: stage launches, receives controls, draws Sonic and exits cleanly; race progression never runs.

**P2 — Core controller**
- Walking, acceleration, high-speed turning, jump, variable control, grounded/air transitions, rolling and Spin Dash.
- Gate: repeatable automated movement tests and satisfying desktop playtest.

**P3 — Camera and collision**
- Camera follow/orbit, occlusion, terrain/platform queries, moving platform support, falls and checkpoints.
- Gate: scripted obstacle course with elevated surfaces and no recurring camera clipping or stuck states.

**P4 — Combat and objects**
- Homing Attack, target selection, enemy states, ring loss, springs, dash panels, item monitors, goal and damage flow.
- Gate: complete loop from initial input to goal and results.

**P5 — Content pipeline**
- Export stage/objects/collision from editor, pack for both backends, build Resort Island Act 1 with vertical shortcuts.
- Gate: designers can move an object or platform via data edit, without recompiling source.

**P6 — Dreamcast profiling and polish**
- SH-4, PVR, memory/audio profiling, asset LOD/batching, HUD/save, tuning, stress/replay testing.
- Gate: both targets pass the same functional stage script, with platform-specific performance budgets met.

See [IMPLEMENTATION_AND_VALIDATION.md](adventure/IMPLEMENTATION_AND_VALIDATION.md) for the exact work packages, dependencies and checks.

## 8. Priorities (P0 / P1 / P2)

**P0 — Must ship in slice:** controller, ground/platform collision, jump, camera, ring/goal interaction, deterministic state update, loading, checkpoint/respawn, basic enemy and attack, desktop+Dreamcast correctness.

**P1 — Substantial value:** moving platforms, spring chains, dash panels, authored loop transitions, more varied Sonic animation, alternate routes, save progression and richer HUD.

**P2 — Deferred:** new characters, bosses, hubs, cutscenes, extensive level streaming, sophisticated enemy AI, collectibles/meta-progression and network co-op.

## 9. Success metrics (provisional targets, not claims)

- 100% of mandatory adventure tests passing in host CI; sanitizer-enabled host tests clean.
- A clean build of the same gameplay sources for SDL and Dreamcast.
- Simulation remains stable during frame drops, pause/resume, checkpoint and death.
- All required actions accessible via Dreamcast controller and keyboard/gamepad on desktop.
- No collision tunneling in defined speed envelopes; no reproducible unrecoverable softlocks in the vertical-slice obstacle course.
- Homing Attack picks the same eligible target for the same replayed state, with occlusion/distance/tie-break tests.
- Stage restart, death, and repeated loading have no resource leak/cumulative corruption.
- Capture explicit median/p95 frame, memory and draw-submission metrics on target hardware/emulator. Final FPS/memory thresholds must be chosen after baseline measurements, not invented in this plan.

## 10. Important project decisions still needing evidence

1. **Target frame step:** 60 Hz update is a proposal; measure original logic and Dreamcast headroom, then decide render/update split.
2. **Collision integration:** start with .TER + separate authored-platform colliders; determine whether existing ground-query code can be safely wrapped or should be ported to a new query backend.
3. **Asset authoring:** choose a specific Blender exporter (glTF input + offline converter proposed) after validating vertex/material constraints and imported terrain coordinates.
4. **Level streaming:** keep first stage fully loaded; add streaming only after memory profiling demonstrates it is necessary.
5. **Movement tuning:** reference Adventure's feel, but calibrate values through measured jump height, acceleration, landing control and playtests, not assumed numeric constants.
6. **Dreamcast control mapping:** confirm analog/trigger data exposed by the existing platform facade, then implement continuous axis support in a dedicated input adapter.
7. **Game data licensing:** verify requirements for any externally distributed demo or stand-alone mod.

## 11. Proposed first ten development commits

1. docs: establish this approved plan and baseline measurements.
2. build: add adventure sources to both Makefiles and build smoke check.
3. core: game-mode dispatch + AdventureGame lifecycle with no race tick.
4. core: typed coordinate conversions, sidecar state, replayable fixed-step harness.
5. player: movement input/grounding/walk/run, with fixture tests.
6. player: jumping, air control, spin states and basic animation adapter.
7. collision: unified terrain/platform queries and test obstacle course.
8. camera: smoothing, orbit, obstruction and rail/camera volumes.
9. combat: target selection, Homing Attack and ring/damage/checkpoint lifecycle.
10. stage: external object placement + playable Resort Island Act 1 proof.

Thereafter, iterate on stage design and Dreamcast performance, not speculative frameworks.

## 12. Exact vertical-slice definition of done

A player can start the adventure mode from the title screen, choose Sonic, enter Resort Island Act 1, move freely, run along slopes and loops, jump between independently placed platforms, travel an alternate elevated route, attack an enemy by homing toward it, collect and lose rings, trigger a checkpoint, fall/die and respawn at the correct checkpoint, activate a spring and dash panel, reach a level goal, see stage results, save progress and return to the menu. The stage works in SDL desktop and Dreamcast builds. No race lap/result/opponent code is needed for this action loop. The stage is authored from data, all new shared code is portable C, and tests/profiling evidence accompany the result.

## 13. Next implementation entry point

Implement **P0 + P1** only first. Build adventure-mode dispatch near the current race setup/loop in **main.c**, initialize the current Resort Island resources through the existing loader, and use a single new **AdventureGame_Tick()** for the new path. Do not implement the combat or stage importer until Sonic can move and the camera can follow in the new isolated mode. Instrument field conversions and verify baseline behavior with replayable test inputs.

**This plan is intentionally a decision document and execution guide. It does not assert that any new feature already exists.**
