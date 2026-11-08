/* Read-only world support queries. Coordinates are Q20.12 and Y is up. */
#ifndef SONICR_ADVENTURE_TERRAIN_H
#define SONICR_ADVENTURE_TERRAIN_H
#include <stdbool.h>
#include <stdint.h>
#include "adventure_collision.h"
typedef struct AdventureGroundHit {
    int32_t height;        /* top surface Y, Q20.12 */
    int32_t surface_id;
    int32_t layer;
} AdventureGroundHit;
/* Highest walkable surface at or below max_height. FALSE means no surface;
 * height==0 is a valid hit (never use 0 as a no-hit sentinel). */
typedef bool (*AdventureGroundQuery)(void *context, int32_t x, int32_t z,
                                      int32_t max_height, AdventureGroundHit *out);
typedef struct AdventureTerrain {
    AdventureGroundQuery query_ground;
    void *context;
    AdventureWallQuery sweep_wall; /* optional collision against static obstacles */
} AdventureTerrain;
#endif
