/* Fixed-point walkability and downhill motion for action-platformer terrain. */
#ifndef SONICR_ADVENTURE_SLOPE_H
#define SONICR_ADVENTURE_SLOPE_H
#include "adventure_terrain.h"

/* Q12 normal.y >= 3072 (~41.4-degree maximum walkable slope).
 * The bound is explicit and constant on desktop and Dreamcast. */
#define ADV_WALKABLE_NORMAL_Y 3072
#define ADV_SLOPE_MAX_PROBES 8
#define ADV_SLOPE_DOWNHILL_STEP (ADV_FP_ONE / 4)

/* Convert a positive-up 3D integer face normal into a Q12 unit vector. */
bool AdventureSlope_Normalize(int64_t x, int64_t y, int64_t z,
                               AdventureVec3 *out);
/* Missing optional provider => up normal for backwards compatibility. */
AdventureVec3 AdventureSlope_Normal(AdventureTerrain terrain,
                                    int32_t x, int32_t z,
                                    int32_t surface_id);
bool AdventureSlope_Walkable(AdventureVec3 normal);
/* Highest walkable support under max_height; skip steep stacked surfaces
 * rather than snapping onto an unwalkable upper layer. */
bool AdventureSlope_QuerySupport(AdventureTerrain terrain,
                                 int32_t x, int32_t z,
                                 int32_t max_height,
                                 AdventureGroundHit *out,
                                 AdventureVec3 *normal);
/* Deterministic downhill X/Z bias for non-walkable slopes. */
AdventureVec3 AdventureSlope_Downhill(AdventureVec3 normal);
#endif
