#include "Ft812Rasterizer.h"
#include "FT81x.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace
{
int decodeVertexCoord(int16_t v, int fracBits)
{
    // VERTEX2F uses 1/16 pixel by default (frac=4) after VERTEX_FORMAT.
    (void)fracBits;
    return v / 16;
}

uint32_t rgb565to888(uint16_t c)
{
    const uint32_t r = ((c >> 11) & 0x1F) * 255 / 31;
    const uint32_t g = ((c >> 5) & 0x3F) * 255 / 63;
    const uint32_t b = (c & 0x1F) * 255 / 31;
    return (r << 16) | (g << 8) | b;
}
} // namespace

Ft812Rasterizer::Ft812Rasterizer()
{
    framebuffer.assign(kWidth * kHeight, 0);
    ramG.assign(kRamGSize, 0);
    cmdFifo.assign(kCmdFifoSize / 4, 0);
    reset();
}

void Ft812Rasterizer::reset()
{
    std::fill(framebuffer.begin(), framebuffer.end(), 0);
    std::fill(ramG.begin(), ramG.end(), 0);
    std::fill(cmdFifo.begin(), cmdFifo.end(), 0);
    displayList.clear();
    stack.clear();
    state = GraphicsState{};
    cmdWrite = 0;
    unknownCommands = 0;
    vertexCount = 0;
}

void Ft812Rasterizer::writeCmd32(uint32_t word)
{
    const size_t idx = (cmdWrite / 4) % cmdFifo.size();
    cmdFifo[idx] = word;
    cmdWrite = (cmdWrite + 4) & (kCmdFifoSize - 1);
    displayList.push_back(word);
}

void Ft812Rasterizer::writeCmdBytes(const uint8_t* data, uint32_t size)
{
    for (uint32_t i = 0; i + 4 <= size; i += 4)
    {
        uint32_t w = 0;
        std::memcpy(&w, data + i, 4);
        writeCmd32(w);
    }
}

void Ft812Rasterizer::writeRamG(const uint8_t* data, uint32_t size, uint32_t dest)
{
    if (dest >= kRamGSize)
        return;
    const uint32_t n = std::min(size, static_cast<uint32_t>(kRamGSize - dest));
    std::memcpy(ramG.data() + dest, data, n);
}

void Ft812Rasterizer::writeFontRamG(const uint8_t* data, uint32_t size, uint32_t dest)
{
    writeRamG(data, size, dest);
}

void Ft812Rasterizer::readRamG(uint8_t* data, uint32_t size, uint32_t source) const
{
    if (source >= kRamGSize)
    {
        std::memset(data, 0, size);
        return;
    }
    const uint32_t n = std::min(size, static_cast<uint32_t>(kRamGSize - source));
    std::memcpy(data, ramG.data() + source, n);
    if (n < size)
        std::memset(data + n, 0, size - n);
}

uint8_t Ft812Rasterizer::writeString(const char* string, uint32_t length)
{
    if (!string)
        return 0;
    // Coprocessor strings are padded to 4 bytes with a trailing NUL.
    uint32_t i = 0;
    while (i < length)
    {
        uint32_t word = 0;
        for (int b = 0; b < 4 && i < length; ++b, ++i)
            reinterpret_cast<uint8_t*>(&word)[b] = static_cast<uint8_t>(string[i]);
        writeCmd32(word);
        if (string[i - 1] == '\0')
            break;
    }
    if (i == 0 || (i > 0 && string[i - 1] != '\0'))
    {
        // ensure NUL terminator word if needed
        if ((i % 4) != 0)
        {
            // already padded by partial word
        }
        else
            writeCmd32(0);
    }
    return 1;
}

void Ft812Rasterizer::beginList()
{
    displayList.clear();
}

void Ft812Rasterizer::endList()
{
    // keep displayList for render()
}

