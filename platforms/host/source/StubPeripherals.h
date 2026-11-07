#ifndef SOURCE_STUBPERIPHERALS_H_
#define SOURCE_STUBPERIPHERALS_H_

#include "ConfigManager.h"
#include "Platform.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

class CrcCalculatorImpl : public CrcCalculator
{
  public:
    virtual uint32_t calculate32bitCrc(const uint8_t* data, const uint16_t datalen) override
    {
        // CRC-32 (Ethernet polynomial), sufficient for host tests.
        uint32_t crc = 0xFFFFFFFFu;
        for (uint16_t i = 0; i < datalen; ++i)
        {
            crc ^= data[i];
            for (int b = 0; b < 8; ++b)
                crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
        return ~crc;
    }
};

class ConfigManagerImpl : public ConfigManager
{
  public:
    void init(const std::string& path)
    {
        storePath = path;
        data.assign(4096, 0);
        FILE* f = std::fopen(storePath.c_str(), "rb");
        if (f)
        {
            std::fread(data.data(), 1, data.size(), f);
            std::fclose(f);
        }
    }
    virtual bool readData(int address, void* out, int size) override
    {
        if (address < 0 || size < 0 || address + size > static_cast<int>(data.size()))
            return false;
        std::memcpy(out, data.data() + address, static_cast<size_t>(size));
        return true;
    }
    virtual void writeData(int address, void* in, int size) override
    {
        if (address < 0 || size < 0 || address + size > static_cast<int>(data.size()))
            return;
        std::memcpy(data.data() + address, in, static_cast<size_t>(size));
        FILE* f = std::fopen(storePath.c_str(), "wb");
        if (f)
        {
            std::fwrite(data.data(), 1, data.size(), f);
            std::fclose(f);
        }
    }

  private:
    std::string storePath;
    std::vector<uint8_t> data;
};

class LedsImpl : public Leds
{
  public:
    virtual void setLed(int idx, bool enabled, int brightness) override
    {
        (void)idx;
        (void)enabled;
        (void)brightness;
    }
};

class BatteryImpl : public Battery
{
  public:
    virtual void enableMonitoring() override {}
    virtual int getLevel() override { return 100; }
    virtual float getVoltage() override { return 4.1f; }
    virtual bool getCharging() override { return false; }
    virtual int getTimeTo() override { return 0; }
    virtual bool getState() override { return true; }
    virtual bool isEmpty() override { return false; }
};

class RadioImpl : public Radio
{
  public:
    virtual void enableLineInput(bool) override {}
    virtual void enable(bool) override {}
    virtual bool setFrequency(float) override { return true; }
    virtual void setSeekCallback(void (*)(void)) override {}
    virtual void seek(SeekDirection) override {}
    virtual void updateSeek() override {}
    virtual bool updateRDS() override { return false; }
    virtual float getFrequency() override { return 88.0f; }
    virtual float getBaseFrequency() override { return 87.5f; }
    virtual float getSpacing() override { return 0.1f; }
    virtual bool isinitialized() override { return true; }
    virtual void clearRDS() override {}
    virtual void setRegion(Region) override {}
    virtual bool isRegionChanged() override { return false; }
    virtual char* getStationName() override
    {
        static char name[] = "HOST";
        return name;
    }
};

class Sgtl5000Impl : public Sgtl5000
{
  public:
    virtual void init() override {}
    virtual void setInputType(InputType) override {}
    virtual void setHeadphoneSource(HeadphoneSource) override {}
    virtual void setLineInLevel(int) override {}
    virtual void setMicGain(int) override {}
    virtual void setHeadphoneMuted(bool) override {}
    virtual void setMasterVolume(float) override {}
    virtual void setAutoVolumeEnabled(bool) override {}
    virtual void configureAutoVolume(AutoVolumeParams*) override {}
    virtual void setEnhanceBassEnabled(bool) override {}
    virtual void configureEnhanceBass(EnhanceBassParams*) override {}
    virtual void setEqualizerType(EqualizerType) override {}
    virtual void configureFiveBandEqualizer(float*) override {}
    virtual void enableSurroundSound(bool) override {}
    virtual void configureSurroundSound(SurroundSoundParams*) override {}
    virtual void enableHardwareProcessor(bool) override {}
};

class UsbAudioImpl : public UsbAudio
{
  public:
    virtual void beginFillData() override {}
    virtual void fillChannelData(void*, int, int) override {}
    virtual void finishFillData() override {}
};

class UsbMidiImpl : public UsbMidi
{
  public:
    virtual bool isReadAvailable() override { return false; }
    virtual void read() override {}
    virtual void sendRealTime(MessageType) override {}
    virtual void sendControlChange(uint8_t, uint8_t, uint8_t) override {}
    virtual void sendProgramChange(uint8_t, uint8_t) override {}
    virtual void sendAfterTouch(uint8_t, uint8_t) override {}
    virtual void sendPolyPressure(uint8_t, uint8_t, uint8_t) override {}
    virtual void sendPitchBend(int, uint8_t) override {}
    virtual void sendNoteOn(uint8_t, uint8_t, uint8_t) override {}
    virtual void sendNoteOff(uint8_t, uint8_t, uint8_t) override {}
    virtual void setReceiveActions(MidiReceiveActions*) override {}
};

class UartMidiImpl : public UartMidi
{
  public:
    virtual bool isReadAvailable() override { return false; }
    virtual void read() override {}
    virtual void sendRealTime(MessageType) override {}
    virtual void sendControlChange(uint8_t, uint8_t, uint8_t) override {}
    virtual void sendProgramChange(uint8_t, uint8_t) override {}
    virtual void sendAfterTouch(uint8_t, uint8_t) override {}
    virtual void sendPolyPressure(uint8_t, uint8_t, uint8_t) override {}
    virtual void sendPitchBend(int, uint8_t) override {}
    virtual void sendNoteOn(uint8_t, uint8_t, uint8_t) override {}
    virtual void sendNoteOff(uint8_t, uint8_t, uint8_t) override {}
    virtual void init() override {}
    virtual void setReceiveActions(MidiReceiveActions*) override {}
    virtual void setReceiveNoteOnAction(void (*)(uint8_t, uint8_t, uint8_t)) override {}
};

class UsbImpl : public Usb
{
  public:
    virtual void init(void) override {}
    virtual void reset(void) override {}
    virtual void update(void) override {}
    virtual void setClassState(ClassType, bool) override {}
    virtual bool getClassState(ClassType) override { return false; }
    virtual bool isAttached() override { return false; }
};

#endif
