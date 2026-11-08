# WP-00 / WP-01 Implementation Status

**Status:** First code changes committed; full platform runtime tests still required.  
**Scope:** New adventure boot preview, not yet a player-controlled action game.

## Implemented

- New pure C \`AdventureGame\` lifecycle and deterministic tick counter, independent from racing physics and platform APIs.
- Isolated adventure runner in \`source/sdl/src/adventure/adventure_mode.c\` after the legacy \`InitLevel\` resource loading phase and before \`InitRaceStart\`.
- Desktop \`--adventure\` option; optional Dreamcast boot preview using \`make ADVENTURE_BOOT=1\` after a clean build. Standard binary entry remains the legacy game.
- View rendering through existing \`BuildCameraViewForViewport\`, \`RenderHUD\` and \`FlipD3D\` with the lap/race status layer suppressed while preview is active.
- Explicit single-player/single-viewport Sonic configuration with no race physics or lap/rank update invoked per frame.
- Headless C test for lifecycle, restart and 10,000 deterministic ticks in \`tests/adventure/\`.

## What is intentionally NOT implemented

- Player movement, jump, analog input or collision integration.
- Homing Attack, rings, stage goals or checkpoints.
- New camera (current preview calls original chase-camera builder).
- New stage authoring format or stage objects.
- Adventure selection through in-game menu.
- Replacement for original race-oriented logic inside the legacy \`InitLevel\` bootstrap.

## Testing limitations

The GitHub connector can commit and read source, but this session has no local checkout, user-supplied Sonic R proprietary data or KallistiOS toolchain. Source-level tests and full SDL/Dreamcast runtime verification need execution on a developer machine/CI. Do not mark a runtime test as passed based on source review.

## How to try

Desktop from a checkout with the standard SDL2 dependencies:

~~~sh
cd source/sdl
make clean && make
./build/sonicr --adventure /path/to/your/SONICR-data
~~~

Dreamcast (with sourced KallistiOS environment and original game data available to the normal CDI packaging):

~~~sh
cd source/dc
make clean && make ADVENTURE_BOOT=1
~~~

The preview should display Resort Island with one stationary Sonic and no race timer. On desktop ESC/window close exits; on Dreamcast START exits the preview. **These expectations require runtime validation.**

Headless test:

~~~sh
make -C tests/adventure test
~~~

### Next steps

1. Compile/test on SDL and KOS; fix any linkage or resource initialization issues in the preview.
2. Prove that no legacy race update counters tick during adventure mode and capture baseline metrics.
3. Implement a canonical coordinate system and old \`Player\` bridge, inspect original +Y/-Y signs.
4. Add real fixed-tick adventure input and low-speed 3D directional movement.
5. Introduce read-only \`.TER\` terrain queries, support/air collision and action camera.

**Do not treat WP-01 as complete until both target builds and runtime launch/exit are tested.**
