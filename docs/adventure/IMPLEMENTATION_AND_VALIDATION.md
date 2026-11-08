# Implementation Backlog, Verification Plan and Dreamcast Performance Gates

**Parent:** [Master plan](../sonic-r-adventure-plan.md)  
**Status:** Detailed implementation work breakdown; no tasks in this document are claimed complete.  
**Baseline used for planning:** main at 3da98f2e134eac82e45cd69660216fa0e4563ac5; refresh before implementation.

## 1. Delivery policy

The first deliverable is **one full Resort Island Act 1 vertical slice** running on both desktop and Dreamcast. Every work package below has a concrete output, dependency and acceptance gate. Merge small independently reviewable commits with runnable tests. Avoid building all systems concurrently before proving movement, collision and camera.

### Priorities

- **Critical:** if blocked, no functional action game is possible.
- **High:** needed for a convincing vertical slice.
- **Medium:** important polish or production workflow; can follow core gameplay.
- **Deferred:** explicitly out of scope until the vertical slice passes.

### Definition of "done" per work package

1. Shared sources compile on all supported build targets or have documented build-platform guards.
2. Changed behavior has host test fixtures or a reproducible manual test script.
3. Run input recorder/replayer for relevant physics/camera paths.
4. Keep no platform-specific includes in adventure simulation modules.
5. No unrelated source cleanup unless required by the work package.
6. New public APIs and owns/borrows semantics documented.
7. Measured effects on FPS, RAM, VRAM and stage load documented where relevant.
8. Known limitations added to a visible issue/backlog, not hidden in code comments.
9. Review whether legacy racing globals or fields were introduced into new logic.
10. Screenshot/recording and test log accompany visual/mechanical changes when reasonable.

## 2. Work breakdown (ordered by dependency)

### WP-00 — Baseline capture and workspace setup
**Priority:** Critical  
**Depends on:** Nothing

Deliverables:
- Record clean main SHA, compiler/toolchains and exact original-data folder layout.
- Desktop build and Dreamcast build smoke script.
- Original Resort Island capture of player position/yaw, rings, ground collision flags, camera and visible scenery.
- Basic performance probe logging simulation and render duration without touching game rules.
- Resource high-water tracking (static/dynamic allocations and significant stage/texture buffers).
- Naming convention and static analysis options, deterministic input fixture scaffold.

Tests:
- Both legacy builds still compile from clean checkout with the expected external toolchains/data.
- Original stage can launch, move and exit; no code was changed other than instrumentation.
- Screenshot/reference data and debug logs are retained as reproducible comparison artifacts where rights allow.
- Baseline memory/FPS numbers labeled with hardware/emulator settings.

### WP-01 — Adventure build target and entrypoint
**Priority:** Critical  
**Depends on:** WP-00

Deliverables:
- Add **source/sdl/src/adventure/** to desktop and Dreamcast Makefiles.
- New **APP_MODE_ADVENTURE** application selection path.
- Stub **AdventureGame_Init/Tick/BuildFrame/Shutdown** modules.
- Safe return to existing title on request, with no race result handler.
- Clear status messages for missing user-supplied legacy assets.

Tests:
- SDL and Dreamcast builds link the same adventure source list.
- Adventure mode enters/resumes/leaves with empty simulation.
- Original race mode unaffected under baseline comparator.
- Instrumented assert shows zero calls to race rank/lap/AI update during action mode.

### WP-02 — Runtime contracts and deterministic math
**Priority:** Critical  
**Depends on:** WP-01

Deliverables:
- Document canonical axes/world unit/angle conventions through empirical probes.
- **adventure_math** for fixed-point vectors, dot, projection, normalization, saturating multiply and interpolation.
- Legacy Player sidecar and explicit renderer bridge.
- Normalize platform input to move/camera axes and action-edge flags.
- Fixed-step accumulator and replay log format.
- Single-owner game/stage/player context.

Tests:
- World/legacy coordinate round trips over positive/negative/extreme positions.
- 90°/180° yaw test; camera forward and model forward agree.
- Jump/up/down signs and terrain normal orientation tested in live scene.
- Deterministic playback of 10k simulation ticks in host harness.
- Analog dead zone/diagonal behavior and button edge tests.
- Fixed-step pause/resume and dropped-render-frame tests.

### WP-03 — Walking/running physics
**Priority:** Critical  
**Depends on:** WP-02

Deliverables:
- Full directional input, acceleration, friction, braking and speed-dependent turning.
- State transitions idle/walk/run and animation adapter.
- Baseline static ground support; no race auto-acceleration.
- Momentum interpolation without reversal snap.
- Tuning instrumentation/CSV results from obstacle course.

Tests:
- No spontaneous drift; no dependence on pressing acceleration.
- Reversal, diagonal movement, speed cap and smooth stopping.
- Simulator hashes stable across repeated host runs.
- Sonic faces movement and camera-relative input correctly.
- Character render position matches collision position.

### WP-04 — Ground/air collision and jumping
**Priority:** Critical  
**Depends on:** WP-03

Deliverables:
- Read-only support queries using legacy .TER and separate platform colliders.
- Explicit player collision shape, contact mask/materials.
- Jump impulse, air control, landing state, variable jump, coyote time and buffering.
- Ceiling/wall handling, narrow platforms, fall volumes.
- Configurable maximum iterations/speed envelope for sweeps.

Tests:
- Horizontal ground, uneven incline, concave region, top/bottom floors.
- Jump under ceiling and landing on small raised target.
- Fast movement through narrow collision section.
- No fall-through under simulated slow rendering.
- Stable contact ID ordering and no unbounded penetration.
- No physics clipping outside old race grid clamp.

### WP-05 — Third-person action camera
**Priority:** Critical  
**Depends on:** WP-04

Deliverables:
- Follow/orbit camera with velocity lead and smooth vertical tracking.
- Desired camera vs obstruction-adjusted camera, hysteresis.
- Stage camera zone/priority data in first scene.
- Camera view bridge to existing renderer.
- Manual camera control and recenter.

Tests:
- Tall jump/fall and stacked platform traversal.
- Collision obstruction near walls and low ceilings.
- Rapid turn/stop/reverse with no severe camera snap.
- Stage zone overlap with deterministic winner.
- Dreamcast camera matrix visual comparison against debug fixed camera.

### WP-06 — Spin moves and authored momentum
**Priority:** High  
**Depends on:** WP-03/04

Deliverables:
- Rolling state, slope gravity, Spin Dash charge/release.
- Dynamic action tuning table.
- Existing loop entry inspection and hybrid guided loop prototype.
- Spring and dash impulse helper functions.

Tests:
- Roll high-speed downhill/uphill and low-speed exit.
- Spin Dash release only when valid; interrupt on damage.
- Loop at too-low speed safely fails or routes to recovery.
- Loop success exits with preserved velocity; no stuck state.

### WP-07 — Entities and runtime events
**Priority:** High  
**Depends on:** WP-04

Deliverables:
- Typed entity registry, stable IDs, bounded allocations.
- Trigger volumes, rings, basic monitor, spring, dash, checkpoint, goal.
- Bounded interaction event queue with deterministic ordering.
- Scene reset/respawn policies.

Tests:
- Ring pickup exactly once; mass ring-scatter bounded.
- Spring cooldown, dash trigger, monitor destruction.
- Checkpoint activated, fall/respawn and no infinite re-trigger.
- Goal event emitted once; stage exit uses adventure results.
- Stage restart resets correct objects without reloading old race state.

### WP-08 — Homing Attack and enemy behavior
**Priority:** High  
**Depends on:** WP-04/07

Deliverables:
- Basic enemy with idle/patrol/attack/hit/defeated states.
- Target acquisition with cone/range/occlusion checks and stable score/tie-breaking.
- Homing trajectory, obstacle sweep, impact, bounce, retry/cooldown.
- Central damage API for Sonic/enemies; invulnerability, ring loss and death.

Tests:
- Two targets same score; same target chosen every replay.
- Enemy behind obstacle ignored.
- Target removed mid-flight does not crash/stick.
- Successful enemy chain, ring loss, zero-ring death, invulnerability duration.
- No duplicate enemy defeat/reward event.
- Homing moves through real collision space, never teleports past a wall.

### WP-09 — Scene schema and offline packer
**Priority:** High  
**Depends on:** WP-02/04/07; may proceed in parallel after contracts freeze

Deliverables:
- Strict authored JSON schema and packer CLI (**validate/build/inspect**).
- Binary versioned stage package with little-endian sections.
- Geometry/collision/object/camera ID pipelines.
- Legacy Resort Island base-stage adapter.
- Packer-generated budget report.
- Dreamcast disc packaging integration with uppercase paths.

Tests:
- Golden binary fixture and byte-for-byte deterministic repack.
- Truncated/corrupt/unsupported version rejection.
- Missing entity references and duplicate IDs cause explicit validation errors.
- No raw serialized pointers.
- Host and Dreamcast loaders interpret identical placement.
- Changing a spring position in authored data moves it without recompilation.

### WP-10 — Resort Island Act 1 blockout
**Priority:** High  
**Depends on:** WP-04/05/07/08/09

Deliverables:
- Stage manifest and stable spawn point.
- Safe introductory route, first jump/spring, enemy area.
- Speed/slope/loop route and optional elevated shortcut.
- Checkpoint, risky platform chain, goal and results.
- Recovery areas and death region.
- Camera volumes for every major dramatic traversal.
- Stage design metrics and route diagram.

Tests:
- Full stage can be completed without exploiting race data.
- Player can backtrack and explore safely in non-scripted areas.
- Alternate path gives visible gameplay value.
- No blind jump or unrecoverable camera lock on critical path.
- Checkpoint restores to safe ground and the correct camera.
- Every mandatory collectible/combat object functions under repeated restart.

### WP-11 — Adventure HUD, results and saves
**Priority:** High  
**Depends on:** WP-07/10

Deliverables:
- New rings display, minimal status/attempts, optional time, pause.
- Stage results and stage-select state.
- New versioned desktop and VMU save namespace.
- Save checksum/recovery policy and progress migration strategy.

Tests:
- Original Sonic R save not overwritten.
- New save corruption handled without crash.
- Stage completion persists and restores on restart.
- Pausing during interactions does not double-process events.
- HUD no longer displays race rank/lap fields.

### WP-12 — Dreamcast hardening, polish and release candidate
**Priority:** Critical final gate  
**Depends on:** WP-01 through WP-11

Deliverables:
- Memory, SH-4 and PVR traces in representative worst-case scenes.
- Sound effects/streaming check while action physics and effects run.
- Optimize expensive collision broadphase, target scanning and camera casts.
- LOD/material batching/texture atlas where bottlenecks are measured.
- Clean failure on memory exhaustion, malformed stage data and missing assets.
- Controller mapping verification on hardware.
- Release notes and updated desktop/Dreamcast build/run documentation.

Tests:
- Complete stage twice without restart and across cold launch.
- 100 stage restarts in host automated test, repeated restart on Dreamcast emulator and hardware where available.
- Sustained gameplay stress around max active rings/enemies/particles.
- Controller unplug/replug on desktop; unplug/port change handling on DC where supported.
- All blocking priority defects closed; captures/performance report attached.

## 3. Dependency graph

~~~text
WP-00 baseline
   |
   v
WP-01 mode and build
   |
   v
WP-02 units, input, sim, sidecar
   |
   +------------------+--------------------+
   |                  |                    |
   v                  v                    v
WP-03 controller   WP-09 packer *      stage/tool research
   |                  |
   v                  |
WP-04 collision/jump |
   |                  |
   +-----------+------+------------------+
   |           |                         |
   v           v                         v
WP-05 camera  WP-06 roll/loops        WP-07 entities
                                            |
                                            v
                                        WP-08 combat
                                            |
                  +-------------------------+
                  v
               WP-10 Act 1 blockout
                  |
                  v
               WP-11 UI/save
                  |
                  v
               WP-12 DC polish / release

* WP-09 can begin on schemas after WP-02 but finalizes after
  collider/entity contracts are stable.
~~~

No need to implement all phases strictly sequentially when two developers can operate on independent contracts, but always gate downstream integration on a verified upstream ABI/data format.

## 4. Testing pyramid

### Level A — Pure host unit tests

Run without SDL/OpenGL, KOS or original proprietary game data whenever possible.

Subjects:
- vector/fixed math, axes and coordinate conversions;
- input dead zone/edge/buffer behavior;
- state machine and transition priorities;
- acceleration, friction, turn, jump and roll curves;
- collision fixture queries and contact-order stability;
- homing target ranking and damage policy;
- entity event queues, duplicate activation and pool bounds;
- stage schema/binary parser and save checksums.

Fixture format: small hand-authored worlds of boxes/planes plus deterministic recorded input. Assert state and bounds, not pixels.

### Level B — Integrated simulator and test scene

Run SDL build with special debug/test stages using minimal/proxy art and original data only where legally supplied.

Subjects:
- rendering bridge and Player sidecar;
- .TER terrain + authored collider merger;
- camera obstruction and zones;
- moving platforms and enemy choreography;
- pause/goal/restart and 100 reload cycles;
- stress with max registered objects;
- generated screenshots/MP4 captures if recording is available.

Maintain debug knobs:
- draw collision shapes and surface normals;
- draw player velocity/camera direction;
- display world vs legacy coordinates;
- freeze/step one simulation tick;
- slow-motion rendering without changing simulation rules;
- force specific entity/camera zone by ID;
- print per-stage and per-frame resource counts.

### Level C — Dreamcast emulator

Use emulator smoke tests to validate:
- correct SH-4 and KOS build linkage;
- stage package file naming/endian/alignment;
- character and scenery rendering;
- camera projection/near clipping;
- input and action button mapping;
- VMU save behavior;
- PVR sorting/alpha and geometry load;
- long-gameplay stability and restart loops.

An emulator is valuable but not an authoritative measure of optical-disc latency, audio synchronization or physical hardware timing.

### Level D — Physical Dreamcast

Verify actual Dreamcast behavior whenever accessible:
- controller analog range, triggers and button edges;
- in-game simulation performance and frame pacing;
- PVR poly/material cost and visible artifacts;
- RAM/VRAM pressure and stack safety;
- load-time spikes and disc/ODE read behavior;
- AICA playback under worst-case CPU pressure;
- VMU save/load and corrupt/missing media handling.

Keep results labelled: **emulator vs real hardware**; do not equate them.

## 5. Deterministic replay and state hashing

A replay fixture should record:
- stage ID and package hash;
- initial player/stage seed and configuration;
- fixed tick rate, build identifier and input mapping version;
- per-tick normalized movement/camera input and action edges;
- optionally checkpoints of expected state hash at tick intervals.

State hash should include the CANONICAL game state (position, velocity, action, rings, entity states, checkpoint, selected targets, RNG state) but exclude opaque platform pointers, SDL handles and render-only transient addresses.

Replay requirements:
- Same build, same fixture and same platform produce matching hashes.
- Cross-compiler/platform divergence is diagnosed with targeted fixed-point conversion tests before claiming cross-platform bit-exactness.
- Record first diverging tick, player coordinates, collision contacts, target candidate set and active events.
- Use stable iteration order for entities and collision candidates.
- Separate deterministic gameplay random generator from particle/visual RNG.
- Log dropped/render frames independently from simulation ticks.

## 6. CI/build matrix proposal

| Target | Build | Unit | Integration | Notes |
| --- | --- | --- | --- | --- |
| Linux SDL | GCC / Clang | Yes | Yes with assets or fixtures | ASan/UBSan when supported |
| macOS SDL | Clang | Yes | Smoke where runner supports | Platform-specific link flags |
| Windows SDL | MSYS2/MinGW | Yes | Smoke | Input/window and path checks |
| Dreamcast | SH-4/KOS cross compiler | Portable subset | Emulator or manual | Requires toolchain and assets |
| Stage packer | Host toolchain | Parser/format goldens | Export/reimport fixtures | Byte-for-byte reproducibility |

Separate tests needing commercial assets from public CI; public CI can use synthetic/rights-clear fixtures and validate core gameplay data formats.

### Suggested source commands (verify existing target names)

~~~sh
# Desktop engine build (existing layout)
cd source/sdl
make

# Dreamcast cross build (existing layout)
# Source the correct local KallistiOS environ.sh first
cd source/dc
make
~~~

Test targets such as **make test-adventure**, **make test-packer** and **make dc-smoke** are TO BE ADDED, not preexisting promises.

## 7. Benchmark protocol

### Record baseline before changes

On a reproducible machine/Dreamcast setup collect:
- build commit, compiler flags, platform and emulator/hardware revision;
- video mode/resolution and render backend;
- current stage, character, weather, camera viewpoint and save state;
- median, p95 and worst simulation tick time;
- median, p95 and worst frame/present time;
- PVR draw lists / submitted primitives and texture page changes where accessible;
- used/peak system RAM, texture VRAM, sound RAM and stack margins;
- file load duration and concurrent audio state.

Save a repeatable input sequence and scene camera path to compare versions.

### Benchmark scenes

1. **Flat training arena:** isolates controller/camera overhead.
2. **Stacked-platform collision scene:** multiple overlapping layers, tiny support surfaces.
3. **High-speed obstacle and loop:** stress sweep/contact sorting and camera motion.
4. **Combat chain:** several target candidates, raycasts and particles.
5. **Ring scatter:** maximum configured number of temporary rings.
6. **High-vista Resort Island:** maximum visible scenery/poly/submission cost.
7. **Moving platform and hazard cluster:** broadphase, platform carry and events.
8. **Stage transition/restart:** peak transient resource lifetime.
9. **Audio-heavy action:** physics, effects and stream decoding at once.

### Performance decision process

1. Measure **before** and **after** each feature using identical input/scene.
2. Identify bottleneck by phase (simulation, collision, camera, entities, render, disc/audio) before optimization.
3. Optimize structural high-cost operations first, e.g. replace O(N) full target scan with grid lookup, precompute static surfaces, reduce allocations, batch draw calls.
4. Track visible quality and input latency alongside FPS, not FPS alone.
5. Choose a real-world target for Dreamcast simulation and rendering only after baseline hardware measurements.
6. Enforce memory headroom and consistent frame pacing during worst-case sections, not only average throughput.
7. Avoid premature fixed-size micro-optimizations that endanger collision correctness.

### Optimization backlog by measured symptoms

| Symptom | First investigations |
| --- | --- |
| CPU collision spike | Candidate broadphase, narrowphase iterations, repeated .TER query |
| Camera stutter | Repeated obstruction raycasts, smoothing state, surface switching |
| Slow homing target select | Spatial filter/occlusion count/target ID sorting |
| PVR stall | Render bucket ordering, texture/material switches, translucency |
| VRAM pressure | Duplicate tpages/atlases/unused assets, LOD |
| AICA stutter | Streaming decode and SH-4 contention with rendering |
| Disc spikes | Too many individual files, stage package grouping, seek behavior |
| Respawn hitch | Unnecessary full reload, cache ownership and entity reset |
| Memory growth | Particle/entity pools, resource lifetime, repeated stage allocations |

No optimization should silently weaken collision correctness or compromise camera control.

## 8. Safety and malformed data requirements

- Validate all packed file offsets/counts/length multiplications.
- Bound entity instance count and event/particle/ring-scatter pools.
- Fail on cyclic/invalid entity references and malformed camera paths.
- Avoid per-frame heap churn for entity/attack/contact calculations.
- Detect out-of-range character IDs, animation indices and texture/material IDs.
- Validate spawn point support and clearance.
- Avoid division by zero when normalizing near-zero vectors or selecting overlapping targets.
- Verify fixed-point multiply overflow before narrowing.
- Prevent save corruption on failed stage transition.
- Original racing saves and asset folders must remain unchanged by new mode.
- No hidden license-incompatible original assets added to git, test captures or release packages.

## 9. Risk register and mitigations

| Risk | Impact | Mitigation | Earliest detection |
| --- | --- | --- | --- |
| Original .TER representation cannot describe stacked platform collisions | Critical | Independent authored collider provider, canonical contact merge | WP-04 |
| Legacy Player fields still used unexpectedly in renderer | High | Central bridge, offsets assertions, field read audit | WP-02 |
| Original race init assumes five players/AI/race results | Critical | Thin app dispatch, isolated lifecycle, instrumentation asserts | WP-01 |
| Speed physics tunnels at high velocity | Critical | Continuous sweeps/substeps and defined collision speed envelope | WP-04 |
| Camera fails at vertical platforms/loops | High | Early obstacle course, camera zones and obstruction casts | WP-05 |
| Dreamcast budget too tight for full proposed stage | Critical | Packer resource report and early hardware benchmark | WP-00/09 |
| Input feels like racing despite added actions | High | Controller tuning tests before stage art | WP-03 |
| Stage repacking non-deterministic or too many disc seeks | Medium | Binary goldens, stable ordering, grouped stage packages | WP-09 |
| Original level geometry constrains platform design | Medium | Supplement with authored platform meshes; do not rewrite legacy geometry | WP-10 |
| Proprietary art/audio included accidentally | High | Explicit asset provenance and public CI rights-clear fixtures | WP-09 |
| Save system corrupts existing VMU slots | High | New namespace/version/checksum, fixture tests | WP-11 |
| Architectural "mode if" creep | High | Review every dependency; separate game mode module | WP-01 onwards |
| Cross-platform fixed math diverges | Medium/High | Conversion suite, stable rounding, replay first-divergence logs | WP-02 |
| Excessive feature scope before playable stage | Critical schedule | Vertical-slice gate, freeze deferred backlog | Throughout |

## 10. Review and code quality checklist

For every adventure gameplay PR/commit:
- [ ] Does this add a new dependency on race-specific state or original binary offsets?
- [ ] Is there exactly one authoritative owner for modified player and entity state?
- [ ] Could the same input sequence produce a different result due to iteration order or FPS?
- [ ] Are all coordinate scales, normal directions and sign inversions explicit?
- [ ] Is one update step called only once per simulation tick?
- [ ] Does it still compile as C on both toolchains?
- [ ] Are failure paths bounded and resources released?
- [ ] Are the new public interfaces smaller than the functionality they abstract?
- [ ] Can a headless test exercise this change without SDL/KOS?
- [ ] Was Dreamcast memory/CPU impact considered and measured if relevant?
- [ ] Does it work with user-supplied original data without bundling that data?
- [ ] Are unused feature flags, compatibility fallbacks and temporary copies removed?

## 11. Issue decomposition / implementation sequence for the tracker

Suggested issue titles (one issue = one reviewable deliverable):

**Milestone M0: Establish an adventure baseline**
- [ ] M0.1: Baseline builds and source auditing
- [ ] M0.2: Benchmark and rendering reference captures
- [ ] M0.3: Minimal host game-state hash/replay harness

**Milestone M1: Isolate new game mode**
- [ ] M1.1: Shared adventure module build integration
- [ ] M1.2: New application dispatch path; no race tick
- [ ] M1.3: AdventureGame lifecycle and stage exit
- [ ] M1.4: Coordinate probes, canonical units and legacy Player bridge
- [ ] M1.5: Normalized input facade (SDL + DC)

**Milestone M2: Controller / collisions**
- [ ] M2.1: Ground directional locomotion and speed curves
- [ ] M2.2: Static support collision and grounded state
- [ ] M2.3: Jump and air control / landing / buffer/coyote
- [ ] M2.4: Sweeps and reliable walls/ceilings/narrow platforms
- [ ] M2.5: Roll/Spin Dash and slopes
- [ ] M2.6: Moving platform support
- [ ] M2.7: Loop/rail hybrid prototype

**Milestone M3: Camera and presentation**
- [ ] M3.1: Follow and manual orbit input
- [ ] M3.2: Camera obstruction and hysteresis
- [ ] M3.3: Camera zones and scripted spline paths
- [ ] M3.4: Animation state mapping and effect events

**Milestone M4: Interaction and combat**
- [ ] M4.1: Entity registry / triggers / event queue
- [ ] M4.2: Ring pickup, damage scatter, invulnerability, death/respawn
- [ ] M4.3: Springs, dash panels and monitors
- [ ] M4.4: Basic enemy AI, hitboxes and reset behavior
- [ ] M4.5: Homing Attack target selection, movement and chaining
- [ ] M4.6: Checkpoint and goal completion
- [ ] M4.7: Adventure HUD/results

**Milestone M5: Stage tooling and Act 1**
- [ ] M5.1: Stage source schema and packer validation
- [ ] M5.2: Binary packer, loader and golden fixtures
- [ ] M5.3: Blender collision/mesh export prototype
- [ ] M5.4: Resort Island collision/debug survey
- [ ] M5.5: Stage blockout and safe/fast routes
- [ ] M5.6: Combat/setpieces/camera/checkpoint pass
- [ ] M5.7: Dreamcast art/geometry/texture budget pass
- [ ] M5.8: Playtest and tuning

**Milestone M6: Production stabilization**
- [ ] M6.1: Versioned adventure save on desktop/VMU
- [ ] M6.2: Replay regression and sanitizer suite
- [ ] M6.3: Dreamcast hardware profile/stress testing
- [ ] M6.4: Compatibility/resource/provenance audit
- [ ] M6.5: Documentation and packaging
- [ ] M6.6: Vertical slice release gate

This backlog can be copied into GitHub Issues after the architecture and target control scheme are approved. Do not create all issues before confirming that the chosen integration path actually works.

## 12. Minimal viable proof sequence (engineering order)

The following sequence is the fastest route to proving feasibility before a large investment:

**Proof A — Legacy stage without racing**
- Boot adventure mode into Resort Island, draw scenery and Sonic, disable all lap/rank/opponent progression.
- Use debug stepping to confirm exactly one tick path executes.

**Proof B — New movement on old geometry**
- Drive a sidecar position through normalized input.
- Bridge into legacy Player for rendering.
- Query .TER support without legacy race movement.
- Run and jump freely.

**Proof C — Action camera**
- Follow Sonic around the scene and elevated test platforms.
- Make movement camera-relative and handle obstruction.
- Confirm no Dreamcast projection/viewport regression.

**Proof D — First platforming loop**
- Place two new platforms, a spring, five rings, one enemy, a checkpoint and goal via sidecar data.
- Jump, attack, fall, respawn and complete.

**Proof E — Authoring-to-binary pipeline**
- Move the objects in a JSON source file.
- Repack, deploy to both targets and validate identical placement.

**Proof F — Full authored stage**
- Complete all Act 1 segments with optional route and debug camera/collision overlays.
- Fix physics/camera before adding more art.

**Proof G — Release candidate**
- Run all mandatory tests and Dreamcast profiles.
- Remove transitional scaffolding, freeze schema and document reproducible build.

## 13. Milestone go/no-go rubric

| Gate | Go only if |
| --- | --- |
| M0→M1 | Original build and performance baseline documented |
| M1→M2 | New mode isolates race code, stable Player bridge, input and tick determinism |
| M2→M3 | Running/jumping/platforms/walls work in headless and rendered fixtures |
| M3→M4 | Camera tracks elevation, obstruction and speed reliably |
| M4→M5 | Ring/damage/goal/combat complete a miniature level with repeatable recovery |
| M5→M6 | Stage authoring is data-driven, Act 1 has complete route/checkpoint/goal |
| M6→release | Both builds verified, no high-severity defects, Dreamcast resource and frame constraints met on agreed hardware/profile |

If a gate fails, fix the foundational subsystem rather than compensating with scripted stage hacks.

## 14. Required final artifacts

- Source: portable adventure subsystem and platform facade adapters.
- Unit/integration tests: deterministic fixtures, scripted gameplay replays and negative file tests.
- Stage: authored Resort Island Act 1 sources and compiled stage package(s), without unlicensed original data.
- Tooling: stage packer with validate/build/inspect and budget report.
- Docs: gameplay controls, build/run for desktop/Dreamcast, stage authoring/schema, technical architecture, save format and known limitations.
- Evidence: baseline/per-feature profiler reports, memory high-water numbers, playable capture, hardware/emulator provenance.
- Clean APIs: no duplicate game loop, no extra race fields in the new Player state, minimal global mutations.

## 15. Final acceptance checklist (vertical slice)

**Functionality**
- [ ] User enters a distinct action mode through a stable menu flow.
- [ ] No lap/race rank/rubber-banding/opponent race logic runs.
- [ ] Sonic can move in any horizontal direction, stop, reverse, turn at speed.
- [ ] Jumping and landing work from both level and elevated/moving surfaces.
- [ ] Air control, coyote/buffer, rolling and Spin Dash are functional.
- [ ] Camera follows, avoids solid walls and supports authored overrides.
- [ ] Homing Attack selects/attacks eligible enemies and handles obstruction.
- [ ] Rings, monitors, springs, panels and checkpoint/respawn work.
- [ ] Goal completes the stage exactly once and transitions to results.
- [ ] Alternate exploration/speed route exists and is testable.
- [ ] Progress can be saved and reloaded without modifying legacy saves.

**Architecture**
- [ ] Pure gameplay is portable C shared by SDL and KOS targets.
- [ ] No duplicate player physics, camera or entity algorithms between targets.
- [ ] Player's binary-compatible size/offset contract remains intact.
- [ ] Stage content is external data, with documented versioned format.
- [ ] Gameplay sim is fixed-step, with replayable deterministic tests.
- [ ] World/collision/camera math and ownership contracts documented.

**Performance / reliability**
- [ ] Desktop and Dreamcast can complete the same level.
- [ ] Worst-case collision, ring, enemy and camera workloads profiled.
- [ ] Memory/VRAM/stack headroom documented and acceptable on target.
- [ ] No repeated stage reload leaks, double events or infinite triggers.
- [ ] Corrupt stage/save inputs fail gracefully.
- [ ] No unacceptable clipping/tunneling/stuck conditions in the required course.
- [ ] Audio and animation remain synchronized under tested frame load.

**Distribution**
- [ ] Engine-only external original-asset requirements respected.
- [ ] New art/audio provenance recorded.
- [ ] README and platform guides updated for new action controls and stage setup.
- [ ] License/no-sale restriction remains clear.
- [ ] Known limitations and next feature priorities documented.

## 16. Recommended immediate next engineering session

Implement WP-00 and WP-01 with no feature creep:
1. Review latest main and existing build path.
2. Add a no-op adventure module compiled into both targets.
3. Add a clean adventure-mode dispatch at the race loop boundary.
4. Load the existing Resort Island environment, render Sonic and preserve menu return.
5. Instrument calls to lap/rank/AI/legacy movement and prove adventure mode invokes none.
6. Commit this isolated skeleton with basic build and runtime evidence.

After this works, implement WP-02: fixed-tick input and position/coordinate probe. Do **not** begin Homing Attack, extra stage geometry or custom camera effects before the new controller and collision foundation are verified.

---

**End of planning package.** All implementation claims require future code, tests and performance measurements. Nothing in this document should be interpreted as already built.
