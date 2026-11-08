/* Pure C test; no Sonic R game assets, SDL or Dreamcast SDK necessary. */
#include "../../source/sdl/src/adventure/adventure_game.h"

#include <assert.h>
#include <stdint.h>

int main(void)
{
    AdventureGame game = {0};
    assert(!AdventureGame_Tick(&game));
    AdventureGame_Init(&game);
    assert(game.phase == ADVENTURE_PHASE_RUNNING);
    assert(game.tick_count == 0u);
    for (uint32_t i = 0; i < 10000u; ++i)
        assert(AdventureGame_Tick(&game));
    assert(game.tick_count == 10000u);
    AdventureGame_Shutdown(&game);
    assert(game.phase == ADVENTURE_PHASE_STOPPED);
    assert(!AdventureGame_Tick(&game));
    assert(game.tick_count == 10000u);
    AdventureGame_Init(&game);
    assert(game.tick_count == 0u);
    assert(game.phase == ADVENTURE_PHASE_RUNNING);
    assert(!AdventureGame_Tick(0));
    return 0;
}
