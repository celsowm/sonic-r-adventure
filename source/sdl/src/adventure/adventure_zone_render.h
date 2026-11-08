#ifndef SONICR_ADVENTURE_ZONE_RENDER_H
#define SONICR_ADVENTURE_ZONE_RENDER_H
#include "adventure_zone.h"
/* Renderer-only bridge. Never called from fixed gameplay simulation. */
void AdventureZoneRender_Draw(const AdventureWorld *world);
#endif
