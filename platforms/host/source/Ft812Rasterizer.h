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
    uint32_t displayListOffset() const { return static_cast<uint32_t>(ramDl.size() * 4); }
    uint16_t cmdOffset() const { return static_cast<uint16_t>(cmdWrite & (kCmdFifoSize - 1)); }
    void incCmdOffset(uint16_t value) { cmdWrite = (cmdWrite + value) & (kCmdFifoSize - 1); }

    // True after a CMD_SWAP paint until consumeFrameDirty() is called.
    bool frameDirty() const { return frameDirtyFlag; }
    bool consumeFrameDirty()
    {
        const bool d = frameDirtyFlag;
        frameDirtyFlag = false;
        return d;
    }

    bool savePng(const char* path) const;

  private:
    // FT81x bitmap transform: screen = M * local + vertex
    struct Affine2D
    {
        float a = 1.f, b = 0.f, c = 0.f, d = 1.f, e = 0.f, f = 0.f;
        bool isIdentity() const
        {
            return a == 1.f && b == 0.f && c == 0.f && d == 1.f && e == 0.f && f == 0.f;
        }
        void map(float x, float y, float& ox, float& oy) const
        {
            ox = a * x + b * y + e;
            oy = c * x + d * y + f;
        }
    };

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
        int vertexFrac = 4; // VERTEX_FORMAT; Tracker uses 0 (pixel coords)
        Affine2D transform{}; // committed via CMD_SETMATRIX
    };

    struct BitmapHandle
    {
        uint32_t source = 0;
        uint8_t format = 0;
        uint16_t stride = 0;
        uint16_t height = 0;
        uint16_t width = 0;
        // SETFONT2: metrics block in RAM_G; firstChar so VERTEX2II cell==ASCII works.
        uint32_t metricsAddr = 0;
        uint8_t firstChar = 0;
        bool isFont = false;
    };

    std::vector<uint32_t> framebuffer;
    std::vector<uint8_t> ramG;
    std::vector<uint32_t> cmdFifo;
    std::vector<uint32_t> displayList;
    std::vector<uint32_t> ramDl; // simulated RAM_DL / REG_CMD_DL contents
    BitmapHandle handles[32]{};
    GraphicsState state{};
    std::vector<GraphicsState> stack;
    uint32_t cmdWrite = 0;
    uint32_t unknownCommands = 0;
    int vertexCount = 0;
    int vx0 = 0, vy0 = 0, vx1 = 1, vy1 = 1;
    bool paintEnabled = true;
    bool frameDirtyFlag = false;
    Affine2D scratchTransform{}; // co-pro matrix before SETMATRIX

    void executeWord(uint32_t word, size_t& i, const std::vector<uint32_t>& list);
    void matrixLoadIdentity();
    void matrixTranslate(int32_t txFixed, int32_t tyFixed);
    void matrixRotate(int32_t angleFixed);
    void matrixScale(int32_t sxFixed, int32_t syFixed);
    void executeCopro(uint32_t cmd, size_t& i, const std::vector<uint32_t>& list);
    void executeDlStream(const std::vector<uint32_t>& words);
    void putPixel(int x, int y, uint32_t rgb, uint8_t a = 255);
    void fillRect(int x0, int y0, int x1, int y1, uint32_t rgb, uint8_t a = 255);
    void drawLine(int x0, int y0, int x1, int y1, uint32_t rgb, uint8_t a = 255);
    void drawBitmap(int x, int y, uint8_t handle, uint8_t cell);
    void drawText(int x, int y, int font, uint16_t options, const char* text);
    bool rectInsideScissor(int x0, int y0, int x1, int y1) const;
    // Returns alpha 0..255 for L-formats, or 255 for color formats (rgb via outRgb).
    uint8_t sampleBitmap(const BitmapHandle& h,
                         uint32_t sourceBase,
                         int px,
                         int py,
                         uint32_t& outRgb) const;
    void memcpyRam(uint32_t dest, uint32_t src, uint32_t num);
    uint8_t fontCharWidth(uint8_t handle, unsigned char ch) const;
};

#endif
