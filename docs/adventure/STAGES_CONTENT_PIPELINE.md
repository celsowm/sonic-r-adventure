# Stage Content, Data Format and Authoring Pipeline

**Parent:** [Master plan](../sonic-r-adventure-plan.md)  
**Related:** [Movement/camera/combat](MOVEMENT_CAMERA_COMBAT.md), [Engine architecture](ENGINE_ARCHITECTURE.md)  
**Status:** Proposed format and production process; no stage packer or native action stage exists in the source tree today.

## 1. Stage design principles

Sonic R's visual assets and winding racing routes provide recognizable environments but **do not automatically create a good 3D platformer**. Each action stage needs intentional 3D depth: places to pause and inspect the world, multiple routes, elevation changes, safe and risky traversal, enemies and interactable objects, recovery points, and a definitive destination.

Stage goals:
- Provide a learnable critical path, with optional harder/faster elevated shortcuts.
- Let speed be rewarding without requiring uninterrupted racing.
- Teach each mechanic before combining it with hazards.
- Clearly communicate which geometry can be reached, stood on, broken or attacked.
- Allow skillful routing and shortcuts, not merely one forced cinematic spline.
- Make camera composition and platform spacing part of stage authoring.
- Keep original level visual identity while adding meaningful platforming geometry.
- Avoid wasting Dreamcast resources on hidden, excessive or needlessly detailed geometry.
- Keep authored source independent of runtime binary optimization.

## 2. Existing asset pipeline and constraints

Repository-visible facts:

- **track_geometry.c / LoadTrack3()** reads Sonic R BIN stage geometry, object data, ring spawns and seven position-data sections.
- **track_load.c / LoadTerrain() / ParseTerrainHeader()** reads .TER terrain and spatial data; **LoadAI()** handles opponent path information.
- **track_per_level.c** selects the legacy track BIN path, loads geometry, terrain and AI; it also configures texture pages and lighting.
- **ter_types.h** documents serialized TerVertex (6 bytes), TerFace (14), TerSurface (16), TerCollisionMesh (16), TerLoopEntry (22) and other typed structures.
- The repository is ENGINE ONLY and depends on user-supplied proprietary game data. The plan must not ship original Sonic R commercial assets or Sonic Adventure material.
- Dreamcast has a distinct PVR backend, and the existing Dreamcast guide requires uppercase original file paths on disc.

Implication: first create a sidecar layer that extends **existing loaded Resort Island** data without changing the original track binary. This limits reverse-engineering risk and allows immediate gameplay iteration. A new native stage format can later replace legacy scenery when the loader/packer is mature.

## 3. Proposed stage-package structure

Human-editable source:

~~~text
stages/
  resort_island_act1/
    stage.json
    objects.json
    collisions/
      platforms.glb          # editor source, never parsed at Dreamcast runtime
      collision.json         # material/mask overrides
    cameras/
      zones.json
      rails.json
    routes/
      shortcuts.json
    design/
      route_map.md
      metrics.md
    authored_assets/
      ...                    # rights-clear sources only
~~~

Compiled output (illustrative):

~~~text
build/stages/
  R_ISL_A1/
    STAGE.ADV                 # magic/version/sections and resource references
    COLLISION.ADC             # quantized collision shape data + spatial index
    OBJECTS.ADO               # entity transforms/types/parameters
    CAMERAS.ADCM              # camera volumes and spline data
    GEOMETRY.ADG              # optional NEW scene geometry, packed for runtime
    TEXTURE.ADT               # optional texture atlas/page data
~~~

Filename rules for the final disc image should follow the repository's existing uppercase conventions as necessary. Exact extensions are a proposal. Avoid creating five separate tiny files per object, which would hurt optical-disc seeking; choose a stage pack or coherently grouped resources.

### Legacy stage adapter

For the vertical slice, a stage manifest contains a **base_legacy_track** reference (Resort Island). The game uses existing **InitIsland() / LoadTrack3() / LoadTerrain()** pathways for scenery/ground, then overlays new objects, collision and cameras. It must NOT depend on race AI, start grids, lap sectors or opponent route activation to launch the stage. When the original loader unconditionally reads AI, tolerate it temporarily and remove the coupling in the later native stage loader.

