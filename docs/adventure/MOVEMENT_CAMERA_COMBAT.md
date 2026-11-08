# Movement, Camera, Collision and Combat Design Specification

**Parent:** [Master plan](../sonic-r-adventure-plan.md)  
**Companion:** [Engine architecture](ENGINE_ARCHITECTURE.md)  
**Target experience:** precise exploratory platforming at low speed, expressive momentum and cinematic traversal at high speed.  
**Status:** Design proposal. Numeric examples are tuning STARTING POINTS only, not reverse-engineered values from Sonic Adventure or measurements from Sonic R.

## 1. Movement philosophy

Sonic Adventure's action stages are more than racing environments. The player must be able to turn, stop, inspect a branching route, jump to small platforms, approach enemies, and launch into high-speed spectacle. Sonic R's original acceleration-button steering cannot simply have its button forced permanently on: low-speed control, facing, animation, braking, jumping and air movement require new state transitions.

Design priorities, in order:

1. **Intent and readability:** player direction and target are visually clear at low speed.
2. **Responsiveness:** commands should take effect promptly and predictably.
3. **Momentum:** high speed is earned and carried through slopes, rolls and flowing arcs.
4. **Forgiveness:** jump buffering, short coyote windows, reliable ledges/checkpoints.
5. **Consistency:** move physics is independent from rendering and frame-time variation.
6. **Spectacle:** authored loops, dash sections and camera zones enrich traversal without making the entire stage a rail shooter.

## 2. Input mapping

### Dreamcast default proposal

| Input | Action | Notes |
| --- | --- | --- |
| Analog stick | Move in 2D ground plane | Full 360-degree direction, analog magnitude |
| A | Jump; air-time Homing Attack when eligible | Context-sensitive with clear state priority |
| B | Roll / Spin Dash (hold to charge on ground) | No acceleration button |
| X | Contextual action/interact | Optional for first slice; avoid overcomplication |
| Y | Recenter camera / optional look toggle | Need responsive manual camera behavior |
| L / R | Rotate camera | Offer alternative for players who prefer explicit control |
| D-pad | Digital movement | Accessible fallback |
| Start | Pause | Freeze game simulation |

Exact mapping should be exposed in config, not hardcoded into gameplay. Dreamcast has one main analog stick; do not assume a right stick. On desktop, use common controller conventions with right-stick camera control where present. Keyboard uses WASD/arrows for movement and keys for camera orbit.

### Normalized input

- Store movement as signed normalized **move_x**, **move_y** with dead zone and magnitude preserving diagonals.
- Implement **raw → filtered → sampled** separation.
- Use radial stick dead zone rather than independent X/Y clipping to avoid preferred diagonal directions.
- Define button **pressed**, **held** and **released** independently; use *pressed* for discrete actions.
- Buffer jump for a small number of simulation ticks if the player presses just before touching a platform.
- Preserve a short coyote window after leaving a ledge; both should be configurable and playtested.
- Respect paused/respawn/goal states by rejecting actions in one central gate, not in each entity.
- Ensure gameplay cannot accidentally process old race countdown/ghost masks.

## 3. State machine

~~~text
                            +--------------------+
                            | IDLE / RUN / WALK  |
                            +----------+---------+
                                       |
              +------------------------+------------------------+
              |                        |                        |
              v                        v                        v
          ROLLING                    JUMP               SPINDASH_CHARGE
              |                        |                        |
          RELEASE/STOP                  v                        v
              |                    JUMP_ASCEND                RELEASE
              |                        |                        |
              |                    JUMP_FALL                   ROLL
              |                        |
              |          +-------------+--------------------+
              |          |             |                    |
              |          v             v                    v
              |       HOMING        BOUNCE                LANDING
              |          |             |                    |
              +----------+-------------+--------------------+
                                       |
                                       v
                                 IDLE / RUN
           
Any valid living state -> HURT -> RECOVER or DEAD -> RESPAWN
Any valid state -> SCRIPTED / GOAL (explicit authored transitions)
~~~

Implement **state + contact flags + modifiers**, not a combinatorial enumeration for every possible situation. For example, *grounded* is an observable contact result; *INVINCIBLE* is a temporary modifier; *HOMING* is an action state.

### Transition priority

Order events deliberately:

1. Fatal state / stage completion / scripted lockout
2. Damage and knockback
3. Accepted jump or attack action
4. Contact/landed/platform event
5. Roll/spin dash
6. Normal locomotion
7. Animation selection

