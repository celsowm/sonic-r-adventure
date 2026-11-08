/* Explicit, author-selected visual roofs. No automatic decoration collision.
 * Coordinates: Q20.12, positive-up; no heap, no legacy Player dependencies. */
#ifndef SONICR_ADVENTURE_ROOF_H
#define SONICR_ADVENTURE_ROOF_H
#include "adventure_math.h"
#include <stdbool.h>
#include <stdint.h>
#define ADV_ROOF_MAX_TRIANGLES 64u
typedef struct AdventureRoofTriangle {
    AdventureVec3 a, b, c;
    int32_t id;
} AdventureRoofTriangle;
typedef struct AdventureRoofMesh {
    AdventureRoofTriangle triangles[ADV_ROOF_MAX_TRIANGLES];
    uint16_t count;
} AdventureRoofMesh;
typedef struct AdventureRoofHit {
    uint16_t fraction;  /* Q12 parametric time of head crossing, 0..4096 */
    int32_t triangle_id;
} AdventureRoofHit;
void AdventureRoof_Clear(AdventureRoofMesh *mesh);
bool AdventureRoof_Add(AdventureRoofMesh *mesh, AdventureRoofTriangle triangle);
/* Analytic segment vs roof TRIANGLE FACE; radius/edge/vertex capsule contacts
 * are deliberately outside this narrow-phase and need a future solver. */
bool AdventureRoof_SweepTriangle(AdventureVec3 head_from,
                                 AdventureVec3 head_to,
                                 const AdventureRoofTriangle *triangle,
                                 AdventureRoofHit *out);
bool AdventureRoof_SweepMesh(void *context, AdventureVec3 head_from,
                             AdventureVec3 head_to, AdventureRoofHit *out);
#endif
