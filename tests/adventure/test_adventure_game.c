/* Pure C portable action-mode tests: no commercial assets, SDL or KOS. */
#include "../../source/sdl/src/adventure/adventure_game.h"
#include <assert.h>
#include <stdint.h>
#include <limits.h>

static const AdventureBasis kEast = {ADV_FP_ONE, 0, 0, ADV_FP_ONE};

static void test_lifecycle(void)
{
    AdventureGame game = {0};
    AdventureRawInput none = {0};
    assert(!AdventureGame_Tick(&game, none, kEast));
    AdventureGame_Init(&game);
    assert(game.phase == ADVENTURE_PHASE_RUNNING);
    for (uint32_t i = 0; i < 10000u; ++i)
        assert(AdventureGame_Tick(&game, none, kEast));
    assert(game.tick_count == 10000u);
    assert(game.player.pos.x == 0 && game.player.pos.z == 0);
    AdventureGame_Shutdown(&game);
    assert(!AdventureGame_Tick(&game, none, kEast));
    assert(game.tick_count == 10000u);
    AdventureGame_Init(&game);
    assert(game.tick_count == 0);
    assert(!AdventureGame_Tick(0, none, kEast));
}
static void test_coordinates(void)
{
    AdventureVec3 p = AdventureMath_FromLegacy(12345, -8192, -54321);
    assert(p.x == 12345 && p.y == 8192 && p.z == -54321);
    int32_t x = 0, y = 0, z = 0;
    AdventureMath_ToLegacy(p, &x, &y, &z);
    assert(x == 12345 && y == -8192 && z == -54321);
    assert(AdventureMath_ClampI32((int64_t)INT32_MAX + 1) == INT32_MAX);
    assert(AdventureMath_ClampI32((int64_t)INT32_MIN - 1) == INT32_MIN);
    p = AdventureMath_FromLegacy(0, INT32_MIN, 0);
    assert(p.y == INT32_MAX); /* saturate, never signed overflow */
    AdventureBasis east = AdventureMath_CameraBasis(0, 0, -10 * ADV_FP_ONE, 0);
    assert(east.forward_x == ADV_FP_ONE && east.forward_z == 0);
    assert(east.right_x == 0 && east.right_z == ADV_FP_ONE);
    AdventureBasis fallback = AdventureMath_CameraBasis(0, 0, 0, 0);
    assert(fallback.forward_x == ADV_FP_ONE && fallback.right_z == ADV_FP_ONE);
}
static void test_input(void)
{
    AdventureInputHistory history = {0};
    AdventureRawInput raw = {0};
    raw.move_x = 200;
    AdventureInput out = AdventureInput_Sample(&history, raw);
    assert(out.move_x == 0 && out.move_y == 0);
    raw.move_x = 32767;
    raw.move_y = 32767; /* diagonal should not exceed full analog magnitude */
    raw.buttons = ADV_ACTION_JUMP;
    out = AdventureInput_Sample(&history, raw);
    assert(out.move_x > 0 && out.move_y > 0);
    int32_t length2 = (int32_t)out.move_x * out.move_x +
                      (int32_t)out.move_y * out.move_y;
    assert(length2 <= (int32_t)ADV_FP_ONE * ADV_FP_ONE + 9000);
    assert(out.pressed == ADV_ACTION_JUMP && out.held == ADV_ACTION_JUMP);
    out = AdventureInput_Sample(&history, raw);
    assert(out.pressed == 0 && out.held == ADV_ACTION_JUMP);
    raw.buttons = 0;
    out = AdventureInput_Sample(&history, raw);
    assert(out.released == ADV_ACTION_JUMP);
    AdventureInput_Reset(&history);
    assert(history.previous_buttons == 0);
}
static void test_movement(void)
{
    AdventureGame game;
    AdventureGame_Init(&game);
    AdventureVec3 spawn = {100 * ADV_FP_ONE, 20 * ADV_FP_ONE,
                           -30 * ADV_FP_ONE};
    AdventureGame_SetSpawn(&game, spawn, 0);
    AdventureRawInput raw = {0};
    raw.move_y = 32767;
    for (int i = 0; i < 120; ++i)
        assert(AdventureGame_Tick(&game, raw, kEast));
    assert(game.player.pos.x > spawn.x && game.player.pos.z == spawn.z);
    assert(game.player.pos.y == spawn.y);
    assert(game.player.velocity.x <= 3 * ADV_FP_ONE);
    assert(game.player.motion == ADV_MOTION_MOVING);
    int32_t old_x = game.player.pos.x;
    raw.move_y = 0;
    for (int i = 0; i < 25; ++i)
        assert(AdventureGame_Tick(&game, raw, kEast));
    assert(game.player.velocity.x == 0);
    assert(game.player.pos.x >= old_x);
    assert(game.player.motion == ADV_MOTION_IDLE);
    raw.move_y = -32767;
    for (int i = 0; i < 20; ++i)
        assert(AdventureGame_Tick(&game, raw, kEast));
    assert(game.player.velocity.x < 0);
    assert(game.player.facing == 2048u);
    raw.move_y = 32767; raw.move_x = 32767;
    for (int i = 0; i < 50; ++i)
        assert(AdventureGame_Tick(&game, raw, kEast));
    int64_t vx = game.player.velocity.x, vz = game.player.velocity.z;
    assert(vx * vx + vz * vz <=
           (int64_t)(3 * ADV_FP_ONE) * (3 * ADV_FP_ONE));
}
static void test_repeatability(void)
{
    AdventureGame a, b;
    AdventureGame_Init(&a);
    AdventureGame_Init(&b);
    AdventureRawInput raw = {0};
    for (int i = 0; i < 2048; i++) {
        raw.move_y = (i % 41 < 29) ? 32767 : -32767;
        raw.move_x = (i % 7 < 2) ? 13000 : -13000;
        raw.buttons = (i % 11 < 3) ? ADV_ACTION_JUMP : 0;
        assert(AdventureGame_Tick(&a, raw, kEast));
        assert(AdventureGame_Tick(&b, raw, kEast));
        assert(a.player.pos.x == b.player.pos.x);
        assert(a.player.pos.y == b.player.pos.y);
        assert(a.player.pos.z == b.player.pos.z);
        assert(a.player.velocity.x == b.player.velocity.x);
        assert(a.player.velocity.z == b.player.velocity.z);
        assert(a.player.facing == b.player.facing);
    }
}
int main(void)
{
    test_lifecycle();
    test_coordinates();
    test_input();
    test_movement();
    test_repeatability();
    return 0;
}
