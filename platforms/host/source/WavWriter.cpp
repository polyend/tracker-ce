#include "WavWriter.h"
#include <cstring>

bool WavWriter::open(const char* path, uint32_t rate, uint16_t ch)
{
    close();
    fp = std::fopen(path, "wb");
    if (!fp)
        return false;
    sampleRate = rate;
    channels = ch;
    dataBytes = 0;
    writeHeader(0);
    return true;
}

void WavWriter::writeHeader(uint32_t dataSize)
{
    if (!fp)
        return;
    std::fseek(fp, 0, SEEK_SET);
    const uint32_t byteRate = sampleRate * channels * 2;
    const uint16_t blockAlign = static_cast<uint16_t>(channels * 2);
    const uint32_t riffSize = 36 + dataSize;
    auto wr = [&](const void* p, size_t n) { std::fwrite(p, 1, n, fp); };
    wr("RIFF", 4);
    wr(&riffSize, 4);
    wr("WAVE", 4);
    wr("fmt ", 4);
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;
    uint16_t bits = 16;
    wr(&fmtSize, 4);
    wr(&audioFormat, 2);
    wr(&channels, 2);
    wr(&sampleRate, 4);
    wr(&byteRate, 4);
    wr(&blockAlign, 2);
    wr(&bits, 2);
    wr("data", 4);
    wr(&dataSize, 4);
}

void WavWriter::writeInterleaved(const int16_t* left, const int16_t* right, int frames)
{
    if (!fp)
        return;
    for (int i = 0; i < frames; ++i)
    {
        int16_t frame[2] = { left[i], right ? right[i] : left[i] };
        std::fwrite(frame, sizeof(int16_t), channels, fp);
        dataBytes += static_cast<uint32_t>(channels * 2);
    }
}

void WavWriter::close()
{
    if (!fp)
        return;
    writeHeader(dataBytes);
    std::fclose(fp);
    fp = nullptr;
}
