#include "platform_math.h"
#include <cmath>

int16_t sin_q15(int16_t value)
{
    // Q15 angle: full circle = 65536. Output is Q15 sin.
    const float angle = (static_cast<float>(value) / 32768.0f) * static_cast<float>(M_PI);
    const float s = std::sin(angle);
    if (s >= 1.0f)
        return 32767;
    if (s <= -1.0f)
        return -32768;
    return static_cast<int16_t>(s * 32767.0f);
}
