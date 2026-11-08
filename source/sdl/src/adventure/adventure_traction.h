/* WP-04.11: deterministic Q20.12 ground traction and slope momentum.
 * All values are displacement per 60Hz tick, not seconds or legacy physics. */
#ifndef SONICR_ADVENTURE_TRACTION_H
#define SONICR_ADVENTURE_TRACTION_H
#include "adventure_slope.h"

#define ADV_TRACTION_RUN_SPEED (3 * ADV_FP_ONE)
#define ADV_TRACTION_SLOPE_SPEED (5 * ADV_FP_ONE)
#define ADV_TRACTION_DRIVE_ACCEL 900
#define ADV_TRACTION_FLAT_BRAKE 1250
#define ADV_TRACTION_ROLL_FRICTION 160
#define ADV_TRACTION_AIR_STEER 340
#define ADV_TRACTION_AIR_DRAG 96
#define ADV_TRACTION_SLOPE_PULL (ADV_FP_ONE / 3)
/* The world-facing normal can differ by 1/4096 from flat after rounding. */
#define ADV_TRACTION_SLOPE_EPSILON 64

/* Grounded slope: steering + gravity tangent + rolling friction.
 * Airborne after leaving a slope: inertia + limited air steering, WITHOUT
 * ground gravity projection. Flat and legacy-pilot cases remain separate. */
void AdventureTraction_Update(AdventureVec3 *velocity,
                              int32_t target_x, int32_t target_z,
                              bool driving, AdventureVec3 ground_normal,
                              bool grounded_slope, bool airborne_coast);
#endif
