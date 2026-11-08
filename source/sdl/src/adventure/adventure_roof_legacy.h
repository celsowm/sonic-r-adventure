/* Explicit opt-in lookup from original, already-loaded track BIN meshes.
 * This bridge does not affect renderer or racing physics. */
#ifndef SONICR_ADVENTURE_ROOF_LEGACY_H
#define SONICR_ADVENTURE_ROOF_LEGACY_H
#include "adventure_roof.h"
#include "adventure_terrain.h"
void AdventureRoofLegacy_LoadSelected(AdventureRoofMesh *mesh, int track_id);
/* Merge original TER support with authored walkable BIN roofs. */
bool AdventureRoofLegacy_QueryGround(void *context, int32_t x, int32_t z,
                                     int32_t max_height,
                                     AdventureGroundHit *out);
#endif
