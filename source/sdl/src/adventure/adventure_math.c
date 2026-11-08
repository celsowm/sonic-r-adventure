#include "adventure_math.h"
#include <limits.h>
static uint32_t isqrt64(uint64_t n)
{
    uint64_t bit = (uint64_t)1 << 62, result = 0;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= result + bit) {
            n -= result + bit;
            result = (result >> 1) + bit;
        } else result >>= 1;
        bit >>= 2;
    }
    return (uint32_t)result;
}
int32_t AdventureMath_ClampI32(int64_t v)
{
    return v > INT32_MAX ? INT32_MAX : v < INT32_MIN ? INT32_MIN : (int32_t)v;
}
AdventureVec3 AdventureMath_FromLegacy(int32_t x, int32_t y, int32_t z)
{
    AdventureVec3 v = {x, AdventureMath_ClampI32(-(int64_t)y), z};
    return v;
}
void AdventureMath_ToLegacy(AdventureVec3 v, int32_t *x, int32_t *y, int32_t *z)
{
    if (x) *x = v.x;
    if (y) *y = AdventureMath_ClampI32(-(int64_t)v.y);
    if (z) *z = v.z;
}
int32_t AdventureMath_Approach(int32_t value, int32_t target, int32_t delta)
{
    if (delta < 0) delta = 0;
    if (value < target) return (int32_t)(((int64_t)target-value < delta) ? target : (int64_t)value+delta);
    if (value > target) return (int32_t)(((int64_t)value-target < delta) ? target : (int64_t)value-delta);
    return value;
}
AdventureBasis AdventureMath_CameraBasis(int32_t player_x, int32_t player_z,
                                        int32_t camera_x, int32_t camera_z)
{
    /* Q20.12 differences: divide by 4096 first to avoid huge distance squares
     * while preserving sufficient accuracy for the camera follow position. */
    int64_t dx = (int64_t)player_x - camera_x;
    int64_t dz = (int64_t)player_z - camera_z;
    int64_t ax = dx / ADV_FP_ONE, az = dz / ADV_FP_ONE;
    if (ax > INT32_MAX) ax = INT32_MAX;
    if (ax < INT32_MIN) ax = INT32_MIN;
    if (az > INT32_MAX) az = INT32_MAX;
    if (az < INT32_MIN) az = INT32_MIN;
    uint32_t magnitude = isqrt64((uint64_t)(ax*ax + az*az));
    if (!magnitude) {
        AdventureBasis fallback = {ADV_FP_ONE, 0, 0, ADV_FP_ONE};
        return fallback;
    }
    int32_t fx = AdventureMath_ClampI32(ax * ADV_FP_ONE / magnitude);
    int32_t fz = AdventureMath_ClampI32(az * ADV_FP_ONE / magnitude);
    AdventureBasis basis = {fx, fz, -fz, fx};
    return basis;
}
