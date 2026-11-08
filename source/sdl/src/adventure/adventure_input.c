#include "adventure_input.h"
#include <stdint.h>
#define ADV_RAW_MAX 32767
#define ADV_DEADZONE 4900  /* radial ~15%, not the racing poller's 60% */
static uint32_t magnitude2(int32_t x, int32_t y)
{
    uint64_t n = (uint64_t)((int64_t)x*x + (int64_t)y*y);
    uint64_t bit = (uint64_t)1 << 62, result = 0;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= result + bit) { n -= result + bit; result = (result >> 1) + bit; }
        else result >>= 1;
        bit >>= 2;
    }
    return (uint32_t)result;
}
void AdventureInput_Reset(AdventureInputHistory *history)
{
    if (history) history->previous_buttons = 0;
}
AdventureInput AdventureInput_Sample(AdventureInputHistory *history, AdventureRawInput raw)
{
    AdventureInput out = {0};
    int32_t x = raw.move_x, y = raw.move_y;
    uint32_t magnitude = magnitude2(x, y);
    if (magnitude > ADV_DEADZONE) {
        uint32_t active = magnitude >= ADV_RAW_MAX ? ADV_RAW_MAX : magnitude;
        int32_t scaled = (int32_t)(((int64_t)(active-ADV_DEADZONE) * 4096) /
                                  (ADV_RAW_MAX-ADV_DEADZONE));
        if (scaled > 4096) scaled = 4096;
        out.move_x = (int16_t)(((int64_t)x * scaled) / magnitude);
        out.move_y = (int16_t)(((int64_t)y * scaled) / magnitude);
    }
    out.held = raw.buttons;
    if (history) {
        out.pressed = raw.buttons & ~history->previous_buttons;
        out.released = history->previous_buttons & ~raw.buttons;
        history->previous_buttons = raw.buttons;
    }
    return out;
}
