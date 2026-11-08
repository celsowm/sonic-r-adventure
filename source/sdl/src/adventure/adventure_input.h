/* Normalized game actions; only raw sampling is platform-specific. */
#ifndef SONICR_ADVENTURE_INPUT_H
#define SONICR_ADVENTURE_INPUT_H
#include <stdint.h>
enum AdventureActionBits {
    ADV_ACTION_JUMP = 1u << 0,
    ADV_ACTION_ROLL = 1u << 1,
    ADV_ACTION_CAMERA_LEFT = 1u << 2,
    ADV_ACTION_CAMERA_RIGHT = 1u << 3
};
typedef struct AdventureRawInput {
    int16_t move_x; /* negative=left, positive=right; range -32768..32767 */
    int16_t move_y; /* positive=forward (SDL/DC analog -Y mapped positive) */
    uint32_t buttons;
} AdventureRawInput;
typedef struct AdventureInput {
    int16_t move_x; /* Q12, from -4096 to 4096 */
    int16_t move_y;
    uint32_t held;
    uint32_t pressed;
    uint32_t released;
} AdventureInput;
typedef struct AdventureInputHistory { uint32_t previous_buttons; } AdventureInputHistory;
AdventureInput AdventureInput_Sample(AdventureInputHistory *history, AdventureRawInput raw);
void AdventureInput_Reset(AdventureInputHistory *history);
#endif
