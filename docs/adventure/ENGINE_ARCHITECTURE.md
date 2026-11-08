# Engine Architecture and Migration Specification

**Companion to:** [Master plan](../sonic-r-adventure-plan.md)  
**Status:** Proposed; interfaces below are illustrative, NOT existing APIs.  
**Priority:** P0/P1 before content expansion.

## 1. Architectural goal

Make adventure gameplay a first-class independent client of existing rendering, asset loading, audio and platform facilities. The existing program is strongly coupled to race states in **main.c**, **race_update.c**, **player_camera.c**, **race_setup.c**, **race_timing.c**, **player_movement.c**, and **camera.c**. A robust conversion must establish clear separation instead of inserting arbitrary mode checks in dozens of legacy branches.

### Target module graph

~~~text
Platform bootstrap (existing SDL or KOS entry point)
  |
  +-- Input/clock/files/save/audio platform facades
  |
  +-- Application + screen state transitions
        |
        +-- AdventureGame (OWNED new mode)
        |     +-- Stage loader / entity registry
        |     +-- Fixed-step simulation
        |     |     +-- Input adapter
        |     |     +-- Player controller / sidecar
        |     |     +-- Collision world (.TER + new colliders)
        |     |     +-- Object/enemy interactions
        |     |     +-- Animation event output
        |     |     +-- Camera director
        |     +-- World snapshot / draw requests
        |     +-- HUD / goals / checkpoint / persistence
        |
        +-- Legacy racing (TEMPORARY, if built)
              +-- Legacy race loop and original logic
  |
  +-- Rendering subsystem (shared)
        +-- SDL OpenGL backend
        +-- KOS PVR backend
~~~

Only AdventureGame can decide stage advancement. It must not call **ComputeRacePositions**, **UpdatePlayerLapSector**, **CheckRaceCompletion**, **AdjustRubberBandAI**, ghost capture, the five-player race physics scheduler or network race sync.

## 2. Mapping existing modules and migration actions

| Current file | Verified responsibility | Proposed action |
| --- | --- | --- |
| source/sdl/src/main.c | Entry point, screen flow, race per-frame scheduling and cameras | Centralize mode selection and dispatch; do not rewrite everything immediately |
| source/sdl/src/game_loop.c | General logic/timers/title/track helpers | Extract minimal reusable resource lifecycle |
| source/sdl/src/player_camera.c | UpdateHumanPlayerPhysics, jump calculations | Read/calibrate formulas, do not reuse original update in adventure mode |
| source/sdl/src/physics_main.c | PlayerPhysicsMain + collision response | Test isolated low-level routines; exclude race/AI update |
| source/sdl/src/player_movement.c | CollectibleCollision, WallCollision, SurfaceNormalPhysics, UpdateLoopMovement, PositionUpdate | Isolate collision/geometry behavior behind adapters |
| source/sdl/src/ground_collision.c | .TER grid/polygon/height queries, loop surface logic | Wrap as one collider provider or reimplement pure query from verified format |
| source/sdl/src/ter_types.h | Binary terrain structs and static layout assertions | Retain and document version |
| source/sdl/src/camera.c | BuildChaseCamera / camera look-at / clip | New camera calculates view independently; reuse low-level view setup where practical |
| source/sdl/src/track_geometry.c | BIN track loading and position sections | Reuse legacy geometry; add new stage object data outside original BIN |
| source/sdl/src/track_load.c | LoadTerrain, LoadAI and path support | Reuse terrain loader; do not require AI file in new native stages |
| source/sdl/src/track_per_level.c | Per-track init, textures, lighting, scenery | Reuse to bootstrap Resort Island; decouple race states later |
| source/sdl/src/animation.c | Player pose/state support | Add action-state adapter before changing renderer |
| source/sdl/src/render_scene.c and render_* | Shared presentation | Keep stable initially, later expose clean draw context |
| source/dc/src/render_pvr.c, r_pvr_backend.c | Dreamcast renderer | Profile, avoid platform-gameplay dependencies |
| source/sdl/src/input/input.c | Original bitmask/race button processing and ghost paths | New normalized analog input mapping |
| source/sdl/src/hud_full.c | Original race HUD | Separate minimal adventure UI |
| source/sdl/src/save.c, source/dc/src/save_vmu.c | Existing saves | New versioned adventure save; preserve old data |
| source/sdl/src/race_update.c, race_timing.c | Race opponents, rings and timer | Keep out of adventure update; only extract audited generic effects |