## 4. Editable manifest proposal

~~~json
{
  "schema_version": 1,
  "id": "resort_island_act1",
  "display_name": "Resort Island - Act 1",
  "legacy": { "track": "RESORT_ISLAND" },
  "player": {
    "character": "sonic",
    "spawn": "spawn_beach",
    "allowed_moves": [
      "run", "jump", "roll", "spindash", "homing"
    ]
  },
  "objective": { "type": "reach_goal", "entity": "goal_capsule" },
  "stage_properties": {
    "fall_region": "fall_ocean",
    "checkpoint_policy": "last_activated",
    "time_trial_enabled": false
  },
  "packages": {
    "objects": "objects.json",
    "collisions": "collisions/",
    "cameras": "cameras/zones.json"
  }
}
~~~

This is an AUTHORING SCHEMA SKETCH, not an existing parser. Production schema should be strict, typed, versioned, validated by JSON Schema or equivalent and compiled. Avoid runtime JSON libraries on Dreamcast unless measured appropriate.

## 5. Native binary package contract

### Header

A clear little-endian, versioned format, for example:

~~~text
offset field
0x00   magic = "SRAV"
0x04   major_version (u16)
0x06   minor_version (u16)
0x08   total_size_bytes (u32)
0x0C   directory_offset (u32)
0x10   section_count (u32)
0x14   stage_id_hash (u32)
0x18   checksum/content hash (choice TBD)
0x1C   reserved/flags
...    section directory
...    aligned data payloads
~~~

Proposed section IDs: **META**, **SPAWN**, **COLL**, **ENTITY**, **CAMERA**, **ROUTE**, **TRIGGER**, **GOAL**, **AUDIO**, **LOD**. Final names are negotiable; explicit alignment, sizes, and counts are not.

Loader requirements:
- Validate magic, major/minor version, lengths, counts, overlap and alignment before accessing any section.
- Reject impossible offsets and integer overflows.
- No embedded raw pointers or host struct padding.
- Materialize runtime arrays with explicit endian conversion if required.
- Use 32-bit offsets and stable IDs, never memory addresses.
- Verify checksum where practical to catch corrupted disc content.
- Packer emits a manifest/build report with counts, bytes, bounding boxes, IDs, collision budget and warnings.
- Keep schema migrations explicit: no silently interpreting v1 as v2.
- Host fuzz tests for truncated, wrong-endian, duplicate, cyclic and adversarial files.

### Coordinate quantization

- Source assets may be floating-point meters/scene units.
- Stage packer converts to canonical game units, with rounding policy defined in **adventure_math**.
- Preserve a source-to-game transform in stage metadata (scale, axis swaps, handedness and origin).
- Check maximum coordinate and negative positions before quantization.
- Quantize normals and UVs with measured errors.
- Include authored collision debug view to inspect conversion mismatches.

## 6. Geometry and collision authoring

### Proposed editor source

Use Blender with glTF/GLB as INTERMEDIATE AUTHORING INPUT, processed offline. Blender's object hierarchy and metadata can identify render meshes, collision proxies, static vs moving geometry, object spawn transforms, trigger volumes and camera zones.

Rules:
- No glTF JSON/mesh parsing on the Dreamcast runtime.
- Export position/normal/UV/material and indices in a deterministic order.
- Triangulate consistently before packing.
- Validate normals, duplicates, degenerate triangles and non-manifold collision faces.
- Collision mesh should be simpler than visual mesh.
- Reuse texture pages/atlases whenever possible instead of arbitrary per-object textures.
- Precompute bounding boxes, collision acceleration structure and surface materials.
- Report PVR target vertex/triangle/material and texture usage from packer output.

### Collider types

| Type | Use | Runtime |
| --- | --- | --- |
| Static triangle surface | Terrain and complex ground | Spatial index / triangle support |
| Box / oriented box | Basic platforms, walls, hazards | Cheap pair tests |
| Capsule/sphere | Player and certain enemies | Sweep/overlap |
| One-way platform plane | Jump-through ledges | Direction-aware support |
| Moving collision shape | Elevators and moving platforms | Dynamic transform and swept contact |
| Trigger volume | Checkpoint, goal, camera, events | Non-blocking overlap |
| Guided loop/rail | Fast traversal routes | Parametric path + admission rules |
| Water/death region | Stage hazards | Volume + movement/damage policy |