Avoid a landing event instantly cancelling a valid attack or re-triggering a jump from a held button.

### Proposed per-state properties

| State | Input | Gravity | Rotation | Exit |
| --- | --- | --- | --- | --- |
| Idle / walk | Full analog | Supported by ground | Camera-relative | Input, jump, roll |
| Run | Analog steering with inertia | Supported by ground | Speed dependent | Jump, roll, stop |
| Roll | Reduced air-independent steering | Ground slopes/drag | Velocity-aligned | Speed below threshold, jump |
| Spin Dash charge | Hold action, mostly stationary | Ground | Locked/limited | Release or interruption |
| Jump ascend | Air control | On | Partly facing input | Apex/contact/attack |
| Fall | Air control | On | Input smoothing | Ground/homing/hurt |
| Homing | Limited steering | Special guided acceleration | Target-directed | Impact, timeout, no target |
| Bounce | Air control after impulse | On | Player-directed | Fall/ground |
| Hurt | Input limited | On | Knockback-oriented | Timer/ground/death |
| Dead | None | Optional effect only | Fixed | Respawn |
| Scripted/rail | Stage-defined | Stage-defined | Stage-defined | Script completes / breaks |

## 4. Locomotion model

A usable simplified ground motion model:

~~~text
desired = normalize(camera_forward_flat * input_y +
                    camera_right_flat   * input_x) * input_magnitude

v_tangent = velocity projected onto ground tangent plane

target = desired * target_speed
turn_strength = f(current_speed, desired_angle, grounded, material)
v_tangent = approach_or_rotate(v_tangent, target, accel, turn_strength)

v_tangent += project_gravity_along_slope(normal) * slope_factor
v_tangent *= (1 - surface_drag)

new_position = sweep_and_resolve(old_position, v_tangent, dt)
~~~

**This is pseudocode**, not an instruction to use float directly in Dreamcast runtime. Implement with selected fixed-point math, stable rounding rules and dt represented by a fixed tick.

### Low-speed steering

At low speed, face and accelerate toward camera-relative input direction. Reversing direction must be possible without describing a full large racing turn radius. Support small ledge corrections without suddenly launching sideways.

### Mid-speed steering

Blend desired direction into momentum over several ticks. Offer tighter turns when moving slowly, wider turns when sprinting. The turning response should be controlled by tunable curves, not one global multiplier.

### High-speed steering

Preserve significant tangential velocity and gradually rotate it with input; avoid instantaneous 180-degree flips. When rapidly reversing direction, apply a braking/turnaround phase rather than snapping velocity. Optional speed-based camera zoom/lead improves readability.

### Friction and drag

Separate *active braking*, *passive friction*, *rolling resistance*, *air drag* and *surface material*. A single racing-era global "friction" value may have too many unrelated effects. Material categories: normal, slippery, sticky, conveyor, steep slope, water/hazard, spring and rail.

### World slope projection

Use contact normal N to project desired acceleration onto the surface tangent:

~~~text
tangent(a, N) = a - N * dot(a, N)
~~~

Ensure contact normals are normalized in the chosen fixed scale. Downhill gravity should accelerate rolling Sonic; uphill should reduce speed. Include a minimum speed for sticky/adhesive loop behavior only in controlled surface modes.

### Speed metrics

Report speeds as units/second in tooling, but internally operate in canonical fixed units/tick as documented. Avoid magic constants transplanted from **g_charStatsTable** without unit conversion. Gameplay tuning sheet should define:
- Ground max run speed
- Low-speed acceleration
- Turn responsiveness curve
- Braking deceleration
- Passive friction / roll drag
- Slope gravity multiplier
- Roll activation threshold
- Jump initial speed and hold curve
- Terminal velocity
- Air horizontal acceleration
- Homing range, speed and time window
- Knockback power and invulnerability duration
- Landing speed retention

## 5. Jumping, air control and landing

### Jump behavior

- Jump from stable support, including a short coyote period immediately after departing a ledge.
- Buffered jump activates on landing if within the configured input window.
- Jump impulse should be computed along world up or an authored local normal, depending on surface mode.
- Variable jump height: release jump early to reduce ascent, but no unnatural instantaneous stop.
- Air control changes velocity gradually and is weaker than ground control at high horizontal speed.
- Landing maintains sensible tangential momentum but may reduce impact speed depending on angle.
- Jump actions emit distinct animation/particle/sound events.