## 3. Proposed source tree and interfaces

~~~text
source/sdl/src/adventure/
  adventure.h                  Public mode lifecycle
  adventure_internal.h         Internal shared definitions
  adventure_game.c             Mode update and transitions
  adventure_state.c            Screen/stage state
  adventure_input.h/.c         Continuous axes + button transitions
  adventure_player.h/.c        State machine and legacy Player bridge
  adventure_physics.h/.c       Velocity, acceleration, jumping, slope projection
  adventure_collision.h/.c     Unified ground/wall/platform/contact API
  adventure_camera.h/.c        Camera state, volumes, obstruction
  adventure_entities.h/.c      Registry/object update and stable IDs
  adventure_combat.h/.c        Targeting, attacks, damage and rings
  adventure_stage.h/.c         Stage manifest, goals, triggers, load/unload
  adventure_animation.h/.c     Action -> legacy animation mapping
  adventure_hud.h/.c           Platform-independent HUD information
  adventure_save.h/.c          Versioned progress/checkpoints
  adventure_math.h/.c          Units, coordinate conversion, deterministic helpers
  adventure_debug.h/.c         Trace, counters, optional debug overlays
tools/stage_packer/
  ...                          Offline native-stage build tools (not linked into DC)
stages/resort_island_act1/
  stage.json                   Editable source data; final schema defined later
  collision/                   Authored platform/hazard collision
  objects.json                 Spawns and interactions
  camera.json                  Camera regions/path annotations
  art/                         Only redistributable authored art; legacy content external
tests/adventure/
  ...                          Host deterministic simulation and loader fixtures
~~~

Do not create needless wrappers for every legacy function; implement facades only at unstable or platform-specific boundaries.

### Public lifecycle contract (illustrative)

~~~c
typedef enum AdventureResult {
    ADVENTURE_RUNNING,
    ADVENTURE_RETURN_TO_MENU,
    ADVENTURE_STAGE_COMPLETE,
    ADVENTURE_RESTART_STAGE,
    ADVENTURE_FATAL_ERROR
} AdventureResult;

typedef struct AdventureGame AdventureGame;
typedef struct AdventureBootConfig AdventureBootConfig;
typedef struct AdventureInput AdventureInput;
typedef struct AdventureFrame AdventureFrame;

bool AdventureGame_Init(AdventureGame *game,
                        const AdventureBootConfig *cfg);
AdventureResult AdventureGame_Tick(AdventureGame *game,
                                  const AdventureInput *input);
void AdventureGame_BuildFrame(const AdventureGame *game,
                             AdventureFrame *frame);
void AdventureGame_Shutdown(AdventureGame *game);
~~~

This API is conceptual: decide allocation and ownership details after auditing current platform entry point and static memory constraints.

### Proposed update dispatcher

~~~c
if (application_mode == APP_MODE_ADVENTURE) {
    AdventureInput input = AdventureInput_Read(platform_input);
    AdventureResult result = AdventureGame_Tick(&adventure, &input);
    AdventureGame_BuildFrame(&adventure, &frame);
    RenderAdventureFrame(&frame);
    ApplicationHandleAdventureResult(result);
} else {
    LegacyRaceUpdate(); /* transitional only */
}
~~~

Never include both legacy and new simulation updates in the same frame. If existing render code requires legacy globals to be current, bridge those globals **after** the new simulation step, not by executing legacy race physics.

## 4. Data ownership and lifecycle

### Strict ownership