All authored colliders require collision masks and an entity/material owner. Artist draw geometry must never implicitly become collision geometry without the packer's explicit flag.

### Collision materials

Define named materials such as **normal**, **slippery**, **sticky**, **rolling-friendly**, **conveyor**, **hazard**, **one-way**, **water**, **loop-entry**, **spring-surface**. Use material IDs encoded in the binary, with an authored property table. Clarify priorities when hazard and support masks overlap.

### World scale

Before authoring new geometry, choose a standard Sonic collider height and jump range in measured game units. Platform spacing must derive from real movement test results:
- comfortable standing jump span;
- running jump span;
- spring launch horizontal/vertical range;
- guided loop minimum entry speed;
- homing target-to-target range;
- reasonable recovery ledge after a risky segment.

Do not design an entire level around uncalibrated imported models.

## 7. Entity authoring schema

Objects have stable IDs, type, transform and type-specific properties. Example:

~~~json
{
  "schema_version": 1,
  "entities": [
    {
      "id": "spawn_beach",
      "type": "player_spawn",
      "position": [0, 0, 0],
      "rotation_degrees": [0, 0, 0]
    },
    {
      "id": "spring_beach_01",
      "type": "spring",
      "position": [18, 2, 42],
      "properties": {
        "direction": [0, 0.9, 0.45],
        "strength": 1.0,
        "cooldown_ticks": 8
      }
    },
    {
      "id": "checkpoint_01",
      "type": "checkpoint",
      "position": [74, 10, 112],
      "properties": {
        "radius": 4,
        "respawn_marker": "checkpoint_spawn_01"
      }
    },
    {
      "id": "goal_capsule",
      "type": "goal",
      "position": [190, 14, 260],
      "properties": { "activation": "overlap" }
    }
  ]
}
~~~

**Important:** sample coordinates and strength values are placeholders; actual placements must be chosen after loading and measuring real Resort Island geometry. Never treat the sample as validated in-game coordinates.

### Entity types required for first stage

- player_spawn, respawn_marker
- ring_static, ring_scattered
- monitor
- spring_floor, spring_wall
- dash_panel
- platform_static, platform_moving
- enemy_basic
- hazard, kill_volume
- camera_zone, camera_rail
- checkpoint
- goal
- collectible_hidden (optional)
- guided_loop / rail_entry
- generic trigger

Each type should have:
- canonical ID and schema version;
- necessary render asset references;
- collider shape/mask;
- fixed-step update function if dynamic;
- activation conditions;
- cooldown/respawn policy;
- deterministic event output;
- maximum instances per stage or pool;
- editor debug representation.

## 8. Entity runtime and event handling

Proposed lightweight event system:

~~~text
EVENT_RING_PICKUP
EVENT_MONITOR_BROKEN
EVENT_ENEMY_HIT
EVENT_SPRING_LAUNCH
EVENT_DASH_PANEL
EVENT_CHECKPOINT
EVENT_GOAL_REACHED
EVENT_PLAYER_DAMAGED
EVENT_PLAYER_DIED
EVENT_PLAYER_RESPAWNED
EVENT_CAMERA_ZONE_ENTER
EVENT_CAMERA_ZONE_EXIT
~~~

- Events created during a tick are stored in bounded arrays.
- Explicit handling order resolves simultaneous goal/hazard events.
- Entity update may not recursively dispatch and mutate the world during iteration.
- Single-activation entities keep a consumed flag.
- Stage resets use a typed reset reason: **FULL_RESTART**, **CHECKPOINT_RESPAWN**, **RETURN_FROM_MENU**.
- Avoid pointer-based object references; serialize stable IDs.

## 9. Camera zones and routes

Author:
- axis-aligned or oriented zone volumes;
- priority, blend-in/out ticks;
- target yaw, pitch, distance, vertical focus bias and FOV;
- optional spline knots and reference to loop/rail ID;
- manual orbit policy and lockout behavior.

Test overlap/priority when two zones intersect. Validate rail tangents and non-zero path length. Do not require every camera zone to be named after a specific track; keep stage formats generic.

## 10. Resort Island — Act 1 vertical slice

