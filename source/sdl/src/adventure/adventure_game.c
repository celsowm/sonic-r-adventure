/* Own the authoritative tick and player state; never call racing physics. */
#include "adventure_game.h"
void AdventureGame_Init(AdventureGame *game)
{
    if (!game) return;
    game->phase = ADVENTURE_PHASE_RUNNING;
    game->tick_count = 0;
    AdventureInput_Reset(&game->input_history);
    AdventureVec3 origin = {0, 0, 0};
    AdventurePlayer_Init(&game->player, origin, 0);
}
void AdventureGame_SetSpawn(AdventureGame *game, AdventureVec3 pos, uint16_t facing)
{
    if (!game || game->phase != ADVENTURE_PHASE_RUNNING) return;
    AdventurePlayer_Init(&game->player, pos, facing);
    AdventureInput_Reset(&game->input_history);
}
bool AdventureGame_Tick(AdventureGame *game, AdventureRawInput raw, AdventureBasis camera_basis)
{
    if (!game || game->phase != ADVENTURE_PHASE_RUNNING) return false;
    AdventureInput input = AdventureInput_Sample(&game->input_history, raw);
    AdventurePlayer_TickGroundPilot(&game->player, input, camera_basis);
    ++game->tick_count;
    return true;
}
void AdventureGame_Shutdown(AdventureGame *game)
{
    if (!game) return;
    game->phase = ADVENTURE_PHASE_STOPPED;
}
