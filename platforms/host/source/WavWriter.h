#ifndef SOURCE_WAVWRITER_H_
#define SOURCE_WAVWRITER_H_

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

class WavWriter
{
  public:
    bool open(const char* path, uint32_t sampleRate, uint16_t channels = 2);
    void writeInterleaved(const int16_t* left, const int16_t* right, int frames);
    void close();
    bool isOpen() const { return fp != nullptr; }

  private:
    FILE* fp = nullptr;
    uint32_t sampleRate = 44100;
    uint16_t channels = 2;
    uint32_t dataBytes = 0;
    void writeHeader(uint32_t dataSize);
};

#endif
