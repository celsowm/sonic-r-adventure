#include "adventure_math.h"
#include <limits.h>
uint32_t AdventureMath_IsqrtU64(uint64_t n)
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
/* Heading is quantized to Sonic R's 12-bit turn convention.
 * atan(r) ~= (pi/4)r + 0.273r(1-r), with maximum angular error
 * well below one degree across the 0..45 degree octant.
 * All operations are integer and deterministic on SH-4 and desktop. */
uint16_t AdventureMath_HeadingXZ(int32_t x, int32_t z)
{
    uint64_t ax = x < 0 ? (uint64_t)(-(int64_t)x) : (uint64_t)x;
    uint64_t az = z < 0 ? (uint64_t)(-(int64_t)z) : (uint64_t)z;
    if (!ax && !az) return 0;
    uint64_t major = ax >= az ? ax : az;
    uint64_t minor = ax >= az ? az : ax;
    uint32_t r = (uint32_t)(minor * 4096u / major);
    uint32_t angle = (uint32_t)((512u * (uint64_t)r) / 4096u +
                   (178u * (uint64_t)r * (4096u-r)) / (4096u*4096u));
    if (az > ax) angle = 1024u - angle;
    if (x < 0 && z >= 0) angle = 2048u - angle;
    else if (x < 0 && z < 0) angle = 2048u + angle;
    else if (z < 0) angle = (4096u - angle) & 4095u;
    return (uint16_t)(angle & 4095u);
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
bool AdventureMath_GridCell(int32_t coordinate, int32_t origin_world,
                            int32_t span_world, int32_t count, int32_t *cell)
{
    if (!cell || span_world <= 0 || count <= 0) return false;
    int64_t offset = (int64_t)coordinate - (int64_t)origin_world * ADV_FP_ONE;
    if (offset < 0) return false;
    int64_t index = offset / ((int64_t)span_world * ADV_FP_ONE);
    if (index >= count) return false;
    *cell = (int32_t)index;
    return true;
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
    uint32_t magnitude = AdventureMath_IsqrtU64((uint64_t)(ax*ax + az*az));
    if (!magnitude) {
        AdventureBasis fallback = {ADV_FP_ONE, 0, 0, ADV_FP_ONE};
        return fallback;
    }
    int32_t fx = AdventureMath_ClampI32(ax * ADV_FP_ONE / magnitude);
    int32_t fz = AdventureMath_ClampI32(az * ADV_FP_ONE / magnitude);
    AdventureBasis basis = {fx, fz, -fz, fx};
    return basis;
}
