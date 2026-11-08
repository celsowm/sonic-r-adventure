/* Sonic R Adventure -- platform-independent fixed-tick gameplay state. */
#ifndef SONICR_ADVENTURE_GAME_H
#define SONICR_ADVENTURE_GAME_H
#include "adventure_input.h"
#include "adventure_player.h"
#include <stdbool.h>
#include <stdint.h>
typedef enum AdventurePhase {
    ADVENTURE_PHASE_UNINITIALIZED = 0,
    ADVENTURE_PHASE_RUNNING, ADVENTURE_PHASE_STOPPED
} AdventurePhase;
typedef struct AdventureGame {
    AdventurePhase phase;
    uint32_t tick_count;
    AdventureInputHistory input_history;
    AdventurePlayer player;
    AdventureTerrain terrain;
} AdventureGame;
void AdventureGame_Init(AdventureGame *game);
void AdventureGame_SetTerrain(AdventureGame *game, AdventureTerrain terrain);
void AdventureGame_SetSpawn(AdventureGame *game, AdventureVec3 pos, uint16_t facing);
bool AdventureGame_Tick(AdventureGame *game, AdventureRawInput raw, AdventureBasis camera_basis);
void AdventureGame_Shutdown(AdventureGame *game);
#endif
