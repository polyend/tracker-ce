#ifndef SOURCE_DISPLAYDRIVERIMPL_H_
#define SOURCE_DISPLAYDRIVERIMPL_H_

#include "DisplayDriver.h"
#include "Ft812Rasterizer.h"

class DisplayDriverImpl : public DisplayDriver
{
  public:
    Ft812Rasterizer& rasterizer() { return eve; }
    const Ft812Rasterizer& rasterizer() const { return eve; }

    virtual void setPowerState(bool state) override { powered = state; }
    virtual void setBrightness(int value) override { (void)value; }
    virtual void setRotation(int value) override { (void)value; }

    virtual void getImgProps(uint32_t& addr, uint32_t& width, uint32_t& height) override
    {
        addr = 0;
        width = Ft812Rasterizer::kWidth;
        height = Ft812Rasterizer::kHeight;
    }
    virtual uint32_t getDisplayListOffset() override { return 0; }
    virtual void IncCMDOffset(uint16_t value) override { eve.incCmdOffset(value); }
    virtual void WriteData32(uint32_t data) override { eve.writeCmd32(data); }

    virtual void beginCoProList() override { eve.beginList(); }
    virtual void beginCoProListNoCheck() override { eve.beginList(); }
    virtual void endCoProList() override
    {
        eve.endList();
        eve.render();
    }
    virtual void awaitCoProEmpty() override {}
    virtual uint8_t isCoProEmpty() override { return 1; }
    virtual void writeDataRAMG(const uint8_t* imgData, uint32_t dataSize, uint32_t destAddress) override
    {
        eve.writeRamG(imgData, dataSize, destAddress);
    }
    virtual void writeFontRAMG(const uint8_t* imgData, uint32_t dataSize, uint32_t destAddress) override
    {
        eve.writeFontRamG(imgData, dataSize, destAddress);
    }
    virtual void readDataRAMG(uint8_t* imgData, uint32_t dataSize, uint32_t sourceAddress) override
    {
        eve.readRamG(imgData, dataSize, sourceAddress);
    }
    virtual void readBackDataRAMG(uint8_t* imgData, uint32_t dataSize, uint32_t sourceAddress) override
    {
        eve.readRamG(imgData, dataSize, sourceAddress);
    }
    virtual uint8_t sendString(const char* string, uint32_t length) override
    {
        return eve.writeString(string, length);
    }
    virtual void writeDataToCMD(const uint8_t* imgData, uint32_t totalDataSize) override
    {
        eve.writeCmdBytes(imgData, totalDataSize);
    }

    virtual void initStreamDma(uint16_t, uint16_t, uint16_t, uint8_t*, uint32_t) override {}
    virtual bool updateStreamDma(bool) override { return true; }
    virtual void stopStreamDma() override {}

  private:
    Ft812Rasterizer eve;
    bool powered = true;
};

#endif
