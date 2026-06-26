#include <PipGrit/Sim/Grid.hpp>

#include <algorithm>
#include <cstring>

namespace pipgrit
{
    Grid::~Grid()
    {
        destroy();
    }

    bool Grid::create(pipcore::Platform *platform, int16_t width, int16_t height) noexcept
    {
        destroy();
        if (!platform || width <= 0 || height <= 0)
            return false;

        const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
        _cells = static_cast<uint8_t *>(platform->alloc(count, pipcore::AllocCaps::PreferInternal));
        if (!_cells)
            return false;

        const size_t tempCount = static_cast<size_t>(width >> 2) * static_cast<size_t>(height >> 2);
        _temps = static_cast<uint8_t *>(platform->alloc(tempCount, pipcore::AllocCaps::PreferInternal));
        if (!_temps)
        {
            platform->free(_cells);
            _cells = nullptr;
            return false;
        }

        _platform = platform;
        _w = width;
        _h = height;
        std::memset(_cells, 0, count);
        std::memset(_temps, 20, tempCount);
        return true;
    }

    void Grid::destroy() noexcept
    {
        if (_cells && _platform)
            _platform->free(_cells);
        if (_temps && _platform)
            _platform->free(_temps);
        _cells = nullptr;
        _temps = nullptr;
        _platform = nullptr;
        _w = _h = 0;
    }

    void Grid::clear() noexcept
    {
        if (_cells)
            std::memset(_cells, 0, static_cast<size_t>(_w) * static_cast<size_t>(_h));
        if (_temps)
            std::memset(_temps, 20, static_cast<size_t>(_w >> 2) * static_cast<size_t>(_h >> 2));
    }

    void Grid::fill(Cell c) noexcept
    {
        if (_cells)
            std::memset(_cells, static_cast<uint8_t>(c), static_cast<size_t>(_w) * static_cast<size_t>(_h));
    }

