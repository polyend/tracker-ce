#ifndef SOURCE_PLATFORMIMPL_H_
#define SOURCE_PLATFORMIMPL_H_

#include "DisplayDriverImpl.h"
#include "EncoderImpl.h"
#include "FileSystemImpl.h"
#include "Platform.h"
#include "StubPeripherals.h"
#include "SwitchGroupImpl.h"
#include "TimerImpl.h"

class PlatformImpl final : public Platform
{
  public:
    void init(const char* sdRoot);
    void update();

    virtual void setPowerMode(bool mode) override { powerMode = mode; }
    virtual void resetMCU() override {}
    virtual void runBootloader(char* /*fileName*/) override {}
    virtual void setPowerButtonCallback(PowerButtonCallback cb) override { powerButtonCallback = cb; }
    virtual bool getPowerButtonState() override { return false; }
    virtual bool readForceStartUp() override { return false; }

    virtual Sgtl5000* getCodecAudio() override { return &codec; }
    virtual Radio* getRadio() override { return &radio; }
    virtual SwitchGroup* getButtons() override { return &buttons; }
    virtual SwitchGroup* getPads() override { return &pads; }
    virtual Leds* getLeds() override { return &leds; }
    virtual Battery* getBattery() override { return &battery; }
    virtual Timer* getTimer() override { return &timer; }
    virtual Encoder* getEncoder() override { return &encoder; }
    virtual DisplayDriver* getDisplayDriver() override { return &displayDriver; }
    virtual UsbAudio* getUsbAudio() override { return &usbAudio; }
    virtual UartMidi* getUartMidi() override { return &uartMidi; }
    virtual UsbMidi* getUsbMidi() override { return &usbMidi; }
    virtual ConfigManager* getConfigManager() override { return &configManager; }
    virtual FileSystem* getFileSystem() override { return &fileSystem; }
    virtual CrcCalculator* getCrcCalculator() override { return &crcCalculator; }
    virtual SystemInfo* getSystemInfo() override { return &systemInfo; }
    virtual Usb* getUsb() override { return &usb; }

    SwitchGroupImpl& buttonsImpl() { return buttons; }
    SwitchGroupImpl& padsImpl() { return pads; }
    EncoderImpl& encoderImpl() { return encoder; }
    TimerImpl& timerImpl() { return timer; }
    DisplayDriverImpl& displayImpl() { return displayDriver; }
    FileSystemImpl& fileSystemImpl() { return fileSystem; }

  private:
    PowerButtonCallback powerButtonCallback = nullptr;
    bool powerMode = true;
    DisplayDriverImpl displayDriver;
    TimerImpl timer;
    Sgtl5000Impl codec;
    SwitchGroupImpl buttons;
    SwitchGroupImpl pads;
    CrcCalculatorImpl crcCalculator;
    BatteryImpl battery;
    ConfigManagerImpl configManager;
    UsbAudioImpl usbAudio;
    UsbMidiImpl usbMidi;
    UartMidiImpl uartMidi;
    FileSystemImpl fileSystem;
    LedsImpl leds;
    EncoderImpl encoder;
    RadioImpl radio;
    UsbImpl usb;
    SystemInfo systemInfo{ "host-emulator" };
};

#endif