void Ft812Rasterizer::putPixel(int x, int y, uint32_t rgb, uint8_t a)
{
    if (x < state.scissorX || y < state.scissorY || x >= state.scissorX + state.scissorW ||
        y >= state.scissorY + state.scissorH)
        return;
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight)
        return;
    uint32_t& dst = framebuffer[static_cast<size_t>(y) * kWidth + x];
    if (a >= 255)
    {
        dst = rgb;
        return;
    }
    const uint32_t dr = (dst >> 16) & 0xFF;
    const uint32_t dg = (dst >> 8) & 0xFF;
    const uint32_t db = dst & 0xFF;
    const uint32_t sr = (rgb >> 16) & 0xFF;
    const uint32_t sg = (rgb >> 8) & 0xFF;
    const uint32_t sb = rgb & 0xFF;
    const uint32_t inv = 255 - a;
    const uint32_t r = (sr * a + dr * inv) / 255;
    const uint32_t g = (sg * a + dg * inv) / 255;
    const uint32_t b = (sb * a + db * inv) / 255;
    dst = (r << 16) | (g << 8) | b;
}

void Ft812Rasterizer::fillRect(int x0, int y0, int x1, int y1, uint32_t rgb, uint8_t a)
{
    if (x0 > x1)
        std::swap(x0, x1);
    if (y0 > y1)
        std::swap(y0, y1);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            putPixel(x, y, rgb, a);
}

