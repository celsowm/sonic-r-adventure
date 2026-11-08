/* Pure portable Sonic movement state, independent of legacy Player ABI. */
#ifndef SONICR_ADVENTURE_PLAYER_H
#define SONICR_ADVENTURE_PLAYER_H
#include "adventure_input.h"
#include "adventure_math.h"
#include <stdint.h>
typedef enum AdventureMotion {
    ADV_MOTION_IDLE = 0, ADV_MOTION_MOVING = 1
} AdventureMotion;
typedef struct AdventurePlayer {
    AdventureVec3 pos;          /* Q20.12 canonical Y-up */
    AdventureVec3 previous_pos;
    AdventureVec3 velocity;     /* Q20.12 displacement per fixed 60Hz tick */
    uint16_t facing;            /* 0..4095, legacy yaw orientation */
    AdventureMotion motion;
} AdventurePlayer;
void AdventurePlayer_Init(AdventurePlayer *player, AdventureVec3 pos, uint16_t facing);
void AdventurePlayer_TickGroundPilot(AdventurePlayer *player, AdventureInput input,
                                    AdventureBasis camera_basis);
#endif
