#ifndef MODULES_PATTERNSTEPFX_H_
#define MODULES_PATTERNSTEPFX_H_

#include <stdint.h>

// Pattern Step FX type ids in this range are owned by feature modules.
// Builtin sequencer FX types remain 0..50. Do not reuse those ordinals.
static constexpr uint8_t kPatternStepFxModuleIdBase = 128;
static constexpr uint8_t kPatternStepFxModuleIdLast = 143;
static constexpr uint8_t kMaxModulePatternStepFx = 16;
static constexpr uint8_t kPatternStepFxNameMax = 28;

// Descriptor exported by a FeatureModule for the pattern FX picker + playback.
struct PatternStepFxInfo
{
    uint8_t typeId; // stored in pattern; must be in [128,143], unique across modules
    char letter;    // picker glyph, e.g. 'N'
    const char* name;
    const char* shortName;
    const char* description;
    uint8_t minValue;
    uint8_t maxValue;
    uint8_t defaultValue;
};

#endif /* MODULES_PATTERNSTEPFX_H_ */
