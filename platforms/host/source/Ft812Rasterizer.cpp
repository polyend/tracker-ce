#include "Ft812Rasterizer.h"
#include "FT81x.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
int decodeVertexCoord(int16_t v, int fracBits)
{
    // VERTEX_FORMAT(frac): Tracker uses frac=0 (pixel coords). Default HW is 4 (1/16).
    if (fracBits <= 0)
        return v;
    return v / (1 << fracBits);
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
    ramDl.clear();
    stack.clear();
    state = GraphicsState{};
    cmdWrite = 0;
    unknownCommands = 0;
    vertexCount = 0;
    paintEnabled = true;
    frameDirtyFlag = false;
    scratchTransform = Affine2D{};
}

void Ft812Rasterizer::matrixLoadIdentity()
{
    scratchTransform = Affine2D{};
}

void Ft812Rasterizer::matrixTranslate(int32_t txFixed, int32_t tyFixed)
{
    const float tx = static_cast<float>(txFixed) / 65536.f;
    const float ty = static_cast<float>(tyFixed) / 65536.f;
    // scratch = scratch * T(tx, ty)
    scratchTransform.e += scratchTransform.a * tx + scratchTransform.b * ty;
    scratchTransform.f += scratchTransform.c * tx + scratchTransform.d * ty;
}

void Ft812Rasterizer::matrixRotate(int32_t angleFixed)
{
    // 65536 = full turn. Use OpenGL-style CCW matrix (a=cos,b=-sin,c=sin,d=cos);
    // matches Tracker song/label vertical text (CMD_ROTATE 270°) on host.
    const float rad =
      static_cast<float>(angleFixed) * (6.28318530718f / 65536.f);
    const float co = std::cos(rad);
    const float si = std::sin(rad);
    const float na = scratchTransform.a * co + scratchTransform.b * si;
    const float nb = -scratchTransform.a * si + scratchTransform.b * co;
    const float nc = scratchTransform.c * co + scratchTransform.d * si;
    const float nd = -scratchTransform.c * si + scratchTransform.d * co;
    scratchTransform.a = na;
    scratchTransform.b = nb;
    scratchTransform.c = nc;
    scratchTransform.d = nd;
}

void Ft812Rasterizer::matrixScale(int32_t sxFixed, int32_t syFixed)
{
    const float sx = static_cast<float>(sxFixed) / 65536.f;
    const float sy = static_cast<float>(syFixed) / 65536.f;
    scratchTransform.a *= sx;
    scratchTransform.b *= sy;
    scratchTransform.c *= sx;
    scratchTransform.d *= sy;
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

    // Match device API_SendString: length==0 means strlen(string).
    const uint32_t rl = static_cast<uint32_t>(std::strlen(string));
    if (length == 0 || length > rl)
        length = rl;

    // Payload is [chars...][NUL] then pad to a multiple of 4 bytes.
    uint32_t total = length + 1;
    const uint32_t pad = (4 - (total & 3)) & 3;
    total += pad;

    uint32_t offset = 0;
    while (offset < total)
    {
        uint32_t word = 0;
        for (int b = 0; b < 4 && offset < total; ++b, ++offset)
        {
            uint8_t v = 0;
            if (offset < length)
                v = static_cast<uint8_t>(string[offset]);
            // offset==length → NUL; offset>length → pad zeros
            reinterpret_cast<uint8_t*>(&word)[b] = v;
        }
        writeCmd32(word);
    }
    return static_cast<uint8_t>(total);
}

void Ft812Rasterizer::beginList()
{
    displayList.clear();
}

void Ft812Rasterizer::endList()
{
    // keep displayList for render()
}

bool Ft812Rasterizer::rectInsideScissor(int x0, int y0, int x1, int y1) const
{
    return x0 >= state.scissorX && y0 >= state.scissorY &&
           x1 < state.scissorX + state.scissorW && y1 < state.scissorY + state.scissorH &&
           x0 >= 0 && y0 >= 0 && x1 < kWidth && y1 < kHeight;
}

