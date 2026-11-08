#include "EncoderImpl.h"

void onPotChange(uint8_t n, int16_t value);

void EncoderImpl::turn(int detents)
{
    pending += detents;
}

void EncoderImpl::update()
{
    if (pending == 0)
        return;
    const int16_t value = static_cast<int16_t>(pending);
    pending = 0;
    onPotChange(0, value);
}
