/* Portable Q20.12 world coordinates and Q12 basis helpers. */
#ifndef SONICR_ADVENTURE_MATH_H
#define SONICR_ADVENTURE_MATH_H
#include <stdint.h>
#define ADV_FP_SHIFT 12
#define ADV_FP_ONE (1 << ADV_FP_SHIFT)
typedef struct AdventureVec3 { int32_t x, y, z; } AdventureVec3;
typedef struct AdventureBasis { int32_t forward_x, forward_z; int32_t right_x, right_z; } AdventureBasis;
/* Legacy X/Z are Q20.12; legacy Y is negative-up Q20.12. */
AdventureVec3 AdventureMath_FromLegacy(int32_t x, int32_t y, int32_t z);
void AdventureMath_ToLegacy(AdventureVec3 v, int32_t *x, int32_t *y, int32_t *z);
int32_t AdventureMath_ClampI32(int64_t v);
int32_t AdventureMath_Approach(int32_t value, int32_t target, int32_t delta);
/* All basis vectors are Q12. Degenerate camera offset uses +X forward. */
AdventureBasis AdventureMath_CameraBasis(int32_t player_x, int32_t player_z,
                                        int32_t camera_x, int32_t camera_z);
#endif
