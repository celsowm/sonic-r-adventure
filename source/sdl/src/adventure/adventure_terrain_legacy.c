/*
 * Read-only support/overhead queries over Sonic R's original .TER floor grid.
 * Both queries share one deterministic spatial traversal and legacy
 * interpolation. PointInPolygon only mutates its private scratch indices.
 * No Player or race collision routines are invoked.
 */
#include "adventure_terrain_legacy.h"
#include "adventure_math.h"
#include "sonicr_types.h"
#include "sonicr_globals.h"
#include "ter_types.h"
#include <stdint.h>

extern int PointInPolygon(int x, int z, int surface);
extern int InterpolateGroundHeight(int x, int z, int surface, int edge);
extern int s_hitEdgeIdx;

/*
 * For support, choose highest Y <= threshold.
 * For a head sweep, choose lowest Y >= threshold.
 *
 * Terrain surfaces are oriented as floors in the original race, but we treat
 * elevated floors as two-sided barriers for the new action-platformer:
 * overhead lookup reuses their polygon footprint and interpolated height.
 */
static bool query_ter_height(int32_t x, int32_t z, int32_t threshold,
                             bool overhead, AdventureGroundHit *out)
{
    if (!out || !g_terGridIndex || !g_terGridData || !g_trackSurfaceData ||
        !g_terFaceTable || !g_terVertexTable ||
        g_aiGridCellWidth <= 0 || g_aiGridCellHeight <= 0) return false;

    /* Reject negative offsets BEFORE integer division (truncates to zero).
     * Otherwise x just outside the grid could accidentally select cell 0. */
    int64_t grid_x = (int64_t)x / ADV_FP_ONE - g_aiGridOriginX;
    int64_t grid_z = (int64_t)z / ADV_FP_ONE - g_aiGridOriginZ;
    if (grid_x < 0 || grid_z < 0) return false;
    int64_t cell_x = grid_x / g_aiGridCellWidth;
    int64_t cell_z = grid_z / g_aiGridCellHeight;
    if (cell_x >= 32 || cell_z >= 32) return false;
    const int16_t *grid = (const int16_t *)g_terGridIndex;
    int16_t offset = grid[cell_z * 32 + cell_x];
    if (offset < 0) return false;
    const uint16_t *polys = (const uint16_t *)g_terGridData + offset;
    const TerSurface *surfaces = (const TerSurface *)g_trackSurfaceData;
    if (*polys == UINT16_MAX) return false;

    AdventureGroundHit best = {0};
    bool found = false;
    /* .TER lists have bit15 on their final entry. Protect against malformed
     * resources causing unbounded loops. Valid original lists are short. */
    for (int count = 0; count < 4096; ++count) {
        uint16_t entry = *polys++;
        bool last = (entry & 0x8000u) != 0;
        if (!(entry & 0x4000u)) {
            int idx = entry & 0x0fffu;
            const TerSurface *surf = &surfaces[idx];
            int64_t dx = ((int64_t)x / ADV_FP_ONE) - surf->centerX;
            int64_t dz = ((int64_t)z / ADV_FP_ONE) - surf->centerZ;
            if (surf->faceCount > 0 && surf->faceBase >= 0 &&
                dx * dx + dz * dz < surf->radiusSq &&
                PointInPolygon(x, z, idx)) {
                int32_t legacy_y = InterpolateGroundHeight(
                    x, z, idx, s_hitEdgeIdx);
                int32_t y = AdventureMath_ClampI32(-(int64_t)legacy_y);
                if (overhead ? (y >= threshold && (!found || y < best.height))
                             : (y <= threshold && (!found || y > best.height))) {
                    best.height = y;
                    best.surface_id = idx;
                    best.layer = surf->layer;
                    found = true;
                }
            }
        }
        if (last) break;
    }
    if (found) *out = best;
    return found;
}

bool AdventureTerrain_QueryLegacy(void *context, int32_t x, int32_t z,
                                   int32_t max_height, AdventureGroundHit *out)
{
    (void)context;
    return query_ter_height(x, z, max_height, false, out);
}

bool AdventureTerrain_QueryLegacyOverhead(void *context, int32_t x, int32_t z,
                                          int32_t min_height, AdventureGroundHit *out)
{
    (void)context;
    return query_ter_height(x, z, min_height, true, out);
}