- **AdventureGame** owns simulation tick counter, game mode and stage instance.
- **AdventureStage** owns stage-local entity arrays, triggers, static colliders, camera zones and goal states.
- **AdventurePlayerState** owns player action state, timers, target ID, jump buffers and checkpoint.
- **AdventureCollisionWorld** owns references/indexes to static .TER data plus dynamic/platform colliders.
- **AdventureCamera** owns its position, velocity, yaw/pitch, obstruction history and interpolation.
- **AdventureRendererBridge** reads immutable or explicitly synchronized snapshots; no simulation mutation in draw calls.
- **Platform** owns OS handles, controller/device state, file handles, sound hardware and PVR/GL resources.
- **SaveSystem** writes only finalized snapshots of persistent data and does not expose platform resources to the game.

### Lifecycle invariants

1. Stage load is transactional. If any required resource fails, roll back all partial allocations and return a controlled error.
2. No stage entity may retain pointers into memory freed on level change.
3. Entity IDs are stable within one stage load and are NOT raw pointers.
4. Event processing is bounded; a malicious stage file cannot spawn unlimited recursive effects.
5. Pause freezes simulation state/timers but can leave renderer/UI running.
6. Death resets only respawn-scoped state; stage completion preserves save-scoped progression.
7. Repeated loading, restart and exit must be leak-free and idempotent.
8. Camera and audio events generated on a gameplay tick must execute at most once, even if a frame is rendered twice.

### Proposed state containers

~~~c
typedef struct AdventureVec3 {
    int32_t x, y, z; /* explicitly documented fixed-point units */
} AdventureVec3;

typedef enum AdventureMoveState {
    MOVE_IDLE, MOVE_GROUND, MOVE_ROLL, MOVE_SPINDASH_CHARGE,
    MOVE_JUMP_ASCEND, MOVE_FALL, MOVE_HOMING, MOVE_BOUNCE,
    MOVE_HURT, MOVE_DEAD, MOVE_SCRIPTED
} AdventureMoveState;

typedef struct AdventurePlayerState {
    AdventureVec3 pos;
    AdventureVec3 previous_pos;
    AdventureVec3 vel;
    uint16_t heading;
    uint16_t rings;
    uint32_t grounded_surface_id;
    uint32_t support_entity_id;
    uint32_t target_entity_id;
    AdventureMoveState state;
    uint16_t action_ticks;
    uint16_t coyote_ticks;
    uint16_t buffered_jump_ticks;
    uint16_t attack_cooldown_ticks;
    uint16_t invulnerability_ticks;
    uint32_t active_checkpoint_id;
    uint32_t flags;
} AdventurePlayerState;
~~~

These are examples; finalize sizes, alignment, field precision, sentinel values and endianness before ABI or save format freezes. Prefer fixed-capacity storage (or bounded arenas) on Dreamcast.

### Legacy Player adapter

Implement a **PlayerAdapter_ApplyAdventureState()** to write ONLY fields directly needed by existing render/animation paths, e.g. **posX/Y/Z**, **prevPosX/Y/Z**, **velX/Y/Z**, **angleYaw**, **groundedFlag**, **ringCount**, **animId**, **charId** and any verified camera/shadow fields. Audit every reader first; this is a preliminary list.

- Do not write **trackProgress**, **racePosition**, **lap times** or **raceOrder** just to bypass checks.
- Make conversion to legacy position scale and sign explicit.
- Keep a per-frame bridge debug mode that prints both state coordinates and flags.
- Use **_Static_assert(sizeof(Player) == 0x71C, ...)** where the type is in scope, and offsets assertions for fields consumed by the bridge.
- When renderer reads additional legacy state, add a documented mapping, not another hidden direct write.

## 5. Coordinate-system specification and reconciliation

Observed code uses original per-player fixed-point coordinates (**posX**, **posY**, **posZ**), several operations shifting by 12, 0..0xFFF yaw and camera logic that negates selected coordinates.

**Do not assume all source files use the same sign conventions.** First create empirical probes in Resort Island:

- Teleport to a known ground point and log positions as decoded by the .TER query, player sprite and camera.
- Move along positive local X/Z and inspect whether model/camera agree.
- Jump once to verify sign and scale for upward velocity.
- Rotate exactly 90/180 degrees and compare visual forward with analytic yaw.
- Probe normal orientation on horizontal and sloped faces.
- Validate conversion round trips: legacy → adventure → legacy with bounded rounding error.

