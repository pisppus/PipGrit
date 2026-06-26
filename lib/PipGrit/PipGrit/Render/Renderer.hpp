#pragma once

#include <PipGrit/Core/Config/Features.hpp>
#include <PipGrit/Sim/Grid.hpp>
#include <PipCore/Display.hpp>
#include <PipCore/Platform.hpp>
#include <cstdint>

namespace pipgrit
{
    class Hud;

    class Renderer
    {
    public:
        Renderer() = default;
        ~Renderer();

        Renderer(const Renderer &) = delete;
        Renderer &operator=(const Renderer &) = delete;

        [[nodiscard]] bool create(pipcore::Platform *platform, int16_t width, int16_t height, int16_t numTiles) noexcept;
        void destroy() noexcept;

        void bindDisplay(pipcore::Display *display) noexcept { _display = display; }
        void bindGrid(const Grid *grid) noexcept { _grid = grid; }

        [[nodiscard]] const Grid *grid() const noexcept { return _grid; }

        [[nodiscard]] int16_t width() const noexcept { return _w; }
        [[nodiscard]] int16_t height() const noexcept { return _h; }
        [[nodiscard]] int16_t bandRows() const noexcept { return _bandRows; }

        void present(Hud *hud = nullptr, uint32_t cellCount = 0, uint8_t touches = 0, int16_t touchX = -1, int16_t touchY = -1) noexcept;
        void presentBand(int16_t bandIndex) noexcept;

    private:
        uint32_t packBand(uint16_t *dst, int16_t y0, int16_t rows) const noexcept;

        pipcore::Platform *_platform = nullptr;
        pipcore::Display *_display = nullptr;
        const Grid *_grid = nullptr;

        uint16_t *_scratch[2] = {nullptr, nullptr};
        int16_t _w = 0;
        int16_t _h = 0;
        int16_t _bandRows = 0;
        int16_t _bandStride = 0;

        uint32_t _frameCount = 0;
    };
}