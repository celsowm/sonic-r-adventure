/*
 * 60Hz Q20.12 motion. All gameplay integration uses integer arithmetic.
 * WP-04 support query handles terrain ramps, edges, stacked floors and jumping.
 * Static walls and upward collisions with elevated terrain are supported;
 * arbitrary mesh ceilings and special loop surfaces are not implemented.
 */
#include "adventure_player.h"
#include "adventure_vertical.h"
#include "adventure_solver.h"
#include "adventure_slope.h"
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
    p->ground_normal=(AdventureVec3){0,ADV_FP_ONE,0};
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
    AdventureVec3 normal;
    if (AdventureSlope_QuerySupport(terrain,p->pos.x,p->pos.z,
                                    upper,&hit,&normal) &&
        hit.height >= AdventureMath_ClampI32((int64_t)p->pos.y - ADV_GROUND_SNAP)) {
        p->ground_normal=normal;
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
    p->previous_pos=p->pos;
    AdventurePlayer_Horizontal(p,input,basis);
    bool was_grounded=p->grounded!=0;

    if(input.pressed & ADV_ACTION_JUMP)
        p->buffered_jump=ADV_JUMP_BUFFER_TICKS;
    else if(p->buffered_jump)
        --p->buffered_jump;
    if(!p->grounded && p->coyote_ticks)
        --p->coyote_ticks;

    /* Evaluate support near the intended X/Z position BEFORE integration.
     * Leaving a ledge must start falling this tick, while a grounded slope
     * can be followed after the full collision resolution. */
    if(p->grounded) {
        int32_t tx=AdventureMath_ClampI32(
            (int64_t)p->pos.x+p->velocity.x);
        int32_t tz=AdventureMath_ClampI32(
            (int64_t)p->pos.z+p->velocity.z);
        int32_t top=AdventureMath_ClampI32(
            (int64_t)p->pos.y+ADV_STEP_UP);
        int32_t bottom=AdventureMath_ClampI32(
            (int64_t)p->pos.y-ADV_GROUND_SNAP);
        AdventureGroundHit ahead;
        AdventureVec3 normal;
        if(!AdventureSlope_QuerySupport(terrain,tx,tz,top,&ahead,&normal) ||
           ahead.height<bottom) {
            /* Steep terrain is a physical obstruction but NOT a support.
             * Bias the airborne motion slightly downhill so Sonic cannot
             * stick indefinitely to an unwalkable slope face. */
            AdventureGroundHit steep;
            if(query(terrain,tx,tz,top,&steep) && steep.height>=bottom) {
                AdventureVec3 n=AdventureSlope_Normal(terrain,tx,tz,
                                                      steep.surface_id);
                AdventureVec3 bias=AdventureSlope_Downhill(n);
                p->velocity.x=AdventureMath_ClampI32(
                    (int64_t)p->velocity.x+bias.x);
                p->velocity.z=AdventureMath_ClampI32(
                    (int64_t)p->velocity.z+bias.z);
            }
            p->grounded=0;
            p->surface_id=-1;
            p->coyote_ticks=ADV_COYOTE_TICKS;
        }
    }

    bool jumped=p->buffered_jump && (p->grounded || p->coyote_ticks);
    if(jumped) {
        p->buffered_jump=0;
        p->coyote_ticks=0;
        p->grounded=0;
        p->surface_id=-1;
        p->velocity.y=ADV_JUMP_IMPULSE;
    }
    if(!p->grounded)
        p->velocity.y=AdventureMath_Approach(
            p->velocity.y,-ADV_TERMINAL,ADV_GRAVITY);
    else
        p->velocity.y=0;

    /* ONE movement path: TER wall discs, TER floor/ceiling probes, authored
     * BIN triangle capsules all share the same first-contact ordering and
     * four-contact tangent-space slide response. */
    AdventureVec3 start=p->pos;
    AdventureVec3 attempted=p->velocity;
    AdventureSolver world={terrain,p->surface_layer};
    AdventureSlideResult result=AdventureSolver_Move(
        world,start,attempted,ADV_CAPSULE_HEIGHT,ADV_CAPSULE_RADIUS);
    p->pos=result.position;
    p->velocity.x=AdventureMath_ClampI32(
        (int64_t)p->pos.x-start.x);
    p->velocity.z=AdventureMath_ClampI32(
        (int64_t)p->pos.z-start.z);
    p->velocity.y=result.tangent_velocity.y;

    /* Snapping is for PREVIOUSLY grounded players following a ramp, or
     * airborne players who genuinely hit a supporting face while falling.
     * Simply passing within two units of a floor in mid-air must NOT
     * teleport Sonic onto it. */
    int32_t upper=AdventureMath_ClampI32(
        (int64_t)p->pos.y+ADV_STEP_UP);
    int32_t lower=AdventureMath_ClampI32(
        (int64_t)p->pos.y-ADV_GROUND_SNAP);
    AdventureGroundHit support;
    AdventureVec3 support_normal;
    bool has_support=AdventureSlope_QuerySupport(
        terrain,p->pos.x,p->pos.z,upper,&support,&support_normal) &&
        support.height>=lower;
    if(!jumped && has_support &&
       (was_grounded || result.touched_floor)) {
        p->pos.y=support.height;
        p->grounded=1;
        p->velocity.y=0;
        p->surface_id=support.surface_id;
        p->surface_layer=support.layer;
        p->ground_normal=support_normal;
        p->coyote_ticks=ADV_COYOTE_TICKS;
    }else{
        if(was_grounded && p->grounded && !has_support)
            p->coyote_ticks=ADV_COYOTE_TICKS;
        p->grounded=0;
        p->surface_id=-1;
        p->ground_normal=(AdventureVec3){0,ADV_FP_ONE,0};
    }

    if(p->grounded)
        p->motion=(p->velocity.x || p->velocity.z)?
                   ADV_MOTION_MOVING:ADV_MOTION_IDLE;
    else
        p->motion=p->velocity.y>0?ADV_MOTION_JUMP:ADV_MOTION_FALL;
}