Proposed canonical adventure math:

- X horizontal, Y positive up, Z horizontal.
- Fixed-point scale defined by **ADVENTURE_FRAC_BITS** (initial proposal: 12 for minimal bridge cost).
- Yaw normalized to [0, 4095], convention chosen after the probes.
- A single unit conversion module maps canonical vectors to old signed and inverted fields.
- A **MovementBasis** type computes camera-relative forward/right vectors.
- Integration uses explicit 64-bit intermediates for multiply/accumulate and clamps before narrowing.
- Rendering conversions happen after authoritative physics, never the reverse.

### Numeric hazards and guards

- Avoid overflow on squared distance; use 64-bit math for large world coordinates.
- Saturate acceleration/velocity and check negative right shifts for implementation portability.
- Use fixed stable tie-breaking for target detection and contact sorting.
- Normalize collision normals once at loading or use encoded unit scale with documentation.
- Account for floating-point differences between desktop and SH-4: deterministic tests should compare selected quantized state, not pixel-perfect outputs unless such equivalence is feasible.

## 6. Fixed-step scheduler

Proposed stages of each authoritative tick:

1. Fetch normalized input sample and persist it in replay log.
2. Advance input button edges/buffers.
3. Resolve active camera/player frame and desired ground movement.
4. Execute jump, roll, spin dash, homing and other action transitions.
5. Integrate velocity and sweep player against world.
6. Resolve static and dynamic contacts; update grounding, slopes and platform carry.
7. Update damage/hazard checks and collectible pickups.
8. Update enemies/platforms; use a documented pre- vs post-movement order.
9. Process queued interaction events (checkpoint, goal, ring scattering, SFX).
10. Update animation state and camera director.
11. Snapshot relevant state for drawing, hashing and optional saves.

**Ordering caveat:** Moving platform transforms normally need to be known before player sweep, while interactions need resolved player position. Final schedule should have explicit **PrePhysicsEntities** and **PostPhysicsEntities** phases. Do not hide extra entity movement inside draw.

A bounded frame accumulator may run multiple fixed ticks after a slow render, but must define a maximum catch-up count to avoid spirals. Make the user-visible game slow rather than passing unbounded deltas into physics when badly overloaded. Pause/resume resets accumulator and input-edge buffers.

### Recommended instrumentation

- tick index, input sample and action state
- canonical position/velocity/heading
- current supporting collider, floor normal and grounded state
- target entity ID/homing cooldown
- rings, health/invulnerability, checkpoint and goal flags
- camera position/orientation/obstruction state
- wall-clock time per simulation, collision, entity and render phase
- counts: visible objects, physics candidates, collision contacts, spawned effects

## 7. Input boundary

Old controller processing combines keyboard/joy words, filters input during countdown, and records ghost/multiplayer state. Adventure must not inherit racing masks by accident.

~~~c
typedef struct AdventureInput {
    int16_t move_x;        /* normalized signed axis */
    int16_t move_y;
    int16_t camera_x;
    int16_t camera_y;
    uint32_t buttons_down;
    uint32_t buttons_pressed;  /* edge this sim tick */
    uint32_t buttons_released;
} AdventureInput;
~~~

- Support keyboard, SDL gamepad and Dreamcast pad with one mapping policy.
- Normalize dead zones radially, preserve analog magnitude and verify inversion.
- Translate button edges once per authoritative tick (not once per rendered frame).
- Use action names rather than undocumented legacy bit masks.
- Record input in deterministic test fixtures; do not record raw platform events.
- Ensure the current Dreamcast platform implementation actually passes analog values; add facade support where missing.
- Separate context-sensitive jump/attack rules from physical button binding.

## 8. Collision integration seam

Define a generic query API with explicit modes: supporting floor, sweep capsule/sphere, overlap, line-of-sight and camera obstruction.

