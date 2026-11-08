/*
 * WP-01: isolated adventure-mode preview runner.
 *
 * Called after the shared legacy stage bootstrap (InitLevel), but BEFORE
 * InitRaceStart and the main race loop. The legacy loaders still initialize
 * some race/AI data, however NO race physics, lap/rank updates, replay,
 * rubber-banding, or race-result handling is executed by this loop.
 *
 * WP-04 adds terrain support and vertical jump/gravity/landing simulation.
 * Static walls and elevated-platform head collision are supported.
 * Full arbitrary-mesh ceiling sweeps and special loops are not yet supported.
 */
#include "adventure_mode.h"
#include "adventure_game.h"
#include "adventure_math.h"
#include "adventure_terrain_legacy.h"
#include "adventure_roof.h"
#include "adventure_roof_legacy.h"
#include "adventure_capsule.h"
#include "sonicr_types.h"
#include "sonicr_globals.h"
#include "sonicr_functions.h"
#include "platform.h"

#include <stdint.h>

extern void BuildChaseCamera(Player *player, CamStateEntry *cam);
extern void BuildCameraViewForViewport(int vp);
extern void FlipD3D(void);
extern unsigned char s_keystate[256];

static int s_adventure_active;
/* A small fixed-capacity registry, allocated once, not per frame/stack. */
static AdventureRoofMesh s_selected_roofs;

/* One-way bridge into exactly the legacy fields consumed by presentation.
 * Never run UpdatePlayerMovement or the five-player race physics scheduler. */
static void AdventureMode_ApplyPlayer(const AdventurePlayer *state)
{
    Player *render = &g_playerBase[0];
    AdventureMath_ToLegacy(state->previous_pos, &render->prevPosX,
                           &render->prevPosY, &render->prevPosZ);
    AdventureMath_ToLegacy(state->pos, &render->posX,
                           &render->posY, &render->posZ);
    render->velX = state->velocity.x;
    render->velY = AdventureMath_ClampI32(-(int64_t)state->velocity.y);
    render->velZ = state->velocity.z;
    render->angleYaw = (int)state->facing;
    render->moveMode = (short)state->facing;
    render->groundedFlag = state->grounded ? 1 : 0;
    render->airTimer = state->grounded ? 0 : 1;
    render->overSurface = state->grounded ? 1 : 0;
    render->collisionLayer = state->surface_layer;
    if (state->grounded) render->groundHeight = render->posY;
}

int AdventureMode_IsActive(void)
{
    return s_adventure_active;
}

int AdventureMode_RunPreview(void)
{
    AdventureGame game;
    AdventureGame_Init(&game);
    /* Explicit roof selections are empty until individual visual faces have
     * been validated as solid. Do not collide all render decoration geometry. */
    AdventureRoofLegacy_LoadSelected(&s_selected_roofs, g_trackId);
    AdventureTerrain terrain = {AdventureTerrain_QueryLegacy, &s_selected_roofs,
                                AdventureTerrain_SweepLegacyWall,
                                AdventureTerrain_QueryLegacyOverhead,
                                AdventureCapsule_SweepMesh};
    AdventureGame_SetTerrain(&game, terrain);

    /* InitLevel already populated the original game resources and Player[0].
     * Use one viewport/one renderable Sonic, never the five race opponents. */
    g_numHumans = 1;
    g_numViewports = 1;
    g_numPlayers = 1;
    g_playerBase[0].charId = CHAR_SONIC;
    g_playerBase[0].renderEnabled = 1;
    g_playerBase[0].moveMode = (short)(g_playerBase[0].angleYaw & 0x0fff);
    AdventureGame_SetSpawn(&game,
        AdventureMath_FromLegacy(g_playerBase[0].posX,
                                 g_playerBase[0].posY,
                                 g_playerBase[0].posZ),
        (uint16_t)g_playerBase[0].angleYaw);

    /* Do not enter the original pre-race countdown/iris. */
    g_introCountdown = 0;
    g_fadeLevel = 0;
    g_fadeState = FADE_VISIBLE;
    g_raceFinished = 0;
    g_isPaused = 0;
    g_skipThisFrame = 0;
    g_renderPass = 2;

    /* Camera bootstraps from InitLevel's original track/camera resources.
     * A dedicated action camera replaces this in WP-05. */
    g_camStateTable[0].fovDetail = 0x20;
    BuildChaseCamera(&g_playerBase[0], &g_camStateTable[0]);

    s_adventure_active = 1;

    uint32_t last_ms = platform_get_time_ms();
    uint32_t tick_accum = 0;
    int quit_requested = 0;

    while (!quit_requested) {
        uint32_t frame_start_ms = platform_get_time_ms();

        platform_pump_events();
        if (platform_poll_events(g_diKeyboardState, 256) != 0 ||
            (g_diKeyboardState[DIK_ESCAPE] & 0x80) != 0) {
            quit_requested = 1;
        }
#ifdef SONICR_DC
        /* The Dreamcast has no CLI. In the optional boot build START exits
         * the preview; the usual system-wide quit chord still works. */
        if ((platform_menu_buttons() & MENUBTN_START) != 0)
            quit_requested = 1;
#endif
        if (quit_requested) break;

        AdventureRawInput controls;
        platform_adventure_read_input(&controls);

        /* 60 fixed ticks/second without floating point or render-time input.
         * Bounded catch-up avoids a spiral when the renderer is overloaded.
         * Future movement ticks will use a normalized input snapshot. */
        uint32_t elapsed_ms = frame_start_ms - last_ms;
        last_ms = frame_start_ms;
        if (elapsed_ms > 100) elapsed_ms = 100;
        tick_accum += elapsed_ms * 60u;
        unsigned int ticks = 0;
        while (tick_accum >= 1000u && ticks < 4u) {
            /* Camera coordinates use 1/16 world units, player uses Q20.12.
             * Derive the movement reference from camera position, not from
             * ambiguously named legacy targetYaw/targetPitch fields. */
            int32_t cam_x = AdventureMath_ClampI32(
                (int64_t)g_camStateTable[0].posX * 256);
            int32_t cam_z = AdventureMath_ClampI32(
                (int64_t)g_camStateTable[0].posZ * 256);
            AdventureBasis basis = AdventureMath_CameraBasis(
                game.player.pos.x, game.player.pos.z, cam_x, cam_z);
            (void)AdventureGame_Tick(&game, controls, basis);
            tick_accum -= 1000u;
            ++ticks;
        }
        if (tick_accum >= 1000u) tick_accum = 0;

        g_currentTime = frame_start_ms;
        AdventureMode_ApplyPlayer(&game.player);
        g_playerBase[0].renderEnabled = 1;
        /* Temporary chase camera is updated after position integration.
         * WP-05 replaces this renderer-facing compatibility bridge. */
        BuildChaseCamera(&g_playerBase[0], &g_camStateTable[0]);
        BuildCameraViewForViewport(0);
        RenderHUD();
        FlipD3D();
        ++g_totalFrames;
        ++g_totalFrames2;

        uint32_t frame_ms = platform_get_time_ms() - frame_start_ms;
        if (frame_ms < 16u)
            platform_sleep_ms((int)(16u - frame_ms));
    }

    s_adventure_active = 0;
    AdventureGame_Shutdown(&game);
    return 0;
}
