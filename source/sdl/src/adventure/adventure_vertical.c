/*
 * Sweep Sonic's head/feet along the ACTUAL horizontal and vertical trajectory.
 * The old single-point query at final X/Z could skip thin platforms whenever
 * both ends of a tick were outside a polygon but the trajectory crossed it.
 *
 * Sample no farther than 1/4 world unit horizontally (bounded 32 steps per
 * tick); refine first contact using binary search in Q12 time. World geometry
 * is read through injected callbacks, not directly through Sonic R globals.
 * This remains a sampled heightfield contact, NOT an exact capsule/triangle
 * time-of-impact algorithm.
 */
#include "adventure_vertical.h"
#include <stdint.h>
#define ADV_VERTICAL_MAX_STEPS 32u
#define ADV_VERTICAL_SAMPLE_Q12 (ADV_FP_ONE/4)
#define ADV_VERTICAL_REFINE 9
#define ADV_VERTICAL_HEAD_SKIN (ADV_FP_ONE/64)

static int32_t at(int32_t from, int32_t to, uint32_t t)
{
    return AdventureMath_ClampI32((int64_t)from +
                                  ((int64_t)to-from)*t/ADV_FP_ONE);
}
static AdventureVec3 position_at(AdventureVec3 from,AdventureVec3 to,
                                 uint32_t t)
{
    AdventureVec3 point={at(from.x,to.x,t),
                         at(from.y,to.y,t),
                         at(from.z,to.z,t)};
    return point;
}
static bool crossed(AdventureTerrain terrain,AdventureVec3 from,
                    AdventureVec3 to,int32_t body_height,bool rising,
                    uint32_t t,AdventureGroundHit *hit)
{
    AdventureVec3 p=position_at(from,to,t);
    if (rising) {
        if (!terrain.query_overhead) return false;
        /* A floor already below the original head is not a ceiling.
         * Starting at the exact head height is allowed and blocks ascent. */
        int32_t start_head=AdventureMath_ClampI32(
            (int64_t)from.y+body_height);
        int32_t head=AdventureMath_ClampI32((int64_t)p.y+body_height);
        return terrain.query_overhead(terrain.context,p.x,p.z,start_head,hit) &&
               hit->height<=head;
    }
    if (!terrain.query_ground) return false;
    /* No landing on surfaces which started above the foot. */
    return terrain.query_ground(terrain.context,p.x,p.z,from.y,hit) &&
           hit->height>=p.y;
}
bool AdventureVertical_Sweep(AdventureTerrain terrain,
                             AdventureVec3 from, AdventureVec3 to,
                             int32_t capsule_height, int32_t capsule_radius,
                             bool rising, AdventureVerticalContact *contact)
{
    if (!contact || capsule_height<=0 || capsule_radius<0 ||
        (int64_t)capsule_radius*2>capsule_height || (rising ? to.y<=from.y
                                                 : to.y>=from.y)) return false;
    int64_t dx=(int64_t)to.x-from.x, dz=(int64_t)to.z-from.z;
    uint32_t dist=AdventureMath_IsqrtU64((uint64_t)(dx*dx+dz*dz));
    uint32_t samples=dist/ADV_VERTICAL_SAMPLE_Q12+1u;
    if (samples>ADV_VERTICAL_MAX_STEPS)
        samples=ADV_VERTICAL_MAX_STEPS;
    uint32_t previous_t=0;
    bool found=false;
    AdventureVerticalContact best={0};
    for(uint32_t i=1;i<=samples;i++){
        uint32_t t=i*ADV_FP_ONE/samples;
        AdventureGroundHit hit;
        if (crossed(terrain,from,to,capsule_height,rising,t,&hit)) {
            /* The sweep is a sequence of discrete X/Z footprint checks. The
             * first positive check brackets a contact; refine within it. */
            uint32_t lo=previous_t,hi=t;
            for(int j=0;j<ADV_VERTICAL_REFINE && lo+1u<hi;j++){
                uint32_t mid=(lo+hi)/2;
                AdventureGroundHit mid_hit;
                if (crossed(terrain,from,to,capsule_height,rising,
                            mid,&mid_hit))
                    hi=mid;
                else
                    lo=mid;
            }
            /* Refined point may change which stacked floor is nearest. */
            AdventureGroundHit refined;
            if (crossed(terrain,from,to,capsule_height,rising,hi,&refined))
                hit=refined;
            best.fraction=(uint16_t)hi;
            best.surface=hit;
            best.position=position_at(from,to,hi);
            if (rising) {
                int32_t ceiling=AdventureMath_ClampI32(
                    (int64_t)hit.height-capsule_height-ADV_VERTICAL_HEAD_SKIN);
                /* Starting head contact must not push Sonic downwards. */
                best.position.y=ceiling>from.y?ceiling:from.y;
            }else{
                best.position.y=hit.height;
            }
            found=true;
            break;
        }
        previous_t=t;
    }
    if (rising && terrain.sweep_capsule) {
        AdventureCapsuleHit hit;
        if (terrain.sweep_capsule(terrain.context,from,to,capsule_height,
                                  capsule_radius,&hit) &&
            (!found || hit.fraction<best.fraction)) {
            best.fraction=hit.fraction;
            best.position=position_at(from,to,hit.fraction);
            /* The whole capsule has a first-contact normal: separate
             * along that normal rather than incorrectly forcing vertical
             * displacement at a SIDE or lower-endcap contact. */
            best.position.x=AdventureMath_ClampI32(
                (int64_t)best.position.x+
                (int64_t)hit.normal.x*ADV_VERTICAL_HEAD_SKIN/ADV_FP_ONE);
            best.position.y=AdventureMath_ClampI32(
                (int64_t)best.position.y+
                (int64_t)hit.normal.y*ADV_VERTICAL_HEAD_SKIN/ADV_FP_ONE);
            best.position.z=AdventureMath_ClampI32(
                (int64_t)best.position.z+
                (int64_t)hit.normal.z*ADV_VERTICAL_HEAD_SKIN/ADV_FP_ONE);
            best.surface.height=AdventureMath_ClampI32(
                (int64_t)best.position.y+capsule_height);
            best.surface.surface_id=-2-hit.triangle_id;
            best.surface.layer=-1;
            found=true;
        }
    }
    if (found) *contact=best;
    return found;
}
