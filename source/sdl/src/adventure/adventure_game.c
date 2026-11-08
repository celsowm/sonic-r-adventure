/*
 * Platform-independent adventure-mode update lifecycle.
 * This intentionally does not simulate Sonic yet: WP-01 isolates the mode,
 * stage render path, and update schedule before introducing new physics.
 */
#include "adventure_game.h"

void AdventureGame_Init(AdventureGame *game)
{
    if (game == 0) return;
    game->tick_count = 0;
    game->phase = ADVENTURE_PHASE_RUNNING;
}

bool AdventureGame_Tick(AdventureGame *game)
{
    if (game == 0 || game->phase != ADVENTURE_PHASE_RUNNING)
        return false;

    ++game->tick_count;
    return true;
}

void AdventureGame_Shutdown(AdventureGame *game)
{
    if (game == 0) return;
    game->phase = ADVENTURE_PHASE_STOPPED;
}
