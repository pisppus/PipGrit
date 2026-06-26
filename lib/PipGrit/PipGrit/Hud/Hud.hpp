#pragma once

#include <PipGrit/Core/Config/Features.hpp>
#include <PipGrit/Hud/BitmapFont.hpp>
#include <PipGrit/Sim/Material.hpp>
#include <PipCore/Platform.hpp>
#include <cstdint>
#include <algorithm>

namespace pipgrit
{
    class Renderer;

    struct SelectorItem {
        const char *name;
        Cell cell;
        uint16_t color;
        uint16_t textColor;
        uint8_t category;
    };

    inline constexpr SelectorItem kSelectorItems[] = {
        {"SAND", Sand,  pipcore::Sprite::color565(0xD6, 0xB8, 0x4E), 0x0000, 0},
        {"ASH",  Ash,   pipcore::Sprite::color565(0x60, 0x60, 0x65), 0xFFFF, 0},
        {"GUNP", Gunpowder, pipcore::Sprite::color565(0x55, 0x55, 0x5B), 0xFFFF, 0},

        {"WATR", Water, pipcore::Sprite::color565(0x32, 0x6A, 0xC8), 0xFFFF, 1},
        {"OIL",  Oil,   pipcore::Sprite::color565(0x7D, 0x50, 0x47), 0xFFFF, 1},
        {"ACID", Acid,  pipcore::Sprite::color565(0x39, 0xFF, 0x14), 0x0000, 1},
        {"LAVA", Lava,  pipcore::Sprite::color565(0xFF, 0x55, 0x00), 0xFFFF, 1},

        {"WALL", Wall,  pipcore::Sprite::color565(0x32, 0x32, 0x38), 0xFFFF, 2},
        {"STNE", Stone, pipcore::Sprite::color565(0x6E, 0x6E, 0x72), 0xFFFF, 2},
        {"WOOD", Wood,  pipcore::Sprite::color565(0x86, 0x55, 0x2E), 0xFFFF, 2},
        {"ICE",  Ice,   pipcore::Sprite::color565(0x90, 0xC0, 0xFF), 0x0000, 2},

        {"FIRE", Fire,  pipcore::Sprite::color565(0xE6, 0x4A, 0x19), 0xFFFF, 3},
        {"VAPR", Steam, pipcore::Sprite::color565(0xE0, 0xE0, 0xE5), 0x0000, 3}
    };
    inline constexpr size_t kSelectorItemCount = sizeof(kSelectorItems) / sizeof(kSelectorItems[0]);

    struct Category {
        const char *name;
        uint16_t color;
    };

    inline constexpr Category kCategories[] = {
        {"POWDERS", pipcore::Sprite::color565(210, 180, 80)},
        {"LIQUIDS", pipcore::Sprite::color565(50, 150, 220)},
        {"SOLIDS",  pipcore::Sprite::color565(140, 140, 145)},
        {"SPECIAL", pipcore::Sprite::color565(230, 80, 40)}
    };

    class PerfCounter
    {
    public:
        void setPlatform(pipcore::Platform *platform) noexcept { _platform = platform; }
        
        inline void beginFrame() noexcept
        {
            _frameStartUs = (_platform ? _platform->nowUs() : 0);
        }

        inline void endFrame() noexcept
        {
            if (!_platform)
                return;
            const uint64_t now = _platform->nowUs();
            const uint32_t dt = static_cast<uint32_t>(now - _frameStartUs);
            _frameTimeUs = dt;
            _fps = (dt > 0) ? (1000000.0f / static_cast<float>(dt)) : 0.0f;

            if (_accStartUs == 0)
                _accStartUs = _frameStartUs;
            ++_accFrames;
            if (now - _accStartUs >= 1000000ull)
            {
                const float windowFps = static_cast<float>(_accFrames) * 1000000.0f /
                                        static_cast<float>(now - _accStartUs);
                _ring[_ringHead] = static_cast<uint16_t>(windowFps + 0.5f);
                _ringHead = (_ringHead + 1) % 16;
                if (_ringFill < 16)
                    ++_ringFill;

                uint32_t sum = 0;
                for (uint8_t i = 0; i < _ringFill; ++i)
                    sum += _ring[i];
                _avgFps = static_cast<float>(sum) / static_cast<float>(_ringFill);

                _accFrames = 0;
                _accStartUs = now;
            }
        }

