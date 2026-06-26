#include <PipGrit/Render/Renderer.hpp>
#include <PipGrit/Sim/Material.hpp>
#include <PipGrit/Hud/Hud.hpp>

#include <algorithm>
#include <cstring>

namespace pipgrit
{
    Renderer::~Renderer()
    {
        destroy();
    }

    bool Renderer::create(pipcore::Platform *platform, int16_t width, int16_t height, int16_t numTiles) noexcept
    {
        destroy();
        if (!platform || width <= 0 || height <= 0 || numTiles <= 0)
            return false;

        _bandRows = std::max<int16_t>(1, height / numTiles);
        _w = width;
        _h = height;

        const int16_t tileW = 160;
        const int16_t tileH = _bandRows;
        const size_t tilePixels = static_cast<size_t>(tileW) * tileH;

        const size_t bytes = tilePixels * sizeof(uint16_t);
        for (int i = 0; i < 2; ++i)
        {
            _scratch[i] = static_cast<uint16_t *>(
                platform->allocAligned(bytes, 4, pipcore::AllocCaps::PreferInternal));
            if (!_scratch[i])
            {
                destroy();
                return false;
            }
        }
        _platform = platform;
        _frameCount = 0;
        _forceFullRedraw = true;
        return true;
    }

    void Renderer::destroy() noexcept
    {
        if (_platform)
        {
            for (int i = 0; i < 2; ++i)
            {
                if (_scratch[i])
                    _platform->freeAligned(_scratch[i]);
                _scratch[i] = nullptr;
            }
        }
        else
        {
            _scratch[0] = _scratch[1] = nullptr;
        }
        _platform = nullptr;
        _w = _h = _bandStride = _bandRows = 0;
    }

    uint32_t Renderer::packTile(uint16_t *dst, int16_t tx, int16_t ty, int16_t tileW, int16_t tileH) const noexcept
    {
        if (!_grid || !dst)
            return 0;

        const int16_t w = _grid->width();
        const uint8_t *cells = _grid->cells();
        const uint8_t *temps = _grid->temps();
        uint16_t *out = dst;

        const int16_t wt = w >> 2;

        const int16_t x0 = tx * tileW;
        const int16_t y0 = ty * tileH;

        for (int16_t y = y0; y < y0 + tileH; ++y)
        {
            const uint8_t *row = cells + static_cast<size_t>(y) * w;
            const uint8_t *temp_row = temps + (static_cast<size_t>(y >> 2) * wt);

            for (int16_t x = x0; x < x0 + tileW; ++x)
            {
                const uint8_t raw = row[x];
                const uint8_t cellType = raw & 0x7F;
                uint16_t colorSwapped = kColorLut[cellType];

                const uint8_t t = temp_row[x >> 2];

                if (cellType == Fire)
                {
                    uint32_t r = 0, g = 0, b = 0;
                    if (t > 210)
                    {
                        r = 31;
                        g = 48 + ((t - 210) * 15 / 45);
                        b = (t - 210) * 20 / 45;
                    }
                    else if (t > 140)
                    {
                        r = 31;
                        g = 16 + ((t - 140) * 32 / 70);
                        b = 0;
                    }
                    else
                    {
                        r = 10 + ((t - 40) * 21 / 100);
                        g = (t - 40) * 16 / 100;
                        b = 0;
                    }
                    uint16_t color = static_cast<uint16_t>((r << 11) | (g << 5) | b);
                    colorSwapped = pipcore::Sprite::swap16(color);
                }
                else if (cellType == Steam)
                {
                    uint32_t noise = ((static_cast<uint32_t>(x >> 1) + static_cast<uint32_t>(y >> 1)) + (_frameCount >> 2)) & 3;
                    uint32_t alpha = 2 + noise;

                    uint32_t r = (28 * alpha + 1 * (8 - alpha)) >> 3;
                    uint32_t g = (56 * alpha + 1 * (8 - alpha)) >> 3;
                    uint32_t b = (28 * alpha + 1 * (8 - alpha)) >> 3;

                    uint16_t color = static_cast<uint16_t>((r << 11) | (g << 5) | b);
                    colorSwapped = pipcore::Sprite::swap16(color);
                }
                else if (cellType == Lava)
                {
                    uint32_t flicker = ((static_cast<uint32_t>(x >> 2) + static_cast<uint32_t>(y >> 1)) + (_frameCount >> 1)) & 7;
                    uint32_t r = 31;
                    uint32_t g = 6 + (flicker * 2);
                    uint32_t b = 0;
                    uint16_t color = static_cast<uint16_t>((r << 11) | (g << 5) | b);
                    colorSwapped = pipcore::Sprite::swap16(color);
                }
                else if (t > 40)
                {
                    uint32_t intensity = (static_cast<uint32_t>(t - 40) * 304) >> 8;

                    if (cellType == Empty)
                    {
                        uint32_t r = (intensity * 24) >> 8;
                        uint32_t g = (intensity * 10) >> 8;
                        r = std::min<uint32_t>(31, r + 1);
                        g = std::min<uint32_t>(63, g + 1);
                        uint16_t color = static_cast<uint16_t>((r << 11) | (g << 5) | 1);
                        colorSwapped = pipcore::Sprite::swap16(color);
                    }
                    else if (cellType == Stone || cellType == Wall || cellType == Sand || cellType == Ash || cellType == Wood)
                    {
                        uint16_t color = __builtin_bswap16(colorSwapped);
                        uint32_t r = (color >> 11) & 0x1F;
                        uint32_t g = (color >> 5) & 0x3F;
                        uint32_t b = color & 0x1F;

                        r = std::min<uint32_t>(31, r + ((intensity * 24) >> 8));
                        g = std::min<uint32_t>(63, g + ((intensity * 16) >> 8));
                        b = (b * (256 - intensity)) >> 8;

                        color = static_cast<uint16_t>((r << 11) | (g << 5) | b);
                        colorSwapped = __builtin_bswap16(color);
                    }
                }

                *out++ = colorSwapped;
            }
        }
        return static_cast<uint32_t>(out - dst);
    }

