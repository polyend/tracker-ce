#include "PlatformImpl.h"

void onButtonPush(uint8_t n);
void onButtonRelease(uint8_t n);
void onButtonHold(uint8_t n);
void onPadPush(uint8_t n);
void onPadRelease(uint8_t n);
void onPadHold(uint8_t n);

void PlatformImpl::init(const char* sdRoot)
{
    fileSystem.init(sdRoot ? sdRoot : "./sdcard");
    configManager.init(std::string(sdRoot ? sdRoot : "./sdcard") + ".eeprom");

    buttons.setActionCallback(SwitchGroup::kPress, onButtonPush);
    buttons.setActionCallback(SwitchGroup::kRelease, onButtonRelease);
    buttons.setActionCallback(SwitchGroup::kHold, onButtonHold);
    pads.setActionCallback(SwitchGroup::kPress, onPadPush);
    pads.setActionCallback(SwitchGroup::kRelease, onPadRelease);
    pads.setActionCallback(SwitchGroup::kHold, onPadHold);

    uartMidi.init();
}

void PlatformImpl::update()
{
    const uint32_t now = timer.millis();
    buttons.update(now);
    pads.update(now);
    encoder.update();
    fileSystem.update();
    usb.update();
}
