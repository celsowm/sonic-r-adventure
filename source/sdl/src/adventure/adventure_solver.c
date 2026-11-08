/*
 * Unified narrow-phase dispatcher. All sources produce a single capsule
 * contact with the same Q12 first-impact fraction and normal convention.
 * The iterative slide solver is the ONLY response path for obstacle hits.
 *
 * TER floors remain sampled heightfields; TER walls use swept discs with
 * height gates; authored BIN faces use full vertical capsule sweeps. They
 * share ordering and tangential response but not an identical narrow phase.
 */
#include "adventure_solver.h"
#include "adventure_vertical.h"
#include <stdint.h>

bool AdventureSolver_Sweep(void *context, AdventureVec3 from,
                           AdventureVec3 to, int32_t height, int32_t radius,
                           AdventureCapsuleHit *out)
{
    const AdventureSolver *world=(const AdventureSolver *)context;
    if (!world || !out) return false;
    AdventureTerrain terrain=world->terrain;
    AdventureCapsuleHit best={0};
    bool found=false;
    if (terrain.sweep_wall) {
        AdventureWallHit wall;
        if (terrain.sweep_wall(terrain.context,from,to,radius,height,
                               world->collision_layer,&wall)) {
            best.fraction=wall.fraction;
            best.triangle_id=-10000-wall.wall_id;
            best.normal=(AdventureVec3){wall.normal_x,0,wall.normal_z};
            best.source=ADV_CONTACT_TER_WALL;
            found=true;
        }
    }
    if (terrain.sweep_capsule) {
        AdventureCapsuleHit mesh={0};
        if (terrain.sweep_capsule(terrain.context,from,to,height,
                                  radius,&mesh) &&
            (!found || mesh.fraction<best.fraction)) {
            best=mesh;
            best.source=ADV_CONTACT_BIN_TRIANGLE;
            found=true;
        }
    }
    /* Do not test BIN twice: sample the native heightfield using the same
     * trajectory but with its optional capsule provider disabled. */
    terrain.sweep_capsule=0;
    if (to.y!=from.y && (terrain.query_ground || terrain.query_overhead)) {
        AdventureVerticalContact vertical;
        if (AdventureVertical_Sweep(terrain,from,to,height,radius,
                                    to.y>from.y,&vertical) &&
            (!found || vertical.fraction<best.fraction)) {
            best.fraction=vertical.fraction;
            best.triangle_id=vertical.surface.surface_id;
            best.normal=(AdventureVec3){
                0,to.y>from.y?-ADV_FP_ONE:ADV_FP_ONE,0
            };
            best.source=to.y>from.y?
                        ADV_CONTACT_TER_CEILING:ADV_CONTACT_TER_FLOOR;
            best.resolved_y=vertical.position.y;
            best.has_resolved_y=true;
            found=true;
        }
    }
    if (found) *out=best;
    return found;
}
AdventureSlideResult AdventureSolver_Move(AdventureSolver world,
                                          AdventureVec3 start,
                                          AdventureVec3 displacement,
                                          int32_t height,int32_t radius)
{
    return AdventureSlide_Resolve(start,displacement,height,radius,
                                  AdventureSolver_Sweep,&world);
}
