/* Read-only world support queries. Coordinates are Q20.12 and Y is up. */
#ifndef SONICR_ADVENTURE_TERRAIN_H
#define SONICR_ADVENTURE_TERRAIN_H
#include <stdbool.h>
#include <stdint.h>
#include "adventure_collision.h"
#include "adventure_roof.h"
typedef struct AdventureGroundHit {
    int32_t height;        /* top surface Y, Q20.12 */
    int32_t surface_id;
    int32_t layer;
} AdventureGroundHit;
/* Highest walkable surface at or below max_height. FALSE means no surface;
 * height==0 is a valid hit (never use 0 as a no-hit sentinel). */
typedef bool (*AdventureGroundQuery)(void *context, int32_t x, int32_t z,
                                      int32_t max_height, AdventureGroundHit *out);
/* Lowest terrain surface at or above min_height, used to prevent upward
 * head penetration into the underside of stacked .TER floor polygons.
 * This is an explicit two-sided platformer policy, not a legacy race rule. */
typedef bool (*AdventureOverheadQuery)(void *context, int32_t x, int32_t z,
                                        int32_t min_height, AdventureGroundHit *out);
typedef struct AdventureTerrain {
    AdventureGroundQuery query_ground;
    void *context;
    AdventureWallQuery sweep_wall; /* optional static-wall collision */
    AdventureOverheadQuery query_overhead; /* optional nearest floor above head */
    /* Optional continuous HEAD-POINT collision with explicitly authored BIN
     * roof faces. The same terrain.context is passed to this callback. */
    bool (*sweep_roof)(void *context, AdventureVec3 head_from,
                       AdventureVec3 head_to, AdventureRoofHit *hit);
} AdventureTerrain;
#endif
