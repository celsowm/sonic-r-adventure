/*
 * Deterministic swept horizontal-disc against static TER wall polylines.
 * A vertical capsule's footprint is a disc; world provider handles vertical
 * wall height gates. Bounded conservative substeps + binary TOI refinement
 * stop high-speed tunnelling without floating point or mutable race physics.
 */
#include "adventure_collision.h"
#include <stdint.h>
#include <limits.h>
#define ADV_SWEEP_SKIN (ADV_FP_ONE / 64)
#define ADV_SWEEP_MAX_STEPS 512
#define ADV_SWEEP_REFINE 12
#define ADV_SLIDE_MAX_CONTACTS 3
static int64_t square(int64_t v) { return v*v; }
static int32_t interpolate(int32_t a, int32_t b, uint32_t t)
{
    return AdventureMath_ClampI32((int64_t)a +
                                   ((int64_t)b-a)*t/ADV_FP_ONE);
}
/* Local calculations use Q8: enough for sub-unit accuracy, safe squared 64-bit
 * for the signed 16-bit legacy terrain coordinate envelope. */
static bool disc_overlap(int64_t px, int64_t pz, int64_t ax, int64_t az,
                         int64_t bx, int64_t bz, int64_t r,
                         int64_t *nx, int64_t *nz)
{
    int64_t vx = bx-ax, vz = bz-az;
    int64_t wx = px-ax, wz = pz-az;
    int64_t length2 = square(vx)+square(vz);
    int64_t cx = ax, cz = az;
    if (length2) {
        int64_t dot = wx*vx + wz*vz;
        if (dot <= 0) {
            cx=ax; cz=az;
        } else if (dot >= length2) {
            cx=bx; cz=bz;
        } else {
            /* Q12 projection to avoid multiplying world coordinates and
             * then overflowing a squared distance. */
            int64_t t = dot*ADV_FP_ONE/length2;
            cx = ax+vx*t/ADV_FP_ONE;
            cz = az+vz*t/ADV_FP_ONE;
        }
    }
    int64_t dx=px-cx, dz=pz-cz;
    if (square(dx)+square(dz)>square(r)) return false;
    *nx=dx;
    *nz=dz;
    return true;
}
bool AdventureCollision_SweepDiscSegment(AdventureVec3 from, AdventureVec3 to,
                                        AdventureVec3 a, AdventureVec3 b,
                                        int32_t radius, AdventureWallHit *out)
{
    if (!out || radius<=0) return false;
    /* Full swept-AABB rejection before costly point-segment checks. */
    int64_t min_x=from.x<to.x?from.x:to.x, max_x=from.x>to.x?from.x:to.x;
    int64_t min_z=from.z<to.z?from.z:to.z, max_z=from.z>to.z?from.z:to.z;
    int64_t edge_min_x=a.x<b.x?a.x:b.x, edge_max_x=a.x>b.x?a.x:b.x;
    int64_t edge_min_z=a.z<b.z?a.z:b.z, edge_max_z=a.z>b.z?a.z:b.z;
    if (max_x+radius<edge_min_x || min_x-radius>edge_max_x ||
        max_z+radius<edge_min_z || min_z-radius>edge_max_z) return false;
    /* Convert to Q8 LOCAL coordinates so dot/distances stay in int64 even
     * when the stage spans most of the signed int16 world-coordinate range. */
    int64_t ox=from.x/16, oz=from.z/16;
    int64_t ex=to.x/16, ez=to.z/16;
    int64_t ax=a.x/16, az=a.z/16, bx=b.x/16, bz=b.z/16;
    int64_t r=(radius+15)/16;
    int64_t vx=ex-ox, vz=ez-oz;
    uint32_t distance=AdventureMath_IsqrtU64((uint64_t)(square(vx)+square(vz)));
    /* Maximum step = one quarter radius, clamped at 1/8 world for exact-ish
     * thin endpoint contacts. A collision through a wall must be seen even
     * when both motion endpoints are on opposite sides of it. */
    uint32_t step=(uint32_t)(r/4);
    if (step<32u) step=32u;
    uint32_t steps=distance/step+1;
    if (steps>ADV_SWEEP_MAX_STEPS) steps=ADV_SWEEP_MAX_STEPS;
    uint32_t prev=0;
    int64_t hit_nx=0, hit_nz=0;
    bool hit=false;
    for (uint32_t i=0; i<=steps; ++i) {
        uint32_t t=i*ADV_FP_ONE/steps;
        int64_t px=ox+vx*t/ADV_FP_ONE;
        int64_t pz=oz+vz*t/ADV_FP_ONE;
        if (disc_overlap(px,pz,ax,az,bx,bz,r,&hit_nx,&hit_nz)) {
            /* Start-touch contacts are ignored when moving away; this makes
             * moving parallel to an existing surface possible. */
            if (i==0 && hit_nx*vx+hit_nz*vz>=0 && (hit_nx||hit_nz))
                return false;
            uint32_t lo=prev, hi=t;
            for (int j=0;j<ADV_SWEEP_REFINE && hi>lo+1;j++) {
                uint32_t mid=(lo+hi)/2;
                int64_t tx=ox+vx*mid/ADV_FP_ONE;
                int64_t tz=oz+vz*mid/ADV_FP_ONE;
                int64_t nx=0,nz=0;
                if (disc_overlap(tx,tz,ax,az,bx,bz,r,&nx,&nz)) hi=mid;
                else lo=mid;
            }
            out->fraction=(uint16_t)hi;
            hit=true;
            break;
        }
        prev=t;
    }
    if (!hit) return false;
    /* Recompute normal at actual time-of-impact, avoid stale last sample. */
    int64_t px=ox+vx*out->fraction/ADV_FP_ONE;
    int64_t pz=oz+vz*out->fraction/ADV_FP_ONE;
    (void)disc_overlap(px,pz,ax,az,bx,bz,r+2,&hit_nx,&hit_nz);
    if (!hit_nx && !hit_nz) {
        hit_nx=bz-az;
        hit_nz=-(bx-ax);
        if (hit_nx*vx+hit_nz*vz>0) {hit_nx=-hit_nx; hit_nz=-hit_nz;}
    }
    uint64_t len2=(uint64_t)(square(hit_nx)+square(hit_nz));
    uint32_t len=AdventureMath_IsqrtU64(len2);
    if (!len) return false;
    out->normal_x=AdventureMath_ClampI32(hit_nx*ADV_FP_ONE/len);
    out->normal_z=AdventureMath_ClampI32(hit_nz*ADV_FP_ONE/len);
    out->wall_id=-1;
    return true;
}
/*
 * The original .TER wall top is an interpolated *negative* legacy Y value.
 * On the adventure side it becomes positive-up. Using the signed legacy
 * height directly would skip every gated wall on Resort Island (all 225
 * gated-edge vertex heights observed in the supplied original TER are <= 0).
 *
 * Project the actual horizontal impact point onto the finite segment to
 * avoid an endpoint-only height approximation on sloping wall tops.
 */
