#ifndef SOURCE_FT812RASTERIZER_H_
#define SOURCE_FT812RASTERIZER_H_

#include <cstdint>
#include <string>
#include <vector>

class Ft812Rasterizer
{
  public:
    static constexpr int kWidth = 800;
    static constexpr int kHeight = 480;
    static constexpr size_t kRamGSize = 1024 * 1024;
    static constexpr size_t kCmdFifoSize = 4096;

    Ft812Rasterizer();

    void reset();
    void writeCmd32(uint32_t word);
    void writeCmdBytes(const uint8_t* data, uint32_t size);
    void writeRamG(const uint8_t* data, uint32_t size, uint32_t dest);
    void writeFontRamG(const uint8_t* data, uint32_t size, uint32_t dest);
    void readRamG(uint8_t* data, uint32_t size, uint32_t source) const;
    uint8_t writeString(const char* string, uint32_t length);

    void beginList();
    void endList();
    void render();

    const uint32_t* pixels() const { return framebuffer.data(); }
    uint32_t unknownCommandCount() const { return unknownCommands; }
    uint16_t cmdOffset() const { return static_cast<uint16_t>(cmdWrite & (kCmdFifoSize - 1)); }
    void incCmdOffset(uint16_t value) { cmdWrite = (cmdWrite + value) & (kCmdFifoSize - 1); }

    bool savePng(const char* path) const;

  private:
    struct GraphicsState
    {
        uint32_t color = 0xFFFFFF;
        uint8_t alpha = 255;
        uint8_t prim = 0;
        int lineWidth = 1;
        int pointSize = 1;
        uint32_t clearColor = 0;
        int scissorX = 0;
        int scissorY = 0;
        int scissorW = kWidth;
        int scissorH = kHeight;
        uint8_t bitmapHandle = 0;
        uint32_t bitmapSource = 0;
        uint8_t bitmapFormat = 0;
        uint16_t bitmapStride = 0;
        uint16_t bitmapHeight = 0;
        uint16_t bitmapWidth = 0;
        uint16_t bitmapLayoutH = 0;
        uint8_t cell = 0;
        uint8_t tag = 0;
        bool tagMask = true;
    };

    struct BitmapHandle
    {
        uint32_t source = 0;
        uint8_t format = 0;
        uint16_t stride = 0;
        uint16_t height = 0;
        uint16_t width = 0;
    };

    std::vector<uint32_t> framebuffer;
    std::vector<uint8_t> ramG;
    std::vector<uint32_t> cmdFifo;
    std::vector<uint32_t> displayList;
    BitmapHandle handles[32]{};
    GraphicsState state{};
    std::vector<GraphicsState> stack;
    uint32_t cmdWrite = 0;
    uint32_t unknownCommands = 0;
    int vertexCount = 0;
    int vx0 = 0, vy0 = 0, vx1 = 1, vy1 = 1;

    void executeWord(uint32_t word, size_t& i, const std::vector<uint32_t>& list);
    void executeCopro(uint32_t cmd, size_t& i, const std::vector<uint32_t>& list);
    void putPixel(int x, int y, uint32_t rgb, uint8_t a = 255);
    void fillRect(int x0, int y0, int x1, int y1, uint32_t rgb, uint8_t a = 255);
    void drawLine(int x0, int y0, int x1, int y1, uint32_t rgb, uint8_t a = 255);
    void drawBitmap(int x, int y, uint8_t handle, uint8_t cell);
    void drawText(int x, int y, int font, uint16_t options, const char* text);
    uint32_t sampleBitmap(const BitmapHandle& h, int px, int py) const;
};

#endif
