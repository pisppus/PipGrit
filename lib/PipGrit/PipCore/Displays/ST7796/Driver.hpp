#pragma once

#include <PipCore/Displays/ST7789/Driver.hpp>
#include <cstdint>
#include <cstddef>

#if defined(ESP_PLATFORM) || defined(ESP32)
#include <esp_attr.h>
#else
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#endif

namespace pipcore::st7796
{
    using Transport = pipcore::st7789::Transport;
    using IoError = pipcore::st7789::IoError;
    using pipcore::st7789::ioErrorText;
    using pipcore::st7789::bswap16;
    using pipcore::st7789::copySwap565;

    class Driver
    {
    public:
        Driver() = default;

        [[nodiscard]] bool configure(Transport *transport,
                                     uint16_t width,
                                     uint16_t height,
                                     uint8_t order = 1,
                                     bool invert = true,
                                     bool swap = false,
                                     int16_t xOffset = 0,
                                     int16_t yOffset = 0);

        [[nodiscard]] bool begin(uint8_t rotation);
        [[nodiscard]] bool setRotation(uint8_t rotation);
        void reset();

        [[nodiscard]] IoError lastError() const noexcept { return _lastError; }
        [[nodiscard]] const char *lastErrorText() const noexcept { return ioErrorText(_lastError); }

        [[nodiscard]] uint16_t width() const noexcept { return _width; }
        [[nodiscard]] uint16_t height() const noexcept { return _height; }
        [[nodiscard]] bool swapBytes() const noexcept { return _swap; }

        [[nodiscard]] inline bool __attribute__((always_inline)) waitComplete()
        {
            if (!_transport)
                return false;
            return _transport->waitComplete();
        }

        [[nodiscard]] inline bool __attribute__((always_inline)) waitOldest()
        {
            if (!_transport)
                return false;
            return _transport->waitOldest();
        }

        [[nodiscard]] inline bool __attribute__((always_inline)) setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
        {
            if (!_transport || !_initialized || !_width || !_height || x1 < x0 || y1 < y0)
            {
                _lastError = (_transport && _initialized) ? IoError::InvalidConfig : IoError::NotReady;
                return false;
            }
            if (x0 >= _width || y0 >= _height)
                return true;

            if (x1 >= _width)
                x1 = static_cast<uint16_t>(_width - 1U);
            if (y1 >= _height)
                y1 = static_cast<uint16_t>(_height - 1U);

            const uint16_t xs = static_cast<uint16_t>(x0 + _xStart);
            const uint16_t xe = static_cast<uint16_t>(x1 + _xStart);
            const uint16_t ys = static_cast<uint16_t>(y0 + _yStart);
            const uint16_t ye = static_cast<uint16_t>(y1 + _yStart);

            if (!_transport->writeAddrWindow(xs, xe, ys, ye))
            {
                return failFromTransport(IoError::CommandTransmit);
            }

            return true;
        }

        [[nodiscard]] inline bool __attribute__((always_inline)) writePixels565(const uint16_t *pixels, size_t pixelCount)
        {
            if (!_transport || !_initialized || !pixels || !pixelCount)
            {
                _lastError = (_transport && _initialized) ? IoError::InvalidConfig : IoError::NotReady;
                return false;
            }
            return sendPixels(pixels, pixelCount * sizeof(uint16_t));
        }

        [[nodiscard]] inline bool __attribute__((always_inline)) writePixels565Async(const uint16_t *pixels, size_t pixelCount)
        {
            if (!_transport || !_initialized || !pixels || !pixelCount)
            {
                _lastError = (_transport && _initialized) ? IoError::InvalidConfig : IoError::NotReady;
                return false;
            }
            return _transport->writePixelsAsync(pixels, pixelCount * sizeof(uint16_t));
        }

        [[nodiscard]] bool fillScreen565(uint16_t color565, bool swapBytes = false);

    private:
        [[nodiscard]] bool hardReset();
        [[nodiscard]] bool setRotationInternal(uint8_t rotation);
        [[nodiscard]] inline bool __attribute__((always_inline)) failFromTransport(IoError fallback)
        {
            IoError err = fallback;
            if (_transport)
            {
                const IoError transportErr = _transport->lastError();
                if (transportErr != IoError::None)
                {
                    err = transportErr;
                }
            }
            _lastError = err;
            return false;
        }

        inline bool __attribute__((always_inline)) sendCommand(uint8_t cmd)
        {
            if (!_transport)
                return failFromTransport(IoError::NotReady);
            if (_transport->writeCommand(cmd))
                return true;
            return failFromTransport(IoError::CommandTransmit);
        }

        inline bool __attribute__((always_inline)) sendBytes(const void *data, size_t len)
        {
            if (!_transport)
                return failFromTransport(IoError::NotReady);
            if (_transport->write(data, len))
                return true;
            return failFromTransport(IoError::DataTransmit);
        }

        inline bool __attribute__((always_inline)) sendPixels(const void *data, size_t len)
        {
            if (!_transport)
                return failFromTransport(IoError::NotReady);
            if (_transport->writePixels(data, len))
                return true;
            return failFromTransport(IoError::DataTransmit);
        }

    private:
        Transport *_transport = nullptr;

        uint16_t _width = 0;
        uint16_t _height = 0;
        uint16_t _physWidth = 0;
        uint16_t _physHeight = 0;

        uint8_t _rotation = 0;
        int16_t _xStart = 0;
        int16_t _yStart = 0;
        int16_t _xOffsetCfg = 0;
        int16_t _yOffsetCfg = 0;

        uint8_t _order = 1;
        bool _invert = true;
        bool _swap = false;
        bool _initialized = false;
        IoError _lastError = IoError::None;
    };
}
