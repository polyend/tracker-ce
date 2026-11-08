#include "TimerImpl.h"
#include <chrono>
#include <thread>

using Clock = std::chrono::steady_clock;

uint64_t TimerImpl::nowUs() const
{
    if (virtualTime)
    {
        return virtualMicros;
    }
    const auto now = Clock::now().time_since_epoch();
    return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

void TimerImpl::setVirtualTime(bool enabled)
{
    virtualTime = enabled;
    if (!enabled)
    {
        startSteadyUs = nowUs();
    }
}

void TimerImpl::advanceVirtualMs(uint32_t ms)
{
    virtualMicros += static_cast<uint64_t>(ms) * 1000ull;
}

void TimerImpl::setVirtualMicros(uint64_t us)
{
    virtualMicros = us;
}

uint32_t TimerImpl::millis()
{
    return static_cast<uint32_t>(nowUs() / 1000ull);
}

uint32_t TimerImpl::micros()
{
    return static_cast<uint32_t>(nowUs());
}

void TimerImpl::delay(int ms)
{
    if (virtualTime)
    {
        advanceVirtualMs(static_cast<uint32_t>(ms));
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

uint32_t TimerImpl::clockCycles()
{
    return static_cast<uint32_t>(nowUs() * clockCyclesPerUs());
}

uint32_t TimerImpl::clockCyclesPerUs()
{
    return 180; // Matches Original Tracker F_CPU / 1e6
}
