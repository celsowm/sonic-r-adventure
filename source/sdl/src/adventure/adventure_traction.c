/*
 * Fixed-60Hz, integer-only slope momentum. No heap or race physics.
 * Gravity projected onto a positive-up surface normal gives a downhill
 * acceleration +n_horizontal*n_y*g. Uphill input fights it; downhill
 * steering reinforces it. The faster slope-speed cap is applied AFTER
 * steering, so holding forward does not reset an earned downhill boost.
 */
#include "adventure_traction.h"
#include <stdint.h>

static int32_t saturate_target(int64_t value)
{
    return AdventureMath_ClampI32(value);
}
static void cap_horizontal(AdventureVec3 *v,int32_t speed_limit)
{
    int64_t x=v->x,z=v->z;
    /* Separate terms before summation to avoid signed overflow when both
     * components are extreme INT32_MIN inputs. */
    uint64_t sq=(uint64_t)(x*x)+(uint64_t)(z*z);
    uint64_t cap=(uint64_t)speed_limit*speed_limit;
    if(sq<=cap) return;
    uint32_t len=AdventureMath_IsqrtU64(sq);
    if((uint64_t)len*len<sq) ++len;
    v->x=(int32_t)(x*speed_limit/len);
    v->z=(int32_t)(z*speed_limit/len);
}
/* Momentum-aligned driving should not forcibly brake an already-fast Sonic
 * back to run speed. Rescale the desired steering direction to the earned
 * horizontal speed while still approaching it at a bounded acceleration.
 * Opposing input reverses normally (no full-speed instant turn). */
static void steer(AdventureVec3 *v,int32_t tx,int32_t tz,int32_t accel,
                  bool preserve_momentum)
{
    if(preserve_momentum && (tx || tz)) {
        int64_t dot=(int64_t)v->x*tx+(int64_t)v->z*tz;
        uint64_t sq=(uint64_t)((int64_t)v->x*v->x)+
                    (uint64_t)((int64_t)v->z*v->z);
        uint64_t baseline=(uint64_t)ADV_TRACTION_RUN_SPEED*
                         ADV_TRACTION_RUN_SPEED;
        if(dot>0 && sq>baseline) {
            uint32_t current=AdventureMath_IsqrtU64((uint64_t)sq);
            uint64_t desired_sq=(uint64_t)((int64_t)tx*tx+
                                          (int64_t)tz*tz);
            uint32_t desired=AdventureMath_IsqrtU64(desired_sq);
            if(desired) {
                int32_t speed=current>(uint32_t)ADV_TRACTION_SLOPE_SPEED?
                              ADV_TRACTION_SLOPE_SPEED:(int32_t)current;
                if(speed>ADV_TRACTION_SLOPE_SPEED)
                    speed=ADV_TRACTION_SLOPE_SPEED;
                tx=saturate_target((int64_t)tx*speed/desired);
                tz=saturate_target((int64_t)tz*speed/desired);
            }
        }
    }
    v->x=AdventureMath_Approach(v->x,tx,accel);
    v->z=AdventureMath_Approach(v->z,tz,accel);
}
void AdventureTraction_Update(AdventureVec3 *velocity,
                              int32_t target_x,int32_t target_z,
                              bool driving,AdventureVec3 normal,
                              bool grounded_slope,bool airborne_coast)
{
    if(!velocity) return;
    if(grounded_slope) {
        /* Query providers are read-only but can be synthetic or invalid.
         * Never multiply an unbounded normal into the gravity projection. */
        if(normal.y<0 || normal.y>ADV_FP_ONE ||
           normal.x < -ADV_FP_ONE || normal.x > ADV_FP_ONE ||
           normal.z < -ADV_FP_ONE || normal.z > ADV_FP_ONE)
            normal=(AdventureVec3){0,ADV_FP_ONE,0};
        if(driving) {
            steer(velocity,target_x,target_z,ADV_TRACTION_DRIVE_ACCEL,true);
        }else{
            velocity->x=AdventureMath_Approach(
                velocity->x,0,ADV_TRACTION_ROLL_FRICTION);
            velocity->z=AdventureMath_Approach(
                velocity->z,0,ADV_TRACTION_ROLL_FRICTION);
        }
        /* Normals are Q12, as is projected gravity. Use 64-bit
         * intermediates so all signed products are well defined. */
        int64_t divisor=(int64_t)ADV_FP_ONE*ADV_FP_ONE;
        velocity->x=AdventureMath_ClampI32((int64_t)velocity->x+
            (int64_t)normal.x*normal.y*ADV_TRACTION_SLOPE_PULL/divisor);
        velocity->z=AdventureMath_ClampI32((int64_t)velocity->z+
            (int64_t)normal.z*normal.y*ADV_TRACTION_SLOPE_PULL/divisor);
        cap_horizontal(velocity,ADV_TRACTION_SLOPE_SPEED);
    }else if(airborne_coast) {
        if(driving)
            steer(velocity,target_x,target_z,ADV_TRACTION_AIR_STEER,true);
        else {
            velocity->x=AdventureMath_Approach(
                velocity->x,0,ADV_TRACTION_AIR_DRAG);
            velocity->z=AdventureMath_Approach(
                velocity->z,0,ADV_TRACTION_AIR_DRAG);
        }
        cap_horizontal(velocity,ADV_TRACTION_SLOPE_SPEED);
    }else{
        /* Exactly preserve the original WP-02/03 flat-ground steering and
         * braking behavior, including the standalone controller pilot. */
        int32_t accel=driving?ADV_TRACTION_DRIVE_ACCEL:
                              ADV_TRACTION_FLAT_BRAKE;
        steer(velocity,target_x,target_z,accel,false);
        cap_horizontal(velocity,ADV_TRACTION_RUN_SPEED);
    }
}
