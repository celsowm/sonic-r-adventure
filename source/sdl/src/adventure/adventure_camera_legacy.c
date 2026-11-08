/*
 * The race camera recalculates its own target/distance in BuildCameraView,
 * so calling BuildCameraViewForViewport here would override the new action
 * camera every render frame. Instead fill the established RenderCamera
 * output, then invoke the SAME matrix/viewport pipeline as the race view.
 *
 * Core camera and physics remain fixed-point. Float conversion is permitted
 * only here, at the rendering boundary (and matches BuildCameraView).
 */
#include "adventure_camera_legacy.h"
#include "sonicr_types.h"
#include "sonicr_globals.h"
#include "sonicr_functions.h"
#include <stdint.h>

extern void SetViewportClipRect(int *camStruct);

/* The camera ray runs through the authored collision world only. A bounded
 * foot/overhead sample is used for native TER heightfields, while native
 * walls and approved BIN capsule triangles provide swept contacts. */
static AdventureVec3 point_on_ray(AdventureVec3 from,AdventureVec3 to,
                                  uint32_t fraction)
{
    AdventureVec3 p={
        AdventureMath_ClampI32((int64_t)from.x+
            ((int64_t)to.x-from.x)*fraction/ADV_FP_ONE),
        AdventureMath_ClampI32((int64_t)from.y+
            ((int64_t)to.y-from.y)*fraction/ADV_FP_ONE),
        AdventureMath_ClampI32((int64_t)from.z+
            ((int64_t)to.z-from.z)*fraction/ADV_FP_ONE)
    };
    return p;
}
static bool near_terrain(const AdventureTerrain *terrain,AdventureVec3 p)
{
    const int32_t radius=ADV_FP_ONE/2;
    AdventureGroundHit hit;
    int32_t top=AdventureMath_ClampI32((int64_t)p.y+radius);
    int32_t bottom=AdventureMath_ClampI32((int64_t)p.y-radius);
    if(terrain->query_ground &&
       terrain->query_ground(terrain->context,p.x,p.z,top,&hit) &&
       hit.height>=bottom && hit.height<=top)
        return true;
    /* A two-sided elevated TER floor may be above the ray. The lower
     * ground query cannot see it unless it has already crossed the plane. */
    if(terrain->query_overhead &&
       terrain->query_overhead(terrain->context,p.x,p.z,bottom,&hit) &&
       hit.height<=top && hit.height>=bottom)
        return true;
    return false;
}
bool AdventureCameraLegacy_Trace(void *context,AdventureVec3 target,
                                  AdventureVec3 eye,int32_t layer,
                                  uint16_t *out)
{
    const AdventureTerrain *terrain=(const AdventureTerrain *)context;
    if(!terrain || !out) return false;
    uint32_t earliest=ADV_FP_ONE+1u;
    if(terrain->sweep_wall) {
        AdventureWallHit hit;
        if(terrain->sweep_wall(terrain->context,target,eye,
                               ADV_FP_ONE/2,2*ADV_FP_ONE,layer,&hit))
            earliest=hit.fraction;
    }
    if(terrain->sweep_capsule) {
        AdventureCapsuleHit hit;
        if(terrain->sweep_capsule(terrain->context,target,eye,
                                  ADV_FP_ONE,ADV_FP_ONE/2,&hit) &&
           hit.fraction<earliest)
            earliest=hit.fraction;
    }
    if(terrain->query_ground || terrain->query_overhead) {
        /* Camera is at most ~12 units behind Sonic: 48 fixed samples
         * give at most 1/4 world unit per step on its ideal view ray.
         * This is still a conservative sampled heightfield test, NOT an
         * exact ray/triangle intersection for scenery. */
        const uint32_t probes=48u;
        uint32_t previous=0u;
        for(uint32_t i=1u;i<=probes;i++){
            uint32_t t=i*ADV_FP_ONE/probes;
            if(t>=earliest) break;
            if(near_terrain(terrain,point_on_ray(target,eye,t))) {
                /* Refine first heightfield contact within the bracket.
                 * No unbounded recursion or per-frame allocations. */
                uint32_t low=previous,high=t;
                for(int pass=0;pass<8;pass++) {
                    uint32_t mid=(low+high)/2u;
                    if(mid==low||mid==high) break;
                    if(near_terrain(terrain,
                                    point_on_ray(target,eye,mid)))
                        high=mid;
                    else
                        low=mid;
                }
                if(high<earliest) earliest=high;
                break;
            }
            previous=t;
        }
    }
    if(earliest>ADV_FP_ONE) return false;
    *out=(uint16_t)earliest;
    return true;
}
void AdventureCameraLegacy_Apply(const AdventureCamera *camera)
{
    if(!camera || !camera->initialized) return;
    RenderCamera *rc=(RenderCamera *)g_viewportConfigArray;
    /* RenderCamera.worldX/Y/Z use player-world Q12. The *CamStateEntry*
     * X/Z use opposite signs (a pre-render convention). */
    rc->worldX=camera->position.x;
    rc->worldY=camera->position.y;
    rc->worldZ=camera->position.z;
    rc->intX=rc->worldX/ADV_FP_ONE;
    rc->intY=rc->worldY/ADV_FP_ONE;
    rc->intZ=rc->worldZ/ADV_FP_ONE;

    int32_t dx=AdventureMath_ClampI32(
        (int64_t)camera->look_at.x-camera->position.x);
    int32_t dy=AdventureMath_ClampI32(
        (int64_t)camera->look_at.y-camera->position.y);
    int32_t dz=AdventureMath_ClampI32(
        (int64_t)camera->look_at.z-camera->position.z);
    uint16_t heading=AdventureMath_HeadingXZ(dx,dz);
    rc->yaw=(3072-heading)&4095;

    int64_t hdx=(int64_t)dx/ADV_FP_ONE,hdz=(int64_t)dz/ADV_FP_ONE;
    uint32_t horizontal=AdventureMath_IsqrtU64(
        (uint64_t)(hdx*hdx+hdz*hdz));
    int64_t vertical=(int64_t)dy/ADV_FP_ONE;
    if(vertical<0) vertical=-vertical;
    if(horizontal>INT32_MAX) horizontal=INT32_MAX;
    if(vertical>INT32_MAX) vertical=INT32_MAX;
    int32_t pitch=AdventureMath_HeadingXZ(
        (int32_t)horizontal,(int32_t)vertical);
    rc->pitch=dy<0?pitch:((4096-pitch)&4095);

    rc->camFloatX=(float)rc->worldX/(float)ADV_FP_ONE;
    rc->camFloatY=(float)rc->worldY/(float)ADV_FP_ONE;
    rc->camFloatZ=(float)rc->worldZ/(float)ADV_FP_ONE;
    /* Frame presentation only; same scale as original render_scene.c. */
    const float angle_scale=0.0015339807878856412f;
    rc->yawFloat=(float)rc->yaw*angle_scale;
    rc->pitchFloat=(float)rc->pitch*angle_scale;

    /* Keep informational legacy camera state coherent for rendering code
     * that still reads it, but never call the race chase camera again. */
    g_camStateTable[0].posX=AdventureMath_ClampI32(
        -(int64_t)camera->position.x/256);
    g_camStateTable[0].posY=camera->position.y/256;
    g_camStateTable[0].posZ=AdventureMath_ClampI32(
        -(int64_t)camera->position.z/256);
    g_camStateTable[0].smoothYaw=(short)rc->yaw;
    g_camStateTable[0].smoothPitch=(short)((4096-rc->pitch)&4095);
    g_camStateTable[0].targetYaw=g_camStateTable[0].smoothYaw;
    g_camStateTable[0].targetPitch=g_camStateTable[0].smoothPitch;

    SetCameraStructPtr((int *)rc);
    BuildViewMatrix();
    ComputeCameraBasis();
    BuildD3DViewMatrix();
    SetViewportClipRect((int *)rc);
    g_currentRenderCam=(int *)rc;
}

