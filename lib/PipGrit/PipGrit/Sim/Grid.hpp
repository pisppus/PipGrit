#pragma once

#include <PipGrit/Core/Config/Features.hpp>
#include <PipGrit/Sim/Material.hpp>
#include <PipCore/Platform.hpp>
#include <cstdint>
#include <algorithm>

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

        [[nodiscard]] uint8_t *chunks() noexcept { return _chunks; }
        [[nodiscard]] const uint8_t *chunks() const noexcept { return _chunks; }

        [[nodiscard]] int16_t chunkWidth() const noexcept { return _chunkW; }
        [[nodiscard]] int16_t chunkHeight() const noexcept { return _chunkH; }

        [[nodiscard]] uint64_t tileDirtyMaskCurrent() const noexcept { return _tileDirtyMaskCurrent; }

        [[nodiscard]] int16_t activeMinCx() const noexcept { return _activeMinCx; }
        [[nodiscard]] int16_t activeMaxCx() const noexcept { return _activeMaxCx; }
        [[nodiscard]] int16_t activeMinCy() const noexcept { return _activeMinCy; }
        [[nodiscard]] int16_t activeMaxCy() const noexcept { return _activeMaxCy; }

        inline void swapDirtyMasksAndBounds() noexcept
        {
            _tileDirtyMaskCurrent = _tileDirtyMaskNext;
            _tileDirtyMaskNext = 0;

            if (_nextMinCx <= _nextMaxCx && _nextMinCy <= _nextMaxCy)
            {
                _activeMinCx = std::max<int16_t>(0, _nextMinCx);
                _activeMaxCx = std::min<int16_t>(_chunkW - 1, _nextMaxCx);
                _activeMinCy = std::max<int16_t>(0, _nextMinCy);
                _activeMaxCy = std::min<int16_t>(_chunkH - 1, _nextMaxCy);
            }
            else
            {
                _activeMinCx = 0;
                _activeMaxCx = -1;
                _activeMinCy = 0;
                _activeMaxCy = -1;
            }

            _nextMinCx = 0x7FFF;
            _nextMaxCx = -0x7FFF;
            _nextMinCy = 0x7FFF;
            _nextMaxCy = -0x7FFF;
        }

        inline void forceFullDirty() noexcept
        {
            _tileDirtyMaskNext = 0xFFFFFFFFFFFFFFFFULL;
            _tileDirtyMaskCurrent = 0xFFFFFFFFFFFFFFFFULL;
            _activeMinCx = 0;
            _activeMaxCx = _chunkW - 1;
            _activeMinCy = 0;
            _activeMaxCy = _chunkH - 1;
            _nextMinCx = 0;
            _nextMaxCx = _chunkW - 1;
            _nextMinCy = 0;
            _nextMaxCy = _chunkH - 1;
        }

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

        inline void activateChunk(int16_t x, int16_t y) noexcept
        {
            const int16_t cx = x >> 3;
            const int16_t cy = y >> 3;
            if (static_cast<uint16_t>(cx) < static_cast<uint16_t>(_chunkW) &&
                static_cast<uint16_t>(cy) < static_cast<uint16_t>(_chunkH))
            {
                _chunks[static_cast<size_t>(cy) * _chunkW + cx] |= 2;

                const int16_t tx = cx / 10;
                const int16_t ty = cy / 5;
                _tileDirtyMaskNext |= (1ULL << (ty * 6 + tx));

                if (cx < _nextMinCx)
                    _nextMinCx = cx;
                if (cx > _nextMaxCx)
                    _nextMaxCx = cx;
                if (cy < _nextMinCy)
                    _nextMinCy = cy;
                if (cy > _nextMaxCy)
                    _nextMaxCy = cy;
            }
        }

        inline void markActive(int16_t x, int16_t y) noexcept
        {
            activateChunk(x, y);
            const int16_t rx = x & 7;
            const int16_t ry = y & 7;
            if (rx == 0)
                activateChunk(x - 1, y);
            else if (rx == 7)
                activateChunk(x + 1, y);
            if (ry == 0)
                activateChunk(x, y - 1);
            else if (ry == 7)
                activateChunk(x, y + 1);
        }

        inline void forceActivateChunk(int16_t cx, int16_t cy) noexcept
        {
            if (static_cast<uint16_t>(cx) < static_cast<uint16_t>(_chunkW) &&
                static_cast<uint16_t>(cy) < static_cast<uint16_t>(_chunkH))
            {
                _chunks[static_cast<size_t>(cy) * _chunkW + cx] |= 3;

                const int16_t tx = cx / 10;
                const int16_t ty = cy / 5;
                _tileDirtyMaskNext |= (1ULL << (ty * 6 + tx));

                if (cx < _activeMinCx)
                    _activeMinCx = cx;
                if (cx > _activeMaxCx)
                    _activeMaxCx = cx;
                if (cy < _activeMinCy)
                    _activeMinCy = cy;
                if (cy > _activeMaxCy)
                    _activeMaxCy = cy;

                if (cx < _nextMinCx)
                    _nextMinCx = cx;
                if (cx > _nextMaxCx)
                    _nextMaxCx = cx;
                if (cy < _nextMinCy)
                    _nextMinCy = cy;
                if (cy > _nextMaxCy)
                    _nextMaxCy = cy;
            }
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
        uint8_t *_chunks = nullptr;

        int16_t _w = 0;
        int16_t _h = 0;
        int16_t _chunkW = 0;
        int16_t _chunkH = 0;

        uint64_t _tileDirtyMaskNext = 0xFFFFFFFFFFFFFFFFULL;
        uint64_t _tileDirtyMaskCurrent = 0xFFFFFFFFFFFFFFFFULL;

        int16_t _activeMinCx = 0;
        int16_t _activeMaxCx = 0;
        int16_t _activeMinCy = 0;
        int16_t _activeMaxCy = 0;

        int16_t _nextMinCx = 0x7FFF;
        int16_t _nextMaxCx = -0x7FFF;
        int16_t _nextMinCy = 0x7FFF;
        int16_t _nextMaxCy = -0x7FFF;
    };
}