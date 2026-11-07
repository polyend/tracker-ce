#include "config.h"
#include <mutex>

static std::recursive_mutex& irqMutex()
{
    static std::recursive_mutex m;
    return m;
}

extern "C" void hostDisableIrq(void)
{
    irqMutex().lock();
}

extern "C" void hostEnableIrq(void)
{
    irqMutex().unlock();
}
