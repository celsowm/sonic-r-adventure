/* Render-only adapter: Action camera Q12 -> legacy viewport camera matrices.
 * Plus a read-only TER/approved-BIN ground/wall ray occlusion provider. */
#ifndef SONICR_ADVENTURE_CAMERA_LEGACY_H
#define SONICR_ADVENTURE_CAMERA_LEGACY_H
#include "adventure_camera.h"
#include "adventure_terrain.h"
bool AdventureCameraLegacy_Trace(void *context, AdventureVec3 target,
                                  AdventureVec3 eye,int32_t collision_layer,
                                  uint16_t *fraction);
void AdventureCameraLegacy_Apply(const AdventureCamera *camera);
/* Scoped dynamic FOV; restore global projection scales after drawing. */
void AdventureCameraLegacy_BeginProjection(const AdventureCamera *camera,
                                           int *saved_x,int *saved_y);
void AdventureCameraLegacy_EndProjection(int saved_x,int saved_y);
#endif
