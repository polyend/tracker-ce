#ifndef SOURCE_SWITCHGROUPIMPL_H_
#define SOURCE_SWITCHGROUPIMPL_H_

#include "Platform.h"
#include <cstring>

class SwitchGroupImpl : public SwitchGroup
{
  public:
    static constexpr int kMaxButtons = 64;

    virtual void setHoldTime(int timeMs) override { holdTimeMs = timeMs; }
    virtual bool isButtonPressed(int buttonIdx) override;
    virtual void setActionCallback(ActionType action, void (*callback)(uint8_t)) override;

    void press(uint8_t idx);
    void release(uint8_t idx);
    void tap(uint8_t idx);
    void update(uint32_t nowMs);

  private:
    bool pressed[kMaxButtons]{};
    uint32_t pressStartMs[kMaxButtons]{};
    bool holdFired[kMaxButtons]{};
    int holdTimeMs = 500;
    void (*onPress)(uint8_t) = nullptr;
    void (*onRelease)(uint8_t) = nullptr;
    void (*onHold)(uint8_t) = nullptr;
};

#endif
