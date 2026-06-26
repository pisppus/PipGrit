#pragma once

#include <PipGrit/Sim/Grid.hpp>
#include <cstdint>

namespace pipgrit
{
    enum class MapMode : uint8_t
    {
        Solid = 0,
        Wrap = 1,
        Void = 2
    };

    class SandSim
    {
    public:
        SandSim() = default;

        void bind(Grid *grid) noexcept { _grid = grid; }
        [[nodiscard]] Grid *grid() const noexcept { return _grid; }

        void setMapMode(MapMode mode) noexcept { _mapMode = mode; }
        [[nodiscard]] MapMode mapMode() const noexcept { return _mapMode; }

        [[nodiscard]] uint32_t step() noexcept;
        [[nodiscard]] uint32_t stepCount() const noexcept { return _stepCount; }

    private:
        [[nodiscard]] uint32_t stepPowder(uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y) noexcept;
        [[nodiscard]] uint32_t stepLiquid(uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y) noexcept;
        [[nodiscard]] uint32_t stepFire(uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y) noexcept;
        [[nodiscard]] uint32_t stepSteam(uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y) noexcept;
        [[nodiscard]] uint32_t stepAsh(uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y) noexcept;
        [[nodiscard]] uint32_t stepLava(uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y) noexcept;
        [[nodiscard]] uint32_t stepGunpowder(uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y) noexcept;

        Grid *_grid = nullptr;
        uint32_t _rng = 0x12345678u;
        uint32_t _stepCount = 0;
        MapMode _mapMode = MapMode::Solid;
    };
}