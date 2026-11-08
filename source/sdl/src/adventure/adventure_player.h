/* Portable Sonic movement and jump state, independent of legacy Player ABI. */
#ifndef SONICR_ADVENTURE_PLAYER_H
#define SONICR_ADVENTURE_PLAYER_H
#include "adventure_input.h"
#include "adventure_math.h"
#include "adventure_terrain.h"
#include <stdint.h>
typedef enum AdventureMotion {
    ADV_MOTION_IDLE = 0, ADV_MOTION_MOVING = 1,
    ADV_MOTION_JUMP = 2, ADV_MOTION_FALL = 3
} AdventureMotion;
typedef struct AdventurePlayer {
    AdventureVec3 pos;
    AdventureVec3 previous_pos;
    AdventureVec3 velocity;       /* Q20.12 per 60Hz tick */
    AdventureVec3 ground_normal;  /* Q12 positive-up support normal */
    uint16_t facing;
    AdventureMotion motion;
    int32_t surface_id;           /* -1 = no support */
    int32_t surface_layer;
    uint8_t grounded;
    uint8_t coyote_ticks;
    uint8_t buffered_jump;
    uint8_t slope_airborne;   /* keep earned incline momentum until landing */
} AdventurePlayer;
void AdventurePlayer_Init(AdventurePlayer *player, AdventureVec3 pos, uint16_t facing);
/* Horizontal-only legacy pilot retained for isolated controller tests. */
void AdventurePlayer_TickGroundPilot(AdventurePlayer *player, AdventureInput input,
                                    AdventureBasis camera_basis);
/* World-step owns horizontal integration, support, jump, gravity, landing. */
void AdventurePlayer_TickWorld(AdventurePlayer *player, AdventureInput input,
                                AdventureBasis camera_basis, AdventureTerrain terrain);
/* Attempt initial support without teleporting to an unrelated lower level. */
void AdventurePlayer_ResolveSpawn(AdventurePlayer *player, AdventureTerrain terrain);
#endif
