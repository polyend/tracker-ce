#include "config.h"

#include <stdint.h>

static bool s_audioMuted = false;

void disableAudioInterrupt()
{
    // No-op on host: main serializes with SDL_LockAudioDevice instead.
}

void enableAudioInterrupt() {}

void muteAudioOutByKey()
{
    s_audioMuted = true;
}

void unmuteAudioOutByKey()
{
    s_audioMuted = false;
}

uint8_t isAudioOutMuted()
{
    return s_audioMuted ? 1 : 0;
}
