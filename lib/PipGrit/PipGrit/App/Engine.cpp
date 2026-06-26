#include <PipGrit/App/Engine.hpp>
#include <PipGrit/Sim/Material.hpp>
#include <PipCore/Input/Touch.hpp>

#include <algorithm>
#include <cstdint>
#include <cmath>

namespace pipgrit
{
    namespace
    {
        constexpr Cell kBrushCycle[] = {Sand, Water, Stone, Wood, Wall};
        constexpr uint8_t kBrushCycleCount = sizeof(kBrushCycle) / sizeof(kBrushCycle[0]);
    }

    Engine::~Engine()
    {
        destroy();
    }

    bool Engine::create(pipcore::Platform *platform) noexcept
    {
        destroy();
        if (!platform)
            return false;

        _platform = platform;
        const int16_t w = PIPGRIT_GRID_W;
        const int16_t h = PIPGRIT_GRID_H;

        if (!_grid.create(platform, w, h))
            return false;
        _sim.bind(&_grid);

        if (!_renderer.create(platform, w, h, PIPGRIT_NUM_TILES))
        {
            destroy();
            return false;
        }
        _renderer.bindGrid(&_grid);
        _renderer.bindDisplay(platform->display());

        _hud.bind(platform, &_renderer);
        _acc = 0.0f;
        _frameSeq = 0;

        _selectorY = h - _renderer.bandRows(); 

        for (uint8_t i = 0; i < 2; ++i)
        {
            _lastTouch[i] = LastTouch{};
        }

        _renderTaskHandle = xTaskGetCurrentTaskHandle();

        const BaseType_t created = xTaskCreatePinnedToCore(
            simTask,
            "GritSimTask",
            4096,
            this,
            5,
            &_simTaskHandle,
            0
        );

        if (created != pdPASS)
        {
            destroy();
            return false;
        }

        xTaskNotifyGive(_simTaskHandle);
        return true;
    }

    void Engine::destroy() noexcept
    {
        if (_simTaskHandle)
        {
            vTaskDelete(_simTaskHandle);
            _simTaskHandle = nullptr;
        }
        _renderer.destroy();
        _grid.destroy();
        _platform = nullptr;
    }

    void Engine::simTask(void *param) noexcept
    {
        Engine *self = static_cast<Engine *>(param);
        while (true)
        {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            (void)self->_sim.step();
            xTaskNotifyGive(self->_renderTaskHandle);
            vTaskDelay(1);
        }
    }

    void Engine::cycleMaterial() noexcept
    {
        uint8_t i = 0;
        for (; i < kBrushCycleCount; ++i)
            if (kBrushCycle[i] == _brush)
                break;
        i = (i + 1) % kBrushCycleCount;
        _brush = kBrushCycle[i];
    }

    void Engine::cycleMapMode() noexcept
    {
        MapMode current = _sim.mapMode();
        MapMode next = MapMode::Solid;
        switch (current)
        {
        case MapMode::Solid:
            next = MapMode::Wrap;
            break;
        case MapMode::Wrap:
            next = MapMode::Void;
            break;
        case MapMode::Void:
            next = MapMode::Solid;
            break;
        }
        _sim.setMapMode(next);
    }

    void Engine::stampBrush(int16_t x, int16_t y) noexcept
    {
        _grid.brush(x, y, _brushRadius, _brush, _frameSeq * 2654435761u);
    }

