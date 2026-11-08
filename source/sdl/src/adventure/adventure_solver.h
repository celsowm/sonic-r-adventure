/*
 * One read-only collision query for native TER wall segments, native TER
 * floor/overhead and explicitly authored BIN triangles.
 * Does not invoke the original racing player or wall response.
 */
#ifndef SONICR_ADVENTURE_SOLVER_H
#define SONICR_ADVENTURE_SOLVER_H
#include "adventure_terrain.h"
#include "adventure_slide.h"

typedef struct AdventureSolver {
    AdventureTerrain terrain;
    int32_t collision_layer;
} AdventureSolver;

/* Choose the earliest contact across all active world providers; all hits
 * use the same normalized AdventureCapsuleHit and Q12 time fraction. */
bool AdventureSolver_Sweep(void *context, AdventureVec3 from,
                           AdventureVec3 to, int32_t height, int32_t radius,
                           AdventureCapsuleHit *hit);

AdventureSlideResult AdventureSolver_Move(AdventureSolver world,
                                          AdventureVec3 start,
                                          AdventureVec3 displacement,
                                          int32_t height, int32_t radius);
#endif
