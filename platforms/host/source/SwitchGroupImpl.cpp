#include "SwitchGroupImpl.h"

bool SwitchGroupImpl::isButtonPressed(int buttonIdx)
{
    if (buttonIdx < 0 || buttonIdx >= kMaxButtons)
        return false;
    return pressed[buttonIdx];
}

void SwitchGroupImpl::setActionCallback(ActionType action, void (*callback)(uint8_t))
{
    switch (action)
    {
        case kPress:
            onPress = callback;
            break;
        case kRelease:
            onRelease = callback;
            break;
        case kHold:
            onHold = callback;
            break;
    }
}

void SwitchGroupImpl::press(uint8_t idx)
{
    if (idx >= kMaxButtons)
        return;
    if (pressed[idx])
        return;
    pressed[idx] = true;
    pressStartMs[idx] = 0;
    holdFired[idx] = false;
    if (onPress)
        onPress(idx);
}

void SwitchGroupImpl::release(uint8_t idx)
{
    if (idx >= kMaxButtons)
        return;
    if (!pressed[idx])
        return;
    pressed[idx] = false;
    if (onRelease)
        onRelease(idx);
}

void SwitchGroupImpl::tap(uint8_t idx)
{
    press(idx);
    release(idx);
}

void SwitchGroupImpl::update(uint32_t nowMs)
{
    for (int i = 0; i < kMaxButtons; ++i)
    {
        if (!pressed[i] || holdFired[i] || !onHold)
            continue;
        if (pressStartMs[i] == 0)
        {
            pressStartMs[i] = nowMs ? nowMs : 1;
            continue;
        }
        if (nowMs - pressStartMs[i] >= static_cast<uint32_t>(holdTimeMs))
        {
            holdFired[i] = true;
            onHold(static_cast<uint8_t>(i));
        }
    }
}
