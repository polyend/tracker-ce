#ifndef SOURCE_TIMERIMPL_H_
#define SOURCE_TIMERIMPL_H_

#include "Platform.h"

class TimerImpl : public Timer
{
  public:
    void setVirtualTime(bool enabled);
    void advanceVirtualMs(uint32_t ms);
    void setVirtualMicros(uint64_t us);

    virtual uint32_t millis() override;
    virtual uint32_t micros() override;
    virtual void delay(int ms) override;
    virtual uint32_t clockCycles() override;
    virtual uint32_t clockCyclesPerUs() override;

  private:
    bool virtualTime = false;
    uint64_t virtualMicros = 0;
    uint64_t startSteadyUs = 0;
    uint64_t nowUs() const;
};

#endif