/* The native 3D track is CPU-projected into already transformed screen
 * vertices, so scaling the OPENGL projection matrix would do nothing.
 * Temporarily scale the game's own integer perspective factors instead.
 * Scope matters: neither racing views nor later frames inherit this zoom. */
void AdventureCameraLegacy_BeginProjection(const AdventureCamera *camera,
                                           int *saved_x,int *saved_y)
{
    if(!saved_x || !saved_y) return;
    *saved_x=g_projScaleXCurrent;
    *saved_y=g_projScaleY;
    if(!camera || !camera->initialized) return;
    int32_t zoom=camera->zoom_q12;
    if(zoom<ADV_CAMERA_ZOOM_MIN) zoom=ADV_CAMERA_ZOOM_MIN;
    if(zoom>ADV_FP_ONE) zoom=ADV_FP_ONE;
    g_projScaleXCurrent=AdventureMath_ClampI32(
        (int64_t)*saved_x*zoom/ADV_FP_ONE);
    g_projScaleY=AdventureMath_ClampI32(
        (int64_t)*saved_y*zoom/ADV_FP_ONE);
}
void AdventureCameraLegacy_EndProjection(int saved_x,int saved_y)
{
    g_projScaleXCurrent=saved_x;
    g_projScaleY=saved_y;
}
