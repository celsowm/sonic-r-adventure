/* Pure C portable action-mode tests: no commercial assets, SDL or KOS. */
#include "../../source/sdl/src/adventure/adventure_game.h"
#include "../../source/sdl/src/adventure/adventure_vertical.h"
#include "../../source/sdl/src/adventure/adventure_capsule.h"
#include "../../source/sdl/src/adventure/adventure_slide.h"
#include "../../source/sdl/src/adventure/adventure_solver.h"
#include "../../source/sdl/src/adventure/adventure_slope.h"
#include "../../source/sdl/src/adventure/adventure_traction.h"
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
    int32_t cell=-1;
    assert(AdventureMath_GridCell(-10*ADV_FP_ONE,-10,5,32,&cell));
    assert(cell==0);
    /* Fractional Q12 coordinate just OUTSIDE a negative world origin. */
    assert(!AdventureMath_GridCell(-10*ADV_FP_ONE-1,-10,5,32,&cell));
    assert(AdventureMath_GridCell(-5*ADV_FP_ONE-1,-10,5,32,&cell));
    assert(cell==0);
    assert(AdventureMath_GridCell(-5*ADV_FP_ONE,-10,5,32,&cell));
    assert(cell==1);
    assert(!AdventureMath_GridCell(150*ADV_FP_ONE,-10,5,32,&cell));
    assert(!AdventureMath_GridCell(0,-10,0,32,&cell));
    assert(!AdventureMath_GridCell(0,-10,5,32,0));
    p = AdventureMath_FromLegacy(0, INT32_MIN, 0);
    assert(p.y == INT32_MAX); /* saturate, never signed overflow */
    AdventureBasis east = AdventureMath_CameraBasis(0, 0, -10 * ADV_FP_ONE, 0);
    assert(east.forward_x == ADV_FP_ONE && east.forward_z == 0);
    assert(east.right_x == 0 && east.right_z == ADV_FP_ONE);
    AdventureBasis fallback = AdventureMath_CameraBasis(0, 0, 0, 0);
    assert(fallback.forward_x == ADV_FP_ONE && fallback.right_z == ADV_FP_ONE);
}
static void test_integer_heading(void)
{
    assert(AdventureMath_HeadingXZ(100, 0) == 0);
    assert(AdventureMath_HeadingXZ(0, 100) == 1024);
    assert(AdventureMath_HeadingXZ(-100, 0) == 2048);
    assert(AdventureMath_HeadingXZ(0, -100) == 3072);
    assert(AdventureMath_HeadingXZ(100, 100) == 512);
    assert(AdventureMath_HeadingXZ(-100, 100) == 1536);
    assert(AdventureMath_HeadingXZ(-100, -100) == 2560);
    assert(AdventureMath_HeadingXZ(100, -100) == 3584);
    assert(AdventureMath_HeadingXZ(INT32_MIN, 0) == 2048);
    assert(AdventureMath_IsqrtU64(0) == 0u);
    assert(AdventureMath_IsqrtU64(144) == 12u);
    assert(AdventureMath_IsqrtU64(145) == 12u);
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

/* Height query fixture: zero-height ground, ramp, two overlapping floors,
 * and a ledge. All positions/heights use Q20.12. */
static bool test_world_ground(void *context, int32_t x, int32_t z,
                              int32_t max_height, AdventureGroundHit *out)
{
    (void)context;
    (void)z;
    int32_t unit = x / ADV_FP_ONE;
    int32_t candidates[2] = {0, 0};
    int count = 0;
    if (unit < 0 || unit >= 45) return false;
    if (unit < 10) candidates[count++] = 0;
    else if (unit < 20) candidates[count++] = (unit - 10) * ADV_FP_ONE;
    else if (unit < 30) candidates[count++] = 10 * ADV_FP_ONE;
    else if (unit < 40) {
        candidates[count++] = 0;
        candidates[count++] = 20 * ADV_FP_ONE;
    } else candidates[count++] = 0;
    bool found = false;
    int32_t best = INT32_MIN;
    for (int i = 0; i < count; ++i) {
        if (candidates[i] <= max_height && candidates[i] > best) {
            best = candidates[i];
            found = true;
        }
    }
    if (found && out) {
        out->height = best;
        out->surface_id = best == 0 ? 11 : best / ADV_FP_ONE + 11;
        out->layer = 1;
    }
    return found;
}
static void test_world_spawn_and_ramp(void)
{
    AdventureGame game;
    AdventureGame_Init(&game);
    AdventureTerrain terrain = {test_world_ground, 0, 0, 0, 0,0};
    AdventureGame_SetTerrain(&game, terrain);
    AdventureVec3 spawn = {5 * ADV_FP_ONE, 0, 0};
    AdventureGame_SetSpawn(&game, spawn, 0);
    assert(game.player.grounded);
    assert(game.player.pos.y == 0);
    AdventureRawInput input = {0};
    input.move_y = 32767;
    for (int i = 0; i < 8; ++i)
        assert(AdventureGame_Tick(&game, input, kEast));
    assert(game.player.pos.x > spawn.x);
    assert(game.player.grounded);
    assert(game.player.pos.y >= 0);
    AdventureGame_Shutdown(&game);
}
static void test_jump_and_landing(void)
{
    AdventureGame game;
    AdventureGame_Init(&game);
    AdventureTerrain terrain = {test_world_ground, 0, 0, 0, 0,0};
    AdventureGame_SetTerrain(&game, terrain);
    AdventureVec3 spawn = {5 * ADV_FP_ONE, 0, 0};
    AdventureGame_SetSpawn(&game, spawn, 0);
    AdventureRawInput input = {0};
    input.buttons = ADV_ACTION_JUMP;
    assert(AdventureGame_Tick(&game, input, kEast));
    assert(!game.player.grounded);
    assert(game.player.velocity.y > 0);
    assert(game.player.pos.y > 0);
    input.buttons = 0;
    bool landed = false;
    for (int i = 0; i < 100; ++i) {
        assert(AdventureGame_Tick(&game, input, kEast));
        if (game.player.grounded) { landed = true; break; }
    }
    assert(landed);
    assert(game.player.pos.y == 0);
    assert(game.player.velocity.y == 0);
    /* Held button must not automatically initiate a second jump. */
    input.buttons = ADV_ACTION_JUMP;
    assert(AdventureGame_Tick(&game, input, kEast));
    assert(!game.player.grounded);
    assert(AdventureGame_Tick(&game, input, kEast));
    assert(!game.player.grounded);
}
static void test_stacked_floor_and_ledge(void)
{
    AdventureGame game;
    AdventureGame_Init(&game);
    AdventureTerrain terrain = {test_world_ground, 0, 0, 0, 0,0};
    AdventureGame_SetTerrain(&game, terrain);
    AdventureVec3 lower = {35 * ADV_FP_ONE, 0, 0};
    AdventureGame_SetSpawn(&game, lower, 0);
    assert(game.player.grounded && game.player.pos.y == 0);
    AdventureVec3 upper = {35 * ADV_FP_ONE, 20 * ADV_FP_ONE, 0};
    AdventureGame_SetSpawn(&game, upper, 0);
    assert(game.player.grounded && game.player.pos.y == 20 * ADV_FP_ONE);
    /* No floor beyond x=45, and falling never invents a zero-height hit. */
    AdventureVec3 edge = {44 * ADV_FP_ONE, 0, 0};
    AdventureGame_SetSpawn(&game, edge, 0);
    AdventureRawInput input = {0};
    input.move_y = 32767;
    bool fell = false;
    for (int i = 0; i < 15; i++) {
        assert(AdventureGame_Tick(&game, input, kEast));
        if (!game.player.grounded) { fell = true; break; }
    }
    assert(fell);
    assert(game.player.coyote_ticks > 0);
    input.buttons = ADV_ACTION_JUMP;
    assert(AdventureGame_Tick(&game, input, kEast));
    assert(game.player.velocity.y > 0);
}
static void test_world_replay(void)
{
    AdventureGame a, b;
    AdventureGame_Init(&a);
    AdventureGame_Init(&b);
    AdventureTerrain terrain = {test_world_ground, 0, 0, 0, 0,0};
    AdventureGame_SetTerrain(&a, terrain);
    AdventureGame_SetTerrain(&b, terrain);
    AdventureVec3 start = {5 * ADV_FP_ONE, 0, 0};
    AdventureGame_SetSpawn(&a, start, 0);
    AdventureGame_SetSpawn(&b, start, 0);
    AdventureRawInput input = {0};
    for (int i = 0; i < 400; ++i) {
        input.move_y = (i % 45 < 22) ? 32767 : -32767;
        input.buttons = (i % 55 == 0) ? ADV_ACTION_JUMP : 0;
        assert(AdventureGame_Tick(&a, input, kEast));
        assert(AdventureGame_Tick(&b, input, kEast));
        assert(a.player.pos.x == b.player.pos.x);
        assert(a.player.pos.y == b.player.pos.y);
        assert(a.player.pos.z == b.player.pos.z);
        assert(a.player.velocity.y == b.player.velocity.y);
        assert(a.player.grounded == b.player.grounded);
        assert(a.player.surface_id == b.player.surface_id);
    }
}


/* Synthetic vertical walls for collision tests: x = 10, z = 10. */
static bool test_wall_x(void *context, AdventureVec3 from, AdventureVec3 to,
                        int32_t radius, int32_t height, int32_t layer,
                        AdventureWallHit *out)
{
    (void)context;
    (void)height;
    (void)layer;
    AdventureVec3 a = {10*ADV_FP_ONE,0,-20*ADV_FP_ONE};
    AdventureVec3 b = {10*ADV_FP_ONE,0, 20*ADV_FP_ONE};
    return AdventureCollision_SweepDiscSegment(from,to,a,b,radius,out);
}
static bool test_wall_corner(void *context, AdventureVec3 from, AdventureVec3 to,
                             int32_t radius, int32_t height, int32_t layer,
                             AdventureWallHit *out)
{
    AdventureWallHit x, z;
    bool hit_x = test_wall_x(context, from, to, radius, height, layer, &x);
    AdventureVec3 a = {-20*ADV_FP_ONE,0,10*ADV_FP_ONE};
    AdventureVec3 b = {20*ADV_FP_ONE,0,10*ADV_FP_ONE};
    bool hit_z = AdventureCollision_SweepDiscSegment(from,to,a,b,radius,&z);
    if (!hit_x && !hit_z) return false;
    *out = (!hit_x || (hit_z && z.fraction < x.fraction)) ? z : x;
    return true;
}
static void test_segment_sweeps(void)
{
    AdventureVec3 from = {0,0,0};
    AdventureVec3 to = {20*ADV_FP_ONE,0,0};
    AdventureVec3 a = {10*ADV_FP_ONE,0,-20*ADV_FP_ONE};
    AdventureVec3 b = {10*ADV_FP_ONE,0, 20*ADV_FP_ONE};
    AdventureWallHit hit;
    assert(AdventureCollision_SweepDiscSegment(from,to,a,b,
                                                ADV_FP_ONE,&hit));
    assert(hit.fraction > 1700 && hit.fraction < 1950);
    assert(hit.normal_x < -3500);
    assert(hit.normal_z > -100 && hit.normal_z < 100);
    /* Crossing the wall backwards also blocks movement, with flipped normal. */
    assert(AdventureCollision_SweepDiscSegment(to,from,a,b,
                                                ADV_FP_ONE,&hit));
    assert(hit.normal_x > 3500);
    /* Moving away from a touching wall is allowed, with no sticky contact. */
    from.x = 9*ADV_FP_ONE;
    to.x = 0;
    assert(!AdventureCollision_SweepDiscSegment(from,to,a,b,
                                                 ADV_FP_ONE,&hit));
    /* Near a segment endpoint, a miss cannot become an infinite-line hit. */
    a.z = 5*ADV_FP_ONE;
    b.z = 15*ADV_FP_ONE;
    from.x = 0;
    to.x = 20*ADV_FP_ONE;
    assert(!AdventureCollision_SweepDiscSegment(from,to,a,b,
                                                 ADV_FP_ONE,&hit));
}
static void test_wall_sliding(void)
{
    AdventureVec3 position = {0,0,0};
    AdventureVec3 velocity = {20*ADV_FP_ONE,0,6*ADV_FP_ONE};
    AdventureCollision_MoveAndSlide(&position,&velocity,test_wall_x,0,
                                     ADV_FP_ONE,4*ADV_FP_ONE,0);
    assert(position.x <= 10*ADV_FP_ONE);
    assert(position.x >= 8*ADV_FP_ONE);
    assert(position.z > 5*ADV_FP_ONE);
    assert(velocity.x == position.x);
    assert(velocity.z == position.z);

    position.x=0; position.z=0;
    velocity.x=20*ADV_FP_ONE; velocity.z=20*ADV_FP_ONE;
    AdventureCollision_MoveAndSlide(&position,&velocity,test_wall_corner,0,
                                     ADV_FP_ONE,4*ADV_FP_ONE,0);
    assert(position.x <= 10*ADV_FP_ONE && position.x >= 7*ADV_FP_ONE);
    assert(position.z <= 10*ADV_FP_ONE && position.z >= 7*ADV_FP_ONE);
}
/* Regression from real .TER height sign: gated wall vertex Y is
 * negative-up, whereas Adventure uses positive-up. Gate top varies along
 * the segment, so endpoint-only approximations produce false collisions. */
static void test_legacy_wall_height_gate(void)
{
    AdventureVec3 a={10*ADV_FP_ONE,0,0};
    AdventureVec3 b={10*ADV_FP_ONE,0,10*ADV_FP_ONE};
    AdventureVec3 from={0,0,5*ADV_FP_ONE};
    AdventureVec3 to={20*ADV_FP_ONE,0,5*ADV_FP_ONE};
    AdventureWallHit hit;
    assert(AdventureCollision_SweepDiscSegment(from,to,a,b,
                                                ADV_FP_ONE,&hit));
    /* Legacy edge heights -2 and -10 mean canonical top +2..+10.
     * At the midpoint the actual wall top is +6 world units. */
    assert(!AdventureCollision_AboveLegacyWallTop(
        5*ADV_FP_ONE,from,to,a,b,-2,-10,hit.fraction));
    assert(!AdventureCollision_AboveLegacyWallTop(
        6*ADV_FP_ONE,from,to,a,b,-2,-10,hit.fraction));
    assert(AdventureCollision_AboveLegacyWallTop(
        7*ADV_FP_ONE,from,to,a,b,-2,-10,hit.fraction));
    from.z=0;to.z=0;
    assert(AdventureCollision_SweepDiscSegment(from,to,a,b,
                                                ADV_FP_ONE,&hit));
    assert(AdventureCollision_AboveLegacyWallTop(
        3*ADV_FP_ONE,from,to,a,b,-2,-10,hit.fraction));
    from.z=10*ADV_FP_ONE;to.z=10*ADV_FP_ONE;
    assert(AdventureCollision_SweepDiscSegment(from,to,a,b,
                                                ADV_FP_ONE,&hit));
    assert(!AdventureCollision_AboveLegacyWallTop(
        3*ADV_FP_ONE,from,to,a,b,-2,-10,hit.fraction));
    /* Heights around -1,000 occur in the provided stage geometry and
     * still fit signed Q20.12 without truncating. */
    assert(!AdventureCollision_AboveLegacyWallTop(
        999*ADV_FP_ONE,from,to,a,b,-1000,-1000,hit.fraction));
    assert(AdventureCollision_AboveLegacyWallTop(
        1001*ADV_FP_ONE,from,to,a,b,-1000,-1000,hit.fraction));
}

static void test_wall_gameplay(void)
{
    AdventureGame game;
    AdventureGame_Init(&game);
    AdventureTerrain terrain = {test_world_ground, 0, test_wall_x, 0, 0,0};
    AdventureGame_SetTerrain(&game, terrain);
    AdventureVec3 spawn = {5*ADV_FP_ONE,0,0};
    AdventureGame_SetSpawn(&game,spawn,0);
    AdventureRawInput input = {0};
    input.move_y=32767;
    for (int i=0;i<120;i++)
        assert(AdventureGame_Tick(&game,input,kEast));
    assert(game.player.pos.x < 10*ADV_FP_ONE);
    assert(game.player.pos.x > 8*ADV_FP_ONE);
    assert(game.player.grounded);
    input.move_y=0;
    input.move_x=32767;
    int32_t z=game.player.pos.z;
    for (int i=0;i<12;i++)
        assert(AdventureGame_Tick(&game,input,kEast));
    assert(game.player.pos.z > z);
    assert(game.player.pos.x < 10*ADV_FP_ONE);
}


/*
 * Synthetic multi-level .TER footprint. The bottom floor is at 0, and
 * overhanging walkable platforms are at 20 and 30 units (two-sided surfaces).
 * These callbacks use the SAME semantic contract as the production adapter.
 */
static bool stack_surface(void *context, int32_t x, int32_t z,
                          int32_t threshold, AdventureGroundHit *out,
                          bool overhead)
{
    (void)context;
    (void)z;
    if (x < 0 || x >= 40 * ADV_FP_ONE || !out) return false;
    const int32_t heights[3] = {0, 20*ADV_FP_ONE, 30*ADV_FP_ONE};
    bool found = false;
    int32_t best = 0;
    int idx = -1;
    for (int i=0;i<3;i++) {
        int32_t h = heights[i];
        if ((overhead ? h >= threshold : h <= threshold) &&
            (!found || (overhead ? h < best : h > best))) {
            best=h;
            idx=i;
            found=true;
        }
    }
    if (found) {
        out->height=best;
        out->surface_id=idx+100;
        out->layer=idx;
    }
    return found;
}
static bool stack_ground(void *context, int32_t x, int32_t z,
                         int32_t maximum, AdventureGroundHit *out)
{
    return stack_surface(context,x,z,maximum,out,false);
}
static bool stack_overhead(void *context, int32_t x, int32_t z,
                           int32_t minimum, AdventureGroundHit *out)
{
    return stack_surface(context,x,z,minimum,out,true);
}
static void test_overhead_query_selection(void)
{
    AdventureGroundHit h;
    assert(stack_ground(0,5*ADV_FP_ONE,0,25*ADV_FP_ONE,&h));
    assert(h.height==20*ADV_FP_ONE && h.surface_id==101);
    assert(stack_overhead(0,5*ADV_FP_ONE,0,4*ADV_FP_ONE,&h));
    assert(h.height==20*ADV_FP_ONE && h.surface_id==101);
    assert(stack_overhead(0,5*ADV_FP_ONE,0,21*ADV_FP_ONE,&h));
    assert(h.height==30*ADV_FP_ONE && h.surface_id==102);
    assert(!stack_overhead(0,5*ADV_FP_ONE,0,31*ADV_FP_ONE,&h));
    assert(stack_overhead(0,5*ADV_FP_ONE,0,0,&h));
    assert(h.height==0 && h.surface_id==100);
    assert(!stack_overhead(0,-ADV_FP_ONE,0,0,&h));
}
static void test_jump_head_hits_underside(void)
{
    AdventureGame game;
    AdventureGame_Init(&game);
    AdventureTerrain terrain={stack_ground,0,0,stack_overhead,0,0};
    AdventureGame_SetTerrain(&game,terrain);
    AdventureVec3 spawn={5*ADV_FP_ONE,0,0};
    AdventureGame_SetSpawn(&game,spawn,0);
    assert(game.player.grounded);
    AdventureRawInput raw={0};
    raw.buttons=ADV_ACTION_JUMP;
    bool bonked=false;
    for (int i=0;i<20;i++) {
        assert(AdventureGame_Tick(&game,raw,kEast));
        if (!game.player.grounded && game.player.velocity.y==0) {
            bonked=true;
            break;
        }
        raw.buttons=0;
    }
    assert(bonked);
    /* Sonic's 4-unit head fits below the 20-unit platform with skin. */
    assert(game.player.pos.y > 15*ADV_FP_ONE);
    assert(game.player.pos.y < 16*ADV_FP_ONE);
    assert(game.player.motion==ADV_MOTION_FALL);
    /* Do not teleport onto the platform overhead on the next frame.
     * Falling must land on the lower zero-height floor. */
    raw.buttons=0;
    bool landed=false;
    for (int i=0;i<120;i++) {
        assert(AdventureGame_Tick(&game,raw,kEast));
        if (game.player.grounded) {landed=true;break;}
    }
    assert(landed);
    assert(game.player.pos.y==0 && game.player.surface_id==100);
}
static void test_jump_from_upper_platform(void)
{
    AdventureGame game;
    AdventureGame_Init(&game);
    AdventureTerrain terrain={stack_ground,0,0,stack_overhead,0,0};
    AdventureGame_SetTerrain(&game,terrain);
    AdventureVec3 spawn={5*ADV_FP_ONE,20*ADV_FP_ONE,0};
    AdventureGame_SetSpawn(&game,spawn,0);
    assert(game.player.grounded && game.player.surface_id==101);
    AdventureRawInput raw={0};
    raw.buttons=ADV_ACTION_JUMP;
    bool bonked=false;
    for (int i=0;i<20;i++) {
        assert(AdventureGame_Tick(&game,raw,kEast));
        if (!game.player.grounded && game.player.velocity.y==0) {
            bonked=true; break;
        }
        raw.buttons=0;
    }
    assert(bonked);
    assert(game.player.pos.y>25*ADV_FP_ONE);
    assert(game.player.pos.y<26*ADV_FP_ONE);
    /* The floor under feet at Y=20 must not block the head at Y=24. */
    assert(game.player.surface_id==-1);
}
static void test_unobstructed_jump_still_works(void)
{
    AdventureGame game;
    AdventureGame_Init(&game);
    AdventureTerrain terrain={test_world_ground,0,0,stack_overhead,0,0};
    AdventureGame_SetTerrain(&game,terrain);
    /* Beyond x=40: no overhead. The ground fixture continues to 45. */
    AdventureVec3 spawn={42*ADV_FP_ONE,0,0};
    AdventureGame_SetSpawn(&game,spawn,0);
    AdventureRawInput raw={0};
    raw.buttons=ADV_ACTION_JUMP;
    for (int i=0;i<10;i++) {
        assert(AdventureGame_Tick(&game,raw,kEast));
        raw.buttons=0;
    }
    assert(game.player.pos.y>16*ADV_FP_ONE);
    assert(game.player.velocity.y>0);
}
static void test_head_bonk_determinism(void)
{
    AdventureGame a,b;
    AdventureGame_Init(&a); AdventureGame_Init(&b);
    AdventureTerrain terrain={stack_ground,0,0,stack_overhead,0,0};
    AdventureGame_SetTerrain(&a,terrain);
    AdventureGame_SetTerrain(&b,terrain);
    AdventureVec3 spawn={5*ADV_FP_ONE,0,0};
    AdventureGame_SetSpawn(&a,spawn,0);
    AdventureGame_SetSpawn(&b,spawn,0);
    AdventureRawInput raw={0};
    for(int i=0;i<300;i++){
        raw.buttons=(i%61==0)?ADV_ACTION_JUMP:0;
        assert(AdventureGame_Tick(&a,raw,kEast));
        assert(AdventureGame_Tick(&b,raw,kEast));
        assert(a.player.pos.y==b.player.pos.y);
        assert(a.player.velocity.y==b.player.velocity.y);
        assert(a.player.grounded==b.player.grounded);
        assert(a.player.surface_id==b.player.surface_id);
    }
}


/* A thin platform crossed diagonally inside ONE fixed tick. The old
 * final-X/Z-only floor and head checks returned no hit at x=10. */
static bool thin_plane(void *context,int32_t x,int32_t z,int32_t threshold,
                       AdventureGroundHit *out,bool ceiling)
{
    (void)context;
    if (!out || x<5*ADV_FP_ONE || x>=5*ADV_FP_ONE+ADV_FP_ONE/2 ||
        z<0 || z>=4*ADV_FP_ONE) return false;
    int32_t h=ceiling?20*ADV_FP_ONE:10*ADV_FP_ONE;
    if (ceiling ? h<threshold : h>threshold) return false;
    out->height=h;
    out->surface_id=42;
    out->layer=2;
    return true;
}
static bool thin_ground(void *ctx,int32_t x,int32_t z,int32_t lim,
                        AdventureGroundHit *out)
{
    return thin_plane(ctx,x,z,lim,out,false);
}
static bool thin_roof(void *ctx,int32_t x,int32_t z,int32_t lim,
                      AdventureGroundHit *out)
{
    return thin_plane(ctx,x,z,lim,out,true);
}
static void test_diagonal_terrain_sweeps(void)
{
    AdventureTerrain t={thin_ground,0,0,thin_roof,0,0};
    AdventureVerticalContact c;
    AdventureVec3 from={0,14*ADV_FP_ONE,ADV_FP_ONE};
    AdventureVec3 to={10*ADV_FP_ONE,4*ADV_FP_ONE,ADV_FP_ONE};
    assert(AdventureVertical_Sweep(t,from,to,4*ADV_FP_ONE,ADV_FP_ONE,false,&c));
    assert(c.surface.surface_id==42);
    assert(c.position.x>=5*ADV_FP_ONE &&
           c.position.x<5*ADV_FP_ONE+ADV_FP_ONE/2);
    assert(c.position.y==10*ADV_FP_ONE);
    assert(c.fraction<ADV_FP_ONE);
    from.y=10*ADV_FP_ONE;
    to.y=30*ADV_FP_ONE;
    assert(AdventureVertical_Sweep(t,from,to,4*ADV_FP_ONE,ADV_FP_ONE,true,&c));
    assert(c.surface.surface_id==42);
    assert(c.position.y>15*ADV_FP_ONE &&
           c.position.y<16*ADV_FP_ONE);
    assert(c.position.x>=5*ADV_FP_ONE &&
           c.position.x<5*ADV_FP_ONE+ADV_FP_ONE/2);
    /* Querying only at final X/Z would return no floor or ceiling. */
    AdventureGroundHit ignored;
    assert(!thin_ground(0,to.x,to.z,to.y,&ignored));
    assert(!thin_roof(0,to.x,to.z,to.y,&ignored));
}
static void test_diagonal_sweep_no_false_contact(void)
{
    AdventureTerrain t={thin_ground,0,0,thin_roof,0,0};
    AdventureVerticalContact c;
    AdventureVec3 from={0,14*ADV_FP_ONE,-2*ADV_FP_ONE};
    AdventureVec3 to={10*ADV_FP_ONE,4*ADV_FP_ONE,-2*ADV_FP_ONE};
    assert(!AdventureVertical_Sweep(t,from,to,4*ADV_FP_ONE,ADV_FP_ONE,false,&c));
    from.y=10*ADV_FP_ONE;
    to.y=30*ADV_FP_ONE;
    assert(!AdventureVertical_Sweep(t,from,to,4*ADV_FP_ONE,ADV_FP_ONE,true,&c));
    /* A roof starting below an already elevated head is not an overhead
     * contact: it should be blocked by side geometry, not snapped below. */
    from.z=ADV_FP_ONE;to.z=ADV_FP_ONE;
    from.y=18*ADV_FP_ONE;
    to.y=32*ADV_FP_ONE;
    assert(!AdventureVertical_Sweep(t,from,to,4*ADV_FP_ONE,ADV_FP_ONE,true,&c));
}
static void test_diagonal_sweep_replay(void)
{
    AdventureTerrain t={thin_ground,0,0,thin_roof,0,0};
    AdventureVerticalContact a,b;
    AdventureVec3 from={0,10*ADV_FP_ONE,ADV_FP_ONE};
    AdventureVec3 to={10*ADV_FP_ONE,30*ADV_FP_ONE,ADV_FP_ONE};
    for(int i=0;i<100;i++){
        assert(AdventureVertical_Sweep(t,from,to,4*ADV_FP_ONE,ADV_FP_ONE,true,&a));
        assert(AdventureVertical_Sweep(t,from,to,4*ADV_FP_ONE,ADV_FP_ONE,true,&b));
        assert(a.position.x==b.position.x);
        assert(a.position.y==b.position.y);
        assert(a.position.z==b.position.z);
        assert(a.fraction==b.fraction);
    }
}

/* Full tick regression: Sonic starts outside a half-unit roof strip, passes
 * through it mid-tick and finishes beyond it. Only a trajectory query can
 * detect the head impact; the legacy endpoint query cannot. */
static bool roof_strip_terrain(void *context,int32_t x,int32_t z,
                               int32_t threshold,AdventureGroundHit *out,
                               bool overhead)
{
    (void)context;
    (void)z;
    int32_t heights[2]={0,20*ADV_FP_ONE};
    int count=(x>=5*ADV_FP_ONE &&
               x<5*ADV_FP_ONE+ADV_FP_ONE/2)?2:1;
    bool found=false;
    int32_t best=0;
    int idx=-1;
    for(int i=0;i<count;i++){
        int32_t h=heights[i];
        if((overhead ? h>=threshold : h<=threshold) &&
           (!found || (overhead ? h<best : h>best))){
            found=true;best=h;idx=i;
        }
    }
    if(found && out){
        out->height=best;
        out->surface_id=100+idx;
        out->layer=1;
    }
    return found;
}
static bool roof_strip_ground(void *ctx,int32_t x,int32_t z,
                              int32_t lim,AdventureGroundHit *out)
{
    return roof_strip_terrain(ctx,x,z,lim,out,false);
}
static bool roof_strip_overhead(void *ctx,int32_t x,int32_t z,
                                int32_t lim,AdventureGroundHit *out)
{
    return roof_strip_terrain(ctx,x,z,lim,out,true);
}
static void test_game_tick_hits_narrow_roof(void)
{
    AdventureGame game;
    AdventureGame_Init(&game);
    AdventureTerrain terrain={roof_strip_ground,0,0,roof_strip_overhead,0,0};
    AdventureGame_SetTerrain(&game,terrain);
    AdventureVec3 spawn={4*ADV_FP_ONE+ADV_FP_ONE/2,
                         15*ADV_FP_ONE,0};
    AdventureGame_SetSpawn(&game,spawn,0);
    assert(!game.player.grounded);
    game.player.velocity.x=3*ADV_FP_ONE;
    game.player.velocity.y=7*ADV_FP_ONE;
    AdventureRawInput raw={0};
    raw.move_y=32767;
    assert(AdventureGame_Tick(&game,raw,kEast));
    /* The unified solver stops vertical ascent at the narrow underside,
     * but allows the rest of the horizontal tick to slide beyond its edge.
     * Old stop-everything behavior left X trapped within the half-unit roof. */
    assert(game.player.pos.x>5*ADV_FP_ONE+ADV_FP_ONE/2);
    assert(game.player.pos.y>15*ADV_FP_ONE &&
           game.player.pos.y<=16*ADV_FP_ONE);
    assert(game.player.velocity.y==0);
    assert(!game.player.grounded);
}

/* The renderer exposes no reliable solid-roof bit. These synthetic faces
 * prove that ONLY authored triangles participate in swept head collision. */
static AdventureRoofTriangle roof(int32_t y,int32_t id)
{
    AdventureRoofTriangle t={
        {0,y,0},{10*ADV_FP_ONE,y,0},{0,y,10*ADV_FP_ONE},id
    };
    return t;
}
static void test_analytic_roof_sweep(void)
{
    AdventureRoofTriangle t=roof(20*ADV_FP_ONE,71);
    AdventureVec3 from={2*ADV_FP_ONE,10*ADV_FP_ONE,2*ADV_FP_ONE};
    AdventureVec3 to={2*ADV_FP_ONE,30*ADV_FP_ONE,2*ADV_FP_ONE};
    AdventureRoofHit hit;
    assert(AdventureRoof_SweepTriangle(from,to,&t,0,&hit));
    assert(hit.fraction==ADV_FP_ONE/2);
    assert(hit.triangle_id==71);
    /* Winding must not change physical solidity of an explicitly picked roof. */
    AdventureVec3 swap=t.b; t.b=t.c; t.c=swap;
    assert(AdventureRoof_SweepTriangle(from,to,&t,0,&hit));
    assert(hit.fraction==ADV_FP_ONE/2);
    from.y=22*ADV_FP_ONE;
    assert(!AdventureRoof_SweepTriangle(from,to,&t,0,&hit));
    from.y=10*ADV_FP_ONE;
    from.x=9*ADV_FP_ONE;from.z=9*ADV_FP_ONE;
    to.x=from.x;to.z=from.z;
    assert(!AdventureRoof_SweepTriangle(from,to,&t,0,&hit));
    to.y=from.y;
    assert(!AdventureRoof_SweepTriangle(from,to,&t,0,&hit));
    /* The true time-of-impact does not depend on velocity substep count. */
    from.x=2*ADV_FP_ONE;from.z=2*ADV_FP_ONE;
    to.x=from.x;to.z=from.z;
    from.y=20*ADV_FP_ONE;to.y=21*ADV_FP_ONE;
    assert(AdventureRoof_SweepTriangle(from,to,&t,0,&hit));
    assert(hit.fraction==0u);
}
static void test_roof_mesh_opt_in_and_priority(void)
{
    AdventureRoofMesh mesh;
    AdventureRoof_Clear(&mesh);
    AdventureRoofHit hit;
    AdventureVec3 from={2*ADV_FP_ONE,10*ADV_FP_ONE,2*ADV_FP_ONE};
    AdventureVec3 to={2*ADV_FP_ONE,30*ADV_FP_ONE,2*ADV_FP_ONE};
    assert(!AdventureRoof_SweepMesh(&mesh,from,to,0,&hit));
    AdventureRoofTriangle a=roof(22*ADV_FP_ONE,22);
    AdventureRoofTriangle b=roof(15*ADV_FP_ONE,15);
    assert(AdventureRoof_Add(&mesh,a));
    assert(AdventureRoof_Add(&mesh,b));
    assert(mesh.count==2);
    assert(AdventureRoof_SweepMesh(&mesh,from,to,0,&hit));
    assert(hit.triangle_id==15);
    assert(hit.fraction==ADV_FP_ONE/4);
    /* Explicitly reject vertical, degenerate or unbounded additions. */
    AdventureRoofTriangle vertical={
        {0,0,0},{0,10*ADV_FP_ONE,0},{0,0,10*ADV_FP_ONE},50
    };
    assert(!AdventureRoof_Add(&mesh,vertical));
    vertical.b=vertical.a;
    assert(!AdventureRoof_Add(&mesh,vertical));
    assert(mesh.count==2);
}
static void test_sloped_roof_and_diagonal_face(void)
{
    AdventureRoofTriangle sloped={
        {3*ADV_FP_ONE,20*ADV_FP_ONE,0},
        {8*ADV_FP_ONE,24*ADV_FP_ONE,0},
        {3*ADV_FP_ONE,20*ADV_FP_ONE,8*ADV_FP_ONE},45
    };
    AdventureVec3 from={0,10*ADV_FP_ONE,2*ADV_FP_ONE};
    AdventureVec3 to={10*ADV_FP_ONE,35*ADV_FP_ONE,2*ADV_FP_ONE};
    AdventureRoofHit hit;
    assert(AdventureRoof_SweepTriangle(from,to,&sloped,0,&hit));
    assert(hit.fraction>ADV_FP_ONE/3);
    assert(hit.fraction<3*ADV_FP_ONE/4);
    assert(hit.triangle_id==45);
    /* Point only intersects the finite roof face mid-path, not at either end. */
    from.z=-5*ADV_FP_ONE;to.z=-5*ADV_FP_ONE;
    assert(!AdventureRoof_SweepTriangle(from,to,&sloped,0,&hit));
}

static void test_roof_edges_and_capacity(void)
{
    AdventureRoofTriangle t=roof(20*ADV_FP_ONE,8);
    AdventureVec3 from={10*ADV_FP_ONE,10*ADV_FP_ONE,0};
    AdventureVec3 to={10*ADV_FP_ONE,30*ADV_FP_ONE,0};
    AdventureRoofHit hit;
    /* An exact triangle corner must not disappear because the Q12 time
     * fraction was rounded during analytic plane intersection. */
    assert(AdventureRoof_SweepTriangle(from,to,&t,0,&hit));
    from.x=11*ADV_FP_ONE;to.x=11*ADV_FP_ONE;
    assert(!AdventureRoof_SweepTriangle(from,to,&t,0,&hit));
    AdventureRoofMesh mesh;
    AdventureRoof_Clear(&mesh);
    for(unsigned i=0;i<ADV_ROOF_MAX_TRIANGLES;i++){
        t.id=(int32_t)i;
        assert(AdventureRoof_Add(&mesh,t));
    }
    assert(mesh.count==ADV_ROOF_MAX_TRIANGLES);
    assert(!AdventureRoof_Add(&mesh,t));
    assert(mesh.count==ADV_ROOF_MAX_TRIANGLES);
    AdventureRoof_Clear(&mesh);
    assert(mesh.count==0);
    assert(!AdventureRoof_SweepMesh(&mesh,from,to,0,&hit));
}

static void test_roof_in_vertical_gameplay(void)
{
    AdventureRoofMesh mesh;
    AdventureRoof_Clear(&mesh);
    AdventureRoofTriangle r=roof(8*ADV_FP_ONE,23);
    assert(AdventureRoof_Add(&mesh,r));
    AdventureTerrain terrain={test_world_ground,&mesh,0,0,AdventureCapsule_SweepMesh,0};
    AdventureGame game;
    AdventureGame_Init(&game);
    AdventureGame_SetTerrain(&game,terrain);
    AdventureVec3 spawn={5*ADV_FP_ONE,0,0};
    AdventureGame_SetSpawn(&game,spawn,0);
    assert(game.player.grounded);
    AdventureRawInput raw={0};
    raw.buttons=ADV_ACTION_JUMP;
    assert(AdventureGame_Tick(&game,raw,kEast));
    assert(!game.player.grounded);
    assert(game.player.velocity.y==0);
    assert(game.player.pos.y>=3*ADV_FP_ONE);
    assert(game.player.pos.y<=4*ADV_FP_ONE);
    /* Head impact should fall back to the original TER ground. */
    raw.buttons=0;
    bool landed=false;
    for(int i=0;i<100;i++){
        assert(AdventureGame_Tick(&game,raw,kEast));
        if(game.player.grounded){landed=true;break;}
    }
    assert(landed && game.player.pos.y==0);
}


/* Swept HEAD SPHERE: tests that the old point-only roof cast could not pass.
 * A radius-one head must hit a finite edge or its vertex even if the head
 * center passes OUTSIDE the triangle footprint throughout the whole tick. */
static void test_roof_sphere_face_edge_and_vertices(void)
{
    AdventureRoofTriangle tri=roof(20*ADV_FP_ONE,401);
    AdventureRoofHit hit;
    AdventureVec3 from={2*ADV_FP_ONE,10*ADV_FP_ONE,2*ADV_FP_ONE};
    AdventureVec3 to={2*ADV_FP_ONE,30*ADV_FP_ONE,2*ADV_FP_ONE};
    /* Point-face crossing occurs at 0.5; the spherical head hits at 0.45. */
    assert(AdventureRoof_SweepTriangle(from,to,&tri,0,&hit));
    assert(hit.fraction==2048);
    assert(AdventureRoof_SweepTriangle(from,to,&tri,ADV_FP_ONE,&hit));
    assert(hit.fraction>1800 && hit.fraction<1900);

    /* Edge AB: center stays 0.5 units outside z=0. */
    from.z=-ADV_FP_ONE/2;to.z=from.z;
    assert(!AdventureRoof_SweepTriangle(from,to,&tri,0,&hit));
    assert(AdventureRoof_SweepTriangle(from,to,&tri,ADV_FP_ONE,&hit));
    assert(hit.fraction>1750 && hit.fraction<2048);
    assert(hit.triangle_id==401);

    /* Vertex A: sphere overlaps vertex despite missing all three edges. */
    from.x=-ADV_FP_ONE/2;to.x=from.x;
    assert(AdventureRoof_SweepTriangle(from,to,&tri,ADV_FP_ONE,&hit));
    assert(hit.fraction>1800 && hit.fraction<2048);
    from.x=-2*ADV_FP_ONE;to.x=from.x;
    from.z=-2*ADV_FP_ONE;to.z=from.z;
    assert(!AdventureRoof_SweepTriangle(from,to,&tri,ADV_FP_ONE,&hit));

    /* Approaching from the upper side never blocks movement upward. */
    from.x=2*ADV_FP_ONE;to.x=from.x;
    from.z=2*ADV_FP_ONE;to.z=from.z;
    from.y=21*ADV_FP_ONE;to.y=30*ADV_FP_ONE;
    assert(!AdventureRoof_SweepTriangle(from,to,&tri,ADV_FP_ONE,&hit));
}
static void test_roof_sphere_not_sticky_on_exit(void)
{
    AdventureRoofTriangle tri=roof(20*ADV_FP_ONE,402);
    AdventureRoofHit hit;
    AdventureVec3 from={2*ADV_FP_ONE,20*ADV_FP_ONE-ADV_FP_ONE/4,
                        -ADV_FP_ONE/2};
    AdventureVec3 to={2*ADV_FP_ONE,20*ADV_FP_ONE-ADV_FP_ONE/8,
                      -2*ADV_FP_ONE};
    /* Start near the edge, then move away horizontally while going up. */
    assert(!AdventureRoof_SweepTriangle(from,to,&tri,ADV_FP_ONE,&hit));
    to.y=30*ADV_FP_ONE;
    to.z=-ADV_FP_ONE/2;
    assert(AdventureRoof_SweepTriangle(from,to,&tri,ADV_FP_ONE,&hit));
    assert(hit.fraction==0);
}
static void test_roof_sphere_priority(void)
{
    AdventureRoofMesh mesh;
    AdventureRoof_Clear(&mesh);
    assert(AdventureRoof_Add(&mesh,roof(22*ADV_FP_ONE,22)));
    assert(AdventureRoof_Add(&mesh,roof(15*ADV_FP_ONE,15)));
    AdventureVec3 from={2*ADV_FP_ONE,10*ADV_FP_ONE,
                        -ADV_FP_ONE/2};
    AdventureVec3 to={2*ADV_FP_ONE,30*ADV_FP_ONE,
                      -ADV_FP_ONE/2};
    AdventureRoofHit hit;
    assert(AdventureRoof_SweepMesh(&mesh,from,to,ADV_FP_ONE,&hit));
    assert(hit.triangle_id==15);
    assert(hit.fraction<ADV_FP_ONE/4);
    /* Larger sphere reaches the edge sooner. */
    uint16_t smaller=hit.fraction;
    assert(AdventureRoof_SweepMesh(&mesh,from,to,2*ADV_FP_ONE,&hit));
    assert(hit.fraction<=smaller);
}
static void test_roof_sphere_game_tick_graze(void)
{
    AdventureRoofMesh mesh;
    AdventureRoof_Clear(&mesh);
    assert(AdventureRoof_Add(&mesh,roof(8*ADV_FP_ONE,406)));
    AdventureTerrain terrain={test_world_ground,&mesh,0,0,
                              AdventureCapsule_SweepMesh,0};
    AdventureGame game;
    AdventureGame_Init(&game);
    AdventureGame_SetTerrain(&game,terrain);
    /* Game's ground mock has no Z restriction, roof is bounded by z>=0. */
    AdventureVec3 spawn={2*ADV_FP_ONE,0,-ADV_FP_ONE/2};
    AdventureGame_SetSpawn(&game,spawn,0);
    assert(game.player.grounded);
    AdventureRawInput raw={0};
    raw.buttons=ADV_ACTION_JUMP;
    assert(AdventureGame_Tick(&game,raw,kEast));
    assert(!game.player.grounded);
    /* Unlike a face-on ceiling collision, a lateral roof-edge graze leaves
     * a nonzero upward tangent. The new slide must REMOVE the normal
     * component without cancelling the entire jump velocity. */
    assert(game.player.velocity.y>0);
    assert(game.player.velocity.y<6*ADV_FP_ONE);
    /* Edge z=-0.5 intersects the unit head sphere at head-center
     * Y=8-sqrt(1-0.5^2) ~= 7.134, hence footY ~= 4.134 minus skin.
     * This is later/higher than direct center-plane contact at footY=4. */
    assert(game.player.pos.y>4*ADV_FP_ONE &&
           game.player.pos.y<5*ADV_FP_ONE);
}


/* Full translated vertical capsule vs selected triangle, including its
 * rounded head, shaft, lower endcap, and finite triangle boundary. */
static void test_capsule_full_body_contacts(void)
{
    AdventureRoofTriangle tri=roof(20*ADV_FP_ONE,501);
    AdventureCapsuleHit hit;
    AdventureVec3 from={2*ADV_FP_ONE,12*ADV_FP_ONE,2*ADV_FP_ONE};
    AdventureVec3 to={2*ADV_FP_ONE,20*ADV_FP_ONE,2*ADV_FP_ONE};
    assert(AdventureCapsule_SweepTriangle(from,to,4*ADV_FP_ONE,
                                          ADV_FP_ONE,&tri,&hit));
    assert(hit.fraction>1850 && hit.fraction<2250);
    assert(hit.triangle_id==501);
    assert(hit.normal.y<0);

    /* Rounded FOOT descending onto the same triangle from above. */
    from.y=22*ADV_FP_ONE;to.y=16*ADV_FP_ONE;
    assert(AdventureCapsule_SweepTriangle(from,to,4*ADV_FP_ONE,
                                          ADV_FP_ONE,&tri,&hit));
    assert(hit.fraction>1100 && hit.fraction<1650);
    assert(hit.normal.y>0);

    /* The shaft straddles Y=20 but never touches with either endcap.
     * Moving sideways from x=-3 into the triangle must hit edge CA. */
    from.x=-3*ADV_FP_ONE;to.x=3*ADV_FP_ONE;
    from.y=17*ADV_FP_ONE;to.y=17*ADV_FP_ONE;
    assert(AdventureCapsule_SweepTriangle(from,to,6*ADV_FP_ONE,
                                          ADV_FP_ONE,&tri,&hit));
    assert(hit.fraction>1000 && hit.fraction<1800);
    assert(hit.normal.x<0);
    assert(hit.normal.y>-1500 && hit.normal.y<1500);

    /* A vertical shaft crossing under its rounded lower sphere
     * collides with a TRIANGLE vertex near x=z=0. */
    from.x=-ADV_FP_ONE/2; to.x=from.x;
    from.z=-ADV_FP_ONE/2; to.z=from.z;
    from.y=10*ADV_FP_ONE;to.y=30*ADV_FP_ONE;
    assert(AdventureCapsule_SweepTriangle(from,to,4*ADV_FP_ONE,
                                          ADV_FP_ONE,&tri,&hit));
    assert(hit.triangle_id==501);

    from.x=-2*ADV_FP_ONE;to.x=from.x;
    from.z=-2*ADV_FP_ONE;to.z=from.z;
    assert(!AdventureCapsule_SweepTriangle(from,to,4*ADV_FP_ONE,
                                           ADV_FP_ONE,&tri,&hit));
}
static void test_capsule_tunnelling_and_escape(void)
{
    AdventureRoofTriangle tri=roof(20*ADV_FP_ONE,502);
    AdventureCapsuleHit hit;
    AdventureVec3 from={-10*ADV_FP_ONE,17*ADV_FP_ONE,
                        2*ADV_FP_ONE};
    AdventureVec3 to={20*ADV_FP_ONE,17*ADV_FP_ONE,
                      2*ADV_FP_ONE};
    /* Both endpoints are beyond the roof bounds; the shaft crosses it. */
    assert(AdventureCapsule_SweepTriangle(from,to,6*ADV_FP_ONE,
                                          ADV_FP_ONE,&tri,&hit));
    assert(hit.fraction>1000 && hit.fraction<2200);
    assert(hit.normal.x<0);
    from.x=-ADV_FP_ONE;to.x=-4*ADV_FP_ONE;
    assert(!AdventureCapsule_SweepTriangle(from,to,6*ADV_FP_ONE,
                                           ADV_FP_ONE,&tri,&hit));
    /* No collision against empty registries or invalid dimensions. */
    AdventureRoofMesh mesh;
    AdventureRoof_Clear(&mesh);
    assert(!AdventureCapsule_SweepMesh(&mesh,from,to,6*ADV_FP_ONE,
                                       ADV_FP_ONE,&hit));
    assert(!AdventureCapsule_SweepTriangle(from,to,0,ADV_FP_ONE,&tri,&hit));
    assert(!AdventureCapsule_SweepTriangle(from,to,ADV_FP_ONE,
                                           ADV_FP_ONE,&tri,&hit));
    assert(!AdventureCapsule_SweepTriangle(from,to,6*ADV_FP_ONE,
                                           0,&tri,&hit));
}
static void test_capsule_first_contact_and_replay(void)
{
    AdventureRoofMesh mesh;
    AdventureRoof_Clear(&mesh);
    assert(AdventureRoof_Add(&mesh,roof(22*ADV_FP_ONE,522)));
    assert(AdventureRoof_Add(&mesh,roof(15*ADV_FP_ONE,515)));
    AdventureVec3 from={2*ADV_FP_ONE,8*ADV_FP_ONE,
                        2*ADV_FP_ONE};
    AdventureVec3 to={2*ADV_FP_ONE,27*ADV_FP_ONE,
                      2*ADV_FP_ONE};
    AdventureCapsuleHit a,b;
    for(int i=0;i<60;i++){
        assert(AdventureCapsule_SweepMesh(&mesh,from,to,4*ADV_FP_ONE,
                                          ADV_FP_ONE,&a));
        assert(AdventureCapsule_SweepMesh(&mesh,from,to,4*ADV_FP_ONE,
                                          ADV_FP_ONE,&b));
        assert(a.triangle_id==515 && a.triangle_id==b.triangle_id);
        assert(a.fraction==b.fraction);
        assert(a.normal.x==b.normal.x);
        assert(a.normal.y==b.normal.y);
        assert(a.normal.z==b.normal.z);
    }
}
static void test_capsule_standing_side_contact(void)
{
    AdventureRoofMesh mesh;
    AdventureRoof_Clear(&mesh);
    AdventureRoofTriangle wall={
        {8*ADV_FP_ONE,2*ADV_FP_ONE,0},
        {18*ADV_FP_ONE,2*ADV_FP_ONE,0},
        {8*ADV_FP_ONE,2*ADV_FP_ONE,10*ADV_FP_ONE},530};
    assert(AdventureRoof_Add(&mesh,wall));
    AdventureTerrain terrain={test_world_ground,&mesh,0,0,
                              AdventureCapsule_SweepMesh,0};
    AdventureGame game;
    AdventureGame_Init(&game);
    AdventureGame_SetTerrain(&game,terrain);
    AdventureVec3 spawn={5*ADV_FP_ONE,0,ADV_FP_ONE};
    AdventureGame_SetSpawn(&game,spawn,0);
    assert(game.player.grounded);
    AdventureRawInput raw={0};
    raw.move_y=32767;
    for(int i=0;i<25;i++){
        assert(AdventureGame_Tick(&game,raw,kEast));
        assert(game.player.pos.x<=8*ADV_FP_ONE);
    }
    assert(game.player.pos.x>6*ADV_FP_ONE);
    assert(game.player.grounded);
}

/* Synthetic hit planes make the slide response deterministic independently
 * of triangle feature selection. Each contact returns a fixed-point TOI and
 * a normal pointing away from the obstacle. */
static bool test_slide_planes(void *context,AdventureVec3 from,
                              AdventureVec3 to,int32_t height,int32_t radius,
                              AdventureCapsuleHit *out)
{
    (void)height;
    (void)radius;
    const int mode=context ? *(const int *)context : 1;
    bool found=false;
    int64_t first=ADV_FP_ONE+1;
    AdventureCapsuleHit hit={0};
    if(from.x<8*ADV_FP_ONE && to.x>=8*ADV_FP_ONE) {
        int64_t t=((int64_t)8*ADV_FP_ONE-from.x)*ADV_FP_ONE/
                  ((int64_t)to.x-from.x);
        hit.normal=(AdventureVec3){-ADV_FP_ONE,0,0};
        hit.triangle_id=1;
        first=t;
        found=true;
    }
    if(mode>=2 && from.z<8*ADV_FP_ONE && to.z>=8*ADV_FP_ONE) {
        int64_t t=((int64_t)8*ADV_FP_ONE-from.z)*ADV_FP_ONE/
                  ((int64_t)to.z-from.z);
        if(!found || t<first){
            hit.normal=(AdventureVec3){0,0,-ADV_FP_ONE};
            hit.triangle_id=2;
            first=t;
            found=true;
        }
    }
    if(mode==3 && from.y<8*ADV_FP_ONE && to.y>=8*ADV_FP_ONE) {
        int64_t t=((int64_t)8*ADV_FP_ONE-from.y)*ADV_FP_ONE/
                  ((int64_t)to.y-from.y);
        if(!found || t<first){
            hit.normal=(AdventureVec3){0,-ADV_FP_ONE,0};
            hit.triangle_id=3;
            first=t;
            found=true;
        }
    }
    if(found){
        hit.fraction=(uint16_t)first;
        *out=hit;
    }
    return found;
}
static void test_slide_tangential_remainder(void)
{
    AdventureVec3 start={0,0,0};
    AdventureVec3 delta={20*ADV_FP_ONE,0,6*ADV_FP_ONE};
    AdventureSlideResult r=AdventureSlide_Resolve(
        start,delta,4*ADV_FP_ONE,ADV_FP_ONE,
        test_slide_planes,0);
    assert(r.contacts==1);
    assert(r.first_hit.triangle_id==1);
    assert(r.position.x<8*ADV_FP_ONE &&
           r.position.x>7*ADV_FP_ONE);
    assert(r.position.z>5*ADV_FP_ONE);
    assert(r.tangent_velocity.x==0);
    assert(r.tangent_velocity.z==delta.z);

    r=AdventureSlide_Resolve(start,delta,4*ADV_FP_ONE,ADV_FP_ONE,0,0);
    assert(r.contacts==0 && r.position.x==delta.x &&
           r.position.z==delta.z);
}
static void test_slide_corners_and_ceilings(void)
{
    int mode=2;
    AdventureVec3 start={0,0,0};
    AdventureVec3 delta={20*ADV_FP_ONE,0,20*ADV_FP_ONE};
    AdventureSlideResult r=AdventureSlide_Resolve(
        start,delta,4*ADV_FP_ONE,ADV_FP_ONE,test_slide_planes,&mode);
    assert(r.contacts>=2);
    assert(r.position.x<8*ADV_FP_ONE && r.position.x>7*ADV_FP_ONE);
    assert(r.position.z<8*ADV_FP_ONE && r.position.z>7*ADV_FP_ONE);
    assert(r.tangent_velocity.x==0 && r.tangent_velocity.z==0);

    mode=3;
    delta=(AdventureVec3){6*ADV_FP_ONE,20*ADV_FP_ONE,3*ADV_FP_ONE};
    r=AdventureSlide_Resolve(start,delta,4*ADV_FP_ONE,ADV_FP_ONE,
                             test_slide_planes,&mode);
    assert(r.contacts>=1);
    assert(r.position.y<8*ADV_FP_ONE);
    assert(r.position.x==delta.x);
    assert(r.position.z==delta.z);
    assert(r.tangent_velocity.y==0);
}
static void test_slide_actual_triangle(void)
{
    AdventureRoofMesh mesh;
    AdventureRoof_Clear(&mesh);
    AdventureRoofTriangle tri={
        {8*ADV_FP_ONE,2*ADV_FP_ONE,0},
        {18*ADV_FP_ONE,2*ADV_FP_ONE,0},
        {8*ADV_FP_ONE,2*ADV_FP_ONE,10*ADV_FP_ONE},610};
    assert(AdventureRoof_Add(&mesh,tri));
    AdventureVec3 from={5*ADV_FP_ONE,0,ADV_FP_ONE};
    AdventureVec3 delta={10*ADV_FP_ONE,0,4*ADV_FP_ONE};
    AdventureSlideResult a=AdventureSlide_Resolve(
        from,delta,4*ADV_FP_ONE,ADV_FP_ONE,
        AdventureCapsule_SweepMesh,&mesh);
    assert(a.contacts>=1);
    assert(a.position.x>=6*ADV_FP_ONE && a.position.x<8*ADV_FP_ONE);
    assert(a.position.z>from.z+3*ADV_FP_ONE);
    for(int i=0;i<10;i++){
        AdventureSlideResult b=AdventureSlide_Resolve(
            from,delta,4*ADV_FP_ONE,ADV_FP_ONE,
            AdventureCapsule_SweepMesh,&mesh);
        assert(a.position.x==b.position.x);
        assert(a.position.y==b.position.y);
        assert(a.position.z==b.position.z);
        assert(a.contacts==b.contacts);
    }
}
static bool test_roof_and_ter_support(void *ctx,int32_t x,int32_t z,
                                      int32_t maximum,AdventureGroundHit *out)
{
    AdventureGroundHit original={0};
    bool has_original=test_world_ground(0,x,z,maximum,&original);
    int32_t roof_y,roof_id;
    bool has_roof=AdventureRoof_QueryGround(ctx,x,z,maximum,
                                             &roof_y,&roof_id);
    if(has_roof && (!has_original || roof_y>original.height)) {
        original.height=roof_y;
        original.surface_id=-2-roof_id;
        original.layer=0;
    }
    if(has_original || has_roof) *out=original;
    return has_original || has_roof;
}
static void test_authored_roof_support(void)
{
    AdventureRoofMesh mesh;
    AdventureRoof_Clear(&mesh);
    assert(AdventureRoof_Add(&mesh,roof(20*ADV_FP_ONE,620)));
    int32_t height,id;
    assert(AdventureRoof_QueryGround(&mesh,2*ADV_FP_ONE,2*ADV_FP_ONE,
                                     30*ADV_FP_ONE,&height,&id));
    assert(height==20*ADV_FP_ONE && id==620);
    assert(!AdventureRoof_QueryGround(&mesh,2*ADV_FP_ONE,2*ADV_FP_ONE,
                                      19*ADV_FP_ONE,&height,&id));
    assert(!AdventureRoof_QueryGround(&mesh,15*ADV_FP_ONE,2*ADV_FP_ONE,
                                      30*ADV_FP_ONE,&height,&id));
    AdventureGroundHit hit;
    assert(test_roof_and_ter_support(&mesh,2*ADV_FP_ONE,2*ADV_FP_ONE,
                                     21*ADV_FP_ONE,&hit));
    assert(hit.height==20*ADV_FP_ONE);
    assert(hit.surface_id==-622 && hit.layer==0);
}
static void test_game_lands_and_stays_on_bin_roof(void)
{
    AdventureRoofMesh mesh;
    AdventureRoof_Clear(&mesh);
    assert(AdventureRoof_Add(&mesh,roof(20*ADV_FP_ONE,630)));
    AdventureTerrain terrain={
        test_roof_and_ter_support,&mesh,0,0,AdventureCapsule_SweepMesh,0};
    AdventureGame game;
    AdventureGame_Init(&game);
    AdventureGame_SetTerrain(&game,terrain);
    AdventureVec3 spawn={2*ADV_FP_ONE,29*ADV_FP_ONE,2*ADV_FP_ONE};
    AdventureGame_SetSpawn(&game,spawn,0);
    assert(!game.player.grounded);
    AdventureRawInput input={0};
    bool landed=false;
    for(int i=0;i<80;i++){
        assert(AdventureGame_Tick(&game,input,kEast));
        if(game.player.grounded){landed=true;break;}
    }
    assert(landed);
    assert(game.player.pos.y==20*ADV_FP_ONE);
    assert(game.player.surface_id==-632);
    for(int i=0;i<15;i++){
        assert(AdventureGame_Tick(&game,input,kEast));
        assert(game.player.grounded);
        assert(game.player.pos.y==20*ADV_FP_ONE);
        assert(game.player.surface_id==-632);
    }
}


/* The single dispatcher orders native TER walls, TER floor/underside and
 * authored BIN triangles using one Q12 fraction and contact normal. */
static bool late_bin_wall(void *ctx,AdventureVec3 from,AdventureVec3 to,
                          int32_t height,int32_t radius,
                          AdventureCapsuleHit *hit)
{
    (void)ctx; (void)height; (void)radius;
    if(from.x>=14*ADV_FP_ONE || to.x<14*ADV_FP_ONE) return false;
    hit->fraction=(uint16_t)(
        ((int64_t)14*ADV_FP_ONE-from.x)*ADV_FP_ONE/
        ((int64_t)to.x-from.x));
    hit->triangle_id=712;
    hit->normal=(AdventureVec3){-ADV_FP_ONE,0,0};
    hit->source=ADV_CONTACT_BIN_TRIANGLE;
    return true;
}
static void test_unified_solver_hit_order(void)
{
    AdventureTerrain terrain={
        test_world_ground,0,test_wall_x,0,test_slide_planes,0};
    AdventureSolver world={terrain,0};
    AdventureCapsuleHit hit;
    AdventureVec3 from={0,0,0};
    AdventureVec3 to={20*ADV_FP_ONE,0,6*ADV_FP_ONE};
    assert(AdventureSolver_Sweep(&world,from,to,4*ADV_FP_ONE,
                                 ADV_FP_ONE,&hit));
    assert(hit.source==ADV_CONTACT_BIN_TRIANGLE);
    assert(hit.triangle_id==1);
    assert(hit.fraction>1550 && hit.fraction<1750);

    terrain.sweep_capsule=late_bin_wall;
    world.terrain=terrain;
    assert(AdventureSolver_Sweep(&world,from,to,4*ADV_FP_ONE,
                                 ADV_FP_ONE,&hit));
    assert(hit.source==ADV_CONTACT_TER_WALL);
    assert(hit.normal.x<-3500);
    assert(hit.normal.z>-100 && hit.normal.z<100);
    assert(hit.fraction>1750 && hit.fraction<1950);

    world.terrain.sweep_wall=0;
    world.terrain.sweep_capsule=0;
    from=(AdventureVec3){5*ADV_FP_ONE,10*ADV_FP_ONE,0};
    to=(AdventureVec3){5*ADV_FP_ONE,-10*ADV_FP_ONE,0};
    assert(AdventureSolver_Sweep(&world,from,to,4*ADV_FP_ONE,
                                 ADV_FP_ONE,&hit));
    assert(hit.source==ADV_CONTACT_TER_FLOOR);
    assert(hit.normal.y==ADV_FP_ONE);

    world.terrain.query_overhead=stack_overhead;
    from.y=0;to.y=30*ADV_FP_ONE;
    assert(AdventureSolver_Sweep(&world,from,to,4*ADV_FP_ONE,
                                 ADV_FP_ONE,&hit));
    assert(hit.source==ADV_CONTACT_TER_CEILING);
    assert(hit.normal.y==-ADV_FP_ONE);
}
static void test_unified_wall_then_floor_and_air_slide(void)
{
    AdventureTerrain terrain={test_world_ground,0,test_wall_x,0,0,0};
    AdventureSolver world={terrain,0};
    AdventureVec3 from={5*ADV_FP_ONE,5*ADV_FP_ONE,1*ADV_FP_ONE};
    AdventureVec3 delta={10*ADV_FP_ONE,-10*ADV_FP_ONE,6*ADV_FP_ONE};
    AdventureSlideResult slide=AdventureSolver_Move(
        world,from,delta,4*ADV_FP_ONE,ADV_FP_ONE);
    assert(slide.contacts>=2);
    assert(slide.first_hit.source==ADV_CONTACT_TER_WALL);
    assert(slide.touched_floor);
    assert(slide.floor_hit.source==ADV_CONTACT_TER_FLOOR);
    assert(slide.position.x>8*ADV_FP_ONE);
    assert(slide.position.x<10*ADV_FP_ONE);
    assert(slide.position.z>from.z+5*ADV_FP_ONE);
    assert(slide.position.y>=0 &&
           slide.position.y<ADV_FP_ONE);
    assert(slide.tangent_velocity.y==0);

    /* Airborne motion along a wall does not cancel vertical ascent. */
    delta.y=6*ADV_FP_ONE;
    slide=AdventureSolver_Move(world,from,delta,4*ADV_FP_ONE,
                                ADV_FP_ONE);
    assert(slide.contacts>=1);
    assert(slide.first_hit.source==ADV_CONTACT_TER_WALL);
    assert(!slide.touched_floor);
    assert(slide.position.y>from.y+5*ADV_FP_ONE);
    assert(slide.position.z>from.z+5*ADV_FP_ONE);
}
static void test_unified_solver_replay(void)
{
    AdventureTerrain terrain={test_world_ground,0,test_wall_x,0,late_bin_wall,0};
    AdventureSolver world={terrain,0};
    AdventureVec3 from={5*ADV_FP_ONE,8*ADV_FP_ONE,1*ADV_FP_ONE};
    AdventureVec3 delta={12*ADV_FP_ONE,-14*ADV_FP_ONE,6*ADV_FP_ONE};
    AdventureSlideResult first=AdventureSolver_Move(
        world,from,delta,4*ADV_FP_ONE,ADV_FP_ONE);
    for(int i=0;i<75;i++){
        AdventureSlideResult r=AdventureSolver_Move(
            world,from,delta,4*ADV_FP_ONE,ADV_FP_ONE);
        assert(r.position.x==first.position.x);
        assert(r.position.y==first.position.y);
        assert(r.position.z==first.position.z);
        assert(r.tangent_velocity.x==first.tangent_velocity.x);
        assert(r.tangent_velocity.y==first.tangent_velocity.y);
        assert(r.tangent_velocity.z==first.tangent_velocity.z);
        assert(r.contacts==first.contacts);
        assert(r.touched_floor==first.touched_floor);
    }
}

/* Mock positive-up TER / BIN face normals, including a stacked steep
 * surface that should NOT conceal an underlying walkable floor. */
static bool test_slope_ground(void *ctx,int32_t x,int32_t z,
                              int32_t maximum,AdventureGroundHit *out)
{
    (void)ctx; (void)z;
    if(!out || x<0 || x>=40*ADV_FP_ONE) return false;
    int32_t h;
    int32_t id;
    if(x<8*ADV_FP_ONE) {h=0;id=800;}
    else if(x<20*ADV_FP_ONE) {
        h=(x-8*ADV_FP_ONE)/2;id=801;
    }else if(x<32*ADV_FP_ONE) {
        h=6*ADV_FP_ONE+2*(x-20*ADV_FP_ONE);id=802;
    }else {h=30*ADV_FP_ONE;id=803;}
    if(h>maximum) return false;
    out->height=h;out->surface_id=id;out->layer=0;
    return true;
}
static bool test_slope_normal(void *ctx,int32_t x,int32_t z,
                              int32_t id,AdventureVec3 *out)
{
    (void)ctx; (void)x; (void)z;
    if(id==800 || id==803)
        return AdventureSlope_Normalize(0,1,0,out);
    if(id==801)
        return AdventureSlope_Normalize(-1,2,0,out);
    if(id==802)
        return AdventureSlope_Normalize(-2,1,0,out);
    return false;
}
static bool test_stacked_slope_ground(void *ctx,int32_t x,int32_t z,
                                      int32_t maximum,AdventureGroundHit *out)
{
    (void)ctx; (void)x; (void)z;
    if(!out) return false;
    if(maximum>=20*ADV_FP_ONE) {
        out->height=20*ADV_FP_ONE;out->surface_id=802;
    }else if(maximum>=0) {
        out->height=0;out->surface_id=800;
    }else return false;
    out->layer=0;
    return true;
}
static void test_slope_normal_math_and_walkability(void)
{
    AdventureVec3 flat,shallow,steep;
    assert(AdventureSlope_Normalize(0,-1,0,&flat));
    assert(flat.y==ADV_FP_ONE && flat.x==0 && flat.z==0);
    assert(AdventureSlope_Normalize(-1,2,0,&shallow));
    assert(shallow.y>ADV_WALKABLE_NORMAL_Y);
    assert(shallow.y<ADV_FP_ONE); /* sqrt(5) must NOT truncate to 2 */
    assert(shallow.x<-1700 && shallow.x>-2000);
    assert(AdventureSlope_Walkable(shallow));
    assert(AdventureSlope_Normalize(-2,1,0,&steep));
    assert(steep.y<ADV_WALKABLE_NORMAL_Y);
    assert(!AdventureSlope_Walkable(steep));
    assert(!AdventureSlope_Normalize(0,0,0,&steep));
    assert(!AdventureSlope_Normalize(100000001,1,0,&steep));
    AdventureVec3 downhill=AdventureSlope_Downhill(steep);
    /* The failed normalization must not be used; restore steep normal. */
    assert(AdventureSlope_Normalize(-2,1,0,&steep));
    downhill=AdventureSlope_Downhill(steep);
    assert(downhill.x<0 && downhill.z==0);
    assert(AdventureSlope_Downhill(shallow).x==0);
}
static void test_slope_support_and_stacked_selection(void)
{
    AdventureTerrain terrain={
        test_stacked_slope_ground,0,0,0,0,test_slope_normal
    };
    AdventureGroundHit hit;
    AdventureVec3 normal;
    assert(AdventureSlope_QuerySupport(terrain,0,0,
                                      25*ADV_FP_ONE,&hit,&normal));
    assert(hit.height==0 && hit.surface_id==800);
    assert(normal.y==ADV_FP_ONE);
    assert(!AdventureSlope_QuerySupport(terrain,0,0,-ADV_FP_ONE,
                                       &hit,&normal));
    terrain.query_normal=0; /* old provider compatibility */
    assert(AdventureSlope_QuerySupport(terrain,0,0,
                                      25*ADV_FP_ONE,&hit,&normal));
    assert(hit.height==20*ADV_FP_ONE);
    assert(normal.y==ADV_FP_ONE);

    /* Fail-safe against a broken query that never lowers its candidate. */
    terrain.query_ground=test_slope_ground;
    terrain.query_normal=test_slope_normal;
    assert(AdventureSlope_QuerySupport(terrain,9*ADV_FP_ONE,0,
                                      2*ADV_FP_ONE,&hit,&normal));
    assert(hit.surface_id==801);
    assert(normal.y>ADV_WALKABLE_NORMAL_Y);
    assert(!AdventureSlope_QuerySupport(terrain,22*ADV_FP_ONE,0,
                                       20*ADV_FP_ONE,&hit,&normal));
}
static void test_slope_gameplay_transition(void)
{
    AdventureGame game;
    AdventureGame_Init(&game);
    AdventureTerrain terrain={
        test_slope_ground,0,0,0,0,test_slope_normal
    };
    AdventureGame_SetTerrain(&game,terrain);
    AdventureGame_SetSpawn(&game,(AdventureVec3){8*ADV_FP_ONE,0,0},0);
    assert(game.player.grounded);
    AdventureRawInput raw={0};
    raw.move_y=32767;
    bool climbed=false,steep_rejected=false;
    for(int i=0;i<50;i++){
        assert(AdventureGame_Tick(&game,raw,kEast));
        if(game.player.grounded && game.player.surface_id==801){
            climbed=true;
            assert(game.player.ground_normal.y>ADV_WALKABLE_NORMAL_Y);
        }
        if(climbed && !game.player.grounded){
            steep_rejected=true;
            break;
        }
    }
    assert(climbed && steep_rejected);
    assert(game.player.ground_normal.y==ADV_FP_ONE);
    /* Respawning directly on the steep slope must never mark grounded. */
    AdventureGame_SetSpawn(&game,(AdventureVec3){
        25*ADV_FP_ONE,16*ADV_FP_ONE,0},0);
    assert(!game.player.grounded);
    /* A flat upper platform remains valid when spawned independently. */
    AdventureGame_SetSpawn(&game,(AdventureVec3){
        34*ADV_FP_ONE,30*ADV_FP_ONE,0},0);
    assert(game.player.grounded);
    assert(game.player.surface_id==803);
}
static void test_bin_roof_normal_selection(void)
{
    AdventureRoofMesh mesh;
    AdventureRoof_Clear(&mesh);
    AdventureRoofTriangle moderate={
        {0,10*ADV_FP_ONE,0},
        {8*ADV_FP_ONE,14*ADV_FP_ONE,0},
        {0,10*ADV_FP_ONE,8*ADV_FP_ONE},850
    };
    assert(AdventureRoof_Add(&mesh,moderate));
    AdventureVec3 n;
    assert(AdventureRoof_QueryNormal(&mesh,850,&n));
    assert(n.y>ADV_WALKABLE_NORMAL_Y);
    assert(n.x<0);
    assert(!AdventureRoof_QueryNormal(&mesh,999,&n));

    AdventureRoofTriangle too_steep={
        {0,20*ADV_FP_ONE,0},
        {8*ADV_FP_ONE,28*ADV_FP_ONE,0},
        {0,20*ADV_FP_ONE,8*ADV_FP_ONE},851
    };
    assert(AdventureRoof_Add(&mesh,too_steep));
    assert(AdventureRoof_QueryNormal(&mesh,851,&n));
    assert(!AdventureSlope_Walkable(n));
}


/* WP-04.11: a walkable incline projects gravity onto horizontal movement.
 * No input rolls downhill; uphill input works against slope acceleration;
 * downhill input amplifies it. None of this uses floats or variable dt. */
static void test_traction_flat_unchanged(void)
{
    AdventureVec3 up={0,ADV_FP_ONE,0};
    AdventureVec3 v={0,0,0};
    for(int i=0;i<32;i++)
        AdventureTraction_Update(&v,3*ADV_FP_ONE,0,true,up,false,false);
    assert(v.x==ADV_TRACTION_RUN_SPEED && v.z==0);
    for(int i=0;i<25;i++)
        AdventureTraction_Update(&v,0,0,false,up,false,false);
    assert(v.x==0 && v.z==0);
}
static void test_traction_uphill_downhill_steer(void)
{
    AdventureVec3 n;
    assert(AdventureSlope_Normalize(-1,2,0,&n));
    assert(AdventureSlope_Walkable(n));
    AdventureVec3 uphill={0,0,0},downhill={0,0,0},coast={0,0,0};
    AdventureTraction_Update(&uphill,3*ADV_FP_ONE,0,true,n,true,false);
    AdventureTraction_Update(&downhill,-3*ADV_FP_ONE,0,true,n,true,false);
    AdventureTraction_Update(&coast,0,0,false,n,true,false);
    assert(uphill.x>0 && uphill.x<ADV_TRACTION_DRIVE_ACCEL);
    assert(downhill.x<-ADV_TRACTION_DRIVE_ACCEL);
    assert(coast.x<0 && coast.z==0);
    /* Continued downhill steering must preserve and build speed above the
     * flat 3-unit run cap while never exceeding the 5-unit slope cap. */
    for(int i=0;i<50;i++)
        AdventureTraction_Update(&downhill,-3*ADV_FP_ONE,0,
                                  true,n,true,false);
    assert(downhill.x<-3*ADV_FP_ONE);
    assert(downhill.x>=-ADV_TRACTION_SLOPE_SPEED);
    /* On release, uphill speed should decay faster than the same speed
     * downhill, rather than being cancelled by a flat-ground brake. */
    AdventureVec3 up_release={2*ADV_FP_ONE,0,0};
    AdventureVec3 down_release={-2*ADV_FP_ONE,0,0};
    AdventureTraction_Update(&up_release,0,0,false,n,true,false);
    AdventureTraction_Update(&down_release,0,0,false,n,true,false);
    assert(up_release.x<2*ADV_FP_ONE);
    assert(down_release.x<-2*ADV_FP_ONE);
}
static void test_traction_air_carry_and_direction(void)
{
    AdventureVec3 n={0,ADV_FP_ONE,0};
    AdventureVec3 vel={4*ADV_FP_ONE,0,0};
    AdventureTraction_Update(&vel,0,0,false,n,false,true);
    assert(vel.x==4*ADV_FP_ONE-ADV_TRACTION_AIR_DRAG);
    AdventureTraction_Update(&vel,0,3*ADV_FP_ONE,true,n,false,true);
    assert(vel.z>0);
    assert(vel.x>3*ADV_FP_ONE);
    int64_t speed_sq=(int64_t)vel.x*vel.x+(int64_t)vel.z*vel.z;
    assert(speed_sq<=(int64_t)ADV_TRACTION_SLOPE_SPEED*
                      ADV_TRACTION_SLOPE_SPEED);
}
static bool test_rolling_ledge(void *ctx,int32_t x,int32_t z,
                                int32_t max_height,AdventureGroundHit *out)
{
    (void)ctx;(void)z;
    if(!out || x<0 || x>=18*ADV_FP_ONE) return false;
    int32_t y=x/2;
    if(y>max_height) return false;
    out->height=y;out->surface_id=890;out->layer=0;
    return true;
}
static bool test_rolling_normal(void *ctx,int32_t x,int32_t z,
                                int32_t id,AdventureVec3 *out)
{
    (void)ctx;(void)x;(void)z;
    if(id!=890) return false;
    return AdventureSlope_Normalize(-1,2,0,out);
}
static void test_game_slope_roll_and_ledge_momentum(void)
{
    AdventureTerrain terrain={
        test_rolling_ledge,0,0,0,0,test_rolling_normal
    };
    AdventureGame game;
    AdventureGame_Init(&game);
    AdventureGame_SetTerrain(&game,terrain);
    AdventureVec3 spawn={8*ADV_FP_ONE,4*ADV_FP_ONE,0};
    AdventureGame_SetSpawn(&game,spawn,0);
    assert(game.player.grounded);
    AdventureRawInput neutral={0};
    for(int i=0;i<4;i++)
        assert(AdventureGame_Tick(&game,neutral,kEast));
    assert(game.player.pos.x<spawn.x); /* natural gravity-driven roll */

    /* Walk uphill and launch past the ledge carrying horizontal speed.
     * Ordinary airborne braking must not destroy the earned momentum. */
    spawn=(AdventureVec3){16*ADV_FP_ONE,8*ADV_FP_ONE,0};
    AdventureGame_SetSpawn(&game,spawn,0);
    assert(game.player.grounded);
    game.player.velocity.x=3*ADV_FP_ONE;
    assert(AdventureGame_Tick(&game,neutral,kEast));
    assert(!game.player.grounded);
    assert(game.player.slope_airborne);
    int32_t outgoing=game.player.velocity.x;
    assert(outgoing>2*ADV_FP_ONE);
    int32_t old_x=game.player.pos.x;
    assert(AdventureGame_Tick(&game,neutral,kEast));
    assert(!game.player.grounded && game.player.slope_airborne);
    assert(game.player.pos.x>old_x+2*ADV_FP_ONE);
    assert(game.player.velocity.x>=outgoing-ADV_TRACTION_AIR_DRAG-8);
    assert(game.player.velocity.x<=outgoing);
}
static void test_traction_deterministic_replay(void)
{
    AdventureVec3 n;
    assert(AdventureSlope_Normalize(-1,2,1,&n));
    AdventureVec3 a={0,0,0},b={0,0,0};
    for(int i=0;i<400;i++){
        bool driving=i%37<29;
        int32_t tx=(i%71<53 ? -3:3)*ADV_FP_ONE;
        int32_t tz=(i%29<11 ? 2:-2)*ADV_FP_ONE;
        bool coast=i>=240;
        AdventureTraction_Update(&a,tx,tz,driving,n,!coast,coast);
        AdventureTraction_Update(&b,tx,tz,driving,n,!coast,coast);
        assert(a.x==b.x && a.z==b.z);
        assert((int64_t)a.x*a.x+(int64_t)a.z*a.z<=
                (int64_t)ADV_TRACTION_SLOPE_SPEED*
                ADV_TRACTION_SLOPE_SPEED);
    }
}

int main(void)
{
    test_lifecycle();
    test_coordinates();
    test_integer_heading();
    test_input();
    test_movement();
    test_repeatability();
    test_world_spawn_and_ramp();
    test_jump_and_landing();
    test_stacked_floor_and_ledge();
    test_world_replay();
    test_segment_sweeps();
    test_wall_sliding();
    test_wall_gameplay();
    test_legacy_wall_height_gate();
    test_overhead_query_selection();
    test_jump_head_hits_underside();
    test_jump_from_upper_platform();
    test_unobstructed_jump_still_works();
    test_head_bonk_determinism();
    test_diagonal_terrain_sweeps();
    test_diagonal_sweep_no_false_contact();
    test_diagonal_sweep_replay();
    test_game_tick_hits_narrow_roof();
    test_analytic_roof_sweep();
    test_roof_mesh_opt_in_and_priority();
    test_sloped_roof_and_diagonal_face();
    test_roof_in_vertical_gameplay();
    test_roof_edges_and_capacity();
    test_roof_sphere_face_edge_and_vertices();
    test_roof_sphere_not_sticky_on_exit();
    test_roof_sphere_priority();
    test_roof_sphere_game_tick_graze();
    test_capsule_full_body_contacts();
    test_capsule_tunnelling_and_escape();
    test_capsule_first_contact_and_replay();
    test_capsule_standing_side_contact();
    test_slide_tangential_remainder();
    test_slide_corners_and_ceilings();
    test_slide_actual_triangle();
    test_authored_roof_support();
    test_game_lands_and_stays_on_bin_roof();
    test_unified_solver_hit_order();
    test_unified_wall_then_floor_and_air_slide();
    test_unified_solver_replay();
    test_slope_normal_math_and_walkability();
    test_slope_support_and_stacked_selection();
    test_slope_gameplay_transition();
    test_bin_roof_normal_selection();
    test_traction_flat_unchanged();
    test_traction_uphill_downhill_steer();
    test_traction_air_carry_and_direction();
    test_game_slope_roll_and_ledge_momentum();
    test_traction_deterministic_replay();
    return 0;
}