~~~c
typedef struct AdventureContact {
    AdventureVec3 position;
    AdventureVec3 normal;
    uint32_t surface_id;
    uint32_t entity_id;     /* zero or invalid sentinel for static terrain */
    uint16_t material_id;
    uint16_t flags;
} AdventureContact;

bool AdventureCollision_SweepPlayer(
    AdventureCollisionWorld *world,
    AdventureVec3 from, AdventureVec3 to,
    AdventureContact *out_contact);

bool AdventureCollision_FindSupport(
    AdventureCollisionWorld *world,
    AdventureVec3 origin,
    AdventureContact *out_ground);

bool AdventureCollision_Raycast(
    AdventureCollisionWorld *world,
    AdventureVec3 from, AdventureVec3 to,
    uint32_t mask,
    AdventureContact *out_hit);
~~~

Actual signatures may need player radius/height, collision layer masks and result arrays. This sketch makes required operations explicit. Do not let **GroundCollision(Player*)** remain the only available terrain query: it MUTATES Player and clamps grid boundaries, which is inappropriate for general raycasts and ghost-free test fixtures. Extract a read-only provider before building a new action controller around it.

### World partitioning

- Preserve the .TER 32×32-style indexing where it maps correctly to the old static data.
- Add an authored static collider grid/BVH offline-built for new platforms and ceilings.
- Use an explicit, bounded dynamic collider list for moving platforms.
- Avoid expensive whole-world per-entity scan during every physics tick.
- Merge candidate contacts with stable filtering and deterministic sort.
- Differentiate blocking, one-way, trigger, damaging, slippery and camera-only collision masks.

## 9. Camera output and renderer bridge

Current camera view building uses **CamStateEntry**, **RenderCamera** and global per-viewport state. New camera should calculate a platform-neutral pose and apply it through a small, audited bridge using current render view construction, not rewrite Dreamcast vertex projection in P1.

~~~text
AdventureCameraState
     |
     +-- desired position/look target/yaw/pitch/FOV
     +-- collision-adjusted and smoothed pose
     |
     v
AdventureRenderCameraAdapter
     +-- SDL render context
     +-- DC existing viewport/camera matrices
~~~