### Ground detection

Do not simply compare current Y against one terrain height. Need:
- a short downward support cast/sweep at current position;
- a collision normal acceptance threshold for walkability;
- upward platform motion detection;
- separate one-way platform policy (land from above; pass from below if authored);
- support for vertically stacked floors;
- precise, bounded edge and ledge behavior;
- landing where a high-speed sweep crosses a narrow target platform.

### Landing orientation

Sonic should not face the camera because the camera rotated while airborne. Prefer maintaining facing from move/velocity input and resolving on landing. Keep movement heading, rendered facing and camera yaw separate.

### Fall and recovery

Stage metadata defines out-of-bounds volumes/fall planes. Avoid teleporting Sonic merely for leaving a race-track corridor. A fall triggers controlled animation/SFX then checkpoint respawn, not an opponent AI recovery routine. Prevent immediate re-death at respawn by validating clearance.

## 6. Roll and Spin Dash

### Roll

Grounded B press while moving initiates rolling, compresses collider if needed (subject to explicit collision safety), lowers active steering and shifts energy transfer to slope physics. A high-speed downhill roll gains momentum; uphill slows. Rolling may damage enemies by contact if attack collider/threshold conditions hold.

### Spin Dash

Hold B while grounded at low enough speed to begin charge. Charge builds over a tuneable window; optional repeated presses can enhance charge only if the control scheme remains simple. Release B launches along the currently chosen heading, preserving a consistent direction. End charge if hurt, displaced, stage finished or if no valid support.

**MVP policy:** prioritize a predictable and responsive basic charge/release. Fancy particle effects or additional sub-states come after controls are stable.

## 7. Loops, ramps and high-speed authored sequences

Sonic R already has **TerLoopEntry**, **LoopSurfaceCheck**, **UpdateLoopMovement** and **ApplyDragAndSteering**, but those routines were authored for race tracks and may forcibly constrain player motion. Treat their data as a reusable source and validate behavior rather than assuming they compose with the new controller.

### Options

1. **Physical loop surfaces:** implement a surface-normal, contact-based traversal with speed threshold and adhesion. High implementation risk but broadly reusable.
2. **Guided loop segments:** authored spline/rail activation only when the player enters at adequate speed. More predictable, cinematic and inexpensive.
3. **Hybrid (recommended for MVP):** normal physical movement to the loop trigger, guided geometry/path during the portion where the old engine's surface constraints are known, seamless exit preserving momentum and player control.

Each guided segment must specify:
- entrance region and minimum speed;
- path reference frame and tangent orientation;
- parametric progress and speed transfer;
- exit velocity, exit direction, vertical impulse and camera zone;
- interruption/failure cases (too slow, damage, leaving path);
- deterministic behavior and no stuck rail state.

Ramps must preserve launch momentum and not silently cancel the player's jump.

## 8. Collision algorithm requirements

### Player shape

Start with a simple capsule (or sphere-plus-feet where legacy geometry makes capsule difficult); define radius, standing height, rolling height and step height. Camera/render meshes are NOT the gameplay collider. Validate contact shape in debug overlay.

### Robust motion

Use substeps or continuous sweeps for speed envelopes where discrete integration tunnels through thin geometry. Resolve contacts iteratively with a bounded iteration cap. Project remaining velocity along contact tangents, with friction and bounce depending on material. Stop penetration via small stable separation epsilon; avoid pushing Sonic through ceilings or nearby walls.

### Collision queries

| Query | Why |
| --- | --- |
| find_support / ground probe | Stable foot contact on slopes/stacked floors |
| sweep_player | Fast movement against walls and thin platforms |
| overlap_player | Check spawn, ring, enemy, hazard, trigger intersections |
| raycast_camera | Move camera in front of obstructions |
| target_line_of_sight | Validate homing target accessibility |
| material_at_contact | Apply slide, roll, hazard or sticky-loop behavior |

### Critical hard cases

- Landing on a platform above a tunnel; never snap to the lower surface.
- Standing beneath a low ceiling; jump does not clip through it.
- Walking off a ledge: short coyote grace but no unlimited midair jump.
- Platform moving upward into a ceiling: controlled push or safe rejection.
- Rolling at maximum speed against a sharp corner.
- Near-vertical wall, steep slope and loop: correct support classification.
- Concave terrain with multiple candidate faces in one grid cell.
- At a collision-grid border: no unexpected velocity-zero/clamp inherited from racing bounds.
- High-speed spring into wall: apply safe collision and remain recoverable.
- Contact order stable when two triangles have the same impact time.

