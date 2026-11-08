# WP-02 / WP-03 — Input, Coordinates and Horizontal Movement

**Status:** Portable headless tests and complete Linux SDL compilation passed in GitHub Actions (run 42482730081, commit 4758c86c); interactive gameplay and Dreamcast runtime still require verification.

## Implemented

- Shared 20.12 world coordinates, positive-up Y and explicit negative-up legacy Player conversion (adventure_math.h/.c).
- Saturating 32-bit math, deterministic velocity approach, and a camera-relative forward/right movement basis derived from positions.
- AdventureInput_Sample radial stick dead zone, Q12 analog direction, and per-tick pressed/held/released edges.
- SDL GameController / fallback joystick and Dreamcast Maple raw analog through a dedicated platform input reader. Legacy racing mappings remain unchanged.
- Keyboard WASD/arrows, face buttons and future camera buttons are also sampled.
- AdventurePlayer sidecar with new authoritative position, velocity and facing without changing the existing 0x71C Player structure.
- 60Hz fixed tick movement with acceleration, braking, diagonal speed cap and free movement in horizontal X/Z.
- One-way adaptation of player render fields and refresh of the temporary chase camera after each movement frame.
- Headless tests for input edges, coordinate conversion, saturated math, camera basis, ground-pilot reversal/stop, diagonal speed and repeatability.

## Not yet implemented

- **There is no terrain collision, vertical movement, gravity or jump yet.** The preview holds the original spawn height and Sonic may pass through walls, ramps or ledges.
- Jump/roll/camera buttons are sampled but do not activate the future actions.
- The camera is the original chase camera, not the WP-05 adventure camera.
- Facing yaw and speed caps now use deterministic integer atan approximation and integer square root; renderer alignment still requires visual verification.
- Full Linux SDL build passes CI without commercial game data; SDL gameplay execution with user-supplied data, Dreamcast KOS cross-compilation, and on-hardware timing remain unverified.

## Try the pilot

~~~sh
make -C tests/adventure test

cd source/sdl
make clean && make
./build/sonicr --adventure /path/to/SONICR-data

cd ../dc
make clean && make ADVENTURE_BOOT=1
~~~

Controls: WASD/arrows or analog stick/D-pad move in camera-relative horizontal directions; ESC on desktop / START on Dreamcast exits the adventure preview. Space/A, Ctrl/B and camera orbit buttons are reserved and currently have no effect.

## Next: WP-04

1. Create a read-only terrain query on .TER without mutating the legacy Player.
2. Resolve ground height, slopes, supporting surfaces, walls, stacked floors and safe fall behavior.
3. Add jump/gravity/air control, landing/ceiling sweeps and deterministic fixtures.
4. Profile Dreamcast under active collision load.
5. Only then replace the temporary chase camera and introduce combat.

**WP-03 is a controller pilot, not a finished playable platform-action vertical slice.**

## Verified CI evidence

- [Adventure gameplay + full SDL build](https://github.com/celsowm/sonic-r-adventure/actions/runs/42482730081) — both jobs succeeded on Ubuntu.
- Portable game tests run without proprietary game assets.
- Desktop application compiled with the repository's full SDL2/OpenGL Makefile and default -Werror.
- No emulator or physical Dreamcast execution is claimed by this run.