void Ft812Rasterizer::putPixel(int x, int y, uint32_t rgb, uint8_t a)
{
    if (!paintEnabled)
        return;
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
    if (a == 0)
        return;
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
    if (!paintEnabled)
        return;
    if (x0 > x1)
        std::swap(x0, x1);
    if (y0 > y1)
        std::swap(y0, y1);

    // Clip to scissor ∩ screen.
    const int sx0 = std::max({ x0, state.scissorX, 0 });
    const int sy0 = std::max({ y0, state.scissorY, 0 });
    const int sx1 = std::min({ x1, state.scissorX + state.scissorW - 1, kWidth - 1 });
    const int sy1 = std::min({ y1, state.scissorY + state.scissorH - 1, kHeight - 1 });
    if (sx0 > sx1 || sy0 > sy1)
        return;

    if (a >= 255)
    {
        for (int y = sy0; y <= sy1; ++y)
        {
            uint32_t* row = framebuffer.data() + static_cast<size_t>(y) * kWidth + sx0;
            std::fill(row, row + (sx1 - sx0 + 1), rgb);
        }
        return;
    }

    for (int y = sy0; y <= sy1; ++y)
        for (int x = sx0; x <= sx1; ++x)
            putPixel(x, y, rgb, a);
}

void Ft812Rasterizer::drawLine(int x0, int y0, int x1, int y1, uint32_t rgb, uint8_t a)
{
    if (!paintEnabled)
        return;
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

uint8_t Ft812Rasterizer::sampleBitmap(const BitmapHandle& h,
                                      uint32_t sourceBase,
                                      int px,
                                      int py,
                                      uint32_t& outRgb) const
{
    outRgb = 0;
    if (px < 0 || py < 0 || px >= h.width || py >= h.height)
        return 0;
    const uint32_t row = sourceBase + static_cast<uint32_t>(py) * h.stride;
    if (row >= kRamGSize)
        return 0;

    switch (h.format)
    {
        case L1:
        {
            const uint32_t addr = row + static_cast<uint32_t>(px / 8);
            if (addr >= kRamGSize)
                return 0;
            const uint8_t bit = (ramG[addr] >> (7 - (px & 7))) & 1;
            return bit ? 255 : 0;
        }
        case L2:
        {
            const uint32_t addr = row + static_cast<uint32_t>(px / 4);
            if (addr >= kRamGSize)
                return 0;
            const uint8_t shift = static_cast<uint8_t>((3 - (px & 3)) * 2);
            return static_cast<uint8_t>(((ramG[addr] >> shift) & 0x3) * 85);
        }
        case L4:
        {
            const uint32_t addr = row + static_cast<uint32_t>(px / 2);
            if (addr >= kRamGSize)
                return 0;
            const uint8_t v = (px & 1) ? (ramG[addr] & 0xF) : (ramG[addr] >> 4);
            return static_cast<uint8_t>(v * 17);
        }
        case L8:
        {
            const uint32_t addr = row + static_cast<uint32_t>(px);
            if (addr >= kRamGSize)
                return 0;
            return ramG[addr];
        }
        case RGB565:
        {
            const uint32_t addr = row + static_cast<uint32_t>(px) * 2;
            if (addr + 1 >= kRamGSize)
                return 0;
            uint16_t c = 0;
            std::memcpy(&c, &ramG[addr], 2);
            outRgb = rgb565to888(c);
            return 255;
        }
        default:
        {
            const uint32_t addr = row + static_cast<uint32_t>(px);
            if (addr >= kRamGSize)
                return 0;
            return ramG[addr];
        }
    }
}

void Ft812Rasterizer::drawBitmap(int x, int y, uint8_t handle, uint8_t cell)
{
    if (!paintEnabled)
        return;
    if (handle >= 32)
        return;
    const BitmapHandle& h = handles[handle];
    if (h.width == 0 || h.height == 0 || h.stride == 0)
        return;

    const uint32_t cellBytes = static_cast<uint32_t>(h.stride) * h.height;
    uint32_t sourceBase = h.source;
    if (h.isFont)
    {
        if (cell < h.firstChar)
            return;
        sourceBase = h.source + static_cast<uint32_t>(cell - h.firstChar) * cellBytes;
    }
    else if (cell != 0)
    {
        sourceBase = h.source + static_cast<uint32_t>(cell) * cellBytes;
    }
    if (sourceBase >= kRamGSize)
        return;

    const bool luminanceAlpha = (h.format == L1 || h.format == L2 || h.format == L4 ||
                                 h.format == L8 || h.format > 16);
    const bool useXform = !state.transform.isIdentity();

    // Fast path: opaque color + L4 glyph, no transform, fully inside scissor.
    if (!useXform && luminanceAlpha && h.format == L4 && state.alpha >= 255 &&
        rectInsideScissor(x, y, x + h.width - 1, y + h.height - 1))
    {
        const uint32_t color = state.color;
        for (int py = 0; py < h.height; ++py)
        {
            const uint32_t row = sourceBase + static_cast<uint32_t>(py) * h.stride;
            if (row >= kRamGSize)
                break;
            uint32_t* dst = framebuffer.data() + static_cast<size_t>(y + py) * kWidth + x;
            for (int px = 0; px < h.width; ++px)
            {
                const uint32_t addr = row + static_cast<uint32_t>(px / 2);
                if (addr >= kRamGSize)
                    break;
                const uint8_t nibble = (px & 1) ? (ramG[addr] & 0xF) : (ramG[addr] >> 4);
                if (nibble != 0)
                    dst[px] = color;
            }
        }
        return;
    }

    for (int py = 0; py < h.height; ++py)
    {
        for (int px = 0; px < h.width; ++px)
        {
            uint32_t rgb = 0;
            const uint8_t a = sampleBitmap(h, sourceBase, px, py, rgb);
            if (a == 0)
                continue;
            int dx = x + px;
            int dy = y + py;
            if (useXform)
            {
                float ox = 0.f, oy = 0.f;
                state.transform.map(static_cast<float>(px) + 0.5f,
                                    static_cast<float>(py) + 0.5f,
                                    ox,
                                    oy);
                dx = static_cast<int>(std::lround(ox + static_cast<float>(x)));
                dy = static_cast<int>(std::lround(oy + static_cast<float>(y)));
            }
            if (luminanceAlpha)
            {
                const uint8_t aa =
                  static_cast<uint8_t>((static_cast<uint16_t>(a) * state.alpha) / 255);
                putPixel(dx, dy, state.color, aa);
            }
            else
            {
                putPixel(dx, dy, rgb, state.alpha);
            }
        }
    }
}

uint8_t Ft812Rasterizer::fontCharWidth(uint8_t handle, unsigned char ch) const
{
    if (handle >= 32)
        return 0;
    const BitmapHandle& h = handles[handle];
    if (!h.isFont || h.metricsAddr + ch >= kRamGSize)
        return 0;
    return ramG[h.metricsAddr + ch];
}

void Ft812Rasterizer::drawText(int x, int y, int font, uint16_t options, const char* text)
{
    if (!paintEnabled || !text)
        return;

    const uint8_t handle = static_cast<uint8_t>(font);
    if (handle < 32 && handles[handle].isFont)
    {
        const BitmapHandle& h = handles[handle];
        int textW = 0;
        for (int i = 0; text[i]; ++i)
            textW += fontCharWidth(handle, static_cast<unsigned char>(text[i]));

        int drawX = x;
        int drawY = y;
        if (options & OPT_CENTERX)
            drawX -= textW / 2;
        if (options & OPT_CENTERY)
            drawY -= static_cast<int>(h.height) / 2;
        if (options & OPT_RIGHTX)
            drawX -= textW;

        for (int i = 0; text[i]; ++i)
        {
            const unsigned char chv = static_cast<unsigned char>(text[i]);
            const uint8_t cw = fontCharWidth(handle, chv);
            if (cw > 0)
                drawBitmap(drawX, drawY, handle, chv);
            drawX += cw;
        }
        return;
    }

    // Built-in ROM font fallback (handles 16–31): blocky stand-in.
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
        if (chv > 32)
            fillRect(drawX + 1, drawY + 1, drawX + cw - 2, drawY + ch - 2, state.color, state.alpha);
        drawX += cw;
    }
}

