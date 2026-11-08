/* Explicit opt-in lookup from original, already-loaded track BIN meshes.
 * This bridge does not affect renderer or racing physics. */
#ifndef SONICR_ADVENTURE_ROOF_LEGACY_H
#define SONICR_ADVENTURE_ROOF_LEGACY_H
#include "adventure_roof.h"
void AdventureRoofLegacy_LoadSelected(AdventureRoofMesh *mesh, int track_id);
#endif
