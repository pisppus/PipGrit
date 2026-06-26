#pragma once

#include <PipCore/Config/Features.hpp>

#if PIPCORE_TARGET_DESKTOP

#include <PipCore/Input/Touch.hpp>

namespace pipcore::desktop
{
    class Touch final : public pipcore::Touch
    {
    public:
        [[nodiscard]] bool configure(int8_t, int8_t, int8_t, uint8_t, uint32_t,
                                     uint16_t width, uint16_t height, uint8_t) noexcept override
        {
            _width = width;
            _height = height;
            return true;
        }

        [[nodiscard]] bool begin() noexcept override { return true; }
        void end() noexcept override {}
        void update() noexcept override {}

        [[nodiscard]] bool ready() const noexcept override { return true; }
        [[nodiscard]] uint8_t count() const noexcept override { return 0; }
        [[nodiscard]] pipcore::TouchPoint point(uint8_t) const noexcept override { return {}; }

        void injectPointer(bool down, uint16_t x, uint16_t y) noexcept
        {
            (void)down;
            (void)x;
            (void)y;
        }

    private:
        uint16_t _width = 0;
        uint16_t _height = 0;
    };
}

#endif
