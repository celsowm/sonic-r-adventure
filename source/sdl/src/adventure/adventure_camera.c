/*
 * Fixed-tick 3D action camera. All simulation math is Q20.12; trig uses a
 * tiny quarter-wave lookup table with integer interpolation. No floating
 * point, racing-camera globals, heap allocation or frame-time dependence.
 */
#include "adventure_camera.h"
#include <stdint.h>
#include <limits.h>

#define CAM_FOLLOW_DIV 6
#define CAM_VERTICAL_DIV 8
#define CAM_YAW_DIV 6
#define CAM_MAX_YAW_STEP 64
#define CAM_ORBIT_RATE 24
#define CAM_ORBIT_MAX 1280
#define CAM_RECENTER 12
#define CAM_WALL_SKIN (ADV_FP_ONE/2)
#define CAM_RECOVER_STEP (ADV_FP_ONE/3)
#define CAM_TELEPORT_DISTANCE (100*ADV_FP_ONE)
#define CAM_LOOKAHEAD_LIMIT (4*ADV_FP_ONE)

static const uint16_t s_quarter_sin[33] = {0, 201, 401, 601, 799, 995, 1189, 1380, 1567, 1751, 1931, 2106, 2276, 2440, 2598, 2751, 2896, 3035, 3166, 3290, 3406, 3513, 3612, 3703, 3784, 3857, 3920, 3973, 4017, 4052, 4076, 4091, 4096};
static int32_t sin_q12(uint16_t angle)
{
    uint16_t wrapped=angle&4095u;
    uint16_t quadrant=wrapped>>10;
    uint16_t t=wrapped&1023u;
    if(quadrant&1u) t=(uint16_t)(1024u-t);
    uint16_t ix=t>>5,rem=t&31u;
    int32_t result=s_quarter_sin[ix];
    if(ix<32) result+=(int32_t)(s_quarter_sin[ix+1u]-result)*rem/32;
    return quadrant>=2u?-result:result;
}
static int32_t cos_q12(uint16_t angle)
{
    return sin_q12((uint16_t)((angle+1024u)&4095u));
}
static int32_t approach(int32_t value,int32_t goal,int32_t divisor)
{
    int64_t diff=(int64_t)goal-value;
    int64_t step=diff/divisor;
    if(!step && diff) step=diff<0?-1:1;
    return AdventureMath_ClampI32((int64_t)value+step);
}
static AdventureVec3 desired_eye(AdventureVec3 target,
                                 uint16_t yaw,int32_t distance)
{
    AdventureVec3 point={
        AdventureMath_ClampI32((int64_t)target.x-
                               (int64_t)cos_q12(yaw)*distance/ADV_FP_ONE),
        AdventureMath_ClampI32((int64_t)target.y+
                               (int64_t)ADV_CAMERA_HEIGHT*distance/
                               ADV_CAMERA_DISTANCE),
        AdventureMath_ClampI32((int64_t)target.z-
                               (int64_t)sin_q12(yaw)*distance/ADV_FP_ONE)
    };
    return point;
}
static AdventureVec3 look_target(const AdventurePlayer *player)
{
    int32_t lead_x=AdventureMath_ClampI32(
        (int64_t)player->velocity.x*3/2);
    int32_t lead_z=AdventureMath_ClampI32(
        (int64_t)player->velocity.z*3/2);
    if(lead_x>CAM_LOOKAHEAD_LIMIT) lead_x=CAM_LOOKAHEAD_LIMIT;
    if(lead_x< -CAM_LOOKAHEAD_LIMIT) lead_x=-CAM_LOOKAHEAD_LIMIT;
    if(lead_z>CAM_LOOKAHEAD_LIMIT) lead_z=CAM_LOOKAHEAD_LIMIT;
    if(lead_z< -CAM_LOOKAHEAD_LIMIT) lead_z=-CAM_LOOKAHEAD_LIMIT;
    AdventureVec3 target={
        AdventureMath_ClampI32((int64_t)player->pos.x+lead_x),
        AdventureMath_ClampI32((int64_t)player->pos.y+ADV_CAMERA_LOOK_HEIGHT),
        AdventureMath_ClampI32((int64_t)player->pos.z+lead_z)
    };
    return target;
}
void AdventureCamera_Reset(AdventureCamera *cam,
                            const AdventurePlayer *player)
{
    if(!cam || !player) return;
    cam->yaw=player->facing&4095u;
    cam->orbit_offset=0;
    cam->distance=ADV_CAMERA_DISTANCE;
    cam->look_at=look_target(player);
    cam->position=desired_eye(cam->look_at,cam->yaw,cam->distance);
    cam->initialized=1;
}
AdventureBasis AdventureCamera_MovementBasis(const AdventureCamera *camera)
{
    uint16_t yaw=camera && camera->initialized ? camera->yaw : 0;
    int32_t fx=cos_q12(yaw),fz=sin_q12(yaw);
    AdventureBasis basis={fx,fz,-fz,fx};
    return basis;
}
static void clamp_obstruction(AdventureVec3 from,AdventureVec3 *eye,
                              uint16_t fraction)
{
    uint32_t time=fraction>128u?(uint32_t)fraction-128u:0u;
    eye->x=AdventureMath_ClampI32((int64_t)from.x+
            ((int64_t)eye->x-from.x)*time/ADV_FP_ONE);
    eye->y=AdventureMath_ClampI32((int64_t)from.y+
            ((int64_t)eye->y-from.y)*time/ADV_FP_ONE);
    eye->z=AdventureMath_ClampI32((int64_t)from.z+
            ((int64_t)eye->z-from.z)*time/ADV_FP_ONE);
}
void AdventureCamera_Tick(AdventureCamera *cam,
                           const AdventurePlayer *player,
                           uint32_t buttons,
                           AdventureCameraTrace trace,void *context)
{
    if(!cam || !player) return;
    if(!cam->initialized) AdventureCamera_Reset(cam,player);
    bool left=(buttons&ADV_ACTION_CAMERA_LEFT)!=0;
    bool right=(buttons&ADV_ACTION_CAMERA_RIGHT)!=0;
    if(left!=right) {
        int32_t offset=(int32_t)cam->orbit_offset+
                       (left?-CAM_ORBIT_RATE:CAM_ORBIT_RATE);
        if(offset>CAM_ORBIT_MAX) offset=CAM_ORBIT_MAX;
        if(offset< -CAM_ORBIT_MAX) offset=-CAM_ORBIT_MAX;
        cam->orbit_offset=(int16_t)offset;
    }else{
        cam->orbit_offset=(int16_t)AdventureMath_Approach(
            cam->orbit_offset,0,CAM_RECENTER);
    }

    uint16_t target_yaw=(uint16_t)((player->facing+
                                         cam->orbit_offset)&4095u);
    int32_t delta=((int32_t)target_yaw-(int32_t)cam->yaw+
                   2048)&4095;
    delta-=2048;
    int32_t turn=delta/CAM_YAW_DIV;
    if(!turn && delta) turn=delta<0?-1:1;
    if(turn>CAM_MAX_YAW_STEP) turn=CAM_MAX_YAW_STEP;
    if(turn< -CAM_MAX_YAW_STEP) turn=-CAM_MAX_YAW_STEP;
    cam->yaw=(uint16_t)((cam->yaw+turn)&4095);

    AdventureVec3 goal=look_target(player);
    int64_t dx=(int64_t)goal.x-cam->look_at.x;
    int64_t dy=(int64_t)goal.y-cam->look_at.y;
    int64_t dz=(int64_t)goal.z-cam->look_at.z;
    if(dx>CAM_TELEPORT_DISTANCE || dx< -CAM_TELEPORT_DISTANCE ||
       dy>CAM_TELEPORT_DISTANCE || dy< -CAM_TELEPORT_DISTANCE ||
       dz>CAM_TELEPORT_DISTANCE || dz< -CAM_TELEPORT_DISTANCE) {
        AdventureCamera_Reset(cam,player);
        goal=cam->look_at;
    }
    cam->look_at.x=approach(cam->look_at.x,goal.x,CAM_FOLLOW_DIV);
    cam->look_at.y=approach(cam->look_at.y,goal.y,CAM_VERTICAL_DIV);
    cam->look_at.z=approach(cam->look_at.z,goal.z,CAM_FOLLOW_DIV);

    AdventureVec3 far=desired_eye(cam->look_at,cam->yaw,
                                  ADV_CAMERA_DISTANCE);
    uint16_t fraction=0;
    int32_t allowed=ADV_CAMERA_DISTANCE;
    bool blocked=trace && trace(context,cam->look_at,far,
                                 player->surface_layer,&fraction);
    if(blocked && fraction<=ADV_FP_ONE) {
        allowed=AdventureMath_ClampI32(
            (int64_t)ADV_CAMERA_DISTANCE*fraction/ADV_FP_ONE-
            CAM_WALL_SKIN);
        if(allowed<ADV_CAMERA_MIN_DISTANCE)
            allowed=ADV_CAMERA_MIN_DISTANCE;
    }
    if(allowed<cam->distance) cam->distance=allowed;
    else {
        cam->distance=AdventureMath_Approach(
            cam->distance,allowed,CAM_RECOVER_STEP);
    }
    if(cam->distance>ADV_CAMERA_DISTANCE)
        cam->distance=ADV_CAMERA_DISTANCE;
    if(cam->distance<ADV_CAMERA_MIN_DISTANCE)
        cam->distance=ADV_CAMERA_MIN_DISTANCE;
    AdventureVec3 desired=desired_eye(cam->look_at,cam->yaw,cam->distance);
    if(blocked) cam->position=desired; /* promptly move inside near walls */
    else {
        cam->position.x=approach(cam->position.x,desired.x,CAM_FOLLOW_DIV);
        cam->position.y=approach(cam->position.y,desired.y,CAM_FOLLOW_DIV);
        cam->position.z=approach(cam->position.z,desired.z,CAM_FOLLOW_DIV);
    }
    /* A rapidly moving target or smoothing may put the actual camera eye
     * behind a new occluder, even if the ideal location was unobstructed. */
    if(trace && trace(context,cam->look_at,cam->position,
                      player->surface_layer,&fraction) &&
       fraction<=ADV_FP_ONE)
        clamp_obstruction(cam->look_at,&cam->position,fraction);
}
