/*
 * 60Hz Q20.12 motion. All gameplay integration uses integer arithmetic.
 * WP-04 support query handles terrain ramps, edges, stacked floors and jumping.
 * Static walls and upward collisions with elevated terrain are supported;
 * arbitrary mesh ceilings and special loop surfaces are not implemented.
 */
#include "adventure_player.h"
#include "adventure_vertical.h"
#include <stdint.h>
#define ADV_MAX_SPEED_Q12 (3 * ADV_FP_ONE)
#define ADV_ACCEL_Q12 900
#define ADV_BRAKE_Q12 1250
#define ADV_FACE_THRESHOLD 160
#define ADV_STEP_UP (2 * ADV_FP_ONE)
#define ADV_GROUND_SNAP (3 * ADV_FP_ONE)
#define ADV_JUMP_IMPULSE (7 * ADV_FP_ONE)
#define ADV_GRAVITY (ADV_FP_ONE / 2)
#define ADV_TERMINAL (14 * ADV_FP_ONE)
#define ADV_COYOTE_TICKS 5
#define ADV_JUMP_BUFFER_TICKS 6
#define ADV_CAPSULE_RADIUS (ADV_FP_ONE)
#define ADV_CAPSULE_HEIGHT (4 * ADV_FP_ONE)

void AdventurePlayer_Init(AdventurePlayer *p, AdventureVec3 pos, uint16_t facing)
{
    if (!p) return;
    p->pos = pos;
    p->previous_pos = pos;
    p->velocity.x = p->velocity.y = p->velocity.z = 0;
    p->facing = facing & 4095u;
    p->motion = ADV_MOTION_IDLE;
    p->surface_id = -1;
    p->surface_layer = 0;
    p->grounded = 0;
    p->coyote_ticks = 0;
    p->buffered_jump = 0;
}
static void AdventurePlayer_Horizontal(AdventurePlayer *p, AdventureInput input,
                                        AdventureBasis basis)
{
    int32_t dx = AdventureMath_ClampI32(
        ((int64_t)basis.forward_x * input.move_y +
         (int64_t)basis.right_x * input.move_x) / ADV_FP_ONE);
    int32_t dz = AdventureMath_ClampI32(
        ((int64_t)basis.forward_z * input.move_y +
         (int64_t)basis.right_z * input.move_x) / ADV_FP_ONE);
    int32_t target_x = (int32_t)(((int64_t)dx * ADV_MAX_SPEED_Q12) / ADV_FP_ONE);
    int32_t target_z = (int32_t)(((int64_t)dz * ADV_MAX_SPEED_Q12) / ADV_FP_ONE);
    int32_t accel = (input.move_x || input.move_y) ? ADV_ACCEL_Q12 : ADV_BRAKE_Q12;
    p->velocity.x = AdventureMath_Approach(p->velocity.x, target_x, accel);
    p->velocity.z = AdventureMath_Approach(p->velocity.z, target_z, accel);
    int64_t vx = p->velocity.x, vz = p->velocity.z;
    int64_t speed_sq = vx * vx + vz * vz;
    int64_t cap_sq = (int64_t)ADV_MAX_SPEED_Q12 * ADV_MAX_SPEED_Q12;
    if (speed_sq > cap_sq) {
        uint32_t speed = AdventureMath_IsqrtU64((uint64_t)speed_sq);
        if ((uint64_t)speed * speed < (uint64_t)speed_sq) ++speed;
        p->velocity.x = (int32_t)(vx * ADV_MAX_SPEED_Q12 / speed);
        p->velocity.z = (int32_t)(vz * ADV_MAX_SPEED_Q12 / speed);
    }
    if (p->velocity.x > ADV_FACE_THRESHOLD || p->velocity.x < -ADV_FACE_THRESHOLD ||
        p->velocity.z > ADV_FACE_THRESHOLD || p->velocity.z < -ADV_FACE_THRESHOLD)
        p->facing = AdventureMath_HeadingXZ(p->velocity.x, p->velocity.z);
}
void AdventurePlayer_TickGroundPilot(AdventurePlayer *p, AdventureInput input,
                                    AdventureBasis basis)
{
    if (!p) return;
    p->previous_pos = p->pos;
    AdventurePlayer_Horizontal(p, input, basis);
    p->pos.x = AdventureMath_ClampI32((int64_t)p->pos.x + p->velocity.x);
    p->pos.z = AdventureMath_ClampI32((int64_t)p->pos.z + p->velocity.z);
    p->velocity.y = 0;
    p->motion = (p->velocity.x || p->velocity.z) ?
                ADV_MOTION_MOVING : ADV_MOTION_IDLE;
}
static bool query(AdventureTerrain terrain, int32_t x, int32_t z,
                  int32_t max_y, AdventureGroundHit *hit)
{
    return terrain.query_ground &&
           terrain.query_ground(terrain.context, x, z, max_y, hit);
}
void AdventurePlayer_ResolveSpawn(AdventurePlayer *p, AdventureTerrain terrain)
{
    if (!p) return;
    AdventureGroundHit hit;
    int32_t upper = AdventureMath_ClampI32((int64_t)p->pos.y + ADV_STEP_UP);
    if (query(terrain, p->pos.x, p->pos.z, upper, &hit) &&
        hit.height >= AdventureMath_ClampI32((int64_t)p->pos.y - ADV_GROUND_SNAP)) {
        p->pos.y = hit.height;
        p->previous_pos.y = hit.height;
        p->grounded = 1;
        p->surface_id = hit.surface_id;
        p->surface_layer = hit.layer;
        p->coyote_ticks = ADV_COYOTE_TICKS;
    }
}
void AdventurePlayer_TickWorld(AdventurePlayer *p, AdventureInput input,
                                AdventureBasis basis, AdventureTerrain terrain)
{
    if (!p) return;
    p->previous_pos = p->pos;
    AdventurePlayer_Horizontal(p, input, basis);
    AdventureCollision_MoveAndSlide(&p->pos, &p->velocity,
                                    terrain.sweep_wall, terrain.context,
                                    ADV_CAPSULE_RADIUS, ADV_CAPSULE_HEIGHT,
                                    p->surface_layer);
    if (p->grounded && terrain.sweep_capsule &&
        (p->pos.x != p->previous_pos.x ||
         p->pos.z != p->previous_pos.z)) {
        /* A standing Sonic can hit the SIDE of an explicitly solid roof
         * with the torso or lower cap even without jumping. Stop the
         * horizontal translation at the first body contact. */
        AdventureCapsuleHit hit;
        if (terrain.sweep_capsule(terrain.context,p->previous_pos,p->pos,
                                  ADV_CAPSULE_HEIGHT,ADV_CAPSULE_RADIUS,&hit)) {
            AdventureVec3 target=p->pos;
            p->pos.x=AdventureMath_ClampI32(
                (int64_t)p->previous_pos.x+
                ((int64_t)target.x-p->previous_pos.x)*hit.fraction/ADV_FP_ONE+
                (int64_t)hit.normal.x*(ADV_FP_ONE/64)/ADV_FP_ONE);
            p->pos.z=AdventureMath_ClampI32(
                (int64_t)p->previous_pos.z+
                ((int64_t)target.z-p->previous_pos.z)*hit.fraction/ADV_FP_ONE+
                (int64_t)hit.normal.z*(ADV_FP_ONE/64)/ADV_FP_ONE);
            p->velocity.x=AdventureMath_ClampI32(
                (int64_t)p->pos.x-p->previous_pos.x);
            p->velocity.z=AdventureMath_ClampI32(
                (int64_t)p->pos.z-p->previous_pos.z);
        }
    }

    if (input.pressed & ADV_ACTION_JUMP) p->buffered_jump = ADV_JUMP_BUFFER_TICKS;
    else if (p->buffered_jump) --p->buffered_jump;
    if (!p->grounded && p->coyote_ticks) --p->coyote_ticks;

    /* Probe for support before the jump. This preserves ledge coyote ticks
     * while rejecting floors above the maximum walkable step rise. */
    AdventureGroundHit hit;
    int32_t probe_top = AdventureMath_ClampI32((int64_t)p->pos.y + ADV_STEP_UP);
    bool support = query(terrain, p->pos.x, p->pos.z, probe_top, &hit);
    bool walkable = support &&
        hit.height >= AdventureMath_ClampI32((int64_t)p->pos.y - ADV_GROUND_SNAP);
    if (p->grounded && !walkable) {
        p->grounded = 0;
        p->surface_id = -1;
        p->coyote_ticks = ADV_COYOTE_TICKS;
    }
    if (p->grounded && walkable) {
        p->pos.y = hit.height;
        p->velocity.y = 0;
        p->surface_id = hit.surface_id;
        p->surface_layer = hit.layer;
    }
    if (p->buffered_jump && (p->grounded || p->coyote_ticks)) {
        p->buffered_jump = 0;
        p->coyote_ticks = 0;
        p->grounded = 0;
        p->surface_id = -1;
        p->velocity.y = ADV_JUMP_IMPULSE;
    }

    if (!p->grounded) {
        /* The horizontal wall solver already produced an X/Z displacement.
         * Test feet or head over the WHOLE diagonal trajectory, not only at
         * the last X/Z point (which missed thin elevated platforms). */
        p->velocity.y = AdventureMath_Approach(p->velocity.y, -ADV_TERMINAL,
                                               ADV_GRAVITY);
        AdventureVec3 from=p->previous_pos;
        from.y=p->pos.y; /* freshly snapped support, if applicable */
        AdventureVec3 to=p->pos;
        to.y=AdventureMath_ClampI32((int64_t)from.y+p->velocity.y);
        bool rising=p->velocity.y>0;
        AdventureVerticalContact contact;
        if (AdventureVertical_Sweep(terrain,from,to,ADV_CAPSULE_HEIGHT,
                                     ADV_CAPSULE_RADIUS,rising,&contact)) {
            p->pos=contact.position;
            /* Stop at the first intersecting footprint rather than skipping
             * a narrow platform and drifting through its far side. */
            p->velocity.x=AdventureMath_ClampI32(
                (int64_t)p->pos.x-p->previous_pos.x);
            p->velocity.z=AdventureMath_ClampI32(
                (int64_t)p->pos.z-p->previous_pos.z);
            p->velocity.y=0;
            if (!rising) {
                p->grounded=1;
                p->surface_id=contact.surface.surface_id;
                p->surface_layer=contact.surface.layer;
                p->coyote_ticks=ADV_COYOTE_TICKS;
            }
        }else{
            p->pos.y=to.y;
        }
    }
    if (p->grounded) {
        p->motion = (p->velocity.x || p->velocity.z) ?
                    ADV_MOTION_MOVING : ADV_MOTION_IDLE;
    } else {
        p->motion = p->velocity.y > 0 ? ADV_MOTION_JUMP : ADV_MOTION_FALL;
    }
}