bool AdventureCollision_AboveLegacyWallTop(
    int32_t foot_y, AdventureVec3 from, AdventureVec3 to,
    AdventureVec3 edge_a, AdventureVec3 edge_b,
    int16_t legacy_y_a, int16_t legacy_y_b, uint16_t impact_fraction)
{
    if (impact_fraction > ADV_FP_ONE) impact_fraction = ADV_FP_ONE;
    int64_t px=(int64_t)from.x+
        ((int64_t)to.x-from.x)*impact_fraction/ADV_FP_ONE;
    int64_t pz=(int64_t)from.z+
        ((int64_t)to.z-from.z)*impact_fraction/ADV_FP_ONE;

    /* Q4 projection retains 1/16-unit precision while keeping
     * (dot * 4096) within signed 64-bit bounds on native 16-bit TER vertices.
     * Avoid __int128 and libm on Dreamcast SH-4. */
    int64_t ax=(int64_t)edge_a.x/256;
    int64_t az=(int64_t)edge_a.z/256;
    int64_t dx=((int64_t)edge_b.x-edge_a.x)/256;
    int64_t dz=((int64_t)edge_b.z-edge_a.z)/256;
    int64_t wx=px/256-ax, wz=pz/256-az;
    int64_t denom=dx*dx+dz*dz;
    int64_t fraction=0;
    if (denom>0) {
        int64_t dot=wx*dx+wz*dz;
        if (dot>=denom) fraction=ADV_FP_ONE;
        else if (dot>0) fraction=dot*ADV_FP_ONE/denom;
    }
    int64_t legacy_top=(int64_t)legacy_y_a*ADV_FP_ONE+
         ((int64_t)legacy_y_b-legacy_y_a)*fraction;
    int64_t canonical_top=-legacy_top;
    return (int64_t)foot_y>canonical_top;
}

