/*
 * Conditional stage overlays. Does not own or change original .BIN/.TER.
 * A bounded, versioned authoring pack supplies new static visual/collision
 * triangles. Each zone independently gates both collision and rendering.
 */
#ifndef SONICR_ADVENTURE_ZONE_H
#define SONICR_ADVENTURE_ZONE_H

#include "adventure_terrain.h"
#include "adventure_roof.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define ADV_ZONE_MAX_ZONES 4u
#define ADV_ZONE_MAX_TRIANGLES 64u
#define ADV_ZONE_TRI_ID_BASE 0x1F000000
#define ADV_ZONE_PACK_HEADER_SIZE 20u
#define ADV_ZONE_PACK_DESCRIPTOR_SIZE 16u
#define ADV_ZONE_PACK_TRIANGLE_SIZE 36u
#define ADV_ZONE_FLAG_SPAWN_RELATIVE 1u

typedef struct AdventureZone {
    uint32_t id;                  /* stable ID: 1..255 in v1 */
    uint16_t first_triangle;
    uint16_t triangle_count;
    uint8_t act_mask;             /* bit (act - 1), act 1..8 */
    uint8_t weather_mask;         /* bit 0 clear / 1 rain / 2 snow */
    uint8_t time_mask;            /* 0..3 sunrise/day/sunset/night */
    uint8_t active;
} AdventureZone;

typedef struct AdventureWorld {
    AdventureTerrain base;        /* read-only legacy TER/approved BIN provider */
    AdventureZone zones[ADV_ZONE_MAX_ZONES];
    AdventureRoofMesh triangles;  /* v1 bounded small overlay mesh */
    uint16_t zone_count;
    uint16_t track_id;
    uint8_t act;
    uint8_t weather;
    uint8_t time_of_day;
} AdventureWorld;

void AdventureWorld_Init(AdventureWorld *world,AdventureTerrain original,
                         uint16_t track_id,uint8_t act,
                         uint8_t weather,uint8_t time_of_day);
bool AdventureWorld_SetEnvironment(AdventureWorld *world,uint8_t act,
                                   uint8_t weather,uint8_t time_of_day);
/* Transactional: invalid/truncated data must not change existing world.
 * Signed triangle positions are absolute Q20.12 unless the descriptor
 * carries SPAWN_RELATIVE, which adds anchor to every vertex at load. */
bool AdventureWorld_LoadPackBytes(AdventureWorld *world,const uint8_t *data,
                                  size_t length,AdventureVec3 anchor);
/* Optional on-disk pack, no runtime JSON/OBJ/Blender dependency. */
bool AdventureWorld_LoadPackFile(AdventureWorld *world,const char *path,
                                 AdventureVec3 anchor);
/* Developer-only tiny, clearly synthetic bridge. No proprietary geometry.
 * Can be switched off by loading a real pack first. */
bool AdventureWorld_AddDemoBridge(AdventureWorld *world,AdventureVec3 anchor);
AdventureTerrain AdventureWorld_MakeTerrain(AdventureWorld *world);
uint16_t AdventureWorld_ActiveTriangles(const AdventureWorld *world);

#endif