    bool Renderer::isTileDirty(int16_t tx, int16_t ty, const uint8_t *chunks, int16_t chunkW, int16_t chunkH) const noexcept
    {
        if (_forceFullRedraw)
            return true;

        if (ty == 0)
            return true;

        if (!chunks)
            return true;

        const int16_t startCx = tx * 20;
        const int16_t endCx = std::min<int16_t>((tx + 1) * 20, chunkW);
        const int16_t startCy = ty * 5;
        const int16_t endCy = std::min<int16_t>((ty + 1) * 5, chunkH);

        for (int16_t cy = startCy; cy < endCy; ++cy)
        {
            const uint8_t *chunkRow = chunks + static_cast<size_t>(cy) * chunkW;
            for (int16_t cx = startCx; cx < endCx; ++cx)
            {
                if (chunkRow[cx] != 0)
                    return true;
            }
        }
        return false;
    }

    void Renderer::presentBand(int16_t bandIndex) noexcept
    {
        (void)bandIndex;
    }

    void Renderer::present(Hud *hud, uint32_t cellCount, uint8_t touches, int16_t touchX, int16_t touchY, bool drawSelector) noexcept
    {
        _frameCount++;

        if (!_display || !_grid || _w <= 0 || _h <= 0)
            return;

        const int16_t tileW = 80;
        const int16_t tileH = _bandRows;
        const int16_t numCols = _w / tileW;
        const int16_t numRows = (_h + tileH - 1) / tileH;

        uint64_t dirtyMask = _grid->tileDirtyMaskCurrent();
        if (_forceFullRedraw)
        {
            dirtyMask = 0xFFFFFFFFFFFFFFFFULL;
        }

        bool inflight = false;
        int inflightSlot = 0;
        int currentSlot = 0;

        const int16_t simRows = numRows - 1;

        for (int16_t ty = 0; ty < simRows; ++ty)
        {
            for (int16_t tx = 0; tx < numCols; ++tx)
            {
                const int16_t tileIdx = ty * numCols + tx;

                bool isDirty = (dirtyMask & (1ULL << tileIdx)) != 0;

                if (ty == 0)
                {
                    isDirty = true;
                }

                if (!isDirty)
                    continue;

                const int slot = currentSlot;

                if (inflight && inflightSlot == slot)
                {
                    _display->waitDMA();
                    inflight = false;
                }

                packTile(_scratch[slot], tx, ty, tileW, tileH);

                if (ty == 0 && hud)
                {
                    hud->composite(_scratch[slot], tx * tileW, 0, tileH, tileW, cellCount, touches, touchX, touchY);
                }

                _display->writeRect565Async(tx * tileW, ty * tileH, tileW, tileH, _scratch[slot], tileW);
                inflight = true;
                inflightSlot = slot;
                currentSlot ^= 1;
            }
        }

        if (drawSelector && hud)
        {
            const int16_t ty = numRows - 1;

            for (int16_t tx = 0; tx < numCols; ++tx)
            {
                const int slot = currentSlot;

                if (inflight && inflightSlot == slot)
                {
                    _display->waitDMA();
                    inflight = false;
                }

                hud->drawSelectorBarTile(_scratch[slot], tx * tileW, tileW, tileH);

                _display->writeRect565Async(tx * tileW, ty * tileH, tileW, tileH, _scratch[slot], tileW);
                inflight = true;
                inflightSlot = slot;
                currentSlot ^= 1;
            }
        }

        if (inflight)
            _display->waitDMA();

        _forceFullRedraw = false;
    }
}