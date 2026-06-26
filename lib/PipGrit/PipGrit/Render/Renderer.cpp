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
        _bandStride = static_cast<int16_t>(width) * _bandRows;

        const size_t bytes = static_cast<size_t>(_bandStride) * sizeof(uint16_t);
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

    uint32_t Renderer::packBand(uint16_t *dst, int16_t y0, int16_t rows) const noexcept
    {
        if (!_grid || !dst)
            return 0;

        const int16_t w = _w;
        const uint8_t *cells = _grid->cells();
        const uint8_t *temps = _grid->temps();
        uint16_t *out = dst;

        const int16_t wt = w >> 2;

        for (int16_t y = y0; y < y0 + rows; ++y)
        {
            const uint8_t *row = cells + static_cast<size_t>(y) * w;
            const uint8_t *temp_row = temps + (static_cast<size_t>(y >> 2) * wt);

            for (int16_t x = 0; x < w; ++x)
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

    void Renderer::presentBand(int16_t bandIndex) noexcept
    {
        if (!_display || !_grid || _bandStride <= 0)
            return;

        const int16_t y0 = bandIndex * _bandRows;
        if (y0 >= _h)
            return;
        const int16_t rows = std::min<int16_t>(_bandRows, static_cast<int16_t>(_h - y0));

        uint16_t *dst = _scratch[bandIndex & 1];
        packBand(dst, y0, rows);
        _display->writeRect565(0, y0, _w, rows, dst, _w);
    }

    void Renderer::present(Hud *hud, uint32_t cellCount, uint8_t touches, int16_t touchX, int16_t touchY) noexcept
    {
        _frameCount++;

        if (!_display || !_grid || _bandStride <= 0 || _w <= 0 || _h <= 0)
            return;

        const int16_t bandCount = (_h + _bandRows - 1) / _bandRows;
        bool inflight = false;
        int inflightSlot = 0;

        for (int16_t b = 0; b < bandCount; ++b)
        {
            const int16_t y0 = b * _bandRows;
            const int16_t rows = std::min<int16_t>(_bandRows, static_cast<int16_t>(_h - y0));
            const int slot = b & 1;

            if (inflight && inflightSlot == slot)
            {
                _display->waitDMA();
                inflight = false;
            }

            if (b == bandCount - 1 && hud)
            {
                hud->drawSelectorBar(_scratch[slot], _w, rows);
            }
            else
            {
                packBand(_scratch[slot], y0, rows);
                if (b == 0 && hud)
                {
                    hud->composite(_scratch[slot], y0, rows, _w, cellCount, touches, touchX, touchY);
                }
            }

            _display->writeRect565Async(0, y0, _w, rows, _scratch[slot], _w);
            inflight = true;
            inflightSlot = slot;
        }

        if (inflight)
            _display->waitDMA();
    }
}