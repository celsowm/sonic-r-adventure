/*
 * Static .TER wall polyline adapter for the portable Adventure swept-disc.
 * Non-destructive: reads g_terCollisionMesh, g_terEdgeList and TER grid data;
 * NEVER calls WallCollision, WallBounce, or modifies mesh masks/race state.
 */
#include "adventure_terrain_legacy.h"
#include "adventure_collision.h"
#include "sonicr_globals.h"
#include "ter_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define ADV_LEGACY_GRID 32
#define ADV_LEGACY_WALLS 4096
#define ADV_LEGACY_MAX_SCAN_STEPS 32

static int32_t sample_cell(int32_t world, int32_t origin, int32_t span)
{
    int64_t diff=(int64_t)world / ADV_FP_ONE - origin;
    if (diff<0 || span<=0) return -1;
    int64_t cell=diff/span;
    return cell>=ADV_LEGACY_GRID ? -1 : (int32_t)cell;
}
static bool nearby(AdventureVec3 from, AdventureVec3 to,
                   const TerCollisionMesh *w, int32_t radius)
{
    /* Bounding sphere in 2D, widened for the capsule radius + one unit.
     * TER radiusSq is expressed in whole world units (not Q20.12). */
    uint32_t bound=AdventureMath_IsqrtU64(w->radiusSq>0?(uint32_t)w->radiusSq:0u);
    int64_t extension=(int64_t)bound+radius/ADV_FP_ONE+2;
    int64_t x0=(int64_t)from.x/ADV_FP_ONE, x1=(int64_t)to.x/ADV_FP_ONE;
    int64_t z0=(int64_t)from.z/ADV_FP_ONE, z1=(int64_t)to.z/ADV_FP_ONE;
    if (x0>x1) {int64_t temp=x0;x0=x1;x1=temp;}
    if (z0>z1) {int64_t temp=z0;z0=z1;z1=temp;}
    return !(x1<(int64_t)w->centerX-extension ||
             x0>(int64_t)w->centerX+extension ||
             z1<(int64_t)w->centerZ-extension ||
             z0>(int64_t)w->centerZ+extension);
}
static void scan_cell(int cellx,int cellz,
                      AdventureVec3 from,AdventureVec3 to,
                      int32_t radius,int32_t layer,
                      unsigned char *visited, unsigned char *visited_cells,
                      AdventureWallHit *best,bool *found)
{
    if (cellx<0 || cellx>=ADV_LEGACY_GRID ||
        cellz<0 || cellz>=ADV_LEGACY_GRID) return;
    int cell_index=cellz*ADV_LEGACY_GRID+cellx;
    if (visited_cells[cell_index>>3] & (1u<<(cell_index&7))) return;
    visited_cells[cell_index>>3] |= (unsigned char)(1u<<(cell_index&7));
    const int16_t *grid=(const int16_t *)g_terGridIndex;
    int index=grid[cell_index];
    if (index<0) return;
    const uint16_t *entries=(const uint16_t *)g_terGridData+index;
    const TerCollisionMesh *walls=(const TerCollisionMesh *)g_terCollisionMesh;
    const TerVertex *vertices=(const TerVertex *)g_terEdgeList;
    const uint16_t layer_bit=(uint16_t)(1u<<layer);
    if (*entries==0xffffu) return;
    /* Every list has bit 15 set on its final entry. Bound scanning for safety. */
    for (int i=0;i<ADV_LEGACY_WALLS;i++) {
        uint16_t entry=entries[i];
        int idx=entry&0xfff;
        if (entry&0x4000u && !(visited[idx>>3]&(1u<<(idx&7)))) {
            visited[idx>>3] |= (unsigned char)(1u<<(idx&7));
            const TerCollisionMesh *w=&walls[idx];
            if (((uint16_t)w->mask&layer_bit) &&
                w->vtxBase>=0 && w->vtxCount>=2 &&
                nearby(from,to,w,radius)) {
                /* Each wall has consecutive 6-byte vertices. For finite
                 * height gates, compare canonical foot Y with the NEGATED
                 * legacy vertex Y interpolated at the actual sweep impact.
                 * Non-gated walls remain unbounded vertically. */
                for (int seg=0;seg<(int)w->vtxCount-1;seg++) {
                    const TerVertex *v0=&vertices[w->vtxBase+seg];
                    const TerVertex *v1=&vertices[w->vtxBase+seg+1];
                    AdventureVec3 a={(int32_t)v0->x*ADV_FP_ONE,0,
                                     (int32_t)v0->z*ADV_FP_ONE};
                    AdventureVec3 b={(int32_t)v1->x*ADV_FP_ONE,0,
                                     (int32_t)v1->z*ADV_FP_ONE};
                    AdventureWallHit h;
                    if (AdventureCollision_SweepDiscSegment(from,to,a,b,
                                                             radius,&h) &&
                        (!w->heightGate ||
                         !AdventureCollision_AboveLegacyWallTop(
                             from.y < to.y ? from.y : to.y,
                             from, to, a, b,
                             v0->y, v1->y, h.fraction)) &&
                        (!*found || h.fraction<best->fraction)) {
                        h.wall_id=idx;
                        *best=h;
                        *found=true;
                    }
                }
            }
        }
        if (entry&0x8000u) break;
    }
}
bool AdventureTerrain_SweepLegacyWall(void *context,AdventureVec3 from,
                                      AdventureVec3 to,int32_t radius,
                                      int32_t capsule_height,int32_t layer,
                                      AdventureWallHit *out)
{
    (void)context;
    (void)capsule_height; /* wall height gates use feet: no ceiling mesh yet */
    if (!out || !g_terGridIndex || !g_terGridData || !g_terCollisionMesh ||
        !g_terEdgeList || g_aiGridCellWidth<=0 || g_aiGridCellHeight<=0 ||
        layer<0 || layer>=16 || radius<=0) return false;
    unsigned char visited[ADV_LEGACY_WALLS/8];
    unsigned char visited_cells[(ADV_LEGACY_GRID*ADV_LEGACY_GRID)/8];
    memset(visited,0,sizeof(visited));
    memset(visited_cells,0,sizeof(visited_cells));
    bool found=false;
    AdventureWallHit best={0};
    /* Sample no farther than half a grid cell between probes. On normal
     * 3-world-unit movement this is usually just one segment; no repeated
     * cell-list scans within a query. */
    int32_t span=g_aiGridCellWidth<g_aiGridCellHeight?
                 g_aiGridCellWidth:g_aiGridCellHeight;
    uint32_t step_q8=(uint32_t)span*128u;
    if (step_q8<32u) step_q8=32u;
    int64_t dx=((int64_t)to.x-from.x)/16;
    int64_t dz=((int64_t)to.z-from.z)/16;
    uint32_t movement=AdventureMath_IsqrtU64((uint64_t)(dx*dx+dz*dz));
    uint32_t steps=movement/step_q8+1;
    if (steps>ADV_LEGACY_MAX_SCAN_STEPS) steps=ADV_LEGACY_MAX_SCAN_STEPS;
    for (uint32_t step=0;step<=steps;step++) {
        int64_t x=(int64_t)from.x+
                  ((int64_t)to.x-from.x)*step/steps;
        int64_t z=(int64_t)from.z+
                  ((int64_t)to.z-from.z)*step/steps;
        int cellx=sample_cell((int32_t)x,g_aiGridOriginX,g_aiGridCellWidth);
        int cellz=sample_cell((int32_t)z,g_aiGridOriginZ,g_aiGridCellHeight);
        if (cellx<0||cellz<0) continue;
        for (int dz=-1;dz<=1;dz++)
            for (int dx=-1;dx<=1;dx++)
                scan_cell(cellx+dx,cellz+dz,from,to,radius,layer,
                          visited,visited_cells,&best,&found);
    }
    if (found) *out=best;
    return found;
}
