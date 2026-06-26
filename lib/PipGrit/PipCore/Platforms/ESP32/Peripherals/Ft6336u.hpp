#pragma once

#include <PipCore/Config/Features.hpp>

#if !PIPCORE_TARGET_ESP32
#error "pipcore::esp32::Ft6336u requires ESP32"
#endif

#include <cstdint>
#include <cstddef>

namespace pipcore::esp32
{
    class Ft6336u
    {
    public:
        static inline constexpr uint8_t DefaultAddress = 0x38;
        static inline constexpr uint8_t MaxPoints = 2;

        struct RawPoint
        {
            uint16_t x = 0;
            uint16_t y = 0;
            uint8_t weight = 0;
            uint8_t id = 0;
            bool valid = false;
        };

        Ft6336u() = default;

        void configure(int8_t sda,
                       int8_t scl,
                       int8_t intr,
                       uint8_t addr7,
                       uint32_t freqHz,
                       int port) noexcept;

        [[nodiscard]] bool init() noexcept;
        void deinit() noexcept;

        [[nodiscard]] uint8_t read(RawPoint *out) noexcept;

        [[nodiscard]] bool ready() const noexcept { return _initialized; }

    private:
        [[nodiscard]] bool writeReg(uint8_t reg, uint8_t value) noexcept;
        [[nodiscard]] bool readReg(uint8_t reg, uint8_t &out) noexcept;
        [[nodiscard]] bool readRegs(uint8_t reg, uint8_t *buf, size_t len) noexcept;

        int8_t _sda = -1;
        int8_t _scl = -1;
        int8_t _intr = -1;
        uint8_t _addr7 = DefaultAddress;
        uint32_t _freqHz = 400000;
        int _port = 0;
        bool _initialized = false;
        bool _busOwned = false;
    };
}
