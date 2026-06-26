#include <PipCore/Config/Features.hpp>

#if PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ST7796

#include <PipCore/Displays/ST7796/Driver.hpp>

namespace pipcore::st7796
{
    namespace
    {
        constexpr uint8_t CmdSWRESET = 0x01;
        constexpr uint8_t CmdSLPOUT = 0x11;
        constexpr uint8_t CmdNORON = 0x13;
        constexpr uint8_t CmdINVOFF = 0x20;
        constexpr uint8_t CmdINVON = 0x21;
        constexpr uint8_t CmdDISPON = 0x29;
        constexpr uint8_t CmdCASD = 0x2A;  // CASET
        constexpr uint8_t CmdRASD = 0x2B;  // RASET
        constexpr uint8_t CmdRAMWR = 0x2C; // RAMWR
        constexpr uint8_t CmdMADCTL = 0x36;
        constexpr uint8_t CmdCOLMOD = 0x3A;
        constexpr uint8_t Colmod16bpp = 0x55;

        constexpr uint8_t MadctlMY = 0x80;
        constexpr uint8_t MadctlMX = 0x40;
        constexpr uint8_t MadctlMV = 0x20;
        constexpr uint8_t MadctlBGR = 0x08;

        constexpr uint8_t CmdPVGAMCTRL = 0xE0; // positive
        constexpr uint8_t CmdNVGAMCTRL = 0xE1; // negative
        constexpr uint8_t CmdFRMCTR1 = 0xB1;
        constexpr uint8_t CmdDISPCTRL = 0xB6;

        [[nodiscard]] inline bool sendCmdParam(Driver &, Transport *t,
                                               uint8_t cmd, const uint8_t *params, size_t n)
        {
            if (!t->writeCommand(cmd))
                return false;
            if (n && !t->write(params, n))
                return false;
            return true;
        }
    }

    void Driver::reset()
    {
        *this = Driver();
    }

    bool Driver::configure(Transport *transport,
                           uint16_t width, uint16_t height, uint8_t order,
                           bool invert, bool swap,
                           int16_t xOffset, int16_t yOffset)
    {
        if (__builtin_expect(!transport || !width || !height || xOffset < 0 || yOffset < 0, 0))
        {
            reset();
            _lastError = IoError::InvalidConfig;
            return false;
        }

        _transport = transport;
        _width = _physWidth = width;
        _height = _physHeight = height;
        _xStart = _xOffsetCfg = xOffset;
        _yStart = _yOffsetCfg = yOffset;

        _order = order ? 1u : 0u;

        _invert = invert;
        _swap = swap;
        _initialized = false;
        _lastError = IoError::None;
        _transport->clearError();
        return true;
    }

    bool Driver::hardReset()
    {
        if (__builtin_expect(!_transport, 0))
            return failFromTransport(IoError::NotReady);

        if (__builtin_expect(!_transport->setRst(false), 0))
            return failFromTransport(IoError::Gpio);

        _transport->delayMs(10);

        if (__builtin_expect(!_transport->setRst(true), 0))
            return failFromTransport(IoError::Gpio);

        _transport->delayMs(120);

        return true;
    }

    bool Driver::begin(uint8_t rotation)
    {
        _initialized = false;
        _lastError = IoError::None;

        if (__builtin_expect(!_transport || !_width || !_height, 0))
        {
            _lastError = IoError::InvalidConfig;
            return false;
        }

        _transport->clearError();
        if (__builtin_expect(!_transport->init(), 0))
            return failFromTransport(IoError::NotReady);

        if (__builtin_expect(!hardReset(), 0))
            return false;

        if (__builtin_expect(!sendCommand(CmdSWRESET), 0))
            return false;
        _transport->delayMs(120);

        if (__builtin_expect(!sendCommand(CmdSLPOUT), 0))
            return false;
        _transport->delayMs(120);

        if (__builtin_expect(!sendCommand(CmdCOLMOD), 0))
            return false;
        {
            uint8_t v = Colmod16bpp;
            if (__builtin_expect(!sendBytes(&v, 1), 0))
                return false;
        }

        if (__builtin_expect(!setRotationInternal(rotation), 0))
            return false;

        if (__builtin_expect(!sendCommand(_invert ? CmdINVON : CmdINVOFF), 0))
            return false;

        if (__builtin_expect(!sendCommand(CmdNORON), 0))
            return false;

        if (__builtin_expect(!sendCommand(CmdDISPON), 0))
            return false;
        _transport->delayMs(20);

        _initialized = true;
        _lastError = IoError::None;
        return true;
    }

    bool Driver::setRotation(uint8_t rotation)
    {
        if (__builtin_expect(!_transport || !_initialized, 0))
        {
            _lastError = IoError::NotReady;
            return false;
        }
        if (__builtin_expect(!_transport->waitComplete(), 0))
            return false;

        return setRotationInternal(rotation);
    }

    bool Driver::setRotationInternal(uint8_t rotation)
    {
        if (__builtin_expect(!_transport, 0))
            return failFromTransport(IoError::NotReady);

        _rotation = rotation & 3U;

        const bool isOdd = (_rotation & 1U);

        _width = isOdd ? _physHeight : _physWidth;
        _height = isOdd ? _physWidth : _physHeight;
        _xStart = isOdd ? _yOffsetCfg : _xOffsetCfg;
        _yStart = isOdd ? _xOffsetCfg : _yOffsetCfg;

        const uint8_t bgr = (_order == 1u) ? MadctlBGR : 0u;
        uint8_t madctl = bgr;

        switch (_rotation)
        {
        case 1:
            madctl |= MadctlMV; 
            break;
        case 2:
            madctl |= MadctlMX | MadctlMY;
            break;
        case 3:
            madctl |= MadctlMX | MadctlMY | MadctlMV; 
            break;
        default:
            break;
        }

        if (__builtin_expect(!sendCommand(CmdMADCTL), 0))
            return false;
        if (__builtin_expect(!sendBytes(&madctl, 1), 0))
            return false;

        return true;
    }

    bool Driver::fillScreen565(uint16_t color565, bool swapBytes)
    {
        if (__builtin_expect(!_transport || !_initialized || !_width || !_height, 0))
        {
            _lastError = (_transport && _initialized) ? IoError::InvalidConfig : IoError::NotReady;
            return false;
        }

        if (__builtin_expect(!setAddrWindow(0, 0, static_cast<uint16_t>(_width - 1U), static_cast<uint16_t>(_height - 1U)), 0))
            return false;

        const uint16_t v = swapBytes ? bswap16(color565) : color565;
        const size_t totalPixels = static_cast<size_t>(_width) * static_cast<size_t>(_height);

        if (__builtin_expect(!_transport->fillPixels(v, totalPixels), 0))
            return failFromTransport(IoError::DataTransmit);

        return true;
    }
}

#endif
