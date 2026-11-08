/*
 * Sonic R Adventure -- portable gameplay lifecycle (WP-01 skeleton).
 * No SDL, KOS, renderer or legacy racing types are allowed in this header.
 */
#ifndef SONICR_ADVENTURE_GAME_H
#define SONICR_ADVENTURE_GAME_H

#include <stdbool.h>
#include <stdint.h>

typedef enum AdventurePhase {
    ADVENTURE_PHASE_UNINITIALIZED = 0,
    ADVENTURE_PHASE_RUNNING,
    ADVENTURE_PHASE_STOPPED
} AdventurePhase;

typedef struct AdventureGame {
    AdventurePhase phase;
    uint32_t tick_count;
} AdventureGame;

/* Stage simulation is deliberately a no-op until the controller (WP-02/03).
 * No original racing update is called from these functions. */
void AdventureGame_Init(AdventureGame *game);
bool AdventureGame_Tick(AdventureGame *game);
void AdventureGame_Shutdown(AdventureGame *game);

#endif /* SONICR_ADVENTURE_GAME_H */
