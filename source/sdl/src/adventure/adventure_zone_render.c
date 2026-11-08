/*
 * Draw authored static extension surfaces after the original track and
 * before characters. Same shared R_Draw primitives on SDL/OpenGL and DC/PVR.
 * No runtime OBJ/glTF or proprietary asset loading. Ice-demo uses an
 * intentionally obvious solid color until the proper material pack exists.
 */
#include "adventure_zone_render.h"
#include "sonicr_globals.h"
#include "nearclip.h"
#include "r_state.h"
#include "r_draw.h"
#include <stdint.h>

static NearClipVert camera_vertex(AdventureVec3 position)
{
    int64_t dx=(int64_t)position.x/ADV_FP_ONE-g_camIntX;
    int64_t dy=(int64_t)position.y/ADV_FP_ONE-g_camIntY;
    int64_t dz=(int64_t)position.z/ADV_FP_ONE-g_camIntZ;
    int64_t mx=(int64_t)g_viewMtx00*dx+(int64_t)g_viewMtx20*dz;
    int64_t my=(int64_t)g_viewMtx01*dx+(int64_t)g_viewMtx11*dy+
               (int64_t)g_viewMtx21*dz;
    int64_t mz=(int64_t)g_viewMtx02*dx+(int64_t)g_viewMtx12*dy+
               (int64_t)g_viewMtx22*dz;
    NearClipVert v={0};
    v.camX=AdventureMath_ClampI32(mx/ADV_FP_ONE);
    v.camY=AdventureMath_ClampI32(my/ADV_FP_ONE);
    v.depth=AdventureMath_ClampI32(mz/ADV_FP_ONE);
    v.colorR=0x8C*8192;
    v.colorG=0xE3*8192;
    v.colorB=0xFA*8192;
    return v;
}
static RenderVertex projected(const NearClipVert *source,float far_clip)
{
    RenderVertex v={0};
    float depth=source->depth>0?(float)source->depth:1.0f;
    v.sx=(float)g_screenCenterX+
         (float)g_projScaleXCurrent*(float)source->camX/depth;
    v.sy=(float)g_screenCenterY-
         (float)g_projScaleY*(float)source->camY/depth;
    v.sz=depth/far_clip;
    v.rhw=1.0f/depth;
    v.color=0xFF8CE3FAu;
    return v;
}
void AdventureZoneRender_Draw(const AdventureWorld *world)
{
    if(!world || !AdventureWorld_ActiveTriangles(world)) return;
    float far_clip=g_farClipFloat>1.0f?g_farClipFloat:88064.0f;
    R_PushState();
    R_SetTexture(-1);
    R_SetBlendMode(R_BLEND_NONE);
    R_SetDepthTest(1);
    R_SetDepthWrite(1);
    R_SetDepthFunc(R_DEPTH_LEQUAL);
    R_SetCullMode(R_CULL_NONE);
    for(uint16_t zone_index=0;zone_index<world->zone_count;zone_index++){
        const AdventureZone *zone=&world->zones[zone_index];
        if(!zone->active) continue;
        for(uint16_t j=0;j<zone->triangle_count;j++){
            const AdventureRoofTriangle *t=
                &world->triangles.triangles[zone->first_triangle+j];
            NearClipVert source[3]={
                camera_vertex(t->a),camera_vertex(t->b),
                camera_vertex(t->c)
            };
            /* All points behind the camera or the far plane: skip.
             * Partially near-clipped geometry must NOT disappear. */
            int behind=0,beyond=0;
            for(int k=0;k<3;k++){
                if(source[k].depth<1) ++behind;
                if((float)source[k].depth>far_clip) ++beyond;
            }
            if(behind==3 || beyond==3) continue;
            NearClipVert visible[4];
            int n=behind?NearClipPolygon(source,3,visible):3;
            if(!behind) {
                for(int k=0;k<3;k++) visible[k]=source[k];
            }
            if(n<3) continue;
            RenderVertex draw[4];
            for(int k=0;k<n;k++)
                draw[k]=projected(&visible[k],far_clip);
            if(n==3) R_DrawTri(draw);
            else if(n==4) R_DrawQuad(draw);
        }
    }
    R_PopState();
}
