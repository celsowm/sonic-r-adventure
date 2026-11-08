/*
 * WP-06.1: explicit conditional overlay zones over original Sonic R tracks.
 * v1 deliberately bounds geometry to 64 static triangles, with no heap.
 * Little-endian pack fields are decoded individually, never cast as structs.
 * The same active-zone bits gate rendering, support, normal and sweep queries.
 */
#include "adventure_zone.h"
#include "adventure_capsule.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1]<<8));
}
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static int32_t signed32(const uint8_t *p)
{
    uint32_t v=le32(p);
    if(v<=INT32_MAX) return (int32_t)v;
    return (int32_t)((int64_t)v-INT64_C(4294967296));
}
static bool region(size_t length,size_t offset,size_t size)
{
    return offset<=length && size<=length-offset;
}
static bool valid_environment(uint8_t act,uint8_t weather,uint8_t tod)
{
    return act>=1 && act<=8 && weather<=2 && tod<=3;
}
static void refresh(AdventureWorld *world)
{
    for(uint16_t i=0;i<world->zone_count;i++){
        AdventureZone *z=&world->zones[i];
        z->active=((z->act_mask & (1u<<(world->act-1)))!=0 &&
                   (z->weather_mask & (1u<<world->weather))!=0 &&
                   (z->time_mask & (1u<<world->time_of_day))!=0);
    }
}
void AdventureWorld_Init(AdventureWorld *world,AdventureTerrain original,
                         uint16_t track_id,uint8_t act,
                         uint8_t weather,uint8_t time_of_day)
{
    if(!world) return;
    memset(world,0,sizeof(*world));
    world->base=original;
    world->track_id=track_id;
    world->act=valid_environment(act,weather,time_of_day)?act:1;
    world->weather=weather<=2?weather:0;
    world->time_of_day=time_of_day<=3?time_of_day:1;
}
bool AdventureWorld_SetEnvironment(AdventureWorld *world,uint8_t act,
                                   uint8_t weather,uint8_t time_of_day)
{
    if(!world || !valid_environment(act,weather,time_of_day)) return false;
    world->act=act;
    world->weather=weather;
    world->time_of_day=time_of_day;
    refresh(world);
    return true;
}
static bool add_triangle(AdventureWorld *world,const uint8_t *data,
                         uint32_t triangle_id,AdventureVec3 offset)
{
    AdventureRoofTriangle t;
    AdventureVec3 *v[3]={&t.a,&t.b,&t.c};
    for(int i=0;i<3;i++){
        int64_t x=(int64_t)signed32(data+i*12)+offset.x;
        int64_t y=(int64_t)signed32(data+i*12+4)+offset.y;
        int64_t z=(int64_t)signed32(data+i*12+8)+offset.z;
        if(x<INT32_MIN || x>INT32_MAX || y<INT32_MIN || y>INT32_MAX ||
           z<INT32_MIN || z>INT32_MAX) return false;
        v[i]->x=(int32_t)x;
        v[i]->y=(int32_t)y;
        v[i]->z=(int32_t)z;
    }
    t.id=(int32_t)triangle_id;
    return AdventureRoof_Add(&world->triangles,t);
}
bool AdventureWorld_LoadPackBytes(AdventureWorld *world,const uint8_t *data,
                                  size_t length,AdventureVec3 anchor)
{
    if(!world || !data || length<ADV_ZONE_PACK_HEADER_SIZE ||
       memcmp(data,"AZN1",4)!=0 || le16(data+4)!=1 ||
       le16(data+6)!=world->track_id) return false;
    uint16_t zones=le16(data+8),count=le16(data+10);
    size_t zones_off=le32(data+12),tri_off=le32(data+16);
    if(zones>ADV_ZONE_MAX_ZONES || count>ADV_ZONE_MAX_TRIANGLES ||
       !region(length,zones_off,(size_t)zones*ADV_ZONE_PACK_DESCRIPTOR_SIZE) ||
       !region(length,tri_off,(size_t)count*ADV_ZONE_PACK_TRIANGLE_SIZE) ||
       zones_off<ADV_ZONE_PACK_HEADER_SIZE ||
       tri_off<ADV_ZONE_PACK_HEADER_SIZE ||
       (zones && count==0)) return false;
    size_t zone_end=zones_off+(size_t)zones*ADV_ZONE_PACK_DESCRIPTOR_SIZE;
    size_t tri_end=tri_off+(size_t)count*ADV_ZONE_PACK_TRIANGLE_SIZE;
    if(zones && count && !(zone_end<=tri_off || tri_end<=zones_off))
        return false;

    /* Preserve the old world on any invalid later descriptor/triangle. */
    AdventureWorld staged=*world;
    staged.zone_count=0;
    AdventureRoof_Clear(&staged.triangles);
    uint16_t expected_first=0;
    for(uint16_t i=0;i<zones;i++){
        const uint8_t *d=data+zones_off+(size_t)i*ADV_ZONE_PACK_DESCRIPTOR_SIZE;
        uint32_t id=le32(d);
        uint8_t acts=d[4],weathers=d[5],times=d[6],flags=d[7];
        uint16_t first=le16(d+8),num=le16(d+10);
        if(!id || id>255 || !acts || !(weathers&7u) || (weathers&~7u) ||
           !(times&15u) || (times&~15u) || (flags&~ADV_ZONE_FLAG_SPAWN_RELATIVE) ||
           le32(d+12)!=0 || !num || num>ADV_ZONE_MAX_TRIANGLES ||
           first!=expected_first || (uint32_t)first+num>count) return false;
        for(uint16_t prev=0;prev<i;prev++)
            if(staged.zones[prev].id==id) return false;
        AdventureZone *z=&staged.zones[staged.zone_count++];
        z->id=id;
        z->act_mask=acts;
        z->weather_mask=weathers;
        z->time_mask=times;
        z->first_triangle=first;
        z->triangle_count=num;
        z->active=0;
        AdventureVec3 origin=(flags&ADV_ZONE_FLAG_SPAWN_RELATIVE)?
                              anchor:(AdventureVec3){0,0,0};
        for(uint16_t j=0;j<num;j++){
            const uint8_t *t=data+tri_off+
                (size_t)(first+j)*ADV_ZONE_PACK_TRIANGLE_SIZE;
            uint32_t stable=ADV_ZONE_TRI_ID_BASE+(id<<8)+j;
            if(!add_triangle(&staged,t,stable,origin)) return false;
        }
        expected_first=(uint16_t)(first+num);
    }
    if(expected_first!=count) return false;
    refresh(&staged);
    *world=staged;
    return true;
}
bool AdventureWorld_LoadPackFile(AdventureWorld *world,const char *path,
                                 AdventureVec3 anchor)
{
    if(!world || !path) return false;
    /* Small fixed max allows use on Dreamcast without dynamic allocation.
     * Prefer a single disc read to a series of per-triangle seeks. */
    enum { MAX_BYTES=ADV_ZONE_PACK_HEADER_SIZE+
                       ADV_ZONE_MAX_ZONES*ADV_ZONE_PACK_DESCRIPTOR_SIZE+
                       ADV_ZONE_MAX_TRIANGLES*ADV_ZONE_PACK_TRIANGLE_SIZE };
    uint8_t data[MAX_BYTES];
    FILE *file=fopen(path,"rb");
    if(!file) return false;
    size_t size=fread(data,1,sizeof(data),file);
    int extra=fgetc(file);
    bool good=!ferror(file) && extra==EOF;
    fclose(file);
    return good && AdventureWorld_LoadPackBytes(world,data,size,anchor);
}
bool AdventureWorld_AddDemoBridge(AdventureWorld *world,AdventureVec3 spawn)
{
    if(!world || world->track_id!=1 || world->zone_count ||
       !valid_environment(world->act,world->weather,world->time_of_day))
        return false;
    /* Small, intentionally synthetic collision/visual fixture. It is
     * relative to the actual stage spawn; NOT a geometrically verified
     * route across the original Resort Island water. */
    AdventureVec3 v[4];
    const int32_t q=ADV_FP_ONE;
    const int32_t x[]={6,22,22,6};
    const int32_t z[]={-3,-3,3,3};
    for(int i=0;i<4;i++){
        v[i].x=AdventureMath_ClampI32((int64_t)spawn.x+x[i]*q);
        v[i].z=AdventureMath_ClampI32((int64_t)spawn.z+z[i]*q);
        v[i].y=AdventureMath_ClampI32((int64_t)spawn.y-2*q);
    }
    AdventureRoofTriangle a={v[0],v[1],v[2],
                               ADV_ZONE_TRI_ID_BASE+(1<<8)};
    AdventureRoofTriangle b={v[0],v[2],v[3],
                               ADV_ZONE_TRI_ID_BASE+(1<<8)+1};
    if(!AdventureRoof_Add(&world->triangles,a) ||
       !AdventureRoof_Add(&world->triangles,b)) {
        AdventureRoof_Clear(&world->triangles);
        return false;
    }
    AdventureZone *zone=&world->zones[0];
    zone->id=1;
    zone->first_triangle=0;
    zone->triangle_count=2;
    zone->act_mask=1u;
    zone->weather_mask=1u<<2;  /* snow only */
    zone->time_mask=15u;        /* every time of day */
    world->zone_count=1;
    refresh(world);
    return true;
}
uint16_t AdventureWorld_ActiveTriangles(const AdventureWorld *world)
{
    if(!world) return 0;
    uint16_t total=0;
    for(uint16_t i=0;i<world->zone_count;i++)
        if(world->zones[i].active)
            total=(uint16_t)(total+world->zones[i].triangle_count);
    return total;
}
static bool ground_query(void *context,int32_t x,int32_t z,
                         int32_t max_y,AdventureGroundHit *out)
{
    const AdventureWorld *w=(const AdventureWorld *)context;
    if(!w || !out) return false;
    AdventureGroundHit best={0};
    bool found=w->base.query_ground &&
        w->base.query_ground(w->base.context,x,z,max_y,&best);
    for(uint16_t i=0;i<w->zone_count;i++){
        const AdventureZone *zone=&w->zones[i];
        if(!zone->active) continue;
        for(uint16_t j=0;j<zone->triangle_count;j++){
            const AdventureRoofTriangle *t=
                &w->triangles.triangles[zone->first_triangle+j];
            int32_t height;
            if(AdventureRoof_TriangleHeight(t,x,z,&height) &&
               height<=max_y && (!found || height>best.height)) {
                best.height=height;
                best.surface_id=-2-t->id;
                best.layer=0;
                found=true;
            }
        }
    }
    if(found) *out=best;
    return found;
}
static bool overhead_query(void *context,int32_t x,int32_t z,
                           int32_t min_height,AdventureGroundHit *out)
{
    const AdventureWorld *w=(const AdventureWorld *)context;
    return w && w->base.query_overhead &&
           w->base.query_overhead(w->base.context,x,z,min_height,out);
}
static bool normal_query(void *context,int32_t x,int32_t z,
                         int32_t surface_id,AdventureVec3 *out)
{
    const AdventureWorld *w=(const AdventureWorld *)context;
    if(!w || !out) return false;
    if(surface_id<=-2) {
        int64_t id=-(int64_t)surface_id-2;
        for(uint16_t i=0;i<w->zone_count;i++){
            const AdventureZone *z=&w->zones[i];
            if(!z->active) continue;
            for(uint16_t j=0;j<z->triangle_count;j++){
                const AdventureRoofTriangle *t=
                    &w->triangles.triangles[z->first_triangle+j];
                if(id==t->id) {
                    return AdventureRoof_QueryNormal(&w->triangles,
                                                      t->id,out);
                }
            }
        }
    }
    return w->base.query_normal &&
           w->base.query_normal(w->base.context,x,z,surface_id,out);
}
static bool wall_query(void *context,AdventureVec3 from,AdventureVec3 to,
                       int32_t radius,int32_t height,int32_t layer,
                       AdventureWallHit *out)
{
    const AdventureWorld *w=(const AdventureWorld *)context;
    return w && w->base.sweep_wall &&
           w->base.sweep_wall(w->base.context,from,to,radius,height,layer,out);
}
static bool capsule_query(void *context,AdventureVec3 from,AdventureVec3 to,
                          int32_t height,int32_t radius,
                          AdventureCapsuleHit *out)
{
    const AdventureWorld *w=(const AdventureWorld *)context;
    if(!w || !out) return false;
    AdventureCapsuleHit best={0};
    bool found=w->base.sweep_capsule &&
               w->base.sweep_capsule(w->base.context,from,to,
                                     height,radius,&best);
    for(uint16_t i=0;i<w->zone_count;i++){
        const AdventureZone *zone=&w->zones[i];
        if(!zone->active) continue;
        for(uint16_t j=0;j<zone->triangle_count;j++){
            AdventureCapsuleHit hit={0};
            const AdventureRoofTriangle *tri=
                &w->triangles.triangles[zone->first_triangle+j];
            if(AdventureCapsule_SweepTriangle(from,to,height,radius,tri,&hit) &&
               (!found || hit.fraction<best.fraction)){
                best=hit;found=true;
            }
        }
    }
    if(found) *out=best;
    return found;
}
AdventureTerrain AdventureWorld_MakeTerrain(AdventureWorld *world)
{
    AdventureTerrain terrain={
        ground_query,world,wall_query,overhead_query,capsule_query,normal_query
    };
    return terrain;
}
