#pragma once

#include <PipGrit/Core/Config/Features.hpp>
#include <PipGrit/Sim/Material.hpp>
#include <PipCore/Platform.hpp>
#include <cstdint>

namespace pipgrit
{
    class Grid
    {
    public:
        Grid() = default;
        ~Grid();

        Grid(const Grid &) = delete;
        Grid &operator=(const Grid &) = delete;

        [[nodiscard]] bool create(pipcore::Platform *platform, int16_t width, int16_t height) noexcept;
        void destroy() noexcept;
        void clear() noexcept;

        [[nodiscard]] int16_t width() const noexcept { return _w; }
        [[nodiscard]] int16_t height() const noexcept { return _h; }
        [[nodiscard]] uint16_t capacity() const noexcept { return static_cast<uint16_t>(_w) * static_cast<uint16_t>(_h); }

        [[nodiscard]] uint8_t *cells() noexcept { return _cells; }
        [[nodiscard]] const uint8_t *cells() const noexcept { return _cells; }

        [[nodiscard]] uint8_t *temps() noexcept { return _temps; }
        [[nodiscard]] const uint8_t *temps() const noexcept { return _temps; }

        [[nodiscard]] inline Cell get(int16_t x, int16_t y) const noexcept
        {
            return static_cast<Cell>(_cells[static_cast<size_t>(y) * _w + x]);
        }

        inline void set(int16_t x, int16_t y, Cell c) noexcept
        {
            _cells[static_cast<size_t>(y) * _w + x] = static_cast<uint8_t>(c);
        }

        [[nodiscard]] inline uint8_t temp(int16_t x, int16_t y) const noexcept
        {
            return _temps[static_cast<size_t>(y >> 2) * (_w >> 2) + (x >> 2)];
        }

        inline void setTemp(int16_t x, int16_t y, uint8_t t) noexcept
        {
            _temps[static_cast<size_t>(y >> 2) * (_w >> 2) + (x >> 2)] = t;
        }

        [[nodiscard]] inline bool inBounds(int16_t x, int16_t y) const noexcept
        {
            return static_cast<uint16_t>(x) < static_cast<uint16_t>(_w) &&
                   static_cast<uint16_t>(y) < static_cast<uint16_t>(_h);
        }

        void fill(Cell c) noexcept;
        void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, Cell c) noexcept;

        void brush(int16_t cx, int16_t cy, int16_t radius, Cell c, uint32_t seed) noexcept;

        [[nodiscard]] uint32_t countNonEmpty() const noexcept;

        void diffuseHeat() noexcept;

        [[nodiscard]] uint8_t localPressure(int16_t cx, int16_t cy) const noexcept;

    private:
        pipcore::Platform *_platform = nullptr;
        uint8_t *_cells = nullptr;
        uint8_t *_temps = nullptr;
        int16_t _w = 0;
        int16_t _h = 0;
    };
}