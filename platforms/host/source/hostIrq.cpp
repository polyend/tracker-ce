#include "config.h"

#include <SDL.h>
#include <atomic>
#include <cstdint>

// Host concurrency model:
// Main thread takes SDL_LockAudioDevice around engine mutation (see main.cpp /
// ScriptRunner). The audio callback therefore never runs concurrently with
// play/stop/noteOff. IRQ disable primitives are no-ops here so they cannot
// deadlock with allocate/release.

static std::atomic<int> audioCallbackDepth{ 0 };
static std::atomic<SDL_AudioDeviceID> audioDev{ 0 };

extern "C" void hostSetAudioDevice(void* device)
{
    audioDev.store(static_cast<SDL_AudioDeviceID>(reinterpret_cast<uintptr_t>(device)),
                   std::memory_order_release);
}

extern "C" void hostEnterAudioCallback(void)
{
    audioCallbackDepth.fetch_add(1, std::memory_order_acq_rel);
}

extern "C" void hostLeaveAudioCallback(void)
{
    audioCallbackDepth.fetch_sub(1, std::memory_order_acq_rel);
}

extern "C" int hostInAudioCallback(void)
{
    return audioCallbackDepth.load(std::memory_order_acquire) > 0 ? 1 : 0;
}

extern "C" int hostAudioIrqDisabled(void)
{
    return 0;
}

extern "C" void hostDisableIrq(void) {}
extern "C" void hostEnableIrq(void) {}

extern "C" void hostDisableAudioInterrupt(void) {}
extern "C" void hostEnableAudioInterrupt(void) {}

extern "C" void hostLockAudio(void)
{
    const SDL_AudioDeviceID dev = audioDev.load(std::memory_order_acquire);
    if (dev && !hostInAudioCallback())
        SDL_LockAudioDevice(dev);
}

extern "C" void hostUnlockAudio(void)
{
    const SDL_AudioDeviceID dev = audioDev.load(std::memory_order_acquire);
    if (dev && !hostInAudioCallback())
        SDL_UnlockAudioDevice(dev);
}
