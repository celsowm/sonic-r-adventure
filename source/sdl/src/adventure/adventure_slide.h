/* Iterative response for a translating vertical capsule (Q20.12).
 * Fixed, bounded contact count, no allocation, independent of Sonic R ABI. */
#ifndef SONICR_ADVENTURE_SLIDE_H
#define SONICR_ADVENTURE_SLIDE_H
#include "adventure_capsule.h"

typedef struct AdventureSlideResult {
    AdventureVec3 position;         /* resolved foot position */
    AdventureVec3 tangent_velocity; /* requested displacement projected off walls */
    AdventureCapsuleHit first_hit;  /* valid only if contacts != 0 */
    AdventureCapsuleHit floor_hit;  /* latest supporting contact */
    uint8_t contacts;
    bool touched_floor;
    bool touched_ceiling;
} AdventureSlideResult;

/* Caller chooses whether to accept vertical motion after sliding (e.g. when
 * airborne) or to reapply a supporting TER floor height (when grounded). */
AdventureSlideResult AdventureSlide_Resolve(AdventureVec3 start,
                                           AdventureVec3 displacement,
                                           int32_t height, int32_t radius,
                                           AdventureCapsuleQuery query,
                                           void *context);
#endif