void Ft812Rasterizer::memcpyRam(uint32_t dest, uint32_t src, uint32_t num)
{
    if (num == 0)
        return;

    if (src == RAM_DL)
    {
        const uint32_t avail = static_cast<uint32_t>(ramDl.size() * 4);
        const uint32_t n = std::min(num, avail);
        if (dest + n > kRamGSize)
            return;
        std::memcpy(ramG.data() + dest, ramDl.data(), n);
        return;
    }
    if (dest == RAM_DL)
    {
        if (src + num > kRamGSize)
            return;
        ramDl.resize((num + 3) / 4);
        std::memcpy(ramDl.data(), ramG.data() + src, num);
        return;
    }
    if (dest + num > kRamGSize || src + num > kRamGSize)
        return;
    std::memmove(ramG.data() + dest, ramG.data() + src, num);
}

void Ft812Rasterizer::executeDlStream(const std::vector<uint32_t>& words)
{
    for (size_t j = 0; j < words.size(); ++j)
        executeWord(words[j], j, words);
}

void Ft812Rasterizer::executeCopro(uint32_t cmd, size_t& i, const std::vector<uint32_t>& list)
{
    auto need = [&](size_t n) -> bool { return i + n < list.size(); };

    if (cmd == CMD_DLSTART)
    {
        // REG_CMD_DL reset — capture happens in render() around this marker.
        return;
    }
    if (cmd == CMD_SWAP)
    {
        return;
    }
    if (cmd == CMD_MEMCPY && need(3))
    {
        const uint32_t dest = list[i + 1];
        const uint32_t src = list[i + 2];
        const uint32_t num = list[i + 3];
        i += 3;
        memcpyRam(dest, src, num);
        return;
    }
    if (cmd == CMD_APPEND && need(2))
    {
        const uint32_t ptr = list[i + 1];
        const uint32_t num = list[i + 2];
        i += 2;
        if (ptr >= kRamGSize || num == 0)
            return;
        const uint32_t bytes = std::min(num, static_cast<uint32_t>(kRamGSize - ptr));
        const size_t words = bytes / 4;
        std::vector<uint32_t> sub(words);
        std::memcpy(sub.data(), ramG.data() + ptr, words * 4);
        executeDlStream(sub);
        return;
    }
    if (cmd == CMD_MEMZERO && need(2))
    {
        const uint32_t ptr = list[i + 1];
        const uint32_t num = list[i + 2];
        i += 2;
        if (ptr < kRamGSize)
        {
            const uint32_t n = std::min(num, static_cast<uint32_t>(kRamGSize - ptr));
            std::memset(ramG.data() + ptr, 0, n);
        }
        return;
    }
    if (cmd == CMD_MEMSET && need(3))
    {
        const uint32_t ptr = list[i + 1];
        const uint32_t value = list[i + 2];
        const uint32_t num = list[i + 3];
        i += 3;
        if (ptr < kRamGSize)
        {
            const uint32_t n = std::min(num, static_cast<uint32_t>(kRamGSize - ptr));
            std::memset(ramG.data() + ptr, static_cast<int>(value & 0xFF), n);
        }
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
    if (cmd == CMD_GRADIENT && need(4))
    {
        // CMD + (x0,y0) + rgb0 + (x1,y1) + rgb1  => 4 param words
        const int16_t x0 = static_cast<int16_t>(list[i + 1] & 0xFFFF);
        const int16_t y0 = static_cast<int16_t>((list[i + 1] >> 16) & 0xFFFF);
        const uint32_t c0 = list[i + 2];
        const int16_t x1 = static_cast<int16_t>(list[i + 3] & 0xFFFF);
        const int16_t y1 = static_cast<int16_t>((list[i + 3] >> 16) & 0xFFFF);
        const uint32_t c1 = list[i + 4];
        i += 4;
        // FT81x fills the scissor region along the gradient axis. Tracker often passes x0==x1=0
        // for a full-width vertical fade.
        int xLeft = std::min(x0, x1);
        int xRight = std::max(x0, x1);
        if (xLeft == xRight)
        {
            xLeft = state.scissorX;
            xRight = state.scissorX + state.scissorW - 1;
        }
        const int yStart = std::min(y0, y1);
        const int yEnd = std::max(y0, y1);
        for (int y = yStart; y <= yEnd; ++y)
        {
            float t = (yEnd == yStart) ? 0.f : float(y - yStart) / float(yEnd - yStart);
            auto lerp = [&](int s, int e) { return int(s + (e - s) * t); };
            uint32_t c = (lerp((c0 >> 16) & 0xFF, (c1 >> 16) & 0xFF) << 16) |
                         (lerp((c0 >> 8) & 0xFF, (c1 >> 8) & 0xFF) << 8) |
                         lerp(c0 & 0xFF, c1 & 0xFF);
            fillRect(xLeft, y, xRight, y, c, state.alpha);
        }
        return;
    }
    if (cmd == CMD_TEXT && need(2))
    {
        // Leave i on the last consumed word so render()/executeDlStream's for-loop
        // ++i lands on the next opcode (same convention as fixed-arg co-pro cmds).
        const int16_t x = static_cast<int16_t>(list[i + 1] & 0xFFFF);
        const int16_t y = static_cast<int16_t>((list[i + 1] >> 16) & 0xFFFF);
        const int16_t font = static_cast<int16_t>(list[i + 2] & 0xFFFF);
        const uint16_t options = static_cast<uint16_t>((list[i + 2] >> 16) & 0xFFFF);
        i += 2;
        std::string text;
        while (i + 1 < list.size())
        {
            uint32_t w = list[++i];
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
    if (cmd == CMD_BUTTON && need(3))
    {
        const int16_t x = static_cast<int16_t>(list[i + 1] & 0xFFFF);
        const int16_t y = static_cast<int16_t>((list[i + 1] >> 16) & 0xFFFF);
        const int16_t w = static_cast<int16_t>(list[i + 2] & 0xFFFF);
        const int16_t h = static_cast<int16_t>((list[i + 2] >> 16) & 0xFFFF);
        const int16_t font = static_cast<int16_t>(list[i + 3] & 0xFFFF);
        const uint16_t options = static_cast<uint16_t>((list[i + 3] >> 16) & 0xFFFF);
        i += 3;
        fillRect(x, y, x + w, y + h, state.color, state.alpha);
        std::string text;
        while (i + 1 < list.size())
        {
            uint32_t word = list[++i];
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
    if (cmd == CMD_STOP || cmd == CMD_COLDSTART)
        return;
    if (cmd == CMD_LOADIDENTITY)
    {
        matrixLoadIdentity();
        return;
    }
    if (cmd == CMD_SETMATRIX)
    {
        state.transform = scratchTransform;
        return;
    }
    if (cmd == CMD_TRANSLATE && need(2))
    {
        matrixTranslate(static_cast<int32_t>(list[i + 1]), static_cast<int32_t>(list[i + 2]));
        i += 2;
        return;
    }
    if (cmd == CMD_ROTATE && need(1))
    {
        matrixRotate(static_cast<int32_t>(list[i + 1]));
        ++i;
        return;
    }
    if (cmd == CMD_SCALE && need(2))
    {
        matrixScale(static_cast<int32_t>(list[i + 1]), static_cast<int32_t>(list[i + 2]));
        i += 2;
        return;
    }
    if (cmd == CMD_SETBITMAP && need(3))
    {
        // CMD_SETBITMAP(source, fmt, w, h) — programs the current bitmap handle.
        const uint32_t source = list[i + 1];
        const uint16_t fmt = static_cast<uint16_t>(list[i + 2] & 0xFFFF);
        const uint16_t w = static_cast<uint16_t>((list[i + 2] >> 16) & 0xFFFF);
        const uint16_t h = static_cast<uint16_t>(list[i + 3] & 0xFFFF);
        i += 3;
        auto& bh = handles[state.bitmapHandle];
        bh.source = source;
        bh.format = static_cast<uint8_t>(fmt);
        bh.width = w;
        bh.height = h;
        // Derive stride for common L-formats when not otherwise set.
        if (fmt == L1)
            bh.stride = static_cast<uint16_t>((w + 7) / 8);
        else if (fmt == L2)
            bh.stride = static_cast<uint16_t>((w + 3) / 4);
        else if (fmt == L4)
            bh.stride = static_cast<uint16_t>((w + 1) / 2);
        else if (fmt == L8)
            bh.stride = w;
        else if (fmt == RGB565)
            bh.stride = static_cast<uint16_t>(w * 2);
        else if (bh.stride == 0)
            bh.stride = w;
        bh.isFont = false;
        return;
    }
    if (cmd == CMD_SETFONT && need(2))
    {
        const uint32_t font = list[i + 1] & 31;
        const uint32_t ptr = list[i + 2];
        i += 2;
        if (font < 32 && ptr + 148 <= kRamGSize)
        {
            auto& h = handles[font];
            h.isFont = true;
            h.metricsAddr = ptr;
            h.firstChar = 0;
            h.format = ramG[ptr + 128];
            uint32_t stride = 0, width = 0, height = 0;
            std::memcpy(&stride, &ramG[ptr + 132], 4);
            std::memcpy(&width, &ramG[ptr + 136], 4);
            std::memcpy(&height, &ramG[ptr + 140], 4);
            h.stride = static_cast<uint16_t>(stride);
            h.width = static_cast<uint16_t>(width);
            h.height = static_cast<uint16_t>(height);
            h.source = ptr + 148;
        }
        return;
    }
    if (cmd == CMD_SETFONT2 && need(3))
    {
        // CMD_SETFONT2(font, pointer, firstchar) — metrics @ pointer, glyphs packed from firstchar.
        const uint32_t font = list[i + 1] & 31;
        const uint32_t ptr = list[i + 2];
        const uint32_t first = list[i + 3] & 0xFF;
        i += 3;
        if (font < 32 && ptr + 148 <= kRamGSize)
        {
            auto& h = handles[font];
            h.isFont = true;
            h.metricsAddr = ptr;
            h.firstChar = static_cast<uint8_t>(first);
            // Prefer layout already programmed via BITMAP_* on this handle; fill gaps from metrics.
            if (h.stride == 0 || h.height == 0 || h.width == 0)
            {
                h.format = ramG[ptr + 128];
                uint32_t stride = 0, width = 0, height = 0;
                std::memcpy(&stride, &ramG[ptr + 132], 4);
                std::memcpy(&width, &ramG[ptr + 136], 4);
                std::memcpy(&height, &ramG[ptr + 140], 4);
                h.stride = static_cast<uint16_t>(stride);
                h.width = static_cast<uint16_t>(width);
                h.height = static_cast<uint16_t>(height);
            }
            if (h.source == 0)
                h.source = ptr + 148;
        }
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
        sx = decodeVertexCoord(static_cast<int16_t>(sx), state.vertexFrac);
        sy = decodeVertexCoord(static_cast<int16_t>(sy), state.vertexFrac);

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
        // VERTEX2II always draws a bitmap cell (no BEGIN required on FT81x).
        const int x = (word >> 21) & 511;
        const int y = (word >> 12) & 511;
        const uint8_t handle = (word >> 7) & 31;
        const uint8_t cell = word & 127;
        drawBitmap(x, y, handle, cell);
        return;
    }

    switch (op)
    {
        case 2: // CLEAR_COLOR_RGB
            state.clearColor = word & 0xFFFFFF;
            break;
        case 38: // CLEAR
            if (paintEnabled && (word & 4))
                std::fill(framebuffer.begin(), framebuffer.end(), state.clearColor);
            break;
        case 39: // VERTEX_FORMAT
            state.vertexFrac = static_cast<int>(word & 7);
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
            // Screen footprint after transform — do not clobber source layout dims used for sampling.
            state.bitmapWidth = (word >> 9) & 511;
            break;
        }
        case 21: // BITMAP_TRANSFORM_A (8.8 signed 17-bit)
        case 22: // BITMAP_TRANSFORM_B
        case 24: // BITMAP_TRANSFORM_D
        case 25: // BITMAP_TRANSFORM_E
        {
            int32_t v = static_cast<int32_t>(word & 0x1FFFF);
            if (v & 0x10000)
                v |= ~0x1FFFF;
            const float f = static_cast<float>(v) / 256.f;
            if (op == 21)
                state.transform.a = f;
            else if (op == 22)
                state.transform.b = f;
            else if (op == 24)
                state.transform.c = f;
            else
                state.transform.d = f;
            break;
        }
        case 23: // BITMAP_TRANSFORM_C (1/256 pixel, signed 24-bit)
        case 26: // BITMAP_TRANSFORM_F
        {
            int32_t v = static_cast<int32_t>(word & 0xFFFFFF);
            if (v & 0x800000)
                v |= ~0xFFFFFF;
            const float f = static_cast<float>(v) / 256.f;
            if (op == 23)
                state.transform.e = f;
            else
                state.transform.f = f;
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
    scratchTransform = Affine2D{};
    stack.clear();
    vertexCount = 0;

    // step1 (control rebuild) has DLSTART but no SWAP — still need ramDl for MEMCPY.
    // step2 is often just MEMCPY — must execute, but painting is wasted.
    // step3 has APPEND…DISPLAY/SWAP — this is the only paint that reaches the screen.
    bool hasSwap = false;
    for (uint32_t w : displayList)
    {
        if (w == CMD_SWAP)
        {
            hasSwap = true;
            break;
        }
    }
    paintEnabled = hasSwap;

    // Capture words after CMD_DLSTART into ramDl (simulates REG_CMD_DL / RAM_DL).
    // Tracker step1 builds a control DL, step2 MEMCPYs it from RAM_DL, step3 APPENDs it.
    size_t dlCaptureFrom = static_cast<size_t>(-1);
    for (size_t i = 0; i < displayList.size(); ++i)
    {
        const uint32_t word = displayList[i];
        if (word == CMD_DLSTART)
        {
            ramDl.clear();
            dlCaptureFrom = i + 1;
        }
        else if (word == CMD_SWAP && dlCaptureFrom != static_cast<size_t>(-1))
        {
            ramDl.assign(displayList.begin() + static_cast<std::ptrdiff_t>(dlCaptureFrom),
                         displayList.begin() + static_cast<std::ptrdiff_t>(i));
            dlCaptureFrom = static_cast<size_t>(-1);
        }
        executeWord(word, i, displayList);
    }
    if (dlCaptureFrom != static_cast<size_t>(-1) && dlCaptureFrom <= displayList.size())
    {
        ramDl.assign(displayList.begin() + static_cast<std::ptrdiff_t>(dlCaptureFrom),
                     displayList.end());
    }

    if (hasSwap)
        frameDirtyFlag = true;
    paintEnabled = true;
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
