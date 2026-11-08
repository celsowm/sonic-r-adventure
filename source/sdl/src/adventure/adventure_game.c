/* Own the authoritative simulation state; never call racing physics. */
#include "adventure_game.h"
void AdventureGame_Init(AdventureGame *game)
{
    if (!game) return;
    game->phase = ADVENTURE_PHASE_RUNNING;
    game->tick_count = 0;
    game->terrain.query_ground = 0;
    game->terrain.context = 0;
    game->terrain.sweep_wall = 0;
    game->terrain.query_overhead = 0;
    game->terrain.sweep_roof = 0;
    AdventureInput_Reset(&game->input_history);
    AdventureVec3 origin = {0, 0, 0};
    AdventurePlayer_Init(&game->player, origin, 0);
}
void AdventureGame_SetTerrain(AdventureGame *game, AdventureTerrain terrain)
{
    if (!game || game->phase != ADVENTURE_PHASE_RUNNING) return;
    game->terrain = terrain;
    AdventurePlayer_ResolveSpawn(&game->player, terrain);
}
void AdventureGame_SetSpawn(AdventureGame *game, AdventureVec3 pos, uint16_t facing)
{
    if (!game || game->phase != ADVENTURE_PHASE_RUNNING) return;
    AdventurePlayer_Init(&game->player, pos, facing);
    AdventurePlayer_ResolveSpawn(&game->player, game->terrain);
    AdventureInput_Reset(&game->input_history);
}
bool AdventureGame_Tick(AdventureGame *game, AdventureRawInput raw, AdventureBasis camera_basis)
{
    if (!game || game->phase != ADVENTURE_PHASE_RUNNING) return false;
    AdventureInput input = AdventureInput_Sample(&game->input_history, raw);
    if (game->terrain.query_ground)
        AdventurePlayer_TickWorld(&game->player, input, camera_basis, game->terrain);
    else
        AdventurePlayer_TickGroundPilot(&game->player, input, camera_basis);
    ++game->tick_count;
    return true;
}
void AdventureGame_Shutdown(AdventureGame *game)
{
    if (!game) return;
    game->phase = ADVENTURE_PHASE_STOPPED;
}
