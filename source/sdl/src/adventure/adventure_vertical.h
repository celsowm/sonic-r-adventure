/* Bounded 3D trajectory query for feet/head crossing .TER floor polygons.
 * No heap, floats, race-player ABI, or direct global terrain dependencies. */
#ifndef SONICR_ADVENTURE_VERTICAL_H
#define SONICR_ADVENTURE_VERTICAL_H
#include "adventure_terrain.h"
#include <stdbool.h>
typedef struct AdventureVerticalContact {
    AdventureVec3 position; /* resolved foot point at impact */
    AdventureGroundHit surface;
    uint16_t fraction;      /* Q12 fraction along full 3D trajectory */
} AdventureVerticalContact;
/* Rising hits an underside with the head; falling lands with the feet.
 * Does not resolve arbitrary scenery triangles or complete capsule side walls. */
bool AdventureVertical_Sweep(AdventureTerrain terrain,
                             AdventureVec3 from, AdventureVec3 to,
                             int32_t capsule_height, bool rising,
                             AdventureVerticalContact *contact);
#endif
