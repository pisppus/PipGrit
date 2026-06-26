#pragma once

#include <PipCore/Config/Features.hpp>

#if !PIPCORE_TARGET_ESP32
#error "pipcore::esp32::services::Touch requires ESP32"
#endif

#include <PipCore/Input/Touch.hpp>
#include <PipCore/Platforms/ESP32/Peripherals/Ft6336u.hpp>

namespace pipcore::esp32::services
{
    class Touch final : public pipcore::Touch
    {
    public:
        Touch() = default;
        ~Touch() override;

        [[nodiscard]] bool configure(int8_t sda,
                                     int8_t scl,
                                     int8_t intr,
                                     uint8_t i2cAddr,
                                     uint32_t freqHz,
                                     uint16_t width,
                                     uint16_t height,
                                     uint8_t rotation) noexcept override;

        [[nodiscard]] bool begin() noexcept override;
        void end() noexcept override;
        void update() noexcept override;

        [[nodiscard]] bool ready() const noexcept override { return _ready; }
        [[nodiscard]] uint8_t count() const noexcept override { return _reported; }
        [[nodiscard]] pipcore::TouchPoint point(uint8_t index) const noexcept override;

        static void markTouched(Touch *self) noexcept;

    private:
        struct Slot
        {
            uint8_t id = 0xFF;
            uint16_t x = 0;
            uint16_t y = 0;
            bool present = false;
            bool wasPresent = false;
            pipcore::TouchState state = pipcore::TouchState::Released;
        };

        void remap(uint16_t inX, uint16_t inY, uint16_t &outX, uint16_t &outY) const noexcept;
        Slot *findSlotById(uint8_t id) noexcept;
        Slot *findFreeSlot() noexcept;

        Ft6336u _dev;
        Slot _slots[MaxPoints] = {};

        uint16_t _width = 0;
        uint16_t _height = 0;
        uint8_t _rotation = 0;
        int8_t _intr = -1;
        bool _ready = false;
        bool _isrAttached = false;
        uint8_t _reported = 0;

        volatile bool _touchedFlag = false;
    };
}