    void Grid::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, Cell c) noexcept
    {
        if (!_cells)
            return;
        const int16_t x1 = std::max<int16_t>(x, 0);
        const int16_t y1 = std::max<int16_t>(y, 0);
        const int16_t x2 = std::min<int16_t>(x + w, _w);
        const int16_t y2 = std::min<int16_t>(y + h, _h);
        const uint8_t v = static_cast<uint8_t>(c);
        for (int16_t yy = y1; yy < y2; ++yy)
            std::memset(_cells + static_cast<size_t>(yy) * _w + x1, v, static_cast<size_t>(x2 - x1));
    }

    void Grid::brush(int16_t cx, int16_t cy, int16_t radius, Cell c, uint32_t seed) noexcept
    {
        if (!_cells)
            return;
        const int32_t r2 = static_cast<int32_t>(radius) * radius;
        const int16_t x1 = std::max<int16_t>(cx - radius, 0);
        const int16_t y1 = std::max<int16_t>(cy - radius, 0);
        const int16_t x2 = std::min<int16_t>(cx + radius + 1, _w);
        const int16_t y2 = std::min<int16_t>(cy + radius + 1, _h);
        const uint8_t v = static_cast<uint8_t>(c);

        uint32_t s = seed ^ 0x9E3779B9u;
        for (int16_t yy = y1; yy < y2; ++yy)
        {
            const int32_t dy = yy - cy;
            uint8_t *row = _cells + static_cast<size_t>(yy) * _w;
            for (int16_t xx = x1; xx < x2; ++xx)
            {
                const int32_t dx = xx - cx;
                if (dx * dx + dy * dy > r2)
                    continue;

                const Cell existing = static_cast<Cell>(row[xx]);
                const bool existingIsSolid = (matterOf(existing) == Matter::Solid);

                bool canOverwrite = false;
                if (!existingIsSolid)
                {
                    canOverwrite = true;
                }
                else
                {
                    if (c == Empty)
                        canOverwrite = true;
                    if (c == Fire && existing == Wood)
                        canOverwrite = true;
                }

                if (canOverwrite)
                {
                    if (c == Sand || c == Ash)
                    {
                        s = s * 1664525u + 1013904223u;
                        if ((s >> 24) < 51)
                            continue;
                    }

                    const size_t tempIdx = static_cast<size_t>(yy >> 2) * (_w >> 2) + (xx >> 2);

                    if (c == Fire)
                    {
                        if (existing == Empty)
                        {
                            row[xx] = Fire;
                            if (_temps) _temps[tempIdx] = 255;
                        }
                        else
                        {
                            if (_temps) _temps[tempIdx] = 255;
                        }
                    }
                    else
                    {
                        row[xx] = v;
                        
                        if (_temps)
                        {
                            if (c == Ice) _temps[tempIdx] = 0;
                            else if (c == Steam) _temps[tempIdx] = 110;
                            else _temps[tempIdx] = 20;
                        }
                    }
                }
            }
        }
    }

    uint32_t Grid::countNonEmpty() const noexcept
    {
        if (!_cells)
            return 0;
        const size_t n = static_cast<size_t>(_w) * static_cast<size_t>(_h);
        uint32_t count = 0;
        for (size_t i = 0; i < n; ++i)
            count += (_cells[i] != static_cast<uint8_t>(Empty));
        return count;
    }

    void Grid::diffuseHeat() noexcept
    {
        if (!_temps)
            return;

        const int16_t wt = _w >> 2;
        const int16_t ht = _h >> 2;
        const int32_t total = static_cast<int32_t>(wt) * ht;

        if (reinterpret_cast<uintptr_t>(_temps) % 4 == 0)
        {
            uint32_t *temps32 = reinterpret_cast<uint32_t *>(_temps);
            const int32_t words = total >> 2;
            for (int32_t i = 0; i < words; ++i)
            {
                uint32_t val = temps32[i];
                if (val == 0x14141414U)
                    continue;

                uint8_t t0 = val & 0xFF;
                uint8_t t1 = (val >> 8) & 0xFF;
                uint8_t t2 = (val >> 16) & 0xFF;
                uint8_t t3 = (val >> 24) & 0xFF;

                auto cool = [](uint8_t t) -> uint8_t {
                    if (t > 20) return t - 1;
                    if (t < 20) return t + 1;
                    return 20;
                };

                temps32[i] = cool(t0) | (cool(t1) << 8) | (cool(t2) << 16) | (cool(t3) << 24);
            }
        }
        else
        {
            for (int32_t i = 0; i < total; ++i)
            {
                uint8_t t = _temps[i];
                if (t > 20) _temps[i] = t - 1;
                else if (t < 20) _temps[i] = t + 1;
            }
        }

        for (int16_t y = ht - 2; y > 0; --y)
        {
            uint8_t *row_me = _temps + static_cast<size_t>(y) * wt;
            const uint8_t *row_down = _temps + static_cast<size_t>(y + 1) * wt;
            const uint8_t *row_up = _temps + static_cast<size_t>(y - 1) * wt;

            for (int16_t x = 1; x < wt - 1; ++x)
            {
                uint32_t sum = static_cast<uint32_t>(row_me[x]) * 10
                             + row_me[x - 1]
                             + row_me[x + 1]
                             + row_up[x]
                             + row_down[x] * 3;
                row_me[x] = static_cast<uint8_t>(sum >> 4);
            }
        }
    }

    uint8_t Grid::localPressure(int16_t cx, int16_t cy) const noexcept
    {
        if (!_cells)
            return 0;
        uint16_t count = 0;
        for (int16_t dy = -2; dy <= 2; ++dy)
        {
            for (int16_t dx = -2; dx <= 2; ++dx)
            {
                int16_t xx = cx + dx;
                int16_t yy = cy + dy;
                if (inBounds(xx, yy))
                {
                    count += (_cells[static_cast<size_t>(yy) * _w + xx] != Empty);
                }
            }
        }
        return static_cast<uint8_t>((count * 99) / 25);
    }
}