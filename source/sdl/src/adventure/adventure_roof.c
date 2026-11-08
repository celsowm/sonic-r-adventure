/*
 * Continuous upward HEAD-POINT vs finite authored roof triangle face.
 * Plane intersection is analytic (not substep sampling). Barycentric
 * footprint containment uses integer cross products in reduced Q8.
 * Deterministic on SH-4 and host. No floats or original race physics.
 *
 * IMPORTANT: This is NOT full swept-capsule-vs-triangle contact. It does not
 * cover rounded head radius, triangle edges, or dynamic/moving roofs.
 */
#include "adventure_roof.h"
#include <stdint.h>
#include <string.h>

static uint64_t abs64(int64_t n) { return n<0 ? (uint64_t)(-n) : (uint64_t)n; }
static int64_t cross2(int64_t ax,int64_t az,int64_t bx,int64_t bz)
{
    return ax*bz-az*bx;
}
static bool roof_normal(const AdventureRoofTriangle *t,
                        int64_t *nx,int64_t *ny,int64_t *nz)
{
    if (!t) return false;
    /* Authored vertices are whole world units, converted to Q12 by loader. */
    int64_t ux=((int64_t)t->b.x-t->a.x)/ADV_FP_ONE;
    int64_t uy=((int64_t)t->b.y-t->a.y)/ADV_FP_ONE;
    int64_t uz=((int64_t)t->b.z-t->a.z)/ADV_FP_ONE;
    int64_t vx=((int64_t)t->c.x-t->a.x)/ADV_FP_ONE;
    int64_t vy=((int64_t)t->c.y-t->a.y)/ADV_FP_ONE;
    int64_t vz=((int64_t)t->c.z-t->a.z)/ADV_FP_ONE;
    *nx=uy*vz-uz*vy;
    *ny=uz*vx-ux*vz;
    *nz=ux*vy-uy*vx;
    if (*ny==0) return false;
    if (*ny<0) { *nx=-*nx; *ny=-*ny; *nz=-*nz; }
    /* Explicit roof restriction: reject vertical/very steep faces. */
    return abs64(*nx)<=(uint64_t)*ny &&
           abs64(*nz)<=(uint64_t)*ny;
}
void AdventureRoof_Clear(AdventureRoofMesh *mesh)
{
    if (mesh) mesh->count=0;
}
bool AdventureRoof_Add(AdventureRoofMesh *mesh,AdventureRoofTriangle triangle)
{
    int64_t nx,ny,nz;
    if (!mesh || mesh->count>=ADV_ROOF_MAX_TRIANGLES ||
        !roof_normal(&triangle,&nx,&ny,&nz)) return false;
    mesh->triangles[mesh->count++]=triangle;
    return true;
}
bool AdventureRoof_SweepTriangle(AdventureVec3 from,AdventureVec3 to,
                                 const AdventureRoofTriangle *tri,
                                 AdventureRoofHit *out)
{
    int64_t nx,ny,nz;
    if (!out || to.y<=from.y ||
        !roof_normal(tri,&nx,&ny,&nz)) return false;
    /* Plane normal is oriented positive Y; only hit from BELOW the roof. */
    int64_t d0=nx*((int64_t)from.x-tri->a.x)+
               ny*((int64_t)from.y-tri->a.y)+
               nz*((int64_t)from.z-tri->a.z);
    int64_t d1=nx*((int64_t)to.x-tri->a.x)+
               ny*((int64_t)to.y-tri->a.y)+
               nz*((int64_t)to.z-tri->a.z);
    if (d0>0 || d1<0 || d1<=d0) return false;
    uint64_t below=(uint64_t)(-d0);
    uint64_t above=(uint64_t)d1;
    /* Scale both sides before multiplying by 4096 (avoid int64 overflow).
     * Q12 fraction is exact to its own resolution after this rounding. */
    while (below+above>UINT64_MAX/ADV_FP_ONE) {
        below=(below+1u)/2u;
        above=(above+1u)/2u;
    }
    uint64_t denominator=below+above;
    if (!denominator) return false;
    uint32_t fraction=(uint32_t)(below*ADV_FP_ONE/denominator);
    /* Check triangle footprint at its analytic plane crossing.
     * Reducing from Q12 to Q8 bounds cross products within int64 for TER
     * style world vertices, even near coordinates ±32768. */
    int64_t px=((int64_t)from.x+
                ((int64_t)to.x-from.x)*fraction/ADV_FP_ONE)/16;
    int64_t pz=((int64_t)from.z+
                ((int64_t)to.z-from.z)*fraction/ADV_FP_ONE)/16;
    int64_t ax=tri->a.x/16,az=tri->a.z/16;
    int64_t bx=tri->b.x/16,bz=tri->b.z/16;
    int64_t cx=tri->c.x/16,cz=tri->c.z/16;
    int64_t area=cross2(bx-ax,bz-az,cx-ax,cz-az);
    if (!area) return false;
    int64_t edge0=cross2(bx-ax,bz-az,px-ax,pz-az);
    int64_t edge1=cross2(cx-bx,cz-bz,px-bx,pz-bz);
    int64_t edge2=cross2(ax-cx,az-cz,px-cx,pz-cz);
    /* 1/32 world-unit edge tolerance compensates Q12 time rounding;
     * explicit selection prevents false scenery collision. */
    uint64_t length_sum=abs64(bx-ax)+abs64(bz-az)+
                        abs64(cx-bx)+abs64(cz-bz)+
                        abs64(ax-cx)+abs64(az-cz);
    int64_t epsilon=(int64_t)(length_sum/32u+1u);
    if (area>0 ? (edge0 < -epsilon || edge1 < -epsilon || edge2 < -epsilon)
               : (edge0 > epsilon || edge1 > epsilon || edge2 > epsilon))
        return false;
    out->fraction=(uint16_t)fraction;
    out->triangle_id=tri->id;
    return true;
}
bool AdventureRoof_SweepMesh(void *context,AdventureVec3 from,
                             AdventureVec3 to,AdventureRoofHit *out)
{
    const AdventureRoofMesh *mesh=(const AdventureRoofMesh *)context;
    if (!mesh || !out) return false;
    bool found=false;
    AdventureRoofHit best={0};
    for(uint16_t i=0;i<mesh->count;i++) {
        AdventureRoofHit hit;
        if (AdventureRoof_SweepTriangle(from,to,&mesh->triangles[i],&hit) &&
            (!found || hit.fraction<best.fraction)) {
            best=hit;
            found=true;
        }
    }
    if (found) *out=best;
    return found;
}