void AdventureCollision_MoveAndSlide(AdventureVec3 *position,
                                    AdventureVec3 *horizontal_velocity,
                                    AdventureWallQuery query, void *context,
                                    int32_t radius, int32_t capsule_height,
                                    int32_t layer)
{
    if (!position || !horizontal_velocity) return;
    AdventureVec3 start=*position;
    AdventureVec3 origin=start;
    AdventureVec3 remainder={horizontal_velocity->x,0,horizontal_velocity->z};
    if (!query) {
        position->x=AdventureMath_ClampI32((int64_t)start.x+remainder.x);
        position->z=AdventureMath_ClampI32((int64_t)start.z+remainder.z);
        return;
    }
    for (int contact=0;contact<ADV_SLIDE_MAX_CONTACTS;contact++) {
        if (!remainder.x && !remainder.z) break;
        AdventureVec3 target=start;
        target.x=AdventureMath_ClampI32((int64_t)start.x+remainder.x);
        target.z=AdventureMath_ClampI32((int64_t)start.z+remainder.z);
        AdventureWallHit hit;
        if (!query(context,start,target,radius,capsule_height,layer,&hit)) {
            start=target;
            remainder.x=remainder.z=0;
            break;
        }
        uint32_t t=hit.fraction>ADV_FP_ONE?ADV_FP_ONE:hit.fraction;
        /* Move until impact, then separate a tiny epsilon against the wall. */
        start.x=interpolate(start.x,target.x,t);
        start.z=interpolate(start.z,target.z,t);
        start.x=AdventureMath_ClampI32((int64_t)start.x+
                       (int64_t)hit.normal_x*ADV_SWEEP_SKIN/ADV_FP_ONE);
        start.z=AdventureMath_ClampI32((int64_t)start.z+
                       (int64_t)hit.normal_z*ADV_SWEEP_SKIN/ADV_FP_ONE);
        remainder.x=(int32_t)(((int64_t)remainder.x*(ADV_FP_ONE-t))/ADV_FP_ONE);
        remainder.z=(int32_t)(((int64_t)remainder.z*(ADV_FP_ONE-t))/ADV_FP_ONE);
        int64_t into=((int64_t)remainder.x*hit.normal_x+
                      (int64_t)remainder.z*hit.normal_z)/ADV_FP_ONE;
        if (into<0) {
            remainder.x=AdventureMath_ClampI32((int64_t)remainder.x-
                             into*hit.normal_x/ADV_FP_ONE);
            remainder.z=AdventureMath_ClampI32((int64_t)remainder.z-
                             into*hit.normal_z/ADV_FP_ONE);
        }
    }
    position->x=start.x;
    position->z=start.z;
    /* Actual displacement, not the original attempted movement. */
    horizontal_velocity->x=AdventureMath_ClampI32((int64_t)start.x-origin.x);
    horizontal_velocity->z=AdventureMath_ClampI32((int64_t)start.z-origin.z);
}
