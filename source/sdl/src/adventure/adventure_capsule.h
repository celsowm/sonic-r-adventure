/*
 * Deterministic continuous sweep of a VERTICAL capsule along a 3D translation
 * against static, author-selected triangle roofs.
 *
 * Includes shaft, rounded head and rounded foot; contact normals face away
 * from geometry. Q20.12 positions / normals, fixed 60-Hz tick fractions.
 * No floating-point math or dynamic allocation. No raster/scenery heuristics.
 */
#ifndef SONICR_ADVENTURE_CAPSULE_H
#define SONICR_ADVENTURE_CAPSULE_H
#include "adventure_roof.h"
typedef enum AdventureContactSource {
    ADV_CONTACT_BIN_TRIANGLE = 0,
    ADV_CONTACT_TER_WALL,
    ADV_CONTACT_TER_FLOOR,
    ADV_CONTACT_TER_CEILING
} AdventureContactSource;
typedef struct AdventureCapsuleHit {
    uint16_t fraction;       /* first Q12 overlap, 0..4096 */
    int32_t triangle_id;     /* BIN face ID, or TER wall/surface ID */
    AdventureVec3 normal;    /* Q12 unit vector from geometry toward capsule */
    AdventureContactSource source;
} AdventureCapsuleHit;

/* Read-only world query. Independent of original Sonic R globals. */
typedef bool (*AdventureCapsuleQuery)(void *context, AdventureVec3 from,
                                     AdventureVec3 to, int32_t height,
                                     int32_t radius, AdventureCapsuleHit *out);

bool AdventureCapsule_SweepTriangle(AdventureVec3 from, AdventureVec3 to,
                                   int32_t height, int32_t radius,
                                   const AdventureRoofTriangle *triangle,
                                   AdventureCapsuleHit *out);
bool AdventureCapsule_SweepMesh(void *context, AdventureVec3 from,
                                AdventureVec3 to, int32_t height,
                                int32_t radius, AdventureCapsuleHit *out);
#endif
