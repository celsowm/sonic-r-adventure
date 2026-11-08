#include "adventure_slope.h"
#include <stdint.h>
#include <limits.h>

bool AdventureSlope_Normalize(int64_t x, int64_t y, int64_t z,
                               AdventureVec3 *out)
{
    if(!out) return false;
    /* Avoid 64-bit squared overflow for arbitrary or corrupted inputs. */
    if(x < -100000000 || x > 100000000 ||
       y < -100000000 || y > 100000000 ||
       z < -100000000 || z > 100000000) return false;
    uint64_t sq=(uint64_t)(x*x+y*y+z*z);
    if(!sq) return false;
    uint32_t len=AdventureMath_IsqrtU64(sq);
    if(!len) return false;
    /* Keep normal positive-up irrespective of original face winding. */
    if(y<0){x=-x;y=-y;z=-z;}
    out->x=AdventureMath_ClampI32(x*ADV_FP_ONE/len);
    out->y=AdventureMath_ClampI32(y*ADV_FP_ONE/len);
    out->z=AdventureMath_ClampI32(z*ADV_FP_ONE/len);
    return true;
}
AdventureVec3 AdventureSlope_Normal(AdventureTerrain terrain,
                                    int32_t x,int32_t z,int32_t surface_id)
{
    AdventureVec3 normal={0,ADV_FP_ONE,0};
    if(terrain.query_normal) {
        AdventureVec3 queried;
        if(terrain.query_normal(terrain.context,x,z,surface_id,&queried) &&
           queried.y>=0 && queried.y<=ADV_FP_ONE &&
           (int64_t)queried.x*queried.x+
           (int64_t)queried.y*queried.y+
           (int64_t)queried.z*queried.z>0)
            normal=queried;
    }
    return normal;
}
bool AdventureSlope_Walkable(AdventureVec3 normal)
{
    return normal.y>=ADV_WALKABLE_NORMAL_Y;
}
bool AdventureSlope_QuerySupport(AdventureTerrain terrain,
                                 int32_t x,int32_t z,int32_t maximum,
                                 AdventureGroundHit *out,
                                 AdventureVec3 *normal)
{
    if(!terrain.query_ground || !out) return false;
    int32_t ceiling=maximum;
    for(int i=0;i<ADV_SLOPE_MAX_PROBES;i++){
        AdventureGroundHit hit;
        if(!terrain.query_ground(terrain.context,x,z,ceiling,&hit))
            return false;
        /* Protect against callbacks violating the upper-height contract. */
        if(hit.height>ceiling) return false;
        AdventureVec3 n=AdventureSlope_Normal(terrain,x,z,hit.surface_id);
        if(AdventureSlope_Walkable(n)) {
            *out=hit;
            if(normal) *normal=n;
            return true;
        }
        if(hit.height==INT32_MIN) return false;
        ceiling=hit.height-1;
    }
    return false;
}
AdventureVec3 AdventureSlope_Downhill(AdventureVec3 normal)
{
    AdventureVec3 bias={0,0,0};
    if(AdventureSlope_Walkable(normal) || normal.y<=0)
        return bias;
    /* For plane n.p=c, the horizontal projection of +n is downhill.
     * Magnitude is a small, bounded displacement each fixed tick. */
    bias.x=AdventureMath_ClampI32(
        (int64_t)normal.x*ADV_SLOPE_DOWNHILL_STEP/ADV_FP_ONE);
    bias.z=AdventureMath_ClampI32(
        (int64_t)normal.z*ADV_SLOPE_DOWNHILL_STEP/ADV_FP_ONE);
    return bias;
}