        [[nodiscard]] float fps() const noexcept { return _fps; }
        [[nodiscard]] float avgFps() const noexcept { return _avgFps; }
        [[nodiscard]] uint32_t frameTimeUs() const noexcept { return _frameTimeUs; }
        [[nodiscard]] uint32_t frameTimeMs() const noexcept { return (_frameTimeUs + 500) / 1000; }

    private:
        pipcore::Platform *_platform = nullptr;
        uint64_t _frameStartUs = 0;
        uint32_t _frameTimeUs = 0;
        float _fps = 0.0f;
        float _avgFps = 0.0f;

        uint32_t _accFrames = 0;
        uint64_t _accStartUs = 0;
        uint16_t _ring[16] = {};
        uint8_t _ringHead = 0;
        uint8_t _ringFill = 0;
    };

    class Hud
    {
    public:
        void bind(pipcore::Platform *platform, Renderer *renderer) noexcept;

        void setEnabled(bool enabled) noexcept { _enabled = enabled; }
        [[nodiscard]] bool enabled() const noexcept { return _enabled; }
        [[nodiscard]] PerfCounter &perf() noexcept { return _perf; }

        void beginFrame() noexcept { _perf.beginFrame(); }
        void endFrame() noexcept { _perf.endFrame(); }

        void composite(uint16_t *bandBuf, int16_t bandY0, int16_t bandRows, int16_t stride, uint32_t cellCount, uint8_t touches, int16_t touchX = -1, int16_t touchY = -1) noexcept;
        
        void drawSelectorBar(uint16_t *dst, int16_t screenW, int16_t rows) noexcept;

        void setScrollX(int16_t scrollX) noexcept { _scrollX = scrollX; }
        [[nodiscard]] int16_t scrollX() const noexcept { return _scrollX; }
        [[nodiscard]] int16_t maxScrollX() const noexcept { return _maxScrollX; }
        
        void selectCategory(uint8_t catIdx) noexcept;
        [[nodiscard]] uint8_t selectedCategory() const noexcept { return _selectedCategory; }

        [[nodiscard]] int16_t catX(size_t i) const noexcept { return _catX[i]; }
        [[nodiscard]] int16_t catW(size_t i) const noexcept { return _catW[i]; }

        [[nodiscard]] uint8_t activeItemCount() const noexcept { return _activeItemCount; }
        [[nodiscard]] int16_t activeItemX(size_t i) const noexcept { return _activeItemX[i]; }
        [[nodiscard]] int16_t activeItemW(size_t i) const noexcept { return _activeItemW[i]; }

        void setSelectedActiveIdx(uint8_t activeIdx) noexcept {
            if (activeIdx < _activeItemCount) {
                _selectedActiveIdx = activeIdx;
                _selectedIdx = _activeItemIndices[activeIdx];
            }
        }
        [[nodiscard]] Cell currentMaterial() const noexcept {
            return kSelectorItems[_selectedIdx].cell;
        }

    private:
        pipcore::Platform *_platform = nullptr;
        Renderer *_renderer = nullptr;
        PerfCounter _perf;
        bool _enabled = (PIPGRIT_HUD_ENABLED != 0);

        int16_t _scrollX = 0;
        int16_t _maxScrollX = 0;
        int16_t _totalWidth = 0;

        uint8_t _selectedCategory = 0;
        int16_t _catX[4] = {};
        int16_t _catW[4] = {};

        uint8_t _activeItemCount = 0;
        uint8_t _activeItemIndices[kSelectorItemCount] = {};
        int16_t _activeItemX[kSelectorItemCount] = {};
        int16_t _activeItemW[kSelectorItemCount] = {};

        uint8_t _selectedActiveIdx = 0;
        uint8_t _selectedIdx = 0;
    };
}