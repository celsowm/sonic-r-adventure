/*
 * WP-05 camera simulation: Q20.12, 60 Hz, standalone from race state.
 * Renderer receives converted snapshots via a separate compatibility bridge.
 */
#ifndef SONICR_ADVENTURE_CAMERA_H
#define SONICR_ADVENTURE_CAMERA_H
#include "adventure_player.h"
#include "adventure_input.h"

#define ADV_CAMERA_DISTANCE (12 * ADV_FP_ONE)
#define ADV_CAMERA_HEIGHT (5 * ADV_FP_ONE)
#define ADV_CAMERA_LOOK_HEIGHT (2 * ADV_FP_ONE)
#define ADV_CAMERA_MIN_DISTANCE (2 * ADV_FP_ONE)

/* Return true and the earliest Q12 ray fraction when a solid obstruction
 * lies between the target and desired camera eye. FALSE means clear.
 * The provider must not modify simulation state. */
typedef bool (*AdventureCameraTrace)(void *context,
                                     AdventureVec3 target,
                                     AdventureVec3 eye,
                                     int32_t collision_layer,
                                     uint16_t *fraction);

typedef struct AdventureCamera {
    AdventureVec3 position;     /* eye, world Q20.12, positive Y up */
    AdventureVec3 look_at;      /* smoothly tracked look target */
    uint16_t yaw;               /* 0=+X, 1024=+Z, clockwise in X/Z */
    int16_t orbit_offset;       /* signed player-relative orbit deviation */
    int32_t distance;           /* smooth recovery from obstruction */
    uint8_t initialized;
} AdventureCamera;

void AdventureCamera_Reset(AdventureCamera *camera,
                            const AdventurePlayer *player);
void AdventureCamera_Tick(AdventureCamera *camera,
                           const AdventurePlayer *player,
                           uint32_t buttons,
                           AdventureCameraTrace trace,
                           void *context);
AdventureBasis AdventureCamera_MovementBasis(const AdventureCamera *camera);
#endif
