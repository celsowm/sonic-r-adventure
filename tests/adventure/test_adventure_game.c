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
    AdventureTerrain terrain = {test_world_ground, 0, 0, 0};
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
    AdventureTerrain terrain = {test_world_ground, 0, 0, 0};
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
    AdventureTerrain terrain = {test_world_ground, 0, 0, 0};
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
    AdventureTerrain terrain = {test_world_ground, 0, 0, 0};
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
    AdventureTerrain terrain = {test_world_ground, 0, test_wall_x, 0};
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
    AdventureTerrain terrain={stack_ground,0,0,stack_overhead};
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
    AdventureTerrain terrain={stack_ground,0,0,stack_overhead};
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
    AdventureTerrain terrain={test_world_ground,0,0,stack_overhead};
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
    AdventureTerrain terrain={stack_ground,0,0,stack_overhead};
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
    return 0;
}
