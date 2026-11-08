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
/* Highest approved triangle beneath the foot. Supports landing on BIN-only
 * rooftops without granting collision to decorative render surfaces. */
/* Direct single-triangle query for larger stage overlay registries.
 * Same X/Z footprint/positive-up interpolation as the existing mesh path. */
bool AdventureRoof_TriangleHeight(const AdventureRoofTriangle *triangle,
                                   int32_t x,int32_t z,int32_t *height);
bool AdventureRoof_QueryGround(void *context, int32_t x, int32_t z,
                                int32_t max_height,
                                int32_t *height, int32_t *triangle_id);
bool AdventureRoof_Add(AdventureRoofMesh *mesh, AdventureRoofTriangle triangle);
/* Return canonical Q12 face normal for an explicitly approved triangle ID. */
bool AdventureRoof_QueryNormal(void *context, int32_t triangle_id,
                                AdventureVec3 *normal);
/* Continuous upward swept HEAD SPHERE against selected roof faces, edges
 * and vertices. Radius is Q20.12. This is not a full body capsule sweep. */
bool AdventureRoof_SweepTriangle(AdventureVec3 head_from,
                                 AdventureVec3 head_to,
                                 const AdventureRoofTriangle *triangle,
                                 int32_t head_radius,
                                 AdventureRoofHit *out);
bool AdventureRoof_SweepMesh(void *context, AdventureVec3 head_from,
                             AdventureVec3 head_to, int32_t head_radius,
                             AdventureRoofHit *out);
#endif
