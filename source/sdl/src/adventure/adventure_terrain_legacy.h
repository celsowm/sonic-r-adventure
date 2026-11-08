/* Sonic R .TER adapter: isolated from portable adventure simulation. */
#ifndef SONICR_ADVENTURE_TERRAIN_LEGACY_H
#define SONICR_ADVENTURE_TERRAIN_LEGACY_H
#include "adventure_terrain.h"
bool AdventureTerrain_QueryLegacy(void *context, int32_t x, int32_t z,
                                   int32_t max_height, AdventureGroundHit *out);
/* Find the nearest .TER floor above a head-height threshold (two-sided
 * elevated platforms). Shares the terrain traversal with ground sampling. */
bool AdventureTerrain_QueryLegacyOverhead(void *context, int32_t x, int32_t z,
                                          int32_t min_height, AdventureGroundHit *out);
/* Query native wall polylines using the same grid; does not mutate racing
 * physics state or TER mesh masks. */
bool AdventureTerrain_SweepLegacyWall(void *context, AdventureVec3 from,
                                      AdventureVec3 to, int32_t radius,
                                      int32_t capsule_height, int32_t layer,
                                      AdventureWallHit *out);
#endif
