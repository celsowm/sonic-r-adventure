/* Portable, deterministic 2D circle vs segment sweep used by a vertical capsule.
 * All positions are Q20.12; Y overlap is resolved by the terrain provider. */
#ifndef SONICR_ADVENTURE_COLLISION_H
#define SONICR_ADVENTURE_COLLISION_H
#include "adventure_math.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct AdventureWallHit {
    uint16_t fraction;       /* Q12 time of impact [0, 4096] */
    int32_t normal_x;       /* Q12 unit normal facing AWAY from wall */
    int32_t normal_z;
    int32_t wall_id;
} AdventureWallHit;
/* Find earliest overlap of a swept disc with a finite wall segment. */
bool AdventureCollision_SweepDiscSegment(AdventureVec3 from, AdventureVec3 to,
                                        AdventureVec3 a, AdventureVec3 b,
                                        int32_t radius, AdventureWallHit *out);
/* Resolve sliding with up to 3 sequential contacts, no heap allocations. */
typedef bool (*AdventureWallQuery)(void *context, AdventureVec3 from,
                                     AdventureVec3 to, int32_t radius,
                                     int32_t capsule_height, int32_t layer,
                                     AdventureWallHit *out);
void AdventureCollision_MoveAndSlide(AdventureVec3 *position,
                                    AdventureVec3 *horizontal_velocity,
                                    AdventureWallQuery query, void *context,
                                    int32_t radius, int32_t capsule_height,
                                    int32_t layer);
#endif
