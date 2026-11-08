/*
 * A read-only walkable-floor query over Sonic R .TER's spatial index.
 * Reuses PointInPolygon / InterpolateGroundHeight, which write only their
 * original scratch result globals. No Player/race collision code is called.
 */
#include "adventure_terrain_legacy.h"
#include "adventure_math.h"
#include "sonicr_types.h"
#include "sonicr_globals.h"
#include "ter_types.h"
#include <limits.h>
#include <stdint.h>

extern int PointInPolygon(int x, int z, int surface);
extern int InterpolateGroundHeight(int x, int z, int surface, int edge);
extern int s_hitEdgeIdx;

bool AdventureTerrain_QueryLegacy(void *context, int32_t x, int32_t z,
                                   int32_t max_height, AdventureGroundHit *out)
{
    (void)context;
    if (!out || !g_terGridIndex || !g_terGridData || !g_trackSurfaceData ||
        !g_terFaceTable || !g_terVertexTable ||
        g_aiGridCellWidth <= 0 || g_aiGridCellHeight <= 0) return false;
    /* Use signed 64-bit differences to avoid coordinate overflow. */
    int64_t cell_x = (((int64_t)x / ADV_FP_ONE) - g_aiGridOriginX) /
                     g_aiGridCellWidth;
    int64_t cell_z = (((int64_t)z / ADV_FP_ONE) - g_aiGridOriginZ) /
                     g_aiGridCellHeight;
    if (cell_x < 0 || cell_x >= 32 || cell_z < 0 || cell_z >= 32)
        return false;
    const int16_t *grid = (const int16_t *)g_terGridIndex;
    const uint16_t *polys = (const uint16_t *)g_terGridData +
                            grid[cell_z * 32 + cell_x];
    const TerSurface *surfaces = (const TerSurface *)g_trackSurfaceData;
    bool found = false;
    AdventureGroundHit best = {0};
    /* .TER polygon lists end with bit15 and use 0xFFFF for empty cells.
     * Corrupt files should not cause an infinite read. */
    if (*polys == UINT16_MAX) return false;
    for (int count = 0; count < 4096; ++count) {
        uint16_t entry = *polys++;
        bool last = (entry & 0x8000u) != 0;
        if (!(entry & 0x4000u)) { /* walls are NOT supporting ground */
            int idx = entry & 0x0fffu;
            const TerSurface *surf = &surfaces[idx];
            int64_t dx = ((int64_t)x / ADV_FP_ONE) - surf->centerX;
            int64_t dz = ((int64_t)z / ADV_FP_ONE) - surf->centerZ;
            if (surf->faceCount > 0 && surf->faceBase >= 0 &&
                dx * dx + dz * dz < surf->radiusSq &&
                PointInPolygon(x, z, idx)) {
                int32_t legacy_y = InterpolateGroundHeight(x, z, idx, s_hitEdgeIdx);
                int32_t height = AdventureMath_ClampI32(-(int64_t)legacy_y);
                if (height <= max_height && (!found || height > best.height)) {
                    best.height = height;
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
