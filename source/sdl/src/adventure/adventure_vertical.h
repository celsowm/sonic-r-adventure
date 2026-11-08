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
    bool capsule_hit;       /* contact came from selected BIN capsule provider */
} AdventureVerticalContact;
/* Rising chooses between a TER platform underside and the full-body
 * capsule hit against selected visual roofs. Falling uses TER foot heights.
 * Dynamic objects and walking support on BIN-only roof faces are not handled. */
bool AdventureVertical_Sweep(AdventureTerrain terrain,
                             AdventureVec3 from, AdventureVec3 to,
                             int32_t capsule_height, int32_t capsule_radius,
                             bool rising, AdventureVerticalContact *contact);
#endif