void Ft812Rasterizer::drawLine(int x0, int y0, int x1, int y1, uint32_t rgb, uint8_t a)
{
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;)
    {
        putPixel(x0, y0, rgb, a);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

uint32_t Ft812Rasterizer::sampleBitmap(const BitmapHandle& h, int px, int py) const
{
    if (px < 0 || py < 0 || px >= h.width || py >= h.height)
        return 0;
    const uint32_t addr = h.source + static_cast<uint32_t>(py) * h.stride;
    if (addr >= kRamGSize)
        return 0;

    auto lum = [](uint8_t v) -> uint32_t {
        return (static_cast<uint32_t>(v) << 16) | (static_cast<uint32_t>(v) << 8) | v;
    };

    switch (h.format)
    {
        case L1:
        {
            const uint8_t byte = ramG[addr + static_cast<uint32_t>(px / 8)];
            const uint8_t bit = (byte >> (7 - (px & 7))) & 1;
            return bit ? 0xFFFFFF : 0;
        }
        case L2:
        {
            const uint8_t byte = ramG[addr + static_cast<uint32_t>(px / 4)];
            const uint8_t shift = static_cast<uint8_t>((3 - (px & 3)) * 2);
            const uint8_t v = (byte >> shift) & 0x3;
            return lum(static_cast<uint8_t>(v * 85));
        }
        case L4:
        {
            const uint8_t byte = ramG[addr + static_cast<uint32_t>(px / 2)];
            const uint8_t v = (px & 1) ? (byte & 0xF) : (byte >> 4);
            return lum(static_cast<uint8_t>(v * 17));
        }
        case L8:
            return lum(ramG[addr + static_cast<uint32_t>(px)]);
        case RGB565:
        {
            uint16_t c = 0;
            std::memcpy(&c, &ramG[addr + static_cast<uint32_t>(px) * 2], 2);
            return rgb565to888(c);
        }
        default:
            return lum(ramG[addr + static_cast<uint32_t>(px)]);
    }
}

void Ft812Rasterizer::drawBitmap(int x, int y, uint8_t handle, uint8_t /*cell*/)
{
    if (handle >= 32)
        return;
    const BitmapHandle& h = handles[handle];
    if (h.width == 0 || h.height == 0)
        return;
    for (int py = 0; py < h.height; ++py)
    {
        for (int px = 0; px < h.width; ++px)
        {
            uint32_t c = sampleBitmap(h, px, py);
            // Treat near-black as transparent for L-format icons.
            if ((c & 0xFFFFFF) == 0)
                continue;
            putPixel(x + px, y + py, c, state.alpha);
        }
    }
}

void Ft812Rasterizer::drawText(int x, int y, int font, uint16_t options, const char* text)
{
    if (!text)
        return;
    // Built-in bitmap font approximation: 8x8 cells.
    const int cw = (font >= 20) ? 12 : 8;
    const int ch = (font >= 20) ? 16 : 10;
    int len = static_cast<int>(std::strlen(text));
    int drawX = x;
    int drawY = y;
    if (options & OPT_CENTERX)
        drawX -= (len * cw) / 2;
    if (options & OPT_CENTERY)
        drawY -= ch / 2;
    if (options & OPT_RIGHTX)
        drawX -= len * cw;

    for (int i = 0; text[i]; ++i)
    {
        const unsigned char chv = static_cast<unsigned char>(text[i]);
        // Tiny 5x7 glyph: filled blocks for printable ASCII.
        if (chv >= 32)
        {
            fillRect(drawX + 1, drawY + 1, drawX + cw - 2, drawY + ch - 2, state.color, state.alpha);
            // carve a rough character hole so text is readable as blocks
            if (chv != ' ')
                fillRect(drawX + 2, drawY + 2, drawX + cw - 3, drawY + ch - 3, 0, 40);
        }
        drawX += cw;
    }
}

void Ft812Rasterizer::executeCopro(uint32_t cmd, size_t& i, const std::vector<uint32_t>& list)
{
    auto need = [&](size_t n) -> bool { return i + n < list.size(); };

    if (cmd == CMD_DLSTART)
    {
        return;
    }
    if (cmd == CMD_SWAP)
    {
        return;
    }
    if (cmd == CMD_BGCOLOR && need(1))
    {
        ++i;
        return;
    }
    if (cmd == CMD_FGCOLOR && need(1))
    {
        ++i;
        return;
    }
    if (cmd == CMD_GRADIENT && need(6))
    {
        const int16_t x0 = static_cast<int16_t>(list[i + 1] & 0xFFFF);
        const int16_t y0 = static_cast<int16_t>((list[i + 1] >> 16) & 0xFFFF);
        const uint32_t c0 = list[i + 2];
        const int16_t x1 = static_cast<int16_t>(list[i + 3] & 0xFFFF);
        const int16_t y1 = static_cast<int16_t>((list[i + 3] >> 16) & 0xFFFF);
        const uint32_t c1 = list[i + 4];
        i += 6;
        const int yStart = std::min(y0, y1);
        const int yEnd = std::max(y0, y1);
        for (int y = yStart; y <= yEnd; ++y)
        {
            float t = (yEnd == yStart) ? 0.f : float(y - yStart) / float(yEnd - yStart);
            auto lerp = [&](int s, int e) { return int(s + (e - s) * t); };
            uint32_t c = (lerp((c0 >> 16) & 0xFF, (c1 >> 16) & 0xFF) << 16) |
                         (lerp((c0 >> 8) & 0xFF, (c1 >> 8) & 0xFF) << 8) |
                         lerp(c0 & 0xFF, c1 & 0xFF);
            fillRect(std::min(x0, x1), y, std::max(x0, x1), y, c, state.alpha);
        }
        return;
    }
    if (cmd == CMD_TEXT && need(3))
    {
        const int16_t x = static_cast<int16_t>(list[i + 1] & 0xFFFF);
        const int16_t y = static_cast<int16_t>((list[i + 1] >> 16) & 0xFFFF);
        const int16_t font = static_cast<int16_t>(list[i + 2] & 0xFFFF);
        const uint16_t options = static_cast<uint16_t>((list[i + 2] >> 16) & 0xFFFF);
        i += 3;
        std::string text;
        while (i < list.size())
        {
            uint32_t w = list[i++];
            for (int b = 0; b < 4; ++b)
            {
                char c = reinterpret_cast<char*>(&w)[b];
                if (c == '\0')
                {
                    drawText(x, y, font, options, text.c_str());
                    return;
                }
                text.push_back(c);
            }
        }
        drawText(x, y, font, options, text.c_str());
        return;
    }
    if (cmd == CMD_NUMBER && need(3))
    {
        const int16_t x = static_cast<int16_t>(list[i + 1] & 0xFFFF);
        const int16_t y = static_cast<int16_t>((list[i + 1] >> 16) & 0xFFFF);
        const int16_t font = static_cast<int16_t>(list[i + 2] & 0xFFFF);
        const uint16_t options = static_cast<uint16_t>((list[i + 2] >> 16) & 0xFFFF);
        const int32_t n = static_cast<int32_t>(list[i + 3]);
        i += 3;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", n);
        drawText(x, y, font, options, buf);
        return;
    }
    if (cmd == CMD_BUTTON && need(4))
    {
        const int16_t x = static_cast<int16_t>(list[i + 1] & 0xFFFF);
        const int16_t y = static_cast<int16_t>((list[i + 1] >> 16) & 0xFFFF);
        const int16_t w = static_cast<int16_t>(list[i + 2] & 0xFFFF);
        const int16_t h = static_cast<int16_t>((list[i + 2] >> 16) & 0xFFFF);
        const int16_t font = static_cast<int16_t>(list[i + 3] & 0xFFFF);
        const uint16_t options = static_cast<uint16_t>((list[i + 3] >> 16) & 0xFFFF);
        i += 4;
        fillRect(x, y, x + w, y + h, state.color, state.alpha);
        std::string text;
        while (i < list.size())
        {
            uint32_t word = list[i++];
            for (int b = 0; b < 4; ++b)
            {
                char c = reinterpret_cast<char*>(&word)[b];
                if (c == '\0')
                {
                    drawText(x + w / 2, y + h / 2, font, options | OPT_CENTER, text.c_str());
                    return;
                }
                text.push_back(c);
            }
        }
        return;
    }
    if (cmd == CMD_LOADIDENTITY || cmd == CMD_SETMATRIX || cmd == CMD_STOP || cmd == CMD_COLDSTART)
        return;
    if (cmd == CMD_TRANSLATE && need(2))
    {
        i += 2;
        return;
    }
    if (cmd == CMD_ROTATE && need(1))
    {
        ++i;
        return;
    }
    if (cmd == CMD_SCALE && need(2))
    {
        i += 2;
        return;
    }
    if (cmd == CMD_SETFONT || cmd == CMD_SETFONT2)
    {
        // skip args loosely
        if (cmd == CMD_SETFONT && need(2))
            i += 2;
        else if (cmd == CMD_SETFONT2 && need(3))
            i += 3;
        return;
    }
    if (cmd == CMD_INFLATE && need(1))
    {
        ++i;
        // inflated payload follows until next command-ish; skip remaining unknown
        ++unknownCommands;
        return;
    }
    if (cmd == CMD_MEMWRITE && need(2))
    {
        const uint32_t ptr = list[i + 1];
        const uint32_t num = list[i + 2];
        i += 2;
        for (uint32_t n = 0; n < (num + 3) / 4 && i + 1 < list.size(); ++n)
        {
            uint32_t w = list[++i];
            for (uint32_t b = 0; b < 4 && n * 4 + b < num; ++b)
                if (ptr + n * 4 + b < kRamGSize)
                    ramG[ptr + n * 4 + b] = reinterpret_cast<uint8_t*>(&w)[b];
        }
        return;
    }
    ++unknownCommands;
}

void Ft812Rasterizer::executeWord(uint32_t word, size_t& i, const std::vector<uint32_t>& list)
{
    if (word >= 0xFFFFFF00u) // co-processor commands are 0xFFFFFF00+
    {
        executeCopro(word, i, list);
        return;
    }

    const uint32_t op = word >> 24;
    if ((word >> 30) == 1)
    {
        // VERTEX2F
        const int16_t x = static_cast<int16_t>((word >> 15) & 32767);
        const int16_t y = static_cast<int16_t>(word & 32767);
        // sign extend 15-bit
        int sx = (x & 0x4000) ? (x | ~32767) : x;
        int sy = (y & 0x4000) ? (y | ~32767) : y;
        sx = decodeVertexCoord(static_cast<int16_t>(sx), 4);
        sy = decodeVertexCoord(static_cast<int16_t>(sy), 4);

        if (state.prim == RECTS)
        {
            if (vertexCount == 0)
            {
                vx0 = sx;
                vy0 = sy;
                vertexCount = 1;
            }
            else
            {
                fillRect(vx0, vy0, sx, sy, state.color, state.alpha);
                vertexCount = 0;
            }
        }
        else if (state.prim == LINES || state.prim == LINE_STRIP)
        {
            if (vertexCount == 0)
            {
                vx0 = sx;
                vy0 = sy;
                vertexCount = 1;
            }
            else
            {
                drawLine(vx0, vy0, sx, sy, state.color, state.alpha);
                if (state.prim == LINE_STRIP)
                {
                    vx0 = sx;
                    vy0 = sy;
                }
                else
                    vertexCount = 0;
            }
        }
        else if (state.prim == FTPOINTS)
        {
            const int r = std::max(1, state.pointSize / 16);
            fillRect(sx - r, sy - r, sx + r, sy + r, state.color, state.alpha);
        }
        else if (state.prim == BITMAPS)
        {
            drawBitmap(sx, sy, state.bitmapHandle, state.cell);
        }
        return;
    }
    if ((word >> 30) == 2)
    {
        // VERTEX2II
        const int x = (word >> 21) & 511;
        const int y = (word >> 12) & 511;
        const uint8_t handle = (word >> 7) & 31;
        const uint8_t cell = word & 127;
        if (state.prim == BITMAPS)
            drawBitmap(x, y, handle, cell);
        else
            fillRect(x, y, x + 1, y + 1, state.color, state.alpha);
        return;
    }

    switch (op)
    {
        case 2: // CLEAR_COLOR_RGB
            state.clearColor = word & 0xFFFFFF;
            break;
        case 38: // CLEAR
            if (word & 4)
                std::fill(framebuffer.begin(), framebuffer.end(), state.clearColor);
            break;
        case 4: // COLOR_RGB
            state.color = word & 0xFFFFFF;
            break;
        case 16: // COLOR_A
            state.alpha = word & 0xFF;
            break;
        case 15: // CLEAR_COLOR_A
            break;
        case 31: // BEGIN
            state.prim = word & 15;
            vertexCount = 0;
            break;
        case 33: // END
            state.prim = 0;
            vertexCount = 0;
            break;
        case 14: // LINE_WIDTH
            state.lineWidth = static_cast<int>(word & 4095);
            break;
        case 13: // POINT_SIZE
            state.pointSize = static_cast<int>(word & 8191);
            break;
        case 27: // SCISSOR_XY
            state.scissorX = (word >> 11) & 2047;
            state.scissorY = word & 2047;
            break;
        case 28: // SCISSOR_SIZE
            state.scissorW = (word >> 12) & 4095;
            state.scissorH = word & 4095;
            break;
        case 1: // BITMAP_SOURCE
            state.bitmapSource = word & 4194303;
            handles[state.bitmapHandle].source = state.bitmapSource;
            break;
        case 5: // BITMAP_HANDLE
            state.bitmapHandle = word & 31;
            break;
        case 6: // CELL
            state.cell = word & 127;
            break;
        case 7: // BITMAP_LAYOUT
        {
            state.bitmapFormat = (word >> 19) & 31;
            state.bitmapStride = (word >> 9) & 1023;
            state.bitmapHeight = word & 511;
            auto& h = handles[state.bitmapHandle];
            h.format = state.bitmapFormat;
            h.stride = state.bitmapStride;
            h.height = state.bitmapHeight;
            h.source = state.bitmapSource;
            break;
        }
        case 8: // BITMAP_SIZE
        {
            state.bitmapWidth = (word >> 9) & 511;
            // height already in layout; size also encodes height
            const uint16_t bh = word & 511;
            auto& h = handles[state.bitmapHandle];
            h.width = state.bitmapWidth;
            if (bh)
                h.height = bh;
            break;
        }
        case 40: // BITMAP_LAYOUT_H
            break;
        case 41: // BITMAP_SIZE_H
            break;
        case 3: // TAG
            state.tag = word & 255;
            break;
        case 20: // TAG_MASK
            state.tagMask = (word & 1) != 0;
            break;
        case 34: // SAVE_CONTEXT
            stack.push_back(state);
            break;
        case 35: // RESTORE_CONTEXT
            if (!stack.empty())
            {
                state = stack.back();
                stack.pop_back();
            }
            break;
        case 11: // BLEND_FUNC
        case 9:  // ALPHA_FUNC
        case 32: // COLOR_MASK
        case 45: // NOP
        case 0:  // DISPLAY / other
            break;
        default:
            if (op != 0)
                ++unknownCommands;
            break;
    }
}

void Ft812Rasterizer::render()
{
    state = GraphicsState{};
    stack.clear();
    vertexCount = 0;
    for (size_t i = 0; i < displayList.size(); ++i)
        executeWord(displayList[i], i, displayList);
}

bool Ft812Rasterizer::savePng(const char* path) const
{
    // Minimal uncompressed PPM (portable, no PNG dependency).
    std::string out = path ? path : "screenshot.ppm";
    if (out.size() >= 4 && (out.ends_with(".png") || out.ends_with(".PNG")))
        out.replace(out.size() - 4, 4, ".ppm");

    FILE* f = std::fopen(out.c_str(), "wb");
    if (!f)
        return false;
    std::fprintf(f, "P6\n%d %d\n255\n", kWidth, kHeight);
    for (uint32_t px : framebuffer)
    {
        const uint8_t rgb[3] = { static_cast<uint8_t>((px >> 16) & 0xFF),
                                 static_cast<uint8_t>((px >> 8) & 0xFF),
                                 static_cast<uint8_t>(px & 0xFF) };
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    return true;
}
