/*
 * Extract ONLY explicitly approved track-part roof faces from the already
 * loaded, world-space BIN render vertices. This never treats generic scenery,
 * decals, translucent quads or objects as solid by heuristic.
 *
 * Authoring: after visual verification of a candidate, add one record
 * { TRACK_RESORT_ISLAND, part_index, face_index } to the list below.
 * The zero-stage sentinel must stay last. Each quad is two triangles.
 *
 * No roof entries have been approved yet, so the default game remains
 * unaffected by visual-mesh collision until a genuine review.
 */
#include "adventure_roof_legacy.h"
#include "adventure_terrain_legacy.h"
#include <limits.h>
#include "sonicr_types.h"
#include "sonicr_globals.h"
#include <stdint.h>

extern int g_objectCount;
extern int g_objectVertexCount;

typedef struct AdventureRoofSelection {
    uint16_t track_id;
    uint16_t part_index;
    uint16_t face_index;
} AdventureRoofSelection;

static const AdventureRoofSelection s_selected_roofs[] = {
    {0, 0, 0} /* Sentinel; DO NOT auto-convert arbitrary BIN geometry. */
};

static AdventureVec3 convert_vertex(const SrcVertex *v)
{
    /* BIN render vertices are whole world units and POSITIVE-up.
     * The TER floor vertices and original Player.posY are negative-up.
     * The supplied original Island BIN/TER share >1600 opposite-Y
     * world-space vertex matches, confirming this explicit conversion. */
    AdventureVec3 out={
        AdventureMath_ClampI32((int64_t)v->posX*ADV_FP_ONE),
        AdventureMath_ClampI32((int64_t)v->posY*ADV_FP_ONE),
        AdventureMath_ClampI32((int64_t)v->posZ*ADV_FP_ONE)};
    return out;
}
void AdventureRoofLegacy_LoadSelected(AdventureRoofMesh *mesh,int track_id)
{
    if (!mesh) return;
    AdventureRoof_Clear(mesh);
    if (!g_objectStructArray || !g_polygonArrayBase || !g_vertexArrayBase ||
        g_objectCount<=0 || g_objectVertexCount<=0) return;

    const unsigned char *objects=(const unsigned char *)g_objectStructArray;
    const unsigned char *polys=(const unsigned char *)g_polygonArrayBase;
    const SrcVertex *vertices=g_vertexArrayBase;
    int vertex_limit=g_modelVertexCount+g_objectVertexCount;
    int polygon_limit=g_modelPolygonCount+g_objectPolygonCount;
    for (unsigned i=0;i<sizeof(s_selected_roofs)/sizeof(s_selected_roofs[0]);i++){
        const AdventureRoofSelection sel=s_selected_roofs[i];
        if (sel.track_id==0) break;
        if (sel.track_id!=track_id || sel.part_index>=(uint16_t)g_objectCount)
            continue;
        const unsigned char *obj=objects+(size_t)sel.part_index*0x44u;
        int face_start=*(const int16_t *)(obj+0x30);
        unsigned face_count=*(const uint16_t *)(obj+0x32);
        if (sel.face_index>=face_count || face_start<0 ||
            (int64_t)face_start+sel.face_index>=polygon_limit) continue;
        const unsigned char *poly=polys+((size_t)face_start+sel.face_index)*0x30u;
        const int16_t *vi=(const int16_t *)(poly+0x20);
        if (vi[0]<0 || vi[1]<0 || vi[2]<0 || vi[3]<0 ||
            vi[0]>=vertex_limit || vi[1]>=vertex_limit ||
            vi[2]>=vertex_limit || vi[3]>=vertex_limit) continue;
        AdventureRoofTriangle tri;
        tri.id=((int32_t)sel.part_index<<16)|((int32_t)sel.face_index<<1);
        tri.a=convert_vertex(&vertices[vi[0]]);
        tri.b=convert_vertex(&vertices[vi[1]]);
        tri.c=convert_vertex(&vertices[vi[2]]);
        (void)AdventureRoof_Add(mesh,tri);
        tri.id++;
        tri.b=convert_vertex(&vertices[vi[2]]);
        tri.c=convert_vertex(&vertices[vi[3]]);
        (void)AdventureRoof_Add(mesh,tri);
    }
}

/*
 * Deterministic highest-support merge. BIN triangles count ONLY when
 * explicitly selected by the author. A roof support layer is zero so the
 * legacy TER wall mask remains active while Sonic stands on an authored roof.
 */
bool AdventureRoofLegacy_QueryGround(void *context,int32_t x,int32_t z,
                                     int32_t max_height,AdventureGroundHit *out)
{
    if(!out) return false;
    AdventureGroundHit ground={0};
    bool has_ter=AdventureTerrain_QueryLegacy(0,x,z,max_height,&ground);
    int32_t roof_y,roof_id;
    bool has_roof=AdventureRoof_QueryGround(context,x,z,max_height,
                                            &roof_y,&roof_id);
    if (has_roof && (!has_ter || roof_y>ground.height)) {
        ground.height=roof_y;
        /* Keep all authored positive IDs; -1 is reserved for airborne. */
        ground.surface_id=(roof_id>=0 && roof_id<=INT32_MAX-2)?
                         -2-roof_id:INT32_MIN;
        ground.layer=0;
    }
    if(has_ter || has_roof) *out=ground;
    return has_ter || has_roof;
}

bool AdventureRoofLegacy_QueryNormal(void *context,int32_t x,int32_t z,
                                      int32_t surface_id,AdventureVec3 *out)
{
    if(!out) return false;
    if(surface_id>=0)
        return AdventureTerrain_QueryLegacyNormal(0,x,z,surface_id,out);
    if(surface_id==-1) return false; /* airborne */
    int64_t roof_id=-(int64_t)surface_id-2;
    if(roof_id<0 || roof_id>INT32_MAX) return false;
    return AdventureRoof_QueryNormal(context,(int32_t)roof_id,out);
}