## 9. Moving platform policy

Platforms have authored motion (e.g. endpoints, closed spline, sine/easing, speed) and separate blocking/trigger geometry.

Recommended order:
1. Evaluate platform transforms at target simulation tick.
2. Compute each player's support displacement.
3. Apply support carry before the player motion sweep.
4. Resolve player movement/collisions against swept/updated platform colliders.
5. Re-evaluate whether player remains supported.
6. Emit landed/left events.

Use local support anchor for rotation and avoid accumulating drift. Distinguish a solid support from a moving hazard or conveyor. The player should not fall through a platform when the simulation frame rate drops because fixed-step sweep is authoritative.

## 10. Camera system

The stock **BuildChaseCamera(Player*, CamStateEntry*)** follows behind the racer, uses a fixed character-dependent distance and samples terrain Y. That is useful as a reference, but not suitable for vertical level design.

### Camera modes

- **FOLLOW:** default; follows player horizontal motion, velocity lead and vertical movement.
- **ORBIT:** manual yaw input, auto-recenter after adjustable delay.
- **LOCKED_REGION:** authored stage zones set yaw/pitch/distance and optionally constrain player framing.
- **RAIL:** action setpieces/loops where camera follows a prepared spline while Sonic stays controllable.
- **LOOK_AT_EVENT:** brief focus on major object, goal or enemy, with safe cancellation.
- **RESPAWN/GOAL:** predictable transition poses, not abrupt unbounded camera motion.

### Camera update model

~~~text
focus_point = player.pos + height_offset + velocity_lead
desired_yaw = blend(player_heading, user_orbit, region_override)
desired_pitch = region_pitch or default_pitch
desired_distance = base_distance + speed_zoom + context_adjustment

desired_camera = orbit(focus_point, yaw, pitch, distance)
candidate_camera = collision_adjust(focus_point, desired_camera)
final_camera = smooth_critically_damped(previous_camera, candidate_camera)
view = look_at(final_camera, focus_point)
~~~

Smoothing must be time-constant based on fixed ticks, never vary wildly with frame rendering. Fast turns should not let Sonic disappear outside view; lock camera transitions must not flip yaw unexpectedly.

### Camera obstruction

Raycast or sphere-cast from player focus to desired camera; pull camera forward to the nearest safe position if a wall intercepts it. Add a small margin and hysteresis so it does not oscillate between front/behind a wall. When near a ceiling, lift or tighten distance sensibly without allowing camera to pass through solids.

### Vertical platforming

Track player elevation with a smooth offset that reveals landing targets; allow quick upward framing during jumps and controlled downward anticipation when falling. Avoid a pure ground-height clamp at camera XZ: stacked floors and ledges require obstacle-aware positioning.

### Cinematic zones

Author camera zones as explicit volumes with priorities, blend times and optional path targets. Highest-priority active zone wins with stable ties by zone ID. Manual rotation should remain available unless the specific zone is clearly scripted and safe. On leaving a zone, blend back to follow rather than snapping.

### Camera debug overlay

Show player focus, desired camera, adjusted camera, obstruction ray, active zone name, FOV, yaw/pitch and smoothing timing. Support fixed-camera test toggles and controlled input capture.

## 11. Homing Attack specification

The Homing Attack is central to Adventure-style combat and must be deterministic and reliable.

### Eligibility

- Sonic is airborne in a legal action state.
- Action button edge occurs and attack cooldown/active-chain rules allow it.
- At least one target is alive, attackable and within configured range/cone.
- Target is not obscured by solid collision geometry, unless explicitly permitted by design.
- Target has stable world position and a valid collision/impact volume.

### Candidate selection

1. Gather attackable entities from spatial query, bounded by configured max results.
2. Reject dead, disabled, out-of-range or occluded entities.
3. Filter field of view using player's intended facing/camera combination.
4. Score distance and angular alignment. Proposed scoring: priority to forward-facing targets then shorter distance, with explicit cone restriction.
5. Break ties with stable entity ID, never pointer order or container iteration accidents.
6. Record chosen target ID for debug and deterministic replay.

No "magnet teleport": Sonic must traverse a real, bounded trajectory; attack either hits the intended target or safely expires.

