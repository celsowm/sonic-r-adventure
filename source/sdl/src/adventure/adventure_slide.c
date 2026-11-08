/*
 * Iterative tangent-space slide for continuous fixed-point capsule sweeps.
 * Handles successive face/edge/corner contacts without throwing away the
 * unconstrained remainder of a 60-Hz tick.
 */
#include "adventure_slide.h"
#include <stdint.h>
#define ADV_SLIDE_CONTACT_LIMIT 4u
#define ADV_SLIDE_SKIN (ADV_FP_ONE / 32)

static AdventureVec3 add(AdventureVec3 a,AdventureVec3 b)
{
    AdventureVec3 c={
        AdventureMath_ClampI32((int64_t)a.x+b.x),
        AdventureMath_ClampI32((int64_t)a.y+b.y),
        AdventureMath_ClampI32((int64_t)a.z+b.z)
    };
    return c;
}
static AdventureVec3 sub(AdventureVec3 a,AdventureVec3 b)
{
    AdventureVec3 c={
        AdventureMath_ClampI32((int64_t)a.x-b.x),
        AdventureMath_ClampI32((int64_t)a.y-b.y),
        AdventureMath_ClampI32((int64_t)a.z-b.z)
    };
    return c;
}
static AdventureVec3 lerp(AdventureVec3 a,AdventureVec3 b,uint32_t t)
{
    AdventureVec3 d=sub(b,a);
    AdventureVec3 p={
        AdventureMath_ClampI32((int64_t)a.x+(int64_t)d.x*t/ADV_FP_ONE),
        AdventureMath_ClampI32((int64_t)a.y+(int64_t)d.y*t/ADV_FP_ONE),
        AdventureMath_ClampI32((int64_t)a.z+(int64_t)d.z*t/ADV_FP_ONE)
    };
    return p;
}
static AdventureVec3 scale(AdventureVec3 v,int32_t t)
{
    AdventureVec3 p={
        AdventureMath_ClampI32((int64_t)v.x*t/ADV_FP_ONE),
        AdventureMath_ClampI32((int64_t)v.y*t/ADV_FP_ONE),
        AdventureMath_ClampI32((int64_t)v.z*t/ADV_FP_ONE)
    };
    return p;
}
static AdventureVec3 clip(AdventureVec3 v,AdventureVec3 normal)
{
    /* Normalize using n.n rather than assuming the integer normal has
     * exactly 4096 length after rounded integer square roots. */
    int64_t squared=(int64_t)normal.x*normal.x+
                    (int64_t)normal.y*normal.y+
                    (int64_t)normal.z*normal.z;
    if(!squared) return v;
    int64_t dot=(int64_t)v.x*normal.x+
                (int64_t)v.y*normal.y+
                (int64_t)v.z*normal.z;
    if(dot>=0) return v;
    /* Round away from zero when removing an inward component: leaving a
     * small negative residual causes a contact loop with the same surface. */
    int64_t amount=(-dot*ADV_FP_ONE+squared-1)/squared;
    AdventureVec3 blocked=scale(normal,(int32_t)amount);
    return add(v,blocked);
}
static AdventureVec3 clip_constraints(AdventureVec3 v,
                                     const AdventureVec3 *normals,
                                     uint8_t count)
{
    for(unsigned pass=0;pass<2;pass++) {
        for(uint8_t i=0;i<count;i++)
            v=clip(v,normals[i]);
    }
    return v;
}
AdventureSlideResult AdventureSlide_Resolve(AdventureVec3 start,
                                           AdventureVec3 delta,
                                           int32_t height,int32_t radius,
                                           AdventureCapsuleQuery query,
                                           void *context)
{
    AdventureSlideResult result={0};
    result.position=start;
    result.tangent_velocity=delta;
    if(!query || (!delta.x && !delta.y && !delta.z)) {
        result.position=add(start,delta);
        return result;
    }
    AdventureVec3 normal[ADV_SLIDE_CONTACT_LIMIT]={0};
    AdventureVec3 current=start;
    AdventureVec3 remaining=delta;
    uint8_t count=0;
    for(unsigned iteration=0;iteration<ADV_SLIDE_CONTACT_LIMIT;iteration++){
        if(!remaining.x && !remaining.y && !remaining.z) break;
        AdventureVec3 target=add(current,remaining);
        AdventureCapsuleHit hit={0};
        if(!query(context,current,target,height,radius,&hit)) {
            current=target;
            remaining=(AdventureVec3){0,0,0};
            break;
        }
        uint32_t time=hit.fraction>ADV_FP_ONE?ADV_FP_ONE:hit.fraction;
        if(!result.contacts) result.first_hit=hit;
        ++result.contacts;
        AdventureVec3 impact=lerp(current,target,time);
        /* Separate very slightly to prevent repeated zero-time hits. */
        current=add(impact,scale(hit.normal,ADV_SLIDE_SKIN));
        if(count<ADV_SLIDE_CONTACT_LIMIT) normal[count++]=hit.normal;
        AdventureVec3 tail=sub(target,impact);
        remaining=clip_constraints(tail,normal,count);
        result.tangent_velocity=clip_constraints(delta,normal,count);
    }
    result.position=current;
    return result;
}
