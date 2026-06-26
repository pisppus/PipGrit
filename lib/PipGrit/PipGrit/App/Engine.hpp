#pragma once

#include <PipGrit/Core/Config/Features.hpp>
#include <PipGrit/Hud/Hud.hpp>
#include <PipGrit/Render/Renderer.hpp>
#include <PipGrit/Sim/Grid.hpp>
#include <PipGrit/Sim/SandSim.hpp>
#include <PipCore/Platform.hpp>
#include <PipCore/Input/Touch.hpp>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace pipgrit
{
    class Engine
    {
    public:
        Engine() = default;
        ~Engine();

        Engine(const Engine &) = delete;
        Engine &operator=(const Engine &) = delete;

        [[nodiscard]] bool create(pipcore::Platform *platform) noexcept;
        void destroy() noexcept;

        void update(float dtSeconds) noexcept;

        void selectMaterial(Cell c) noexcept { _brush = c; }
        [[nodiscard]] Cell brush() const noexcept { return _brush; }
        void cycleMaterial() noexcept;

        void setMapMode(MapMode mode) noexcept { _sim.setMapMode(mode); }
        [[nodiscard]] MapMode mapMode() const noexcept { return _sim.mapMode(); }
        void cycleMapMode() noexcept;

        [[nodiscard]] Grid &grid() noexcept { return _grid; }
        [[nodiscard]] Renderer &renderer() noexcept { return _renderer; }
        [[nodiscard]] Hud &hud() noexcept { return _hud; }

        void seedScene() noexcept;

    private:
        void handleTouch() noexcept;
        void stampBrush(int16_t x, int16_t y) noexcept;
        void stampLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) noexcept;
        void handleSelectorBarTouch(const pipcore::TouchPoint &p, uint8_t slot, bool released) noexcept;

        static void simTask(void *param) noexcept;

        pipcore::Platform *_platform = nullptr;

        Grid _grid;
        SandSim _sim;
        Renderer _renderer;
        Hud _hud;

        Cell _brush = Sand;
        int16_t _brushRadius = PIPGRIT_BRUSH_RADIUS;

        float _acc = 0.0f;
        float _stepDt = 1.0f / static_cast<float>(PIPGRIT_SIM_HZ);
        uint32_t _frameSeq = 0;

        TaskHandle_t _simTaskHandle = nullptr;
        TaskHandle_t _renderTaskHandle = nullptr;

        struct LastTouch {
            int16_t x = -1;
            int16_t y = -1;
            bool active = false;
        };
        LastTouch _lastTouch[2] = {};

        int16_t _touchStartX[2] = {0, 0};
        int16_t _scrollStartOffset = 0;
        bool _isDragging[2] = {false, false};

        int16_t _selectorY = 280; 
    };
}