### Attack motion

- Use a short windup or direct launch (tune based on responsiveness).
- Compute desired target direction from current player location.
- Accelerate toward target with capped speed and turn rate.
- Sweep against world solids during motion; solid obstruction cancels/deflects appropriately.
- Verify impact using attack volume against target collision, not only center distance.
- On hit: damage target, emit particles/SFX, bounce Sonic upward/back along controlled trajectory, allow an intentional next attack window.
- On expiry, lost target or invalid contact: transition into ordinary airborne state with plausible velocity.

### Edge cases

- Two equal-score enemies: stable selection.
- Target destroyed by another event during flight: safe fallback.
- Player is already overlapping target when attack starts: no NaN/zero normalization.
- Enemy behind thin wall: never snap through.
- Target above/below player: valid 3D trajectory if within cone.
- Moving enemy changes position: pursue within bounded turning, not infinite lock-on.
- Jump and attack share the same input: first press on ground jumps, subsequent valid airborne press attacks; air control must not accidentally trigger jump again.
- Fast stage object moves across line of sight: handle occlusion consistently.

## 12. Other interaction types

### Rings

- Stable ring spawn IDs and pickup radii.
- Controlled ring attraction as optional authored ability, not default.
- Visible ring count and pickup effect.
- On damage with rings, scatter a bounded pool of recoverable rings with lifetime, no infinite duplication.
- On damage without rings, enter death state.
- Persistent collectibles are separate from respawnable stage rings.

### Item monitors

- Distinct idle/activated/broken states and one-shot activation.
- Use collision/attack events rather than frame-by-frame health polling.
- Examples: ring bonus, speed, temporary invulnerability, shield.
- Respawn policy controlled by object metadata.

### Springs

- Spring collider with direction and impulse.
- Floor, wall and diagonal variants.
- Enforce cooldown to avoid repeated firing on adjacent ticks.
- Hand off to ordinary air physics except during explicit short launch lockout.
- Camera may switch to a short authored zone to reveal the next platform.

### Dash panels

- Ground speed impulse or short velocity clamp along authored direction.
- Preserve steering after a brief predictable period.
- Never forcibly steer Sonic into hazards unless visible or designed.

### Checkpoints

- Trigger once on entering volume or reaching authored activation condition.
- Capture stage/position/facing, ring restore policy and current entity reset scope.
- On respawn, restore stable support and pre-validate camera position.
- Reset or retain enemies and rings according to explicit metadata, not global assumptions.

### Goal

- Goal volume triggers an event only if optional prerequisites are satisfied.
- Gate input and switch to a safe goal animation/state.
- Record result once; do not repeatedly save on every frame of overlap.
- Transition to result UI after animation/transition completes.

### Enemy lifecycle

Minimum enemy has **Idle → Patrol → Alert/Attack → Hurt/Defeated → Despawn/Reset** with simple path/waypoint behavior. Attack collision and body collision are distinct. Do not reuse five-racer AI for enemy combat.

### Damage

One authoritative function processes damage: **AdventurePlayer_ApplyDamage(type, source, impulse)**. It checks invincibility, protective effects, attack state, ring count and death, then queues state/event transitions. No enemy or hazard should directly zero rings or forcibly overwrite player velocity.

## 13. Animation contract

Existing character models/animations are reused where appropriate, but new action states need explicit mapping. Start with:
- Idle, walk, jog, sprint
- Start/turn/brake
- Jump/ball/roll
- Spin Dash charge/release
- Homing Attack
- Landing
- Hit/knockback
- Death
- Goal/celebration

Audit which animations already exist in **anim_data.c**, **animation.c** and asset sheets; do not promise unavailable clips. Implement a fallback state table with safe animation IDs and optional procedural spin/pose, then add custom assets only if permitted and supported.

Blend/transition behavior:
- Avoid resetting animation time every tick.
- Separate locomotion speed from animation playback rate.
- Treat animation-driven sound/contact cues as events.
- Never let animation root motion move authoritative physics without an explicit design exception.
- If a character lacks a requested clip, fallback gracefully.

## 14. Tuning strategy