    void Engine::stampLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) noexcept
    {
        const int16_t dx = std::abs(x1 - x0);
        const int16_t dy = std::abs(y1 - y0);
        const float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));

        if (dist < 1.0f)
        {
            stampBrush(x1, y1);
            return;
        }

        const float step = std::max(2.0f, static_cast<float>(_brushRadius) * 0.5f);
        int16_t steps = static_cast<int16_t>(dist / step);
        if (steps < 1)
            steps = 1;

        const float xStep = static_cast<float>(x1 - x0) / steps;
        const float yStep = static_cast<float>(y1 - y0) / steps;

        for (int16_t i = 0; i <= steps; ++i)
        {
            const int16_t lx = static_cast<int16_t>(x0 + i * xStep);
            const int16_t ly = static_cast<int16_t>(y0 + i * yStep);
            stampBrush(lx, ly);
        }
    }

    void Engine::handleSelectorBarTouch(const pipcore::TouchPoint &p, uint8_t slot, bool released) noexcept
    {
        const int16_t localY = p.y - _selectorY;

        if (localY < 14 && !released)
        {
            const int16_t tappedX = p.x;
            for (size_t c = 0; c < 4; ++c)
            {
                if (tappedX >= _hud.catX(c) - 4 && tappedX < _hud.catX(c) + _hud.catW(c) + 4)
                {
                    _hud.selectCategory(static_cast<uint8_t>(c));
                    selectMaterial(_hud.currentMaterial());
                    break;
                }
            }
            return;
        }

        if (p.state == pipcore::TouchState::Pressed)
        {
            _touchStartX[slot] = static_cast<int16_t>(p.x);
            _scrollStartOffset = _hud.scrollX();
            _isDragging[slot] = false;
        }
        else if (p.state == pipcore::TouchState::Moved || p.state == pipcore::TouchState::Held)
        {
            const int16_t dx = static_cast<int16_t>(p.x) - _touchStartX[slot];
            if (std::abs(dx) > 6)
            {
                _isDragging[slot] = true;
            }

            if (_isDragging[slot])
            {
                const int16_t targetScroll = _scrollStartOffset - dx;
                _hud.setScrollX(std::clamp<int16_t>(targetScroll, 0, _hud.maxScrollX()));
            }
        }
        
        if (released)
        {
            if (!_isDragging[slot])
            {
                if (_lastTouch[slot].y - _selectorY >= 14)
                {
                    const int16_t tappedX = _lastTouch[slot].x + _hud.scrollX();
                    for (size_t i = 0; i < _hud.activeItemCount(); ++i)
                    {
                        const int16_t ix = _hud.activeItemX(i);
                        const int16_t iw = _hud.activeItemW(i);
                        if (tappedX >= ix && tappedX < ix + iw)
                        {
                            _hud.setSelectedActiveIdx(static_cast<uint8_t>(i));
                            selectMaterial(_hud.currentMaterial());
                            break;
                        }
                    }
                }
            }
            _isDragging[slot] = false;
        }
    }

    void Engine::handleTouch() noexcept
    {
#if PIPCORE_ENABLE_TOUCH
        pipcore::Touch *const touch = _platform->touch();
        if (!touch)
            return;
        touch->update();
        const uint8_t n = touch->count();
        
        bool activeThisFrame[2] = {false, false};

        for (uint8_t i = 0; i < n; ++i)
        {
            const pipcore::TouchPoint p = touch->point(i);
            if (p.active())
            {
                const uint8_t slot = (p.id < 2) ? p.id : 0;
                activeThisFrame[slot] = true;

                if (p.y >= _selectorY)
                {
                    handleSelectorBarTouch(p, slot, false);
                }
                else
                {
                    if (p.state == pipcore::TouchState::Pressed || !_lastTouch[slot].active || _lastTouch[slot].y >= _selectorY)
                    {
                        stampBrush(static_cast<int16_t>(p.x), static_cast<int16_t>(p.y));
                    }
                    else
                    {
                        stampLine(_lastTouch[slot].x, _lastTouch[slot].y, static_cast<int16_t>(p.x), static_cast<int16_t>(p.y));
                    }
                }

                _lastTouch[slot].x = static_cast<int16_t>(p.x);
                _lastTouch[slot].y = static_cast<int16_t>(p.y);
                _lastTouch[slot].active = true;
            }
        }

        for (uint8_t slot = 0; slot < 2; ++slot)
        {
            if (!activeThisFrame[slot])
            {
                if (_lastTouch[slot].active)
                {
                    if (_lastTouch[slot].y >= _selectorY)
                    {
                        pipcore::TouchPoint releasePoint;
                        releasePoint.x = _lastTouch[slot].x;
                        releasePoint.y = _lastTouch[slot].y;
                        releasePoint.state = pipcore::TouchState::Released;
                        handleSelectorBarTouch(releasePoint, slot, true);
                    }
                    _lastTouch[slot].active = false;
                }
            }
        }
#else
        (void)this;
#endif
    }

    void Engine::update(float dtSeconds) noexcept
    {
        if (!_platform)
            return;

        _hud.beginFrame();

        handleTouch();

        if (_simTaskHandle)
        {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }

        if (_simTaskHandle)
        {
            xTaskNotifyGive(_simTaskHandle);
        }

        int16_t activeTouchX = -1;
        int16_t activeTouchY = -1;
        for (uint8_t i = 0; i < 2; ++i)
        {
            if (_lastTouch[i].active)
            {
                activeTouchX = _lastTouch[i].x;
                activeTouchY = _lastTouch[i].y;
                break;
            }
        }

        _renderer.present(&_hud, _grid.countNonEmpty(),
                          (PIPCORE_ENABLE_TOUCH && _platform->touch()) ? _platform->touch()->count() : 0,
                          activeTouchX, activeTouchY);

        _hud.endFrame();
        ++_frameSeq;
    }

    void Engine::seedScene() noexcept
    {
        const int16_t w = _grid.width();
        const int16_t h = _grid.height();
        if (w <= 0 || h <= 0)
            return;

        _grid.fillRect(0, h - 4, w, 4, Stone);
        const int16_t wallX1 = w / 3;
        const int16_t wallX2 = (2 * w) / 3;
        _grid.fillRect(wallX1, h - 14, 3, 10, Stone);
        _grid.fillRect(wallX2 - 3, h - 14, 3, 10, Stone);
        _grid.brush(w / 2, 8, 10, Sand, 0xDEADBEEFu);
    }
}