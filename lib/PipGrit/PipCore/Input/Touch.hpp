#pragma once

#include <PipCore/Config/Features.hpp>
#include <cstdint>

namespace pipcore
{
    enum class TouchState : uint8_t
    {
        Released = 0,
        Pressed = 1,
        Held = 2,
        Moved = 3
    };

    struct TouchPoint
    {
        uint16_t x = 0;
        uint16_t y = 0;
        uint16_t pressure = 0;
        uint8_t id = 0;
        TouchState state = TouchState::Released;

        [[nodiscard]] bool active() const noexcept { return state != TouchState::Released; }
    };

    class Touch
    {
    public:
        static inline constexpr uint8_t MaxPoints = 2;

        virtual ~Touch() = default;

        [[nodiscard]] virtual bool configure(int8_t sda,
                                             int8_t scl,
                                             int8_t intr,
                                             uint8_t i2cAddr,
                                             uint32_t freqHz,
                                             uint16_t width,
                                             uint16_t height,
                                             uint8_t rotation) noexcept = 0;

        [[nodiscard]] virtual bool begin() noexcept = 0;
        virtual void end() noexcept = 0;

        virtual void update() noexcept = 0;

        [[nodiscard]] virtual bool ready() const noexcept = 0;
        [[nodiscard]] virtual uint8_t count() const noexcept = 0;
        [[nodiscard]] virtual TouchPoint point(uint8_t index) const noexcept = 0;
        [[nodiscard]] virtual bool touched() const noexcept { return count() > 0; }
    };
}
