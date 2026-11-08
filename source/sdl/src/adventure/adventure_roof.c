/*
 * Continuous upward swept HEAD SPHERE vs static, explicitly selected roof
 * triangle. Face contacts use an analytic plane crossing. Edge and vertex
 * contacts find the first sphere-vs-finite-edge intersection through a
 * convex integer closest-approach search over the entire [0,1] tick.
 *
 * The head sphere (not a zero-radius point) now catches roof face grazing,
 * all three edges and triangle vertices. The rest of the vertical body is
 * not swept here: this is NOT a full capsule/triangle solver.
 *
 * Coordinate input: Q20.12, positive-up. All calculations are integer
 * deterministic, with bounded work and without dynamic allocation.
 */
#include "adventure_roof.h"
#include <stdint.h>

#define ADV_ROOF_MAX_EDGE_WORLD 8192
#define ADV_ROOF_Q6_DIVISOR 64
#define ADV_ROOF_END_FRACTION ADV_FP_ONE

static uint64_t magnitude(int64_t n)
{
    return n<0 ? (uint64_t)(-(n+1))+1u : (uint64_t)n;
}
static int64_t cross2(int64_t ax,int64_t az,int64_t bx,int64_t bz)
{
    return ax*bz-az*bx;
}
static bool roof_normal(const AdventureRoofTriangle *t,
                        int64_t *nx,int64_t *ny,int64_t *nz)
{
    if (!t) return false;
    /* Authored BIN vertices are integral world units. Bound input spans
     * so each normal's squared length and plane dot fit in uint64/int64
     * even for the worst representable Q20.12 world coordinate. */
    int64_t ux=((int64_t)t->b.x-t->a.x)/ADV_FP_ONE;
    int64_t uy=((int64_t)t->b.y-t->a.y)/ADV_FP_ONE;
    int64_t uz=((int64_t)t->b.z-t->a.z)/ADV_FP_ONE;
    int64_t vx=((int64_t)t->c.x-t->a.x)/ADV_FP_ONE;
    int64_t vy=((int64_t)t->c.y-t->a.y)/ADV_FP_ONE;
    int64_t vz=((int64_t)t->c.z-t->a.z)/ADV_FP_ONE;
    if (magnitude(ux)>ADV_ROOF_MAX_EDGE_WORLD ||
        magnitude(uy)>ADV_ROOF_MAX_EDGE_WORLD ||
        magnitude(uz)>ADV_ROOF_MAX_EDGE_WORLD ||
        magnitude(vx)>ADV_ROOF_MAX_EDGE_WORLD ||
        magnitude(vy)>ADV_ROOF_MAX_EDGE_WORLD ||
        magnitude(vz)>ADV_ROOF_MAX_EDGE_WORLD) return false;
    *nx=uy*vz-uz*vy;
    *ny=uz*vx-ux*vz;
    *nz=ux*vy-uy*vx;
    if (*ny==0) return false;
    if (*ny<0) { *nx=-*nx; *ny=-*ny; *nz=-*nz; }
    /* Conservative: roof rather than a steep wall. */
    return magnitude(*nx)<=(uint64_t)*ny &&
           magnitude(*nz)<=(uint64_t)*ny;
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
static AdventureVec3 position_at(AdventureVec3 from,AdventureVec3 to,
                                 uint32_t fraction)
{
    AdventureVec3 p;
    p.x=AdventureMath_ClampI32((int64_t)from.x+
                 ((int64_t)to.x-from.x)*fraction/ADV_FP_ONE);
    p.y=AdventureMath_ClampI32((int64_t)from.y+
                 ((int64_t)to.y-from.y)*fraction/ADV_FP_ONE);
    p.z=AdventureMath_ClampI32((int64_t)from.z+
                 ((int64_t)to.z-from.z)*fraction/ADV_FP_ONE);
    return p;
}
static bool point_in_roof(AdventureVec3 point,
                          const AdventureRoofTriangle *tri)
{
    /* Roof projection onto X/Z is nondegenerate for our slope limit.
     * Q8 cross products avoid overflow on original 16-bit local vertices. */
    int64_t px=(int64_t)point.x/16, pz=(int64_t)point.z/16;
    int64_t ax=(int64_t)tri->a.x/16, az=(int64_t)tri->a.z/16;
    int64_t bx=(int64_t)tri->b.x/16, bz=(int64_t)tri->b.z/16;
    int64_t cx=(int64_t)tri->c.x/16, cz=(int64_t)tri->c.z/16;
    int64_t area=cross2(bx-ax,bz-az,cx-ax,cz-az);
    if (!area) return false;
    int64_t e0=cross2(bx-ax,bz-az,px-ax,pz-az);
    int64_t e1=cross2(cx-bx,cz-bz,px-bx,pz-bz);
    int64_t e2=cross2(ax-cx,az-cz,px-cx,pz-cz);
    /* One Q8 unit of tolerance, scaled by projected edge lengths.
     * Large epsilon would effectively inflate a roof for point casts. */
    int64_t epsilon=(int64_t)(
        magnitude(bx-ax)+magnitude(bz-az)+
        magnitude(cx-bx)+magnitude(cz-bz)+
        magnitude(ax-cx)+magnitude(az-cz)+1u);
    return area>0 ?
        (e0>=-epsilon && e1>=-epsilon && e2>=-epsilon) :
        (e0<=epsilon && e1<=epsilon && e2<=epsilon);
}
static uint32_t fraction_between(uint64_t numerator,uint64_t denominator)
{
    if (!denominator || numerator>=denominator) return ADV_FP_ONE;
    /* Avoid unsigned overflow before multiplying by fixed scale. */
    while (denominator>UINT64_MAX/ADV_FP_ONE) {
        numerator=(numerator+1u)/2u;
        denominator=(denominator+1u)/2u;
    }
    return (uint32_t)(numerator*ADV_FP_ONE/denominator);
}
static bool face_contact(AdventureVec3 from,AdventureVec3 to,
                         const AdventureRoofTriangle *tri,int32_t radius,
                         int64_t nx,int64_t ny,int64_t nz,
                         uint32_t *fraction)
{
    uint64_t squared=(uint64_t)(nx*nx+ny*ny+nz*nz);
    uint32_t norm=AdventureMath_IsqrtU64(squared);
    if (!norm) return false;
    int64_t d0=nx*((int64_t)from.x-tri->a.x)+
               ny*((int64_t)from.y-tri->a.y)+
               nz*((int64_t)from.z-tri->a.z);
    int64_t d1=nx*((int64_t)to.x-tri->a.x)+
               ny*((int64_t)to.y-tri->a.y)+
               nz*((int64_t)to.z-tri->a.z);
    /* The solid side is approached from below. Touching from below
     * returns t=0, but movement down/away must not sticky-collide. */
    int64_t offset=(int64_t)radius*norm;
    if (d0>0 || d1< -offset || d1<=d0) return false;
    uint32_t t=d0>=-offset ? 0u :
        fraction_between((uint64_t)(-offset-d0),(uint64_t)(d1-d0));
    AdventureVec3 point=position_at(from,to,t);
    /* Footprint is checked at the nearest point on the plane (sphere
     * touches with its top/angled normal), not at the center projection. */
    point.x=AdventureMath_ClampI32((int64_t)point.x+
                                   (int64_t)radius*nx/norm);
    point.z=AdventureMath_ClampI32((int64_t)point.z+
                                   (int64_t)radius*nz/norm);
    if (!point_in_roof(point,tri)) return false;
    *fraction=t;
    return true;
}
/* Distance squared between a moving head sphere CENTER at time t and a
 * finite triangle EDGE. The endpoint clamp handles vertex contacts too.
 * Q6 (1/64 world) is enough for an accurate Q12 contact fraction, and
 * keeps all intermediate dot products within signed int64.
 */
static uint64_t edge_distance_sq(AdventureVec3 from,AdventureVec3 to,
                                AdventureVec3 a,AdventureVec3 b,
                                uint32_t fraction)
{
    AdventureVec3 point=position_at(from,to,fraction);
    int64_t ax=(int64_t)a.x/ADV_ROOF_Q6_DIVISOR;
    int64_t ay=(int64_t)a.y/ADV_ROOF_Q6_DIVISOR;
    int64_t az=(int64_t)a.z/ADV_ROOF_Q6_DIVISOR;
    int64_t ex=((int64_t)b.x-a.x)/ADV_ROOF_Q6_DIVISOR;
    int64_t ey=((int64_t)b.y-a.y)/ADV_ROOF_Q6_DIVISOR;
    int64_t ez=((int64_t)b.z-a.z)/ADV_ROOF_Q6_DIVISOR;
    int64_t wx=(int64_t)point.x/ADV_ROOF_Q6_DIVISOR-ax;
    int64_t wy=(int64_t)point.y/ADV_ROOF_Q6_DIVISOR-ay;
    int64_t wz=(int64_t)point.z/ADV_ROOF_Q6_DIVISOR-az;
    int64_t len=ex*ex+ey*ey+ez*ez;
    int64_t dot=wx*ex+wy*ey+wz*ez;
    if (len>0 && dot>0) {
        if (dot>=len) {
            wx-=ex; wy-=ey; wz-=ez;
        } else {
            wx-=ex*dot/len;
            wy-=ey*dot/len;
            wz-=ez*dot/len;
        }
    }
    return (uint64_t)(wx*wx+wy*wy+wz*wz);
}
static bool edge_contact(AdventureVec3 from,AdventureVec3 to,
                         AdventureVec3 a,AdventureVec3 b,int32_t radius,
                         uint32_t *fraction)
{
    uint64_t r=(uint64_t)(radius+ADV_ROOF_Q6_DIVISOR-1)/
                        ADV_ROOF_Q6_DIVISOR;
    uint64_t limit=r*r;
    /* The exact distance function is convex, but fixed-point rounding
     * creates short FLAT plateaus: d(t)==d(t+1) does NOT imply its minimum
     * lies left of t. A binary discrete-derivative search could therefore
     * miss a genuine grazing contact entirely. Use ternary bracketing with
     * equality retaining the middle interval, then inspect that interval
     * exhaustively. Bounded at 4096 Q12 fractions, no floating point. */
    uint32_t lo=0,hi=ADV_ROOF_END_FRACTION;
    while (hi-lo>24u) {
        uint32_t third=(hi-lo)/3u;
        uint32_t left=lo+third,right=hi-third;
        uint64_t dl=edge_distance_sq(from,to,a,b,left);
        uint64_t dr=edge_distance_sq(from,to,a,b,right);
        if (dl<dr) hi=right;
        else if (dl>dr) lo=left;
        else { lo=left;hi=right; }
    }
    uint32_t minimum=lo;
    uint64_t min_distance=edge_distance_sq(from,to,a,b,minimum);
    for(uint32_t t=lo+1u;t<=hi;t++) {
        uint64_t d=edge_distance_sq(from,to,a,b,t);
        if(d<min_distance) { min_distance=d;minimum=t; }
    }
    if (min_distance>limit) return false;
    uint64_t initial=edge_distance_sq(from,to,a,b,0);
    if (initial<=limit) {
        /* A resting/touching head must not be trapped when leaving or
         * sliding parallel to the edge. Only return t=0 if the path moves
         * deeper into contact than the initial discrete distance. */
        if (min_distance>=initial) return false;
        *fraction=0;
        return true;
    }
    lo=0;hi=minimum;
    while (lo<hi) {
        uint32_t mid=(lo+hi)/2u;
        if (edge_distance_sq(from,to,a,b,mid)<=limit) hi=mid;
        else lo=mid+1u;
    }
    *fraction=lo;
    return true;
}
static bool broadphase(AdventureVec3 from,AdventureVec3 to,
                       const AdventureRoofTriangle *t,int32_t r)
{
    const AdventureVec3 v[3]={t->a,t->b,t->c};
    int64_t minx=v[0].x,maxx=v[0].x;
    int64_t miny=v[0].y,maxy=v[0].y;
    int64_t minz=v[0].z,maxz=v[0].z;
    for(int i=1;i<3;i++){
        if(v[i].x<minx) minx=v[i].x;
        if(v[i].x>maxx) maxx=v[i].x;
        if(v[i].y<miny) miny=v[i].y;
        if(v[i].y>maxy) maxy=v[i].y;
        if(v[i].z<minz) minz=v[i].z;
        if(v[i].z>maxz) maxz=v[i].z;
    }
    return !(((int64_t)from.x<minx-r && (int64_t)to.x<minx-r) ||
             ((int64_t)from.x>maxx+r && (int64_t)to.x>maxx+r) ||
             ((int64_t)from.y<miny-r && (int64_t)to.y<miny-r) ||
             ((int64_t)from.y>maxy+r && (int64_t)to.y>maxy+r) ||
             ((int64_t)from.z<minz-r && (int64_t)to.z<minz-r) ||
             ((int64_t)from.z>maxz+r && (int64_t)to.z>maxz+r));
}
bool AdventureRoof_SweepTriangle(AdventureVec3 from,AdventureVec3 to,
                                 const AdventureRoofTriangle *tri,
                                 int32_t radius,AdventureRoofHit *out)
{
    int64_t nx,ny,nz;
    if (!out || !tri || radius<0 || to.y<=from.y ||
        !roof_normal(tri,&nx,&ny,&nz) ||
        !broadphase(from,to,tri,radius)) return false;

    /* No underside contact if the moving head center starts above the roof
     * plane. This suppresses fake hits with floors under Sonic's feet. */
    int64_t signed_start=nx*((int64_t)from.x-tri->a.x)+
                         ny*((int64_t)from.y-tri->a.y)+
                         nz*((int64_t)from.z-tri->a.z);
    if (signed_start>0) return false;

    bool found=false;
    uint32_t first=ADV_FP_ONE;
    uint32_t t;
    if (face_contact(from,to,tri,radius,nx,ny,nz,&t)) {
        first=t;
        found=true;
    }
    if (radius>0) {
        if (edge_contact(from,to,tri->a,tri->b,radius,&t) &&
            (!found || t<first)) { first=t;found=true; }
        if (edge_contact(from,to,tri->b,tri->c,radius,&t) &&
            (!found || t<first)) { first=t;found=true; }
        if (edge_contact(from,to,tri->c,tri->a,radius,&t) &&
            (!found || t<first)) { first=t;found=true; }
    }
    if (!found) return false;
    out->fraction=(uint16_t)first;
    out->triangle_id=tri->id;
    return true;
}
bool AdventureRoof_SweepMesh(void *context,AdventureVec3 from,
                             AdventureVec3 to,int32_t radius,
                             AdventureRoofHit *out)
{
    const AdventureRoofMesh *mesh=(const AdventureRoofMesh *)context;
    if (!mesh || !out) return false;
    bool found=false;
    AdventureRoofHit best={0};
    for(uint16_t i=0;i<mesh->count;i++){
        AdventureRoofHit hit;
        if (AdventureRoof_SweepTriangle(from,to,&mesh->triangles[i],
                                        radius,&hit) &&
            (!found || hit.fraction<best.fraction)) {
            best=hit;
            found=true;
        }
    }
    if (found) *out=best;
    return found;
}
