/*
 * WP-03 horizontal ground pilot. World collision, vertical movement, jumping,
 * material friction and air movement belong to WP-04, not this module.
 * Position and velocity remain integer Q20.12 for deterministic simulation.
 */
#include "adventure_player.h"
#include <stdint.h>
/* These are per-60-Hz-tick tuning constants, NOT measured Sonic Adventure data. */
#define ADV_MAX_SPEED_Q12 (3 * ADV_FP_ONE)  /* 180 world units/second */
#define ADV_ACCEL_Q12     900
#define ADV_BRAKE_Q12     1250
#define ADV_FACE_THRESHOLD 160

void AdventurePlayer_Init(AdventurePlayer *p, AdventureVec3 pos, uint16_t facing)
{
    if (!p) return;
    p->pos = pos;
    p->previous_pos = pos;
    p->velocity.x = p->velocity.y = p->velocity.z = 0;
    p->facing = facing & 4095u;
    p->motion = ADV_MOTION_IDLE;
}
void AdventurePlayer_TickGroundPilot(AdventurePlayer *p, AdventureInput input,
                                    AdventureBasis basis)
{
    if (!p) return;
    p->previous_pos = p->pos;
    /* Camera basis and stick both Q12. Products are bounded 64-bit. */
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
    /* 2-axis vector cap: avoids diagonal speed boost in integer rounding. */
    int64_t vx = p->velocity.x, vz = p->velocity.z;
    int64_t speed_sq = vx*vx + vz*vz;
    int64_t cap_sq = (int64_t)ADV_MAX_SPEED_Q12 * ADV_MAX_SPEED_Q12;
    if (speed_sq > cap_sq) {
        /* Integer ceiling sqrt prevents overspeed and cross-libm divergence. */
        uint32_t speed = AdventureMath_IsqrtU64((uint64_t)speed_sq);
        if ((uint64_t)speed * speed < (uint64_t)speed_sq) ++speed;
        p->velocity.x = (int32_t)(vx * ADV_MAX_SPEED_Q12 / speed);
        p->velocity.z = (int32_t)(vz * ADV_MAX_SPEED_Q12 / speed);
    }
    p->pos.x = AdventureMath_ClampI32((int64_t)p->pos.x + p->velocity.x);
    p->pos.z = AdventureMath_ClampI32((int64_t)p->pos.z + p->velocity.z);
    /* Y stays unchanged: real support resolution is not implemented yet. */
    p->velocity.y = 0;
    p->motion = (p->velocity.x || p->velocity.z) ?
                ADV_MOTION_MOVING : ADV_MOTION_IDLE;
    if ((p->velocity.x > ADV_FACE_THRESHOLD || p->velocity.x < -ADV_FACE_THRESHOLD ||
         p->velocity.z > ADV_FACE_THRESHOLD || p->velocity.z < -ADV_FACE_THRESHOLD)) {
        /* Integer octant approximation, deterministic on SH-4 and host.
         * Visually confirm old renderer's yaw convention on Resort Island. */
        p->facing = AdventureMath_HeadingXZ(p->velocity.x, p->velocity.z);
    }
}