### Concept (requires actual asset inspection before placing objects)

Repurpose the recognizable Resort Island scenery as a guided tour of 3D movement, then weave new geometry above and alongside existing race routes. The precise order below is a **design target**: confirm the physical feasibility and actual landmarks against the legally supplied original data and the rendered collision maps.

**Segment A — Beach arrival / movement tutorial**
- Spawn in a safe, open location with unobstructed camera view.
- Ring trail makes forward motion clear but does not force a direction.
- Small corner/obstacle teaches turning, braking and jumping.
- Optional shallow side route rewards exploration with rings.

**Segment B — First elevation**
- One clearly visible spring launches Sonic to a new platform.
- Build a broad landing surface, then a smaller optional target.
- Demonstrate vertical camera tracking and basic air control.
- Provide safe falling recovery if the first jump misses.

**Segment C — Combat playground**
- A basic enemy guards a corridor or platform.
- A second enemy can be reached by Homing Attack bounce.
- Include an alternative walking route to avoid mandatory expert combo before mechanics are taught.
- Add monitor as reward for exploration.
- Check homing target occlusion where a wall separates two areas.

**Segment D — Speed and slope**
- Downhill route enables higher top speed and rolling.
- Provide one branch with carefully spaced platforms.
- Include a dash panel and a visible safe landing.
- Demonstrate that Sonic can slow down and turn around after the setpiece.

**Segment E — Loop or elevated shortcut**
- Use an existing recognizable loop/curved route if geometry and collision allow it.
- First iteration: guided entry/exit with momentum preservation.
- Add a slower alternate route with spring/platform sequence.
- Make camera blend conspicuous but not disorienting.

**Segment F — Checkpoint**
- Place checkpoint after player has learned all core mechanics.
- Attach a respawn marker with stable floor and safe camera location.
- Add a visible branching path: easy main path versus optional quicker elevated path.

**Segment G — Final platforming chain**
- Combine one moving platform, spring and enemy into a short traversal challenge.
- Keep fail/retry duration reasonable.
- Enforce no unavoidable camera obstruction, blind jump or fatal collision seam.

**Segment H — Goal capsule and results**
- Present clearly visible end goal with short celebratory transition.
- Trigger exactly once; show ring/time optional details.
- Persist completion and return to stage selection.

### Optional shortcuts

- High ledges accessible by accurately controlled high-speed jump.
- A ring trail leading toward a risky elevated platform with a meaningful reward.
- Faster guided loop entrance for players who maintain momentum.
- A hidden item tucked behind a low-speed exploration detour.

A shortcut should not depend on collision glitches or clipping through geometry.

### Playtesting checkpoints

Test with three archetypes:
- New player who explores slowly and misses jumps.
- Competent platformer who clears with few retries.
- Speedrunner who carries high speed and intentionally takes shortcuts.

The stage must remain understandable and completable by the first group, satisfying for the second, and skill expressive for the third.

### Metrics to collect

- Time to first jump, first enemy hit and first checkpoint.
- Death and respawn locations; repeated deaths at same obstacle.
- Camera manual corrections and obstruction events.
- Collision stuck/tunneling count.
- Route selection percentages and completion times.
- Average/p95 per-frame cost in expensive stage segments.
- Peak object count, active physics contacts and ring effects.
- Memory peaks on load, on restart and on goal transition.

Do not commit to specific target times before a real playtest.

## 11. Stage-building process

**Step 1: Instrument the legacy scenery.** Export or debug-dump track bounds, terrain triangle normals, loop segments, ring spawns, sky/texture page data and camera paths. Render a collision overlay. Verify no legally restricted assets are committed.

**Step 2: Establish playable scale.** Implement a test map with known platform distances; measure Sonic's jump and sprint envelope.

**Step 3: Block out action routes.** Use proxy platforms and colorless collision material, build safe/optional paths and respawn points. Validate with desktop and Dreamcast early.

**Step 4: Author interactions.** Place rings, enemies, springs, panels, cameras, triggers and goal by data, not C code.

**Step 5: Add scenery/visual polish.** Keep original recognizable environment and add new original objects where appropriate; optimize texture/material reuse.

