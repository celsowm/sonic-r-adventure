/*
 * Author-selected static BIN roof faces can act as walkable floor surfaces.
 * Fixed-point X/Z footprint + planar interpolation, independent of TER.
 * Decorative BIN faces are never considered. No heap, no floating point.
 */
#include "adventure_roof.h"
#include <stdint.h>
#include <limits.h>

static uint64_t abs64(int64_t v)
{
    return v<0 ? (uint64_t)(-(v+1))+1u : (uint64_t)v;
}
static int64_t cross(int64_t ax,int64_t az,int64_t bx,int64_t bz)
{
    return ax*bz-az*bx;
}
static bool triangle_height(const AdventureRoofTriangle *tri,
                            int32_t x,int32_t z,int32_t *out)
{
    /* Q8 footprint: permits one sub-world-unit of height interpolation
     * precision while keeping cross products inside signed 64-bit bounds. */
    int64_t ax=tri->a.x/16, az=tri->a.z/16;
    int64_t bx=tri->b.x/16, bz=tri->b.z/16;
    int64_t cx=tri->c.x/16, cz=tri->c.z/16;
    int64_t px=(int64_t)x/16, pz=(int64_t)z/16;
    int64_t area=cross(bx-ax,bz-az,cx-ax,cz-az);
    if(!area) return false;
    int64_t e0=cross(bx-ax,bz-az,px-ax,pz-az);
    int64_t e1=cross(cx-bx,cz-bz,px-bx,pz-bz);
    int64_t e2=cross(ax-cx,az-cz,px-cx,pz-cz);
    int64_t epsilon=(int64_t)(
        abs64(bx-ax)+abs64(bz-az)+abs64(cx-bx)+abs64(cz-bz)+
        abs64(ax-cx)+abs64(az-cz)+1u);
    if(area>0 ? (e0 < -epsilon || e1 < -epsilon || e2 < -epsilon)
              : (e0 > epsilon || e1 > epsilon || e2 > epsilon)) return false;
    /* The BIN source loader uses POSITIVE-up vertex Y, so there is no
     * legacy coordinate negation. World-unit normals bound multiplication. */
    int64_t ux=((int64_t)tri->b.x-tri->a.x)/ADV_FP_ONE;
    int64_t uy=((int64_t)tri->b.y-tri->a.y)/ADV_FP_ONE;
    int64_t uz=((int64_t)tri->b.z-tri->a.z)/ADV_FP_ONE;
    int64_t vx=((int64_t)tri->c.x-tri->a.x)/ADV_FP_ONE;
    int64_t vy=((int64_t)tri->c.y-tri->a.y)/ADV_FP_ONE;
    int64_t vz=((int64_t)tri->c.z-tri->a.z)/ADV_FP_ONE;
    int64_t nx=uy*vz-uz*vy;
    int64_t ny=uz*vx-ux*vz;
    int64_t nz=ux*vy-uy*vx;
    if(!ny) return false;
    if(ny<0) {nx=-nx;ny=-ny;nz=-nz;}
    if(abs64(nx)>(uint64_t)ny || abs64(nz)>(uint64_t)ny)
        return false;
    int64_t yq8=(int64_t)tri->a.y/16 -
                 (nx*(px-ax)+nz*(pz-az))/ny;
    int64_t yq12=yq8*16;
    if(yq12<INT32_MIN || yq12>INT32_MAX) return false;
    *out=(int32_t)yq12;
    return true;
}
bool AdventureRoof_QueryGround(void *context,int32_t x,int32_t z,
                                int32_t maximum,int32_t *height,
                                int32_t *triangle_id)
{
    const AdventureRoofMesh *mesh=(const AdventureRoofMesh *)context;
    if(!mesh || !height || !triangle_id) return false;
    bool found=false;
    int32_t best=INT32_MIN;
    int32_t id=-1;
    for(uint16_t i=0;i<mesh->count;i++){
        const AdventureRoofTriangle *tri=&mesh->triangles[i];
        int32_t y;
        if(triangle_height(tri,x,z,&y) &&
           y<=maximum && (!found || y>best)) {
            best=y; id=tri->id; found=true;
        }
    }
    if(found) {*height=best;*triangle_id=id;}
    return found;
}