Do not start with arbitrary copied numbers from a different Sonic game or engine. Tune in the following order:
1. Fixed timestep and canonical world-unit scale.
2. Height of a standard platform and effective character collider.
3. Walk acceleration and stop distance.
4. Running speed curve and high-speed turn radius.
5. Jump apex height, duration, air influence and landing retention.
6. Roll drag, downhill gain and Spin Dash release.
7. Camera response/obstruction handling.
8. Homing range/cone and impact/bounce trajectory.
9. Springs, panels and authored loop thresholds.
10. Ring loss/recovery and enemy difficulty.

Create a reproducible tuning scene with quantitative markers and record outputs, rather than tweaking values only inside a full stage.

### Example tuning benchmarks (NOT fixed gameplay values)

- **Walk-turn:** player can reverse from near-stationary in a small visible area without circling.
- **Sprint-turn:** the same reversal takes longer, preserves motion continuity and still permits recovery.
- **Jump consistency:** jump arcs from identical inputs hit matching landing bounds across runs.
- **Small platform:** a skilled user can land from a standing jump without fighting the camera.
- **Homing chain:** three enemies in sequence can be targeted intentionally with no accidental wall penetration.
- **Spring transfer:** post-spring camera keeps the next landing platform legible.
- **Loop entry:** Sonic either enters successfully at valid speed or gets an obvious failed-entry outcome.

## 15. Gameplay test matrix

| ID | Test | Pass condition |
| --- | --- | --- |
| MOV-001 | No input, level ground | No uncommanded movement or drift |
| MOV-002 | Cardinal vs diagonal stick | Equivalent normalized speed envelope |
| MOV-003 | Slow 180-degree reversal | Responsive and stable |
| MOV-004 | Fast 180-degree reversal | Smooth deceleration/turn, no instant velocity flip |
| MOV-005 | Jump edge then early release | Predictably lower arc than held jump |
| MOV-006 | Buffered jump before landing | Fires once if within buffer window |
| MOV-007 | Coyote jump | Accepted only within grace window |
| MOV-008 | Jump beneath ceiling | No penetration/teleport |
| MOV-009 | Land on stacked upper floor | Selects proper support surface |
| MOV-010 | High-speed thin obstacle | No tunneling within defined speed envelope |
| MOV-011 | Steep slope and edge | Does not invent walkable ground |
| MOV-012 | Rolling downhill | Gains speed from consistent gravity projection |
| MOV-013 | Spin Dash cancel by hit | Correct cleanup, no stuck charge |
| MOV-014 | Moving platform/ceiling | Player carried, pushed or safely interrupted |
| CAM-001 | Camera behind wall | Camera adjusts and keeps player visible |
| CAM-002 | Tall jump / fall | Focus anticipates correct vertical targets |
| CAM-003 | Camera authored zone enter/exit | Smooth transition with stable priority |
| CAM-004 | Fast 180-degree player turn | No excessive oscillation or flip |
| ATK-001 | Two similar eligible targets | Stable deterministic choice |
| ATK-002 | Target behind wall | Not selectable / safe cancellation |
| ATK-003 | Target destroyed midair | Safe return to falling |
| ATK-004 | Homing chain | No teleport or duplicate damage |
| DMG-001 | Hit with rings | Ring scattering + temporary invulnerability |
| DMG-002 | Hit with zero rings | Death and checkpoint respawn |
| OBJ-001 | Spring launch | Exactly one launch per activation |
| OBJ-002 | Goal overlap | Stage completion event emitted exactly once |
| OBJ-003 | Checkpoint + restart | Correct spawn and entity reset behavior |

These tests should use both micro-fixtures and actual Resort Island scene probes. Record simulation state as well as video.

## 16. Future character extension points (not MVP)

- **Tails:** flight duration/energy, hovering, air acceleration and unique animation; reuse common collision/camera.
- **Knuckles:** glide, dive, climbable surfaces and ledge transitions; needs separate wall-climb contact contract.
- **Amy:** distinct attack/recovery timing and slower but precise platforming.
- **Other characters:** only after the shared action API survives at least one second character without proliferating exceptions.

Do not pre-implement these systems until Sonic's vertical slice passes its tests.

## 17. Exact movement feature completion criteria

A controller is feature-complete for the first slice when an input replay can traverse a test course with idle-to-run, high-speed turns, jump gaps, air control, low ceilings, upper/lower platforms, slope roll, Spin Dash release, ramp, spring, one guided loop, homing target chain, ring damage, checkpoint and goal. No stale race AI/catch-up/lap code participates; controls produce the same state trajectory given the same fixed input sequence and build. Camera remains usable on all mandatory obstacles.