**Step 6: Compile and validate.** Packer generates runtime package, warnings and reproducible hash. Refuse invalid references, malformed paths and over-budget resources.

**Step 7: Profile on Dreamcast.** Test worst-case visible triangle/material counts, water/alpha effects, entities and camera close-ups.

**Step 8: Ship fixture.** Archive the exact stage package, input replay and instrumentation logs used in acceptance testing.

## 12. Authoring tool / packer roadmap

~~~text
Blender + JSON authoring
    |
    +-- Source validator (IDs, transforms, sizes, bounds, source rights)
    +-- Geometry processor (triangulation, normal repair, tpage assignment)
    +-- Collision compiler (quantization, surfaces, broadphase grid/BVH)
    +-- Entity compiler (typed properties and stable IDs)
    +-- Camera/path compiler (zone and spline lookup)
    +-- Budget analyzer (RAM/VRAM, object counts, triangles, texture pages)
    |
    +-- STAGE PACK in binary format
          +-- Desktop loader
          +-- Dreamcast loader
~~~

A simple command-line tool is preferable to a GUI at first:
- **validate** checks source and prints diagnostics.
- **build** emits deterministic binary package.
- **inspect** prints section summary, bounding boxes and dependencies.
- **dump** disassembles a compiled package into meaningful metadata for debugging.
- **compare** optionally verifies deterministic repacking and diff summaries.

Choose tool language by team preference; the packer does not need to run on Dreamcast and may be Python/Rust/C. Prefer stable, well-tested file I/O and avoid introducing an engine dependency into runtime.

### Deterministic packing

- Stable ordering by authored ID/type and explicit priorities.
- Stable float quantization and endian output.
- No timestamps or absolute filesystem paths in runtime binary payloads.
- A clean rebuild of identical source should yield a byte-for-byte identical binary.
- Packer reports every resource reference and cannot silently fall back to a different proprietary asset.

## 13. Performance-aware level design

Dreamcast budget actions:
- Prefer solid opaque geometry and texture atlases when visual quality permits.
- Avoid many tiny meshes/material changes in the camera's main field of view.
- Keep collision proxy meshes dramatically simpler than render meshes.
- Precompute static broadphase and object spatial buckets offline.
- Cap active enemies and ring-scatter particles.
- Keep authored camera distances within known visibility budget.
- Reuse small effects and animation data across stage segments.
- Profile near water, overlapping translucency and large vistas before adding detail.
- Prefer coarse LOD and distance culling if measurements show overdraw/CPU draw overhead.

Do **not** invent a fixed triangle count, texture budget or FPS promise before profiling the current engine on this exact Dreamcast build. See [IMPLEMENTATION_AND_VALIDATION.md](IMPLEMENTATION_AND_VALIDATION.md) for measurement protocol.

## 14. Compatibility and provenance

### Legacy original data

Continue to require the user to provide their own Sonic R data as documented in root README and desktop/Dreamcast guides. The new authored sidecar may reference those existing files but must not contain copies of their proprietary geometry/textures unless rights have been established.

### New authored data

- Record original creators/attribution/license for each asset.
- Do not copy Sonic Adventure assets, stage geometry or music into the public repo without permission.
- Ensure open-source third-party content licenses fit the repository's unusual no-sale license and distribution plan.
- Avoid shipping a prebuilt disc/image with unlicensed original game data.

## 15. Stage acceptance matrix

- Stage loads from authored data with deterministic binary output.
- All object IDs unique and all references resolve.
- Every mandatory platform's collider matches visible geometry within documented tolerance.
- No inaccessible goal, unreachable checkpoint or automatic infinite trigger loop.
- Fast route, slow route and recovery route all playable.
- Collision layer handles both upper bridge and lower passage.
- A moving platform can carry a player and survive interruption/collision.
- All camera zones transition with no hard snap unless intentionally authored.
- Rings do not duplicate across checkpoint/respawn resets.
- Run-to-goal can be replayed through fixed input fixture with stable state.
- Stage unload/reload restores correct entities and no stale pointers.
- Dreamcast file naming, endian and resource limits are validated by packer.
- Original track asset ownership and legal status remain clear.

**Do not scale to Radical City, Regal Ruin, Reactive Factory or Radiant Emerald until Resort Island Act 1 passes this gate.**
