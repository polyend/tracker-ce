#ifndef SOURCE_DRIVERS_SPIDRIVER_H_
#define SOURCE_DRIVERS_SPIDRIVER_H_

#include "fsl_lpspi.h"
#include "stdio.h"

class SpiBus
{
  public:
    static constexpr int kCphaLeadingEdge = 0;
    static constexpr int kCphaTrailingEdge = 1;
    typedef void (*DmaCompleteCallback)(void);

    void init(LPSPI_Type* spiBase, int pcs = 0, int cpha = kCphaLeadingEdge);
    void setSpeed(uint32_t baudRate);
    uint8_t transferByte(uint8_t data, int pcs = 0, bool pcsContinous = true);
    uint32_t transfer4Bytes(uint32_t data);
    // Sends 32-bit words, least significant byte first, without reading
    // anything back. Blocks until the last bit is out.
    void writeWords(const uint32_t* data, uint32_t count);

    int transferDmaBuff();

    void initDma(DmaCompleteCallback callback, uint8_t* data, uint32_t dataSize);
    void stopDma();

  private:
    static constexpr uint32_t kDefaultBaudRate = 1000000;
    LPSPI_Type* spiBase;
    uint32_t baudRate;
    lpspi_which_pcs_t pcs;
    lpspi_clock_phase_t cpha;

    void start();
    void waitForFifoSpace(uint32_t fifoSize);
};

extern SpiBus spiBus4;

#endif /* SOURCE_DRIVERS_SPIDRIVER_H_ */