Exact mapping to **BuildCameraView**/**SetViewportClipRect** must be validated with a test scene; those functions may perform additional race-dependent behavior. Never write camera fields independently in multiple modules.

## 10. Stage and resource APIs

A stage loader should load an immutable stage definition, then create a mutable stage instance:

~~~c
AdventureStage *AdventureStage_Load(const char *stage_id,
                                   AdventureResourceContext *resources);
void AdventureStage_Reset(AdventureStage *stage,
                          AdventureResetReason reason);
void AdventureStage_Unload(AdventureStage *stage);
~~~

Initial legacy stage adapter should call currently proven track loaders and keep their object allocations intact. New stage metadata should be loaded from an external binary sidecar so it can be authored independently of the original .BIN/.TER format. Later a native stage source can replace legacy geometry.

- Verify all read offsets and maximum counts on desktop before running on Dreamcast.
- Endian handling must be explicit; file format includes version, file byte size and section directory.
- Never embed pointers in serialized stage files.
- Record stage ID, spawn ID and asset package IDs as stable strings/hashes only as appropriate for the runtime.
- Support optional features without assuming all legacy tracks have AI/spline/racing metadata.

## 11. Persistence and backward compatibility

A new save namespace must avoid corrupting existing Sonic R data, particularly Dreamcast VMU saves. Proposed **ADVSAVE** versioned layout:

~~~text
header: magic, major/minor version, total bytes, checksum
profile: settings and control preferences
progress: unlocked stages, completed acts, collectibles
records: optional best time/rings/rank per stage
checkpoint: optional volatile resume point (only if design requires)
~~~

- Write to a separate desktop filename and separate VMU filename/application ID if supported.
- Specify endian, checksums, interrupted-save behavior and validation on load.
- Corrupt or newer save versions must fail gracefully without deleting the file.
- Persist stage progression only after successful completion/verified checkpoint policy.
- Avoid copying raw legacy Player or entity structs into saves.
- Add host round-trip tests, version migration tests and fake interrupted-write tests.

## 12. Memory/resource model

Draft resource accounting categories for Dreamcast:

| Memory / cost | Consumers | Expected actions |
| --- | --- | --- |
| System RAM | stage geometry, .TER, collision metadata, entity arrays, animation, gameplay stack | Count and bound; avoid unbounded heap churn |
| PVR texture VRAM | texture pages, UI, sky and effects | Retain original texture pages; add budgeted atlas slots |
| PVR geometry submission | world polygons, characters, effects | Profile opaque/translucent modifier lists and batching |
| AICA/sound memory | sound effects/music streams | Reuse dispatch; avoid large concurrent new audio assets |
| CPU | movement, collision, targeting, camera, sound decode, render preparation | Measure per phase, especially worst-case object counts |
| Disc I/O | stage load and music | Preload stage manifest and hot objects; profile seeks on hardware |

Dreamcast has strict resource ceilings, but the **remaining free capacity of this particular engine is unknown** until profiled. Set budget ceilings from measured high-water marks, not hardware maximums alone.

## 13. Build integration

- Extend **source/sdl/Makefile** source list for the shared adventure files.
- Extend **source/dc/Makefile** while respecting existing SDL replacements.
- Introduce a dedicated test build (e.g. **make adventure-test**) for logic with fake platform/collider adapters.
- Avoid platform headers leaking into adventure public headers.
- Compile with GCC/Clang warning checks and run sanitizers on desktop where supported.
- Add compiler-time feature flags only to eliminate platform-only code; use runtime data for gameplay variation.
- Preserve a single authoritative game logic implementation, no copy-pasted Dreamcast physics.

## 14. Integration procedure, with explicit gates

1. **Baseline:** run original game and collect deterministic input/render observations.
2. **Build hook:** compile empty adventure module for both targets.
3. **Mode hook:** launch stage without original race progression; add controlled return to title.
4. **World bootstrap:** share existing geometry/character/sky render path.
5. **Coordinate probe:** demonstrate player/camera/terrain agreement.
6. **Player sidecar:** render-driven legacy Player adapter with no original physics.
7. **New simulation:** process analog inputs and apply controlled velocity.
8. **Collision seam:** query ground/platform, resolve contacts and integrate motion.
9. **Camera seam:** direct existing render camera from new pose, no racing follow path.
10. **Stage entities:** load new object sidecar and run interaction events.
11. **Remove transitional dependencies:** delete obsolete race references from adventure mode only after instrumentation proves not required.
12. **Clean API review:** remove compatibility scaffolding, consolidate public headers and ensure no anonymous globals have become new infrastructure.

## 15. Architecture acceptance tests

- Build SDL and DC without linking missing adventure symbols.
- Adventure mode runs zero racing physics, lap/rank/ghost/net updates; add counters/assertions in test build.
- Exit/re-enter a stage 100 times in host harness without memory leaks.
- Compare adapted legacy Player fields to canonical adventure state at every tick.
- World-to-render coordinate conversion passes origin/large/negative/elevated round-trip tests.
- Camera can orbit player while player movement remains camera-relative and motion remains stable.
- Loading malformed stage files fails safely without out-of-bounds reads.
- Save load/write uses dedicated namespace and leaves legacy save fixture unchanged.
- Replaying recorded input from identical initial state yields identical quantized gameplay hashes on the same build, with cross-platform divergence documented and minimized.

## 16. Technical anti-patterns explicitly prohibited

- Scattering "adventure mode" conditionals across 30 original race source files.
- Adding fields to Player without an audit of binary offsets.
- A world entity querying or altering renderer backend globals directly.
- Using real-time delta to update movement physics.
- Mutable position conversions performed in three different places.
- Overloading race-position/lap fields as adventure counters.
- Hardcoding stage object indices/positions in gameplay C source.
- Duplicating collision physics for desktop and Dreamcast.
- Implementing Homing Attack before verifying collision and camera convergence.
- Adding generic plugin/ECS abstractions solely for hypothetical future features.

The desired architecture is boring, strict and testable. The game's complexity belongs in its movement, authored stages and interactions, not in hidden cross-module side effects.
