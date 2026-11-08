/*
 * Sonic R Adventure -- temporary rendering/bootstrap adapter.
 * Called only after InitLevel has loaded Resort Island.
 */
#ifndef SONICR_ADVENTURE_MODE_H
#define SONICR_ADVENTURE_MODE_H

/* Returns nonzero on a window-close request. */
int AdventureMode_RunPreview(void);

/* Rendering uses this to omit the legacy race timer/status overlay. */
int AdventureMode_IsActive(void);

#endif /* SONICR_ADVENTURE_MODE_H */
