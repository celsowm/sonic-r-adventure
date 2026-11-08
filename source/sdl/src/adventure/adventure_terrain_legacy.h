/* Sonic R .TER adapter: isolated from portable adventure simulation. */
#ifndef SONICR_ADVENTURE_TERRAIN_LEGACY_H
#define SONICR_ADVENTURE_TERRAIN_LEGACY_H
#include "adventure_terrain.h"
bool AdventureTerrain_QueryLegacy(void *context, int32_t x, int32_t z,
                                   int32_t max_height, AdventureGroundHit *out);
#endif